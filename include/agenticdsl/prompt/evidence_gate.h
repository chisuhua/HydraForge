// include/agenticdsl/prompt/evidence_gate.h
// 功能描述：Prompt Evidence Gate 质量门控层契约 (T21, ADR-0074)
//          基于 parse-valid 阈值返回 Go/Conditional/No-Go 决策，为
//          Wave 2 → Wave 3 推进提供客观标准。作为编排层复用 IEvaluator V2
//          (CompositeEvaluator) 评估 task-success，并通过 IInteractionBus
//          发射 llm.dsl.parse_failed / llm.dsl.schema_validation_failed 事件。
//
//          校验规则前缀约定（V1）：
//            - "P:" 前缀 = 语法/结构规则 (parse)，未满足 → llm.dsl.parse_failed (重试一次)
//            - "S:" 前缀或无前缀 = 语义/约束规则 (schema)，未满足 → llm.dsl.schema_validation_failed (不重试)
//          parse-valid 比例 = 满足规则数 / 总规则数。
//
// 设计依据：ADR-0074 §决策 1/4/7 + openspec/changes/t21-prompt-evidence-gate/
// 作者：HydraForge Sprint 25 T21 ship
// 最后修改日期：2026-08-28
#ifndef AGENTICDSL_PROMPT_EVIDENCE_GATE_H
#define AGENTICDSL_PROMPT_EVIDENCE_GATE_H

#include "agenticdsl/contract/ievaluator.h"
#include "agenticdsl/contract/iinteraction_bus.h"

#include <memory>
#include <string>
#include <vector>

namespace agenticdsl {

// ============================================================================
// Go/No-Go 决策
// ============================================================================
enum class GateDecision { Go, Conditional, No_Go };

// ============================================================================
// Held-out 黄金任务
// ============================================================================
struct GoldenTask {
  std::string input;
  std::string expected_output;
  std::vector<std::string> validation_rules;  // "P:" 语法规则 / "S:" 语义规则
};

// ============================================================================
// 失败分类 (ADR-0074 §决策 7 差异化处理)
// ============================================================================
enum class GateFailureKind { None, ParseError, SchemaError };

struct GateFailureDetail {
  GateFailureKind kind = GateFailureKind::None;
  std::string error_position;  // parse 失败: 首个未满足的语法规则
  std::string violation;       // schema 失败: 首个未满足的语义规则
  int retry_count = 0;         // parse 重试 1 次 → 1; schema → 0
  bool no_retry = false;       // schema 不重试 → true
};

// ============================================================================
// Prompt Evidence Gate (质量门控层, 非契约层)
// ============================================================================
class PromptEvidenceGate {
 public:
  explicit PromptEvidenceGate(std::shared_ptr<IEvaluator> evaluator,
                              std::shared_ptr<IInteractionBus> bus = nullptr);

  // 核心评估: 计算 parse-valid → 阈值决策 + 失败分类 → (可选) 事件发射
  GateDecision evaluate(const std::string& prompt,
                        const std::string& response,
                        const GoldenTask& golden);

  // 最近一次评估的 parse-valid 比例 (0.0 ~ 1.0)
  double last_parse_valid_rate() const { return last_parse_valid_rate_; }

  // 最近一次评估的失败分类 (含 parse/schema 定位)
  const GateFailureDetail& last_failure() const { return last_failure_; }

  // 最近一次评估的 task-success RewardSignal (IEvaluator V2 输出)
  const RewardSignal& last_reward() const { return last_reward_; }

  // 纯 helper: 计算 parse-valid 比例 (satisfied_rules / total_rules)
  static double parse_valid_rate(const std::string& response,
                                 const GoldenTask& golden);

 private:
  std::shared_ptr<IEvaluator> evaluator_;
  std::shared_ptr<IInteractionBus> bus_;
  double parse_valid_threshold_ = 0.90;         // ≥90% → Go
  double conditional_lower_bound_ = 0.80;       // 80-89% → Conditional
  double last_parse_valid_rate_ = 0.0;
  GateFailureDetail last_failure_;
  RewardSignal last_reward_{RewardSignal::Quality::Acceptable, 0.0, 0.0};
  bool emitted_parse_failed_ = false;  // 同一 evaluate 调用至多发射一次
  bool emitted_schema_failed_ = false;
};

}  // namespace agenticdsl

#endif  // AGENTICDSL_PROMPT_EVIDENCE_GATE_H
