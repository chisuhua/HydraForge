// include/agenticdsl/pdk/model_router.h
// 功能描述：IModelRouter 模型路由 Plugin 接口 (C7 Phase 1 MVP, ADR-0034)。
//           PDK 插件头文件, 命名空间 agenticdsl::pdk。
//           包含 4 个类型:
//             - RoutingContext: 路由决策上下文 (7 字段)
//             - ModelCapability: 模型能力描述 (9 字段)
//             - IModelRouter: 路由策略抽象接口 (2 纯虚)
//             - ModelRoutingError: 路由异常 (3 错误码)
//           引擎零变更 (C7 纯 PDK), 路由通过 call_tool("model_router/cost", ...) 调用。
// 设计依据：openspec/changes/2026-06-26-adr-0034-model-router-plugin/design.md Decision 1-4
//           ADR-0034 + Oracle Q1-Q4 决策 (2026-07-02)
// 作者：C7 Phase 1 MVP
// 最后修改日期：2026-07-02

#pragma once

#include <cstdint>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

namespace agenticdsl {
namespace pdk {

// ============================================================================
// RoutingContext — 路由决策上下文
// ============================================================================
// 调用方填充, 告知路由策略当前的 session/预算/tag 约束。
// 除 task_type 为必填外, 其余字段均可选。
// ============================================================================
struct RoutingContext {
  std::string task_type;                       // "completion" / "code_generation" / "reasoning"
  std::string session_id;                      // 会话唯一标识
  std::optional<int> max_tokens;               // 最大输出 token 数
  std::optional<double> budget_remaining;      // 剩余预算 (美元)
  std::vector<std::string> required_tags;      // 能力标签 ("fast" / "code" / "reasoning")
  std::string preferred_model;                 // 用户偏好模型 (可忽略)
  bool is_fleet_mode = false;                  // 是否舰队模式
};

// ============================================================================
// ModelCapability — 模型能力描述 (PDK 侧, 独立于 ILLMProvider::ModelCapability enum)
// ============================================================================
// 包含路由决策所需全部字段: 标识 / 上下文 / 功能 / 成本 / 延迟 / 标签。
// 与 agenticdsl::ILLMProvider::ModelCapability(enum) 命名空间隔离, 避免类型冲突。
// ============================================================================
struct ModelCapability {
  std::string model_id;                // 模型唯一标识 (e.g. "gpt-4", "claude-3-opus")
  std::string model_name;              // 模型显示名 (e.g. "GPT-4")
  int n_ctx = 4096;                    // 上下文窗口大小 (tokens)
  int max_tokens = 4096;               // 最大输出 token 数
  bool supports_streaming = true;      // 支持流式输出
  bool supports_function_call = false; // 支持函数调用
  double per_token_cost = 0.0;         // 每 token 成本 (美元)
  int avg_latency_ms = 500;            // 平均延迟 (毫秒)
  std::vector<std::string> tags;       // 能力标签 ["fast", "vision", "code"]
};

// ============================================================================
// IModelRouter — 模型路由策略抽象接口
// ============================================================================
// 路由策略的公共契约: 接收上下文 + 候选列表 → 返回最优 model_id。
// 策略本身 stateless (纯函数), 模型注册表缓存在引擎层。
// 实现类: CostModelRouterPolicy / QualityModelRouterPolicy / LatencyModelRouterPolicy.
// ============================================================================
class IModelRouter {
public:
  virtual ~IModelRouter() = default;

  /// 路由决策: 从 candidates 中选择最优模型
  /// @param ctx 路由决策上下文 (task_type / budget / required_tags 等)
  /// @param candidates 候选模型列表 (来自 Provider::available_models() 或 mock)
  /// @return 选中的 model_id
  /// @throws ModelRoutingError 无合适模型时 (NoViableModel / ProviderUnavailable)
  virtual std::string route(const RoutingContext& ctx,
                            const std::vector<ModelCapability>& candidates) = 0;

  /// 策略名称 (e.g. "cost" / "quality" / "latency")
  virtual std::string name() const = 0;
};

// ============================================================================
// ModelRoutingError — 路由异常
// ============================================================================
// 3 种错误码:
//   - NoViableModel:      无模型满足约束 (budget / tags / latency)
//   - ProviderUnavailable: 配置的 provider 未加载
//   - AmbiguousCapability: 多个模型 tie (返回第一个 + log warning)
// what() 自动包含错误码前缀, 便于日志/错误处理识别。
// ============================================================================
class ModelRoutingError : public std::runtime_error {
public:
  enum class Code {
    NoViableModel,
    ProviderUnavailable,
    AmbiguousCapability
  };

  Code code;

  ModelRoutingError(Code c, const std::string& msg)
    : std::runtime_error(make_message(c, msg)), code(c) {}

  static std::string make_message(Code c, const std::string& msg) {
    switch (c) {
      case Code::NoViableModel:       return "[NoViableModel] " + msg;
      case Code::ProviderUnavailable:  return "[ProviderUnavailable] " + msg;
      case Code::AmbiguousCapability: return "[AmbiguousCapability] " + msg;
    }
    return msg;
  }
};

} // namespace pdk
} // namespace agenticdsl