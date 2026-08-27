// tests/test_skill_compiler.cpp
// 功能描述：SkillCompiler 单元测试 (T17, ADR-0061-03)
//          Phase 2 骨架: ≥5 cases 验证 L1 契约层类型编译
//          Phase 3+: compile_basic_skill / validate / 集成 (IEvaluator + T14 + G11 emit-only)
// 设计依据：docs/adr/skill/adr-0061-03-skill-compiler.md
//          + openspec/changes/2026-08-24-adr-0061-03-skill-compiler/specs/skill-compiler/spec.md
// 作者：HydraForge Sprint 24 T17 ship
// 最后修改日期：2026-08-27

#include "catch_amalgamated.hpp"

#include "agenticdsl/contract/iskill_compiler.h"
#include "agenticdsl/types/compiled_skill.h"
#include "agenticdsl/cognitive/skill_compiler.h"
#include "agenticdsl/ir/trajectory_ir.h"  // T15: TrajectoryIR::hash 集成验证

#include "agenticdsl/contract/bus_event.h"
#include "agenticdsl/contract/ievaluator.h"
#include "agenticdsl/types/execution_trace.h"
#include "agenticdsl/types/reward_signal.h"

#include <string>
#include <type_traits>
#include <vector>

using namespace agenticdsl;

// ============================================================================
// Phase 2 骨架 Case 1: ISkillCompiler 是纯虚抽象接口 (不可实例化)
// ============================================================================
TEST_CASE("ISkillCompiler is abstract contract", "[skill_compiler][phase2]") {
  REQUIRE(std::is_abstract_v<ISkillCompiler>);
  REQUIRE(std::has_virtual_destructor_v<ISkillCompiler>);
}

// ============================================================================
// Phase 2 骨架 Case 2: CompiledSkill 默认值类型字段
// ============================================================================
TEST_CASE("CompiledSkill default-constructed fields", "[skill_compiler][phase2]") {
  CompiledSkill cs;
  REQUIRE(cs.ok == false);
  REQUIRE(cs.compiler_version == "skill-compiler-v1.0.0");
  REQUIRE(cs.original_content.empty());
  REQUIRE(cs.compiled_content.empty());
  REQUIRE(cs.ievaluator_score == 0.0);
  REQUIRE(cs.failure_reason.empty());
}

// ============================================================================
// Phase 2 骨架 Case 3: CompiledSkill trajectory_ir_hash 默认为空
// (T15: TrajectoryPlaceholder 占位已删除, 升级为 TrajectoryIR::hash)
// ============================================================================
TEST_CASE("CompiledSkill trajectory_ir_hash default empty",
          "[skill_compiler][phase2]") {
  CompiledSkill cs;
  REQUIRE(cs.trajectory_ir_hash.empty());
}

// ============================================================================
// Phase 2 骨架 Case 4: skill.compilation.* 主题常量 (G11 emit-only)
// ============================================================================
TEST_CASE("skill.compilation topic constants", "[skill_compiler][phase2]") {
  REQUIRE(std::string(skill_compilation_topics::kStarted) ==
          "skill.compilation.started");
  REQUIRE(std::string(skill_compilation_topics::kSucceeded) ==
          "skill.compilation.succeeded");
  REQUIRE(std::string(skill_compilation_topics::kFailed) ==
          "skill.compilation.failed");
}

// ============================================================================
// Phase 2 骨架 Case 5: 编译产物 regression_verdict 默认 NotRun
// ============================================================================
TEST_CASE("CompiledSkill regression_verdict default NotRun",
          "[skill_compiler][phase2]") {
  CompiledSkill cs;
  REQUIRE(cs.regression_verdict == "NotRun");
}

// ============================================================================
// Phase 3: 核心编译逻辑 (T3.1-T3.5)
// ============================================================================

namespace {

const std::string kBasicSkillMd = R"(---
name: test-skill
description: A test skill for compilation
---

# Test Skill

## When to use
Use this skill for testing.

## Steps
1. Do the thing.
2. Verify the result.
)";

}  // namespace

