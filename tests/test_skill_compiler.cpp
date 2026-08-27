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
