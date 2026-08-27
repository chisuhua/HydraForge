// include/agenticdsl/types/compiled_skill.h
// 功能描述：CompiledSkill 值类型 (T17, ADR-0061-03 SkillCompiler 编译产物)
//          + TrajectoryPlaceholder (T15 Trajectory IR 软依赖 V1 占位结构,
//            T15 ship 后无缝替换为 CanonicalIR, 见 tasks.md T1.3 DEFERRED)
// 设计依据：openspec/changes/2026-08-24-adr-0061-03-skill-compiler/specs/skill-compiler/spec.md
//          "编译 metadata 持久化" Requirement (frontmatter 6 字段)
// 作者：HydraForge Sprint 24 T17 ship
// 最后修改日期：2026-08-27
#pragma once

#include <cstdint>
#include <sstream>
#include <string>

namespace agenticdsl {

// ============================================================================
// TrajectoryPlaceholder — T15 Trajectory IR (ADR-0061-06 v1.1) V1 占位
// V1 边界: 仅持有 opaque 原始文本 + 确定性 hash; 不做结构化解析。
// T15 ship 后由 CanonicalIR 替换, 本结构保留 hash() 语义以兼容编译 metadata。
// ============================================================================
struct TrajectoryPlaceholder {
  std::string raw;  // 不透明轨迹文本 (V1: 可为空 = 无轨迹输入)

  // 确定性 hash (std::hash 十六进制, 空输入亦产生非空稳定值)
  // 用于编译 frontmatter 的 trajectory_ir_hash 字段
  std::string hash() const {
    std::ostringstream oss;
    oss << std::hex << std::hash<std::string>{}(raw);
    return oss.str();
  }
};

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
  std::string trajectory_ir_hash;           // TrajectoryPlaceholder::hash()
  double ievaluator_score = 0.0;            // IEvaluator scalar [-1.0, 1.0] (无评估器时 0.0)
  std::string regression_verdict = "NotRun";  // T14 Verdict: Pass/Fail/Inconclusive/NotRun
  std::string failure_reason;               // ok=false 时的失败分类原因
};

}  // namespace agenticdsl