// ============================================================================
// T15: CompiledSkill.trajectory_ir_hash 由 TrajectoryIR::hash 生成
// (替换 TrajectoryPlaceholder 占位, spec "真实 TrajectoryIR hash 集成")
// ============================================================================
TEST_CASE("compiled_skill_trajectory_ir_hash", "[skill_compiler][t15]") {
  SkillCompiler compiler;
  auto cs = compiler.compile(kBasicSkillMd);
  REQUIRE(cs.ok == true);
  REQUIRE(cs.trajectory_ir_hash.empty() == false);
  // V1: 编译时空 CanonicalIR 输入 (真实轨迹提取 V2 集成)
  const auto expected =
      ir::TrajectoryIR::hash(ir::TrajectoryIR::CanonicalIR{});
  REQUIRE(cs.trajectory_ir_hash == expected);
}

TEST_CASE("compile_basic_skill produces valid CompiledSkill",
          "[skill_compiler][phase3]") {
  SkillCompiler compiler;
  auto cs = compiler.compile(kBasicSkillMd);

  REQUIRE(cs.ok == true);
  REQUIRE(cs.skill_id == "test-skill");
  // 纯函数式不变量: 原内容不被修改
  REQUIRE(cs.original_content == kBasicSkillMd);
  // 编译产物包含 6 个 metadata frontmatter 字段 (spec "编译 metadata 持久化")
  REQUIRE(cs.compiled_content.find("compiled_at:") != std::string::npos);
  REQUIRE(cs.compiled_content.find("compiler_version:") != std::string::npos);
  REQUIRE(cs.compiled_content.find("baseline_skill_id:") != std::string::npos);
  REQUIRE(cs.compiled_content.find("trajectory_ir_hash:") != std::string::npos);
  REQUIRE(cs.compiled_content.find("ievaluator_score:") != std::string::npos);
  REQUIRE(cs.compiled_content.find("regression_verdict:") != std::string::npos);
  // 编译产物保留原 body
  REQUIRE(cs.compiled_content.find("# Test Skill") != std::string::npos);
  REQUIRE(compiler.validate(cs) == true);
}

TEST_CASE("compile is deterministic modulo timestamp",
          "[skill_compiler][phase3]") {
  SkillCompiler compiler;
  auto a = compiler.compile(kBasicSkillMd);
  auto b = compiler.compile(kBasicSkillMd);
  REQUIRE(a.ok);
  REQUIRE(b.ok);
  REQUIRE(a.skill_id == b.skill_id);
  REQUIRE(a.trajectory_ir_hash == b.trajectory_ir_hash);
  REQUIRE(a.regression_verdict == b.regression_verdict);
}

TEST_CASE("compile rejects empty content as infrastructure_error",
          "[skill_compiler][phase3]") {
  SkillCompiler compiler;
  auto cs = compiler.compile("");
  REQUIRE(cs.ok == false);
  REQUIRE(cs.failure_reason == "infrastructure_error");
}

TEST_CASE("compile handles missing frontmatter with anonymous skill_id",
          "[skill_compiler][phase3]") {
  SkillCompiler compiler;
  auto cs = compiler.compile("# Body Only\n\nNo frontmatter here.\n");
  REQUIRE(cs.ok == true);
  REQUIRE(cs.skill_id == "anonymous");
  REQUIRE(compiler.validate(cs) == true);
}

TEST_CASE("validate rejects structurally incomplete CompiledSkill",
          "[skill_compiler][phase3]") {
  SkillCompiler compiler;
  CompiledSkill empty;
  REQUIRE(compiler.validate(empty) == false);
}

// ============================================================================
// Phase 4: 集成 (T4.1-T4.4) — T14 回归 / IEvaluator / G11 emit-only
// ============================================================================

namespace {

// 可配置 quality 的 IEvaluator 测试替身 (与 test_mutation_governance StubEvaluator 同模式)
class StubEvaluator : public IEvaluator {
 public:
  RewardSignal::Quality quality = RewardSignal::Quality::Excellent;

  RewardSignal evaluate(const ExecutionTrace&) const override {
    switch (quality) {
      case RewardSignal::Quality::Excellent:
        return RewardSignal::excellent();
      case RewardSignal::Quality::Acceptable:
        return RewardSignal::acceptable();
      case RewardSignal::Quality::Poor:
        return RewardSignal::poor();
    }
    return RewardSignal::acceptable();
  }

  int compare(const ExecutionTrace&, const ExecutionTrace&) const override {
    return 0;
  }
};

// 同步记录全部事件的 IInteractionBus 测试替身
class RecordingBus : public IInteractionBus {
 public:
  std::vector<BusEvent> events;

