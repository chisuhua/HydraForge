// include/agenticdsl/contract/iskill_compiler.h
// 功能描述：ISkillCompiler L1 契约层接口 (T17, ADR-0061-03)
//          SKILL.md → CompiledSkill 纯函数式编译契约 + validate 验证。
//          skill.compilation.{started,succeeded,failed} 3 主题常量
//          (G11 emit-only 模式, ADR-0068 附录 A 注册, 不触发 MutationGovernor)。
// 设计依据：docs/adr/skill/adr-0061-03-skill-compiler.md
//          + openspec/changes/2026-08-24-adr-0061-03-skill-compiler/specs/skill-compiler/spec.md
//          §不变量 (纯函数式 / T14 回归门 / IEvaluator 评分门 / 3 事件审计)
// 作者：HydraForge Sprint 24 T17 ship
// 最后修改日期：2026-08-27
#pragma once

#include "agenticdsl/types/compiled_skill.h"

#include <string>

namespace agenticdsl {

// ============================================================================
// skill.compilation.* 主题常量 (ADR-0068 附录 A 文档注册; emit-only 模式)
// ============================================================================
namespace skill_compilation_topics {
inline constexpr const char* kStarted = "skill.compilation.started";
inline constexpr const char* kSucceeded = "skill.compilation.succeeded";
inline constexpr const char* kFailed = "skill.compilation.failed";
}  // namespace skill_compilation_topics

// ============================================================================
// ISkillCompiler — SKILL.md 编译器抽象接口
//
// 契约 (spec §Requirement 1):
//   - 纯函数式: compile() 不修改输入, 无副作用 (事件发射除外, 见 G11 emit-only)
//   - 编译产物必须通过 validate() 结构校验
//   - 实现必须线程安全 (const 方法, 无可变成员状态)
// ============================================================================
class ISkillCompiler {
 public:
  virtual ~ISkillCompiler() = default;

  /**
   * @brief 编译 SKILL.md 内容为优化产物
   * @param skill_md_content SKILL.md 原始内容 (Anthropic Skills 格式)
   * @return CompiledSkill — ok=true 含编译产物; ok=false 含 failure_reason
   */
  virtual CompiledSkill compile(const std::string& skill_md_content) const = 0;

  /**
   * @brief 验证编译产物结构完整性 (metadata frontmatter 6 字段齐全)
   * @param compiled 编译产物
   * @return true = 结构合法
   */
  virtual bool validate(const CompiledSkill& compiled) const = 0;
};

}  // namespace agenticdsl
