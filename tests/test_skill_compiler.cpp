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

#include <string>
#include <type_traits>

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
// Phase 2 骨架 Case 3: TrajectoryPlaceholder (T15 软依赖占位)
// ============================================================================
TEST_CASE("TrajectoryPlaceholder default is empty", "[skill_compiler][phase2]") {
  TrajectoryPlaceholder tp;
  REQUIRE(tp.raw.empty());
  REQUIRE(tp.hash().empty() == false);  // 空输入仍有确定性 hash
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
