// src/evolution/transition_guard.cpp
// H→D→M Transition Guard 实现文件 (ADR-0088 + OpenSpec change 2026-09-16-h-d-m-transition-guard)
// 头文件 transition_guard.h 包含核心 inline 实现 (D1-D5 状态机 + D3 evaluate_readiness + D4 reset_to_idle)
// 本文件实现:
//   D8: evolution.transition.denied + evolution.readiness.denied 事件发射 helper
//   预留: future complexity 增量 (Sprint 34+ C4 harness-rsi-pilot 集成)

#include "agenticdsl/evolution/transition_guard.h"
#include <string>

namespace agenticdsl::evolution {

// D8: evolution.transition.denied event payload schema (per ADR-0068 EventBuilder)
// payload.data: { from_state: string, to_state: string, reason: string }
// payload.meta: { trace_id: string, cycle_id: string }
// (实际发射由上层 caller 调 EventBuilder, 此处仅声明 schema 常量)

constexpr const char* kEventTransitionDenied = "evolution.transition.denied";
constexpr const char* kEventReadinessDenied = "evolution.readiness.denied";

// 预留给 C4 harness-rsi-pilot 集成:
// - evaluate_readiness 包装为 Class method (目前 inline 在 header)
// - can_transition + EventBuilder.emit 集成 (C4 增量)
// - 与 GEPALoop::reflect 集成 (C4 + Phase 6c Stage Gate 评估后)

}  // namespace agenticdsl::evolution
