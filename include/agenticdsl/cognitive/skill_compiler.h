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
//
// V1 简化语义 (避免误读):
//   - IEvaluator 质量门评估对象 = "编译动作成功" 的合成 ExecutionTrace,
//     **不是**编译产物的语义/质量。V1 不构建 ParsedGraph, 无产物轨迹可评估。
//     实际产物质量评估待 V2 接入真实 TrajectoryIR (SKILL→ParsedGraph + ADR-0061-13
//     DistillationRecord.steps) 后扩展。
//   - T14 行为回归自检 = 空指纹恒等比较恒为 Pass (V1 模板包装不改语义);
//     真实回归测试待 V2 同上条件满足后接入。
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
