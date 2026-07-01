// include/agenticdsl/pdk/agent_macros.h
// 文件头注释
// 功能描述：DEFINE_AGENT 宏 — Agent 循环脚手架 (Sprint 4 → Sprint 20 扩展, ADR-0021 §3.2)。
//          展开为 class XXXAgent 含构造 + run(prompt) 方法, 内部委托 LoopDispatcher
//          模板分发的具体循环类 (ReactLoop / PlanExecuteLoop / ForkJoinLoop)。
//          Sprint 20 移除 static_assert: 3 种 AgentLoopType 全部支持 (React + PlanExecute + ForkJoin),
//          LoopDispatcher<LoopType>::Type 编译期分发, 0 运行时开销。
//          返回类型统一为 LoopResult (Sprint 20 引入), 内部 React 仍委托 SimpleCognitiveOrchestrator
//          单轮 ReAct (per-agent 隔离, ADR-0020 §2.2.1)。
// 设计依据：ADR-0021 §3.2 + ADR-0020 §3.1 CognitiveWorker 模式 + ADR-0008 LayeredContext
//          + openspec/changes/pdk-plan-execute-fork-join
// 作者：AgenticDSL Phase 1 Sprint 4 (React MVP) + Sprint 20 (PlanExecute/ForkJoin)
// 最后修改日期：2026-08-01

#pragma once

#include "agenticdsl/contract/iinteraction_bus.h"
#include "agenticdsl/cognitive/simple_orchestrator.h"
#include "agenticdsl/pdk/agent_loops/fork_join_loop.h"
#include "agenticdsl/pdk/agent_loops/loop_result.h"
#include "agenticdsl/pdk/agent_loops/plan_execute_loop.h"
#include "agenticdsl/pdk/agent_loops/react_loop.h"
#include "agenticdsl/types/layered_context.h"
#include "core/engine.h"
#include "core/types/tool_result.h"

#include <memory>
#include <string>
#include <vector>

namespace hydraforge::pdk {

/**
 * @brief Agent 循环类型 (Sprint 4 MVP + Sprint 20 扩展)
 *
 * - React:       思考 → 行动 → 观察 (MVP Sprint 4 已实现, Sprint 20 升级为独立 class)
 * - PlanExecute: 规划 → 执行 → 验证 (Sprint 20 实施, ADR-0021 §3.2)
 * - ForkJoin:    并行分支 → 合并结果 (Sprint 20 实施)
 */
enum class AgentLoopType {
  React,
  PlanExecute,
  ForkJoin,
};

/**
 * @brief LoopDispatcher 主模板 + 3 个 specialization
 *
 * 编译期通过 AgentLoopType 枚举值分发到具体循环类。
 * 0 运行时开销 (模板 specialization 编译期展开)。
 */
template <AgentLoopType T>
struct LoopDispatcher;

template <>
struct LoopDispatcher<AgentLoopType::React> {
  using Type = ReactLoop;
};

template <>
struct LoopDispatcher<AgentLoopType::PlanExecute> {
  using Type = PlanExecuteLoop;
};

template <>
struct LoopDispatcher<AgentLoopType::ForkJoin> {
  using Type = ForkJoinLoop;
};

/**
 * @brief AgentRunner 主模板 + 3 个 specialization
 *
 * AgentRunner::run(loop, prompt) 统一处理 3 种循环的入口适配:
 *   - React:       prompt 直接传入 ReactLoop.run(prompt, ctx)
 *   - PlanExecute: prompt 作为 goal 传入 PlanExecuteLoop.run(goal, ctx)
 *   - ForkJoin:    prompt 按逗号分割为 branches vector, 传入 ForkJoinLoop.run(branches, ctx)
 *
 * 返回类型统一为 LoopResult (Sprint 20 引入, 5 层结构化上下文统一表示)。
 */
template <AgentLoopType T>
struct AgentRunner;

template <>
struct AgentRunner<AgentLoopType::React> {
  static LoopResult run(ReactLoop& loop, const std::string& prompt) {
    agenticdsl::LayeredContext ctx;
    return loop.run(prompt, ctx);
  }
};

template <>
struct AgentRunner<AgentLoopType::PlanExecute> {
  static LoopResult run(PlanExecuteLoop& loop, const std::string& goal) {
    agenticdsl::LayeredContext ctx;
    return loop.run(goal, ctx);
  }
};

template <>
struct AgentRunner<AgentLoopType::ForkJoin> {
  static LoopResult run(ForkJoinLoop& loop, const std::string& branches_csv) {
    agenticdsl::LayeredContext ctx;
    std::vector<std::string> branches;
    std::string current;
    for (char c : branches_csv) {
      if (c == ',') {
        if (!current.empty()) {
          branches.push_back(current);
          current.clear();
        }
      } else {
        current += c;
      }
    }
    if (!current.empty()) {
      branches.push_back(current);
    }
    return loop.run(branches, ctx);
  }
};

/**
 * @brief DEFINE_AGENT 宏 — Agent 循环脚手架 (Sprint 20: 3 种 LoopType 全支持)
 *
 * 展开为 class XXXAgent 含构造 + run(prompt) 方法。
 * 内部持有 LoopDispatcher<LoopType>::Type 实例, run() 委托给 AgentRunner<LoopType>::run。
 *
 * Sprint 20 变更:
 *   - 移除 Sprint 4 的 static_assert (PlanExecute / ForkJoin 现在可用)
 *   - 返回类型从 ToolResult 统一为 LoopResult
 *   - React 内部仍委托 SimpleCognitiveOrchestrator 单轮 ReAct (零行为变化)
 *
 * @param name      Agent 名 (展开为 class XXXAgent)
 * @param loop_type 循环类型 (React / PlanExecute / ForkJoin, Sprint 20 全支持)
 */
#define DEFINE_AGENT(name, loop_type)                                              \
  class name##Agent {                                                              \
   public:                                                                         \
    using Loop = typename ::hydraforge::pdk::LoopDispatcher<loop_type>::Type;      \
    name##Agent(std::unique_ptr<agenticdsl::DSLEngine> engine,                     \
                std::shared_ptr<agenticdsl::IInteractionBus> bus)                  \
        : loop_(std::move(engine), std::move(bus)) {}                             \
    ::hydraforge::pdk::LoopResult run(const std::string& prompt) {                 \
      return ::hydraforge::pdk::AgentRunner<loop_type>::run(loop_, prompt);        \
    }                                                                              \
                                                                                   \
   private:                                                                        \
    Loop loop_;                                                                    \
  };

} // namespace hydraforge::pdk