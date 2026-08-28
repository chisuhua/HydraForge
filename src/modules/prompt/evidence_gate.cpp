// src/modules/prompt/evidence_gate.cpp
// 功能描述：Prompt Evidence Gate 实现 (T21, ADR-0074)。
//          evaluate(): 计算 parse-valid → 阈值决策 (Go ≥90% / Conditional 80-89% /
//          No-Go <80%) + 失败分类 (P: 语法规则未满足 → ParseError / S: 语义规则
//          未满足 → SchemaError) + 复用 IEvaluator V2 评估 task-success。
// 设计依据：ADR-0074 §决策 1/4/7 + openspec/changes/t21-prompt-evidence-gate/
// 作者：HydraForge Sprint 25 T21 ship
// 最后修改日期：2026-08-28

#include "agenticdsl/prompt/evidence_gate.h"

#include "agenticdsl/contract/event_builder.h"

#include <algorithm>
#include <stdexcept>
#include <utility>

namespace agenticdsl {

namespace {
std::string rule_body(const std::string& rule) {
  if (rule.size() > 2 && rule[1] == ':' && (rule[0] == 'P' || rule[0] == 'S')) {
    return rule.substr(2);
  }
  return rule;
}
}  // namespace

PromptEvidenceGate::PromptEvidenceGate(std::shared_ptr<IEvaluator> evaluator,
                                       std::shared_ptr<IInteractionBus> bus)
    : evaluator_(std::move(evaluator)), bus_(std::move(bus)) {
  if (!evaluator_) {
    throw std::invalid_argument("PromptEvidenceGate requires an IEvaluator");
  }
}

GateDecision PromptEvidenceGate::evaluate(const std::string& prompt,
                                          const std::string& response,
                                          const GoldenTask& golden) {
  last_parse_valid_rate_ = parse_valid_rate(response, golden);
  last_failure_ = GateFailureDetail{};
  for (const auto& validation_rule : golden.validation_rules) {
    if (response.find(rule_body(validation_rule)) != std::string::npos) continue;
    if (validation_rule.rfind("P:", 0) == 0) {
      // 语法错误: 定位 + 重试一次 (retry_count=1)
      last_failure_.kind = GateFailureKind::ParseError;
      last_failure_.error_position = rule_body(validation_rule);
      last_failure_.retry_count = 1;
      break;
    }
    if (last_failure_.kind == GateFailureKind::None) {
      // 语义错误: 违规规则 + 不重试
      last_failure_.kind = GateFailureKind::SchemaError;
      last_failure_.violation = rule_body(validation_rule);
      last_failure_.no_retry = true;
    }
  }

  // ADR-0068 事件发射 (owner: PromptEvidenceGate)
  if (bus_) {
    if (last_failure_.kind == GateFailureKind::ParseError) {
      bus_->emit(
          EventBuilder("llm.dsl.parse_failed")
              .args(nlohmann::json{{"prompt", prompt},
                                   {"error_position", last_failure_.error_position},
                                   {"retry_count", last_failure_.retry_count}})
              .build());
    } else if (last_failure_.kind == GateFailureKind::SchemaError) {
      bus_->emit(
          EventBuilder("llm.dsl.schema_validation_failed")
              .args(nlohmann::json{{"prompt", prompt},
                                   {"violation", last_failure_.violation},
                                   {"no_retry", last_failure_.no_retry}})
              .build());
    }
  }

  ExecutionTrace trace;
  trace.final_result.ok = last_parse_valid_rate_ >= parse_valid_threshold_;
  last_reward_ = evaluator_->evaluate(trace);

  if (last_parse_valid_rate_ >= parse_valid_threshold_) return GateDecision::Go;
  if (last_parse_valid_rate_ >= conditional_lower_bound_) return GateDecision::Conditional;
  return GateDecision::No_Go;
}

double PromptEvidenceGate::parse_valid_rate(const std::string& response,
                                            const GoldenTask& golden) {
  if (golden.validation_rules.empty()) return 0.0;
  const std::size_t satisfied = static_cast<std::size_t>(std::count_if(
      golden.validation_rules.begin(), golden.validation_rules.end(),
      [&response](const std::string& validation_rule) {
        return response.find(rule_body(validation_rule)) != std::string::npos;
      }));
  return static_cast<double>(satisfied) /
         static_cast<double>(golden.validation_rules.size());
}

}  // namespace agenticdsl
