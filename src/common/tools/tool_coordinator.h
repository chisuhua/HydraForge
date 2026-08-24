// src/common/tools/tool_coordinator.h
// 功能描述：ToolCoordinator — standalone middleware (Oracle Option C)
// 设计依据：ADR-0031 §决策 5 (Oracle session ses_0ed4408faffeLv8VfrC0s5PzW7, 2026-06-29)
//          + §决策 4 (Layer check) + §决策 7 (Audit log)
//          + ADR-0051 §决策 5 (Phase 6 RAII nesting guard, 2026-07-15)
// 作者：AgenticDSL Phase3 / Sprint 14 C4 ship + Phase 6 W1 escalation triggers
// 最后修改日期：2026-07-15
#pragma once

#include <memory>
#include <stop_token>
#include <string>
#include <unordered_map>
#include <vector>

#include "agenticdsl/contract/iinteraction_bus.h"
#include "agenticdsl/contract/itool_hook_registry.h"
#include "agenticdsl/policy/iexecution_policy.h"
#include "common/policy/approval_handler.h"
#include "common/tools/registry.h"  // IToolRegistry + ToolResult
#include "core/types/tool_result.h"  // ErrorCode (ADR-0073 D3 ValidationStage 错误映射)

namespace agenticdsl {

/**
 * @brief ToolCoordinatorNestingGuard — RAII 嵌套深度防护 (ADR-0051 §5.5)
 *
 * 每次 ToolCoordinator::execute() 开始时构造, 结束时析构。
 * 检测:
 *  - nesting depth > 2 → HARD KILL (throw, 防止无界递归)
 *  - cycle detected (同一工具名已在 call stack 中) → HARD KILL
 *
 * 已知限制 (v1, ADR-0051 §不变量):
 *  thread_local 变量绑定到 DomainWorkerPool 的 jthread worker 线程 (ADR-0020, Sprint 3)。
 *  跨线程 cycle (e.g. G1 on Worker A → G3 on Worker B → G1 on Worker A) 不可检测。
 *  这是 v1 接受限制, 记录在 ADR-0051 §不变量中。
 */
class ToolCoordinatorNestingGuard {
 public:
  /// @param tool_name 当前工具名
  /// @param bus 审计总线 (optional, nullptr 跳过 cycle_detected_log 发射)
  ToolCoordinatorNestingGuard(const std::string& tool_name,
                               const std::shared_ptr<IInteractionBus>& bus);
  ~ToolCoordinatorNestingGuard();

  ToolCoordinatorNestingGuard(const ToolCoordinatorNestingGuard&) = delete;
  ToolCoordinatorNestingGuard& operator=(const ToolCoordinatorNestingGuard&) = delete;

 private:
  std::string name_;
};

/**
 * @brief ToolCoordinator — standalone middleware (ADR-0031 §决策 5)
 *
 * 内部流程:
 * 0. NestingGuard 构造 (RAII, ADR-0051 §5.5) — depth check + cycle detection
 * 1. check_layer_permission(meta, ctx) → denied return ToolResult::error
 * 2. ApprovalHandler::process_request(meta, ctx, preview) → denied return ToolResult::error
 * 3. emit "tool.audit.invoked"
 * 4. registry.call_tool()
 * 5. emit "tool.audit.completed"
 * 6. return ToolResult
 */
class ToolCoordinator {
 public:
  /**
   * @brief 构造函数
   * @param registry IToolRegistry 引用 (non-owning)
   * @param policy IExecutionPolicy shared_ptr (委托给内部 ApprovalHandler)
   * @param callback ApprovalCallback (传入内部 ApprovalHandler)
   * @param bus IInteractionBus shared_ptr (可选, null=skip audit emit)
   * @param default_timeout_ms ApprovalHandler 默认超时 (ms, 默认 300000 = 5 分钟)
   */
  ToolCoordinator(IToolRegistry& registry,
                  std::shared_ptr<IExecutionPolicy> policy,
                  ApprovalCallback callback,
                  std::shared_ptr<IInteractionBus> bus = nullptr,
                  int default_timeout_ms = 300000);

  /**
   * @brief 主入口 — 执行一次工具调用 (含 layer + approval + audit)
   *
   * @param meta 工具元数据 (ToolMetadata V2)
   * @param ctx 工具调用上下文 (ToolCallContext)
   * @param args 工具参数
   * @param token stop_token for cancellation (default: empty token)
   * @return ToolResult (执行结果或错误)
   */
  ToolResult execute(const ToolMetadata& meta,
                     const ToolCallContext& ctx,
                     const std::unordered_map<std::string, std::string>& args,
                     std::stop_token token = {});

  /**
   * @brief 注入 hook registry（可空；空 = 保持原 6 步行为）
   * @param registry IToolHookRegistry 指针 (non-owning, 可为 nullptr)
   */
  void set_hook_registry(IToolHookRegistry* registry) {
    hook_registry_ = registry;
  }

  // ===== ADR-0073 D3: 4 步 sanitization pipeline (声明; 实现在 Batch 2 Task 4) =====

  /**
   * @brief 校验阶段枚举 — pipeline 4 步顺序: SchemaValidate → Coercion →
   *        RequiredField → BusinessRules
   */
  enum class ValidationStage {
    SchemaValidate,
    Coercion,
    RequiredField,
    BusinessRules,
  };

  /**
   * @brief 校验阶段 → ErrorCode 映射 (4 步全部映射到 InvalidParams 语义)
   */
  static ErrorCode map_stage_to_error(ValidationStage stage);

  /**
   * @brief ErrorCode → JSON-RPC 错误码映射 (-32602 InvalidParams / -32603 InternalError 等)
   */
  static int map_to_jsonrpc(ErrorCode code);

 private:
  // --- 4 步 pipeline 私有 helpers (声明; 实现在 Batch 2 Task 4) ---

  /// strict 类型检查: schema.properties[key].type 是否匹配 val 的实际类型
  static bool check_type(const nlohmann::json& schema,
                         const std::string& key,
                         const nlohmann::json& val);

  /// 提取 schema.required[] 字段名列表 (无 required 返回空 vector)
  static std::vector<std::string> get_required_fields(const nlohmann::json& schema);

  /// Coerce 模式: 按 schema 声明类型对 args 做自动类型转换
  /// 返回 failures 供 Step 2 (Warn) 触发 stderr+audit 拒绝
  struct CoerceFailure {
    std::string field_path;
    std::string original_value;
    std::string target_type;
  };
  struct CoerceResult {
    nlohmann::json coerced;
    std::vector<CoerceFailure> failures;
  };
  static CoerceResult coerce_args(const nlohmann::json& schema,
                                  const nlohmann::json& args);

  /// 拒绝路径审计: emit "tool.audit.denied" (per ADR-0068 EventBuilder)
  /// 仅记录 stage + tool_name + reason, 不记录 raw args (defense-in-depth)
  /// @param ctx 用于把 session_id 继承进 audit meta（避免硬编码空串）
  void emit_audit_denied(ValidationStage stage,
                         const std::string& tool_name,
                         const std::string& reason,
                         const ToolCallContext& ctx);


  IToolRegistry& registry_;
  std::shared_ptr<IExecutionPolicy> policy_;
  std::unique_ptr<ApprovalHandler> approval_handler_;
  std::shared_ptr<IInteractionBus> bus_;
  IToolHookRegistry* hook_registry_ = nullptr;

  // C6: ToolMetadata → JSON string for ToolPreview.metadata_json
  static std::string metadata_to_json(const ToolMetadata& meta);
};

}  // namespace agenticdsl