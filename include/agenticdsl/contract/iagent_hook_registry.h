// include/agenticdsl/contract/iagent_hook_registry.h
// 功能描述：Agent pre/post-step hook L3 契约 (ADR-0081 Approved 2026-08-21)
//
//          设计要点（ADR-0081 §决策 D1 + ADR-0082 §决策 C5）：
//          - Agent-scoped（per-agent 类型注册，不是 per-LLM-call）
//          - agent_glob 通配（如 "react-loop/*" / "*"，与 ADR-0043 命名约定一致）
//          - 复用 IToolHookRegistry 的 HookErrorPolicy（避免双轨）
//          - 与 IToolRegistry / IAgentRegistry 接口正交
//
// 设计依据：ADR-0081 + ADR-0069 HookErrorPolicy 复用 + ADR-0043 tool_glob 约定 + ADR-0082 §决策 C5
// 作者：HydraForge Sprint 22 / adr-0081-promote-to-approved
// 最后修改日期：2026-08-21

#pragma once

#include "agenticdsl/contract/itool_hook_registry.h"  // HookErrorPolicy (复用 ADR-0069)
#include "agenticdsl/contract/iagent_registry.h"       // IAgent (V1 最小集: name + id)

#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace agenticdsl {

// Agent pre-step hook 返回值
//
// 不变量（ADR-0081 §不变量）：
//   - 拦截点不修改 core 行为：hook 失败/异常不阻断主流程
//   - fail-closed 安全语义：deny 决策不可被后续 hook 覆盖
struct AgentPreHookResult {
  enum Action { Continue, Deny, ModifyContext } action = Continue;
  std::string deny_reason;
  std::unordered_map<std::string, std::string> modified_context;
};

// Agent post-step hook 返回值
struct AgentPostHookResult {
  bool modify_result = false;
  std::string modified_output;
};

// Agent pre-step hook 函数签名
//
// 参数：
//   - agent: 当前 Agent 实例（type 标识通过 agent->name() 获取）
//   - step_input: Agent step 输入（V1 简化为字符串，复杂类型留 Sprint 24+）
// 返回：AgentPreHookResult（含 Continue/Deny/ModifyContext）
using AgentPreHook = std::function<AgentPreHookResult(
    const IAgent& agent, const std::string& step_input)>;

// Agent post-step hook 函数签名
using AgentPostHook = std::function<AgentPostHookResult(
    const IAgent& agent, const std::string& step_output)>;

/**
 * * Agent pre/post-step hook registry L3 契约 (ADR-0081 Approved)
 *
 * Agent-scoped 注册粒度（per-agent 类型，与 ADR-0082 IAgentRegistry 对齐）。
 *
 * 与 IToolHookRegistry 接口正交（C5 决议）：
 * - - tool hook: per-tool 调用（`tools/*`）
 * - - agent hook: per-agent step（`agent/*`）
 * 调用顺序：agent step → tool call（hook 触发点不重叠）
 */
class IAgentHookRegistry {
 public:
  virtual ~IAgentHookRegistry() = default;

  // 注册 agent pre-step hook
  virtual void register_pre_hook(const std::string& agent_glob,
                                 AgentPreHook hook,
                                 int priority,
                                 HookErrorPolicy policy) = 0;

  // 注册 agent post-step hook
  virtual void register_post_hook(const std::string& agent_glob,
                                  AgentPostHook hook,
                                  int priority,
                                  HookErrorPolicy policy) = 0;

  // 执行匹配的 pre-hooks（按 priority 顺序），返回最终 action
  // `warnings` 累积 FailOpen 异常消息
  virtual AgentPreHookResult apply_pre_hooks(
      const IAgent& agent,
      const std::string& step_input,
      std::vector<std::string>& warnings) const = 0;

  // 执行匹配的 post-hooks（按 priority 顺序），返回最终 result
  virtual AgentPostHookResult apply_post_hooks(
      const IAgent& agent,
      const std::string& step_output,
      std::vector<std::string>& warnings) const = 0;
};

// 工厂函数：创建 InMemory 参考实现（与 IToolHookRegistry 模式一致）
std::unique_ptr<IAgentHookRegistry> make_in_memory_agent_hook_registry();

}  // namespace agenticdsl