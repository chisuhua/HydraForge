// include/agenticdsl/types/compiled_skill.h
// 功能描述：CompiledSkill 值类型 (T17, ADR-0061-03 SkillCompiler 编译产物)
//          trajectory_ir_hash 由 TrajectoryIR::hash(CanonicalIR) 生成
//          (T15 ship 2026-08-27, V1 占位结构已删除,
//           见 agenticdsl/ir/trajectory_ir.h)
// 设计依据：openspec/changes/2026-08-24-adr-0061-03-skill-compiler/specs/skill-compiler/spec.md
//          "编译 metadata 持久化" Requirement (frontmatter 6 字段)
// 作者：HydraForge Sprint 24 T17 ship
// 最后修改日期：2026-08-27 (T15: 占位结构 → TrajectoryIR 升级)
#pragma once

#include <string>

namespace agenticdsl {

// ============================================================================
// CompiledSkill — SkillCompiler::compile() 编译产物 (值类型, 纯数据)
// 不变量 (proposal §不变量):
//   - SkillCompiler 纯函数式: original_content 永不被修改
//   - ok=false 时 failure_reason 非空 (quality_poor / regression_fail /
//     budget_exceeded / infrastructure_error, 见 spec "编译失败原因分类")
// ============================================================================
struct CompiledSkill {
  bool ok = false;                          // 编译是否成功
  std::string skill_id;                     // 技能标识 (frontmatter name 或 "anonymous")
  std::string original_content;             // 原 SKILL.md 内容 (只读快照)
  std::string compiled_content;             // 编译产物 markdown (含 metadata frontmatter)
  std::string compiler_version = "skill-compiler-v1.0.0";
  std::string compiled_at;                  // ISO 8601 时间戳 (编译完成时刻)
  std::string trajectory_ir_hash;           // TrajectoryIR::hash(CanonicalIR) (T15)
  double ievaluator_score = 0.0;            // IEvaluator scalar [-1.0, 1.0] (无评估器时 0.0)
  std::string regression_verdict = "NotRun";  // T14 Verdict: Pass/Fail/Inconclusive/NotRun
  std::string failure_reason;               // ok=false 时的失败分类原因
};

}  // namespace agenticdsl
