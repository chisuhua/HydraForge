// include/agenticdsl/cognitive/skill_compiler.h
// 功能描述：SkillCompiler V1 实现类 (T17, ADR-0061-03)
//          纯函数式 SKILL.md → CompiledSkill 编译器:
//          YAML frontmatter 解析 + 模板驱动 metadata 注入 + T14 回归自检
//          + IEvaluator 质量门 + G11 emit-only 审计事件。
//          V1 边界: 仅支持 Anthropic/Cline Skills 标准 (ADR-0061-01);
//          不实现 L4 权重 / V2 dynamic composition; 不触发 MutationGovernor。
// 设计依据：docs/adr/skill/adr-0061-03-skill-compiler.md §决策 1-3
//          + openspec/changes/2026-08-24-adr-0061-03-skill-compiler/specs/skill-compiler/spec.md
// 作者：HydraForge Sprint 24 T17 ship
// 最后修改日期：2026-08-27
#pragma once

#include "agenticdsl/contract/ievaluator.h"
#include "agenticdsl/contract/iinteraction_bus.h"
#include "agenticdsl/contract/iskill_compiler.h"

#include <memory>
#include <string>

namespace agenticdsl {

// ============================================================================
// SkillCompiler — ISkillCompiler V1 实现
// 构造注入 (与 CognitiveWorker::set_evaluator 注入模式一致):
//   - evaluator: 可选 IEvaluator (nullptr = 跳过质量门, ievaluator_score=0.0)
//   - bus: 可选 IInteractionBus (nullptr = 不发射 skill.compilation.* 事件)
// 线程安全: 全部成员 const 方法 + 无可变状态。
// ============================================================================
class SkillCompiler : public ISkillCompiler {
 public:
  explicit SkillCompiler(std::shared_ptr<IEvaluator> evaluator = nullptr,
                         std::shared_ptr<IInteractionBus> bus = nullptr);

  CompiledSkill compile(const std::string& skill_md_content) const override;
  bool validate(const CompiledSkill& compiled) const override;

 private:
  std::shared_ptr<IEvaluator> evaluator_;
  std::shared_ptr<IInteractionBus> bus_;
};

}  // namespace agenticdsl
