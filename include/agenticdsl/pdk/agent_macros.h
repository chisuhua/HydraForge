// include/agenticdsl/pdk/agent_macros.h
// 文件头注释
// 功能描述：DEFINE_AGENT 宏 — Agent 循环脚手架 (ADR-0021 §3.2)。
//          展开为 class XXXAgent 含 run(prompt) 方法, 内部委托 SimpleCognitiveOrchestrator
//          单轮 ReAct (per-agent 隔离, ADR-0020 §2.2.1)。MVP 仅支持 React 循环, PlanExecute
//          与 ForkJoin 通过 static_assert 编译失败 + 明确错误信息 (Phase 2 实施)。
// 设计依据：ADR-0021 §3.2 + ADR-0020 §3.1 CognitiveWorker 模式
//          + openspec/changes/2026-07-07-pdk-skeleton
// 作者：AgenticDSL Phase 1 Sprint 4
// 最后修改日期：2026-06-19

#pragma once

#include "agenticdsl/contract/iinteraction_bus.h"
#include "agenticdsl/cognitive/simple_orchestrator.h"
#include "core/engine.h"
#include "core/types/tool_result.h"

#include <memory>
#include <string>

// 注: SimpleCognitiveOrchestrator 需要完整定义 (DEFINE_AGENT 宏展开为
// SimpleCognitiveOrchestrator orch(&engine_->get_tool_registry(), ...)
// 需要构造函数的完整类型)

namespace hydraforge::pdk {

/**
 * @brief Agent 循环类型 (Sprint 4 MVP)
 *
 * - React:       思考 → 行动 → 观察 → 重复 (MVP Sprint 4 已实现)
 * - PlanExecute: 规划 → 执行 → 验证 (Phase 2 TODO)
 * - ForkJoin:    并行分支 → 合并结果 (Phase 2 TODO)
 */
enum class AgentLoopType {
  React,
  PlanExecute,
  ForkJoin,
};

/**
 * @brief DEFINE_AGENT 宏 — Agent 循环脚手架 (MVP React)
 *
 * 展开为 class XXXAgent 含构造 + run(prompt) 方法。
 * MVP 仅支持 AgentLoopType::React, PlanExecute/ForkJoin 通过 static_assert 编译失败。
 *
 * @param name      Agent 名 (展开为 class XXXAgent)
 * @param loop_type 循环类型 (MVP 仅 React)
 */
#define DEFINE_AGENT(name, loop_type)                                                  \
  static_assert(loop_type == ::hydraforge::pdk::AgentLoopType::React,                  \
                "DEFINE_AGENT MVP only supports AgentLoopType::React. "                 \
                "PlanExecute / ForkJoin are Phase 2 TODO (see ADR-0021 §3.2).");       \
  class name##Agent {                                                                  \
   public:                                                                             \
    name##Agent(std::unique_ptr<agenticdsl::DSLEngine> engine,                         \
                std::shared_ptr<agenticdsl::IInteractionBus> bus)                      \
        : engine_(std::move(engine)), bus_(std::move(bus)) {                           \
      if (engine_ && bus_) {                                                           \
        engine_->set_interaction_bus(bus_);                                            \
      }                                                                                \
    }                                                                                  \
    agenticdsl::ToolResult run(const std::string& prompt) {                             \
      if (!engine_) {                                                                  \
        agenticdsl::ToolResult err;                                                    \
        err.ok = false;                                                                \
        err.error_code = agenticdsl::ErrorCode::Unknown;                               \
        err.meta["error_message"] = "Agent DSLEngine is null";                         \
        return err;                                                                    \
      }                                                                                \
      agenticdsl::SimpleCognitiveOrchestrator orch(                                    \
          &engine_->get_tool_registry(), engine_->get_llm_provider());                 \
      agenticdsl::ToolResult result;                                                   \
      orch.process(prompt, [&result](agenticdsl::ToolResult r) { result = std::move(r); }); \
      return result;                                                                  \
    }                                                                                  \
                                                                                       \
   private:                                                                            \
    std::unique_ptr<agenticdsl::DSLEngine> engine_;                                   \
    std::shared_ptr<agenticdsl::IInteractionBus> bus_;                                 \
  };

} // namespace hydraforge::pdk