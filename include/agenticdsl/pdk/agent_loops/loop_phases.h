// include/agenticdsl/pdk/agent_loops/loop_phases.h
// 文件头注释
// 功能描述：Loop Phase 共享 helper 自由函数 (per ADR-0089 v1.3 + ADR-0021 §3.2 amendment)
//          抽取自 PlanExecuteLoop::plan_phase / execute_phase / verify_phase,
//          供 C++ 薄壳类 (PlanExecuteLoop / ReactLoop) 与 pdk_entry 工具函数共同调用, 实现单源.
//          注意: helper 无 retry 语义 (D2.inv.retry), retry 编排由 PlanExecuteLoop 负责.
// 设计依据：ADR-0089 v1.3 amendment + ADR-0021 §3.2 (修订对象) + AGENTS.md Pattern #4 + Pattern #11
//          + openspec/changes/consolidate-loop-phases-to-shared-helpers/{proposal,design,tasks,specs/*}.md
// 作者：AgenticDSL Sprint 37+ (consolidate-loop-phases 立项)
// 最后修改日期：2026-10-09

#pragma once

#include "agenticdsl/types/layered_context.h"
#include "common/llm/llm_types.h"
#include "core/engine.h"

#include <nlohmann/json.hpp>

#include <optional>
#include <stop_token>
#include <string>

namespace hydraforge::pdk::loop_phases {

/**
 * @brief Plan 阶段: LLM 生成 DSL 片段
 * @param llm  LLM provider (调用方持有, helper 无所有权)
 * @param goal 用户目标 (作为 Plan 阶段 prompt 输入)
 * @param ctx  LayeredContext (作为 execute 阶段 context 起点)
 * @return std::optional<string> 非空表示生成的 DSL, nullopt 表示失败 (空响应)
 *
 * 契约:
 * - MUST 显式调 req.params.model.clear() 在调 llm->generate() 之前 (Oracle M1 fix)
 * - MUST 保留 "NOT redundant" 注释 (per fix-generation-request-model-default 修复链)
 * - MUST NOT 持 retry 状态 (D2.inv.retry — retry 在 C++ 薄壳类内编排)
 * - MUST NOT 发射 loop.turn.* 事件 (D5 — 事件发射由 caller 统一负责)
 */
inline std::optional<std::string> run_plan_phase(
    agenticdsl::ILLMProvider& llm,
    const std::string& goal,
    const agenticdsl::LayeredContext& ctx,
    std::stop_token token = {}) {
  agenticdsl::GenerationRequest req;
  req.prompt =
      "Goal: " + goal +
      "\nContext: " + ctx.dump().dump() +
      "\nGenerate AgenticDSL markdown for /main subgraph:";
  // ⚠️ NOT redundant: LLMParams = LLMConfig 别名, 默认 model = "gpt-4o-mini"
  // (非空). 若不清空, CloudLLMAdapter::build_request_body L164
  // (req.params.model.empty() ? config_.model : req.params.model) 会拿默认
  // "gpt-4o-mini" 遮蔽 adapter 构造时 factory 设置的真实 model
  // (如 deepseek-v4-flash) → deepseek server 拒绝
  // ("you passed gpt-4o-mini"). 站点无 model 概念 (model 由 adapter/factory 持有),
  // 清空让 adapter fallback.
  // 详见 openspec/changes/fix-generation-request-model-default/ + plan-execute-loop-realllm/.
  // (从 plan_execute_loop.h:217-224 同步, per ADR-0089 v1.3 D1.inv.model)
  req.params.model.clear();
  auto gen_result = llm.generate(req, token);
  if (!gen_result.has_value()) {
    return std::nullopt;
  }
  const auto& text = gen_result.value().text;
  if (text.empty()) {
    return std::nullopt;
  }
  return text;
}

/**
 * @brief Execute 阶段: DSLEngine 解析 + 追加生成的 DSL (跨 retry 累积)
 * @param engine              DSLEngine& (caller 持有, helper 无所有权, 跨 retry 复用)
 * @param generated_dsl       LLM 生成的 DSL 字符串 (Markdown 格式)
 * @param execute_error_out   [out] 失败时填充异常消息 (非 std::nullopt)
 * @return true 表示解析成功, false 表示失败
 *
 * 契约:
 * - MUST 接受 DSLEngine& (reference), MUST NOT 获取 unique_ptr 所有权 (Oracle C2 fix)
 * - MUST NOT 跨 retry 创建/析构 engine (engine lifecycle 由 PlanExecuteLoop::engine_ 持有)
 * - 调用方负责: 失败时不调 verify (per plan_execute_loop.h:163 retry 编排)
 */
inline bool run_execute_phase(
    agenticdsl::DSLEngine& engine,
    const std::string& generated_dsl,
    std::optional<std::string>& execute_error_out) {
  try {
    engine.continue_with_generated_dsl(generated_dsl);
    return true;
  } catch (const std::exception& e) {
    execute_error_out = std::string(e.what());
    return false;
  }
}

/**
 * @brief Verify 阶段: LLM 评估 ExecutionResult (返回 yes/no 决策)
 * @param llm         LLM provider (caller 持有)
 * @param goal        用户目标 (作为 verify 阶段 prompt 输入)
 * @param result_data 执行结果 data (JSON 序列化, 通常是 LayeredContext.working["data"])
 * @return true 表示验证通过 (LLM 响应含 "yes", 大小写不敏感), false 表示失败
 *
 * 契约:
 * - MUST 显式调 req.params.model.clear() (与 plan_phase 同理由, Oracle M1 fix)
 * - MUST NOT 持 retry 状态 (D2.inv.retry)
 * - 响应含 "yes" → true, 含 "no" 或 空 → false (per plan_execute_loop.h:294-299)
 */
inline bool run_verify_phase(
    agenticdsl::ILLMProvider& llm,
    const std::string& goal,
    const nlohmann::json& result_data,
    std::stop_token token = {}) {
  // ⚠️ NOT redundant: 与 run_plan_phase 同理 (LLMParams 默认 model 遮蔽 adapter
  // config_.model), 详见 run_plan_phase 上方注释 +
  // openspec/changes/fix-generation-request-model-default/.
  // (从 plan_execute_loop.h:287-288 同步, per ADR-0089 v1.3 D1.inv.model)
  const std::string data_dump = result_data.is_null()
                                    ? std::string{"{}"}
                                    : result_data.dump();
  agenticdsl::GenerationRequest req;
  req.prompt =
      "Goal: " + goal +
      "\nPlan status: appended\n"
      "Result: " + data_dump +
      "\nVerify the plan was appended successfully (yes/no):";
  req.params.model.clear();
  auto gen_result = llm.generate(req, token);
  if (!gen_result.has_value()) {
    return false;
  }
  const auto& text = gen_result.value().text;
  std::string lower = text;
  for (auto& c : lower) {
    c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  }
  return lower.find("yes") != std::string::npos;
}

}  // namespace hydraforge::pdk::loop_phases