  void emit(const BusEvent& event) override { events.push_back(event); }
  void emit(const std::string&, const std::string&) override {}
  size_t subscribe(const std::string&,
                   std::function<void(const BusEvent&)>) override {
    return 0;
  }
  void unsubscribe(size_t) override {}
};

}  // namespace

// T4.1: T14 行为回归集成 — 编译后自动验证, 产物带 Verdict
TEST_CASE("compile integrates T14 regression verdict",
          "[skill_compiler][phase4]") {
  SkillCompiler compiler;
  auto cs = compiler.compile(kBasicSkillMd);
  REQUIRE(cs.ok);
  // V1 模板包装不改语义: 恒等指纹比较必为 Pass
  REQUIRE(cs.regression_verdict == "Pass");
}

// T4.2: IEvaluator 集成 — 评分透传 + Poor 质量门拒绝
TEST_CASE("compile integrates IEvaluator quality gate",
          "[skill_compiler][phase4]") {
  auto excellent = std::make_shared<StubEvaluator>();
  excellent->quality = RewardSignal::Quality::Excellent;
  SkillCompiler ok_compiler(excellent, nullptr);
  auto cs = ok_compiler.compile(kBasicSkillMd);
  REQUIRE(cs.ok);
  REQUIRE(cs.ievaluator_score == 1.0);

  auto poor = std::make_shared<StubEvaluator>();
  poor->quality = RewardSignal::Quality::Poor;
  SkillCompiler poor_compiler(poor, nullptr);
  auto rejected = poor_compiler.compile(kBasicSkillMd);
  REQUIRE(rejected.ok == false);
  REQUIRE(rejected.failure_reason == "quality_poor");
  // 纯函数式回滚语义: 原内容保持只读快照
  REQUIRE(rejected.original_content == kBasicSkillMd);
  REQUIRE(rejected.compiled_content.empty());
}

// T4.3: G11 emit-only — started + succeeded 恰好各一次
TEST_CASE("compile emits started and succeeded events (emit-only)",
          "[skill_compiler][phase4]") {
  auto bus = std::make_shared<RecordingBus>();
  SkillCompiler compiler(nullptr, bus);
  auto cs = compiler.compile(kBasicSkillMd);
  REQUIRE(cs.ok);

  REQUIRE(bus->events.size() == 2);
  REQUIRE(bus->events[0].topic == "skill.compilation.started");
  REQUIRE(bus->events[0].payload.data["skill_id"] == "test-skill");
  REQUIRE(bus->events[0].payload.data["compiler_version"] ==
          "skill-compiler-v1.0.0");
  REQUIRE(bus->events[1].topic == "skill.compilation.succeeded");
  REQUIRE(bus->events[1].payload.data["regression_verdict"] == "Pass");
}

// T4.3: G11 emit-only — 失败路径 started + failed, 终态互斥
TEST_CASE("failed compile emits started and failed (terminal exclusive)",
          "[skill_compiler][phase4]") {
  auto bus = std::make_shared<RecordingBus>();
  auto poor = std::make_shared<StubEvaluator>();
  poor->quality = RewardSignal::Quality::Poor;
  SkillCompiler compiler(poor, bus);
  auto cs = compiler.compile(kBasicSkillMd);
  REQUIRE_FALSE(cs.ok);

  REQUIRE(bus->events.size() == 2);
  REQUIRE(bus->events[0].topic == "skill.compilation.started");
  REQUIRE(bus->events[1].topic == "skill.compilation.failed");
  REQUIRE(bus->events[1].payload.ok == false);
  REQUIRE(bus->events[1].payload.data["reason"] == "quality_poor");
}

// T4.3 边界: 空内容 + bus — 仅 failed (解析前置失败, 无 started)
TEST_CASE("empty content emits only failed event", "[skill_compiler][phase4]") {
  auto bus = std::make_shared<RecordingBus>();
  SkillCompiler compiler(nullptr, bus);
  auto cs = compiler.compile("");
  REQUIRE_FALSE(cs.ok);
  REQUIRE(bus->events.size() == 1);
  REQUIRE(bus->events[0].topic == "skill.compilation.failed");
  REQUIRE(bus->events[0].payload.data["reason"] == "infrastructure_error");
}
