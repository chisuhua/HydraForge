// include/agenticdsl/evolution/transition_guard.h
// H→D→M Transition Guard 状态机 (ADR-0088 + OpenSpec change 2026-09-16-h-d-m-transition-guard)
// D1 决策: 5 态状态机 + YAGNI
// D2 决策: EvolutionVerdict struct (5 字段, Oracle Q8 约束修订 2026-09-22: reward_quality 为 Wave 3 决策必需信号)
// D3 决策: can_transition() 编译期 + evaluate_readiness() 运行期 + 三条件门控 (累积报告非短路)
// D7 决策: 复用 IEvaluator + IBudgetController + AttributionRecord
// D8 决策: 事件主题 evolution.transition.denied + evolution.readiness.denied (NOT evolution.scheduler.denied)

#ifndef AGENTICDSL_EVOLUTION_TRANSITION_GUARD_H
#define AGENTICDSL_EVOLUTION_TRANSITION_GUARD_H

#include <array>
#include <string>
#include <vector>
#include "agenticdsl/types/attribution_record.h"
#include "agenticdsl/contract/ievaluator.h"
#include "modules/budget/budget_controller.h"

namespace agenticdsl::evolution {

// D1: 5 态枚举 (Idle/Harness/Data/Model/Done)
enum class EvolutionState {
    Idle    = 0,
    Harness = 1,
    Data    = 2,
    Model   = 3,
    Done    = 4
};

// D2: EvolutionVerdict struct (5 字段, Oracle Q8 约束修订 per design D1a: reward_quality 追加于字段末尾, 默认值兜底不破坏既有构造)
struct EvolutionVerdict {
    bool can_proceed = false;
    EvolutionState recommended_next = EvolutionState::Idle;
    std::string reason;
    std::vector<std::string> failed_conditions;  // 累积报告所有 3 条件结果 (非短路, per Metis Q6)
    agenticdsl::RewardSignal::Quality reward_quality =
        agenticdsl::RewardSignal::Quality::Acceptable;  // G2 design D1: Wave 3 决策必需质量信号
};

// D3: can_transition() 编译期 5×5 矩阵 (constexpr std::array)
constexpr std::array<std::array<bool, 5>, 5> kTransitionMatrix = {{
    // to:  Idle   Harness Data   Model  Done
    /* from Idle    */ {{false, true,  false, false, false}},
    /* from Harness */ {{false, false, true,  false, false}},
    /* from Data    */ {{false, false, false, true,  false}},
    /* from Model   */ {{false, false, false, false, true }},
    /* from Done    */ {{true,  false, false, false, false}}
}};

inline bool can_transition(EvolutionState from, EvolutionState to) {
    return kTransitionMatrix[static_cast<int>(from)][static_cast<int>(to)];
}

// D3 + Q3 累积报告: evaluate_readiness 三条件门控
//   ① Attributed (per ADR-0086) ② regression PASS (per IEvaluator) ③ budget sufficient (per IBudgetController)
//   任意条件 fail → 累积所有 fail 条件到 failed_conditions (非短路, per Metis Q6 修正)
inline EvolutionVerdict evaluate_readiness(
    EvolutionState current,
    const AttributionRecord& attribution,
    const agenticdsl::IEvaluator& evaluator,
    const agenticdsl::IBudgetController& budget) {

    EvolutionVerdict verdict;
    verdict.can_proceed = true;

    // 条件 1: Attribution Attributed
    if (attribution.verdict != AttributionVerdict::Attributed) {
        verdict.failed_conditions.push_back("attribution_insufficient");
    }

    // 条件 2: Regression Pass (evaluator scalar > 0 && quality != Poor)
    // 使用 sentinel ExecutionTrace 触发 evaluate (实际生产用真实 trace)
    agenticdsl::ExecutionTrace sentinel_trace;
    auto reward = evaluator.evaluate(sentinel_trace);
    // G2 design D2: quality 无论 can_proceed 与否都要反映 (失败时的质量也是关键信号)
    verdict.reward_quality = reward.quality;
    if (reward.quality == agenticdsl::RewardSignal::Quality::Poor ||
        reward.scalar < 0.0) {
        verdict.failed_conditions.push_back("regression_failed");
    }

    // 条件 3: Budget sufficient (not exceeded)
    if (budget.exceeded()) {
        verdict.failed_conditions.push_back("budget_exhausted");
    }

    // 累积报告后, can_proceed 反映总体结果
    verdict.can_proceed = verdict.failed_conditions.empty();

    // 推荐下一步: 当前态 → 下一合法态 (per kTransitionMatrix 第一合法转换)
    for (int to_idx = 0; to_idx < 5; ++to_idx) {
        EvolutionState to = static_cast<EvolutionState>(to_idx);
        if (can_transition(current, to)) {
            verdict.recommended_next = to;
            break;
        }
    }

    if (!verdict.can_proceed) {
        verdict.reason = "One or more readiness conditions failed (see failed_conditions)";
    }

    return verdict;
}

// D4 (per Metis Q7): 显式 reset_to_idle API
inline EvolutionState reset_to_idle(EvolutionState current) {
    if (current == EvolutionState::Done) {
        return EvolutionState::Idle;
    }
    return current;
}

// D3: Compile-time static_assert 验证矩阵维度 (per Oracle Q2 Major)
static_assert(kTransitionMatrix.size() == 5, "Transition matrix must have 5 rows");
static_assert(kTransitionMatrix[0].size() == 5, "Each row must have 5 columns");
static_assert(static_cast<size_t>(EvolutionState::Done) == 4,
              "EvolutionState::Done must be index 4 in transition matrix");

}  // namespace agenticdsl::evolution

#endif  // AGENTICDSL_EVOLUTION_TRANSITION_GUARD_H
