// src/common/policy/execution_policy.h
// 功能描述：执行策略模块的公共类型定义。集中定义 ToolMetadata / ToolCallContext
// / ToolCategory / ApprovalPolicy / LayerProfile 等值类型，作为
// IExecutionPolicy 接口（位于 include/agenticdsl/policy/iexecution_policy.h）
// 的输入参数与决策依据。三种模式策略（Plan / Agent / YOLO）依赖本头
//提供的类型实现 requires_approval 等决策。
// 设计依据：ADR-0031 (IExecutionPolicy) §1 + ADR-0004 (ToolRegistry 安全模型) §6-9
// 作者：AgenticDSL Phase3 / Track A
// 最后修改日期：2026-06-10
#pragma once

#include <cstddef>
#include <string>
#include <vector>
#include <optional>

#include <nlohmann/json.hpp>

namespace agenticdsl {

/**
 * @brief工具安全分类（ADR-0004 §6）
 *
 * 用于将工具按风险等级分类，以供各模式策略与 Layer Profile 检查使用。
 */
enum class ToolCategory {
 ReadOnly, // 只读操作：ls / cat / grep / search
 WriteFile, // 文件写入：edit_file / create_file / delete_file
 Execute, // 命令执行：exec_shell / run_tests
 Network, // 网络操作：http_request / api_call
 StateModify //状态修改：set_mode / clear_context
};

/**
 * @brief 三模式审批策略（ADR-0004 §7 / ADR-0031 §1）
 *
 * 每个工具可独立声明在三种模式下是否需要用户审批，并提供一个
 * force_approval_always 安全底线（用于 delete_file 等危险工具）。
 */
struct ApprovalPolicy {
 bool requires_approval_in_plan = true; // Plan模式是否审批
 bool requires_approval_in_agent = true; // Agent模式是否审批
 bool requires_approval_in_yolo = false; // YOLO模式是否审批
 // 安全底线：即使 YOLO模式也强制审批的操作（如 delete_file）
 bool force_approval_always = false;
};

/**
 * @brief 调用层级限制（ADR-0004 §8）
 *
 *标识调用方所属的层级，用于限制不同层级的工具调用权限。
 * - Workflow (L2)：完全允许，沙箱内
 * - Thinking (L3)：只读工具
 * - Cognitive (L4)：禁止 tool_call（仅 state.read）
 */
enum class LayerProfile {
 Workflow =0,
 Thinking =1,
 Cognitive =2
};

/**
 * @brief工具元数据（ADR-0004 §9 / ADR-0031 §1）
 *
 *描述一个已注册工具的全部静态信息：名称、说明、所属域、安全分类、
 *最低调用层级以及审批策略。是 IExecutionPolicy::requires_approval
 * 的核心输入。
 */
struct ToolMetadata {
  std::string name; // 例如 "code::edit_file"
  std::string description; //工具用途描述
  std::string domain; //工具所属域，例如 "code"
  ToolCategory category; // 安全分类
  LayerProfile min_layer; //最低调用层级
  ApprovalPolicy approval; // 三模式审批策略

  // ===== V2 extensions (C4 Sprint 14, Oracle 2026-06-29) =====
  std::vector<LayerProfile> allowed_layers;
  double cost_estimate = 0.0;
  int timeout_ms = 30000;

  // ===== V3 extensions (ADR-0073 D2, Phase 6c C8) =====
  std::optional<nlohmann::json> input_schema;
  std::optional<nlohmann::json> output_schema;
  enum class ValidationMode { Strict, Warn, Ignore };
  ValidationMode validation_mode = ValidationMode::Strict;
};

/**
 * @brief工具调用上下文（ADR-0031 §1）
 *
 *描述一次工具调用的运行时上下文，作为 IExecutionPolicy决策的辅助输入。
 * 当前主要承载 Session标识、调用方层级、目标路径、舰队模式标记以及
 * 本 Session 内累计调用次数。
 */
struct ToolCallContext {
 std::string session_id; //所属 Session标识
 std::string caller_layer; // 调用方层级，例如 "cognitive"
 std::string target_path; //目标文件 /目录路径
 bool is_in_fleet_mode = false; // 是否在舰队模式中
 size_t call_count_this_session =0; // 本 Session 第几次调用
};

} // namespace agenticdsl
