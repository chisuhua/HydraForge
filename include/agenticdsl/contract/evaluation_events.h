// include/agenticdsl/contract/evaluation_events.h
// 文件头注释
// 功能描述：evaluation.result 事件构造 helper (ADR-0083 / ADR-0068 附录 A v1.2)。
//          统一 CognitiveWorker 与 DomainWorkerPool 的 evaluation_id 生成
//          (timestamp + atomic counter, V1 简单实现不持久化) 与
//          evaluation.result 事件 payload schema (design D5)。
// 设计依据：openspec/changes/2026-08-26-ship-ievaluator-reward-contract/design.md D5+D6
// 作者：HydraForge Phase 6 / IEvaluator contract
// 最后修改日期：2026-08-26
#pragma once

#include "agenticdsl/contract/bus_event.h"
#include "agenticdsl/contract/event_builder.h"
#include "agenticdsl/types/execution_trace.h"
#include "agenticdsl/types/reward_signal.h"

#include <nlohmann/json.hpp>

#include <atomic>
#include <chrono>
#include <cstdint>
#include <string>
#include <utility>

namespace agenticdsl {
namespace evaluation {

inline const char* quality_name(RewardSignal::Quality q) {
  switch (q) {
    case RewardSignal::Quality::Excellent:  return "Excellent";
    case RewardSignal::Quality::Acceptable: return "Acceptable";
    case RewardSignal::Quality::Poor:       return "Poor";
  }
  return "Unknown";
}

// V1 简单实现: 毫秒时间戳 + 进程内 atomic counter, 保证跨调用唯一 (不持久化)
inline std::string make_evaluation_id() {
  static std::atomic<std::uint64_t> counter{0};
  const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                      std::chrono::steady_clock::now().time_since_epoch())
                      .count();
  return "eval_" + std::to_string(ms) + "_" +
         std::to_string(counter.fetch_add(1, std::memory_order_relaxed));
}

// 构造 evaluation.result BusEvent (flat 字段 + 嵌套 reward 对象, 兼容 design D5)
inline BusEvent build_evaluation_result_event(const std::string& evaluator_type,
                                              const ExecutionTrace& trace,
                                              const RewardSignal& signal) {
  nlohmann::json reward = {
      {"quality", quality_name(signal.quality)},
      {"scalar", signal.scalar},
      {"confidence", signal.confidence},
  };
  nlohmann::json args = {
      {"evaluation_id", make_evaluation_id()},
      {"schema_version", "v1"},
      {"evaluator_type", evaluator_type},
      {"trace_ref", trace.trace_id},
      {"trace_id", trace.trace_id},
      {"quality", reward["quality"]},
      {"scalar", reward["scalar"]},
      {"confidence", reward["confidence"]},
      {"reward", reward},
      {"evaluation_refs", nlohmann::json::array()},
  };
  return EventBuilder("evaluation.result")
      .args(std::move(args))
      .meta(nlohmann::json{{"trace_id", trace.trace_id}})
      .build();
}

} // namespace evaluation
} // namespace agenticdsl
