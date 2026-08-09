// PDK Manifest POD types 编译测试 - Phase 6a step 1 Task 1.1
// 验证 Manifest/ToolSpec/Resources/ValidationError/ManifestValidationResult
// 可以正确构造和访问 (header compile test)
//
// 设计依据: openspec/changes/pdk-manifest-validation (Phase 6a)
// 关联 ADR: ADR-0052 §决策 2 (9 必填字段)
// 作者: Sisyphus
// 日期: 2026-08-09

#include "catch_amalgamated.hpp"

#include "agenticdsl/pdk/manifest.h"

using namespace agenticdsl::pdk;

TEST_CASE("Manifest POD types are default-constructible", "[pdk][manifest][types]") {
  Manifest m;
  REQUIRE(m.id.empty());
  REQUIRE(m.name.empty());
  REQUIRE(m.version.empty());
  REQUIRE(m.abi_version == 0);
  REQUIRE(m.implementation_forms.empty());
  REQUIRE(m.provided_tools.empty());
  REQUIRE(m.trust_level == "untrusted");
  REQUIRE_FALSE(m.requires_isolation);
  REQUIRE(m.resources.timeout_ms == 30000);
  REQUIRE(m.resources.max_concurrent == 1);
  REQUIRE_FALSE(m.signature.has_value());
}

TEST_CASE("Manifest can be populated with all required fields", "[pdk][manifest][types]") {
  Manifest m;
  m.id = "code.review";
  m.name = "Code Review Agent";
  m.version = "0.1.0";
  m.abi_version = 2;
  m.min_host_version = "2.0.0";
  m.max_host_version = "3.0.0";
  m.implementation_forms = {"dsl", "cpp"};
  m.entry_tool = "code_review/run";
  m.provided_tools = {"code_review/run", "code_review/suggest"};

  REQUIRE(m.id == "code.review");
  REQUIRE(m.abi_version == 2);
  REQUIRE(m.implementation_forms.size() == 2);
  REQUIRE(m.entry_tool == "code_review/run");
  REQUIRE(m.provided_tools.size() == 2);
}

TEST_CASE("ToolSpec carries tool schema fields", "[pdk][manifest][types]") {
  ToolSpec ts;
  ts.name = "code_review/run";
  ts.description = "Run code review on a diff";
  ts.input_schema = R"({"type":"object","required":["diff"]})";
  ts.approval_policy = "plan";

  REQUIRE(ts.name == "code_review/run");
  REQUIRE(ts.approval_policy == "plan");
}

TEST_CASE("ValidationError carries field/reason/value/expected", "[pdk][manifest][types]") {
  ValidationError err;
  err.field = "abi_version";
  err.reason = "mismatch";
  err.value = "3";
  err.expected = "1|2";
  REQUIRE(err.field == "abi_version");
  REQUIRE(err.reason == "mismatch");
  REQUIRE(err.value == "3");
  REQUIRE(err.expected == "1|2");
}

TEST_CASE("ManifestValidationResult is default-invalid", "[pdk][manifest][types]") {
  ManifestValidationResult result;
  REQUIRE_FALSE(result.valid);
  REQUIRE_FALSE(result.manifest.has_value());
  REQUIRE(result.errors.empty());
}
