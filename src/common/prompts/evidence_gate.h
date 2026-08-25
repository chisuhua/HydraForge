// ADR-0074 D-4: Evidence Gate Go/No-Go 决策树纯函数
// MOMUS REJECT 修复后 v1: header-only, no IO, no global state
// 4 状态枚举 + 4 参 evaluate_gate
// 数据完整性 check (D-3): parse_valid sentinel → Abort
// 临界带判定 (D-4): left-closed right-open [85.0, 90.0)
#pragma once

namespace agenticdsl::prompts {

enum class GateStatus {
  Pass,         // ≥ 90.0 parse-valid → proceed
  Fail,         // < 85.0 parse-valid → trigger ADR-0072 D2 `$var`
  Conditional,  // 85.0 ≤ parse-valid < 90.0 → trigger ADR-0072 D3 declarative
  Abort         // data incomplete → re-measure
};

// D-3 数据完整性 sentinel: 实际测量必为 [0, 1] 区间,负值表示数据缺失/异常
inline constexpr double kGateDataMissing = -1.0;

// evaluate_gate: 决策树主入口
// - parse_valid: 3 模型平均 parse-valid 率,范围 [0, 1],sentinel = Abort
// - l1, l2, l3: task-success 率(L1/L2/L3 加权平均),范围 [0, 1]
//                本版仅签名占位(per design D-4 临界带仅基于 parse-valid),
//                完整 L1/L2/L3 阈值判定留 ADR-0074 §决策 D5 v2 amendment
// 返回 4 状态之一。constexpr + noexcept + inline (header-only)
constexpr GateStatus evaluate_gate(double parse_valid,
                                   double l1,
                                   double l2,
                                   double l3) noexcept {
  // D-3 数据完整性 check: 数据缺失或异常立即 Abort
  if (parse_valid < 0.0 || parse_valid > 1.0) {
    return GateStatus::Abort;
  }

  // l1/l2/l3 接受签名但本版仅 pass-through (Phase 6c D-4 scope)
  // 完整 D-4 "全部满足" 语义由 v2 amendment 实施 (Sprint 25+)
  (void)l1;
  (void)l2;
  (void)l3;

  // D-4 左闭右开临界带
  if (parse_valid < 85.0 / 100.0) {
    return GateStatus::Fail;
  }
  if (parse_valid < 90.0 / 100.0) {
    return GateStatus::Conditional;
  }
  return GateStatus::Pass;
}

// 字符串化 (供决议文档渲染)
inline constexpr const char* to_string(GateStatus s) noexcept {
  switch (s) {
    case GateStatus::Pass:        return "Pass";
    case GateStatus::Fail:        return "Fail";
    case GateStatus::Conditional: return "Conditional";
    case GateStatus::Abort:       return "Abort";
  }
  return "Unknown";
}

}  // namespace agenticdsl::prompts
