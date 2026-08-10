// tests/test_pdk_manifest_validator.cpp
// PDK ManifestValidator TDD - Phase 6a step 2 (Tasks 2.1-2.11)
// 11 TEST_CASE 覆盖 manifest 校验全部场景
// 设计依据: openspec/changes/pdk-manifest-validation (Phase 6a)
// 关联 ADR: ADR-0052 §决策 2-3 (9 必填 + 8 推荐)
// 作者: Sisyphus
// 日期: 2026-08-10

#include "catch_amalgamated.hpp"

#include "agenticdsl/pdk/manifest_validator.h"

using namespace agenticdsl::pdk;

// ============================================================================
// Task 2.1: valid_minimal_manifest
// ============================================================================
TEST_CASE("ManifestValidator: valid_minimal_manifest", "[pdk][validator]") {
  std::string json = R"({
    "id": "code.review",
    "name": "Code Review Agent",
    "version": "0.1.0",
    "abi_version": 2,
    "min_host_version": "2.0.0",
    "max_host_version": "3.0.0",
    "implementation_forms": ["cpp"],
    "entry_tool": "review/run",
    "provided_tools": ["review/run", "review/suggest"]
  })";
  auto result = ManifestValidator::validate(json);
  REQUIRE(result.valid == true);
  REQUIRE(result.errors.empty());
  REQUIRE(result.manifest.has_value());
  REQUIRE(result.manifest->id == "code.review");
  REQUIRE(result.manifest->name == "Code Review Agent");
  REQUIRE(result.manifest->version == "0.1.0");
  REQUIRE(result.manifest->abi_version == 2);
  REQUIRE(result.manifest->min_host_version == "2.0.0");
  REQUIRE(result.manifest->max_host_version == "3.0.0");
  REQUIRE(result.manifest->implementation_forms.size() == 1);
  REQUIRE(result.manifest->implementation_forms[0] == "cpp");
  REQUIRE(result.manifest->entry_tool == "review/run");
  REQUIRE(result.manifest->provided_tools.size() == 2);
}

// ============================================================================
// Task 2.2: missing_required_field_id
// ============================================================================
TEST_CASE("ManifestValidator: missing_required_field_id", "[pdk][validator]") {
  std::string json = R"({
    "name": "Code Review Agent",
    "version": "0.1.0",
    "abi_version": 2,
    "min_host_version": "2.0.0",
    "max_host_version": "3.0.0",
    "implementation_forms": ["cpp"],
    "entry_tool": "review/run",
    "provided_tools": ["review/run", "review/suggest"]
  })";
  auto result = ManifestValidator::validate(json);
  REQUIRE(result.valid == false);
  REQUIRE_FALSE(result.manifest.has_value());
  REQUIRE_FALSE(result.errors.empty());
  bool found_id_error = false;
  for (const auto& err : result.errors) {
    if (err.field == "id" && err.reason == "required") {
      found_id_error = true;
      break;
    }
  }
  REQUIRE(found_id_error);
}

// ============================================================================
// Task 2.3: invalid_semver_version
// ============================================================================
TEST_CASE("ManifestValidator: invalid_semver_version", "[pdk][validator]") {
  std::string json = R"({
    "id": "code.review",
    "name": "Code Review Agent",
    "version": "1.0",
    "abi_version": 2,
    "min_host_version": "2.0.0",
    "max_host_version": "3.0.0",
    "implementation_forms": ["cpp"],
    "entry_tool": "review/run",
    "provided_tools": ["review/run", "review/suggest"]
  })";
  auto result = ManifestValidator::validate(json);
  REQUIRE(result.valid == false);
  REQUIRE_FALSE(result.manifest.has_value());
  bool found_semver_error = false;
  for (const auto& err : result.errors) {
    if (err.field == "version" && err.reason == "invalid_semver") {
      found_semver_error = true;
      break;
    }
  }
  REQUIRE(found_semver_error);
}

// ============================================================================
// Task 2.4: abi_version_out_of_range
// ============================================================================
TEST_CASE("ManifestValidator: abi_version_out_of_range", "[pdk][validator]") {
  std::string json = R"({
    "id": "code.review",
    "name": "Code Review Agent",
    "version": "0.1.0",
    "abi_version": 3,
    "min_host_version": "2.0.0",
    "max_host_version": "3.0.0",
    "implementation_forms": ["cpp"],
    "entry_tool": "review/run",
    "provided_tools": ["review/run", "review/suggest"]
  })";
  auto result = ManifestValidator::validate(json);
  REQUIRE(result.valid == false);
  REQUIRE_FALSE(result.manifest.has_value());
  bool found_abi_error = false;
  for (const auto& err : result.errors) {
    if (err.field == "abi_version" && err.reason == "mismatch") {
      found_abi_error = true;
      break;
    }
  }
  REQUIRE(found_abi_error);
}

// ============================================================================
// Task 2.5: wrong_type_string_for_uint32
// ============================================================================
TEST_CASE("ManifestValidator: wrong_type_string_for_uint32", "[pdk][validator]") {
  std::string json = R"({
    "id": "code.review",
    "name": "Code Review Agent",
    "version": "0.1.0",
    "abi_version": "1",
    "min_host_version": "2.0.0",
    "max_host_version": "3.0.0",
    "implementation_forms": ["cpp"],
    "entry_tool": "review/run",
    "provided_tools": ["review/run", "review/suggest"]
  })";
  auto result = ManifestValidator::validate(json);
  REQUIRE(result.valid == false);
  REQUIRE_FALSE(result.manifest.has_value());
  bool found_type_error = false;
  for (const auto& err : result.errors) {
    if (err.field == "abi_version" && err.reason == "wrong_type") {
      found_type_error = true;
      break;
    }
  }
  REQUIRE(found_type_error);
}

// ============================================================================
// Task 2.6: wrong_type_null_for_string
// ============================================================================
TEST_CASE("ManifestValidator: wrong_type_null_for_string", "[pdk][validator]") {
  std::string json = R"({
    "id": "code.review",
    "name": null,
    "version": "0.1.0",
    "abi_version": 2,
    "min_host_version": "2.0.0",
    "max_host_version": "3.0.0",
    "implementation_forms": ["cpp"],
    "entry_tool": "review/run",
    "provided_tools": ["review/run", "review/suggest"]
  })";
  auto result = ManifestValidator::validate(json);
  REQUIRE(result.valid == false);
  REQUIRE_FALSE(result.manifest.has_value());
  bool found_type_error = false;
  for (const auto& err : result.errors) {
    if (err.field == "name" && err.reason == "wrong_type") {
      found_type_error = true;
      break;
    }
  }
  REQUIRE(found_type_error);
}

// ============================================================================
// Task 2.7: invalid_implementation_forms_value
// ============================================================================
TEST_CASE("ManifestValidator: invalid_implementation_forms_value", "[pdk][validator]") {
  std::string json = R"({
    "id": "code.review",
    "name": "Code Review Agent",
    "version": "0.1.0",
    "abi_version": 2,
    "min_host_version": "2.0.0",
    "max_host_version": "3.0.0",
    "implementation_forms": ["python"],
    "entry_tool": "review/run",
    "provided_tools": ["review/run", "review/suggest"]
  })";
  auto result = ManifestValidator::validate(json);
  REQUIRE(result.valid == false);
  REQUIRE_FALSE(result.manifest.has_value());
  bool found_impl_error = false;
  for (const auto& err : result.errors) {
    if (err.field == "implementation_forms[0]" && err.reason == "invalid_enum") {
      found_impl_error = true;
      break;
    }
  }
  REQUIRE(found_impl_error);
}

// ============================================================================
// Task 2.8: entry_tool_not_in_provided_tools
// ============================================================================
TEST_CASE("ManifestValidator: entry_tool_not_in_provided_tools", "[pdk][validator]") {
  std::string json = R"({
    "id": "code.review",
    "name": "Code Review Agent",
    "version": "0.1.0",
    "abi_version": 2,
    "min_host_version": "2.0.0",
    "max_host_version": "3.0.0",
    "implementation_forms": ["cpp"],
    "entry_tool": "foo/run",
    "provided_tools": ["review/run", "review/suggest"]
  })";
  auto result = ManifestValidator::validate(json);
  REQUIRE(result.valid == false);
  REQUIRE_FALSE(result.manifest.has_value());
  bool found_entry_error = false;
  for (const auto& err : result.errors) {
    if (err.field == "entry_tool" && err.reason == "not_in_provided_tools") {
      found_entry_error = true;
      break;
    }
  }
  REQUIRE(found_entry_error);
}

// ============================================================================
// Task 2.9: tools_missing_input_schema
// ============================================================================
TEST_CASE("ManifestValidator: tools_missing_input_schema", "[pdk][validator]") {
  std::string json = R"({
    "id": "code.review",
    "name": "Code Review Agent",
    "version": "0.1.0",
    "abi_version": 2,
    "min_host_version": "2.0.0",
    "max_host_version": "3.0.0",
    "implementation_forms": ["cpp"],
    "entry_tool": "review/run",
    "provided_tools": ["review/run", "review/suggest"],
    "tools": [
      {
        "name": "review/run",
        "description": "Run code review"
      }
    ]
  })";
  auto result = ManifestValidator::validate(json);
  REQUIRE(result.valid == false);
  REQUIRE_FALSE(result.manifest.has_value());
  bool found_tool_error = false;
  for (const auto& err : result.errors) {
    if (err.field == "tools[0].input_schema" && err.reason == "required") {
      found_tool_error = true;
      break;
    }
  }
  REQUIRE(found_tool_error);
}

// ============================================================================
// Task 2.10: invalid_approval_policy_value
// ============================================================================
TEST_CASE("ManifestValidator: invalid_approval_policy_value", "[pdk][validator]") {
  std::string json = R"({
    "id": "code.review",
    "name": "Code Review Agent",
    "version": "0.1.0",
    "abi_version": 2,
    "min_host_version": "2.0.0",
    "max_host_version": "3.0.0",
    "implementation_forms": ["cpp"],
    "entry_tool": "review/run",
    "provided_tools": ["review/run", "review/suggest"],
    "tools": [
      {
        "name": "review/run",
        "description": "Run code review",
        "input_schema": "{}",
        "output_schema": "{}",
        "approval_policy": "invalid_policy"
      }
    ]
  })";
  auto result = ManifestValidator::validate(json);
  REQUIRE(result.valid == false);
  REQUIRE_FALSE(result.manifest.has_value());
  bool found_policy_error = false;
  for (const auto& err : result.errors) {
    if (err.field == "tools[0].approval_policy" && err.reason == "invalid_enum") {
      found_policy_error = true;
      break;
    }
  }
  REQUIRE(found_policy_error);
}

// ============================================================================
// Task 2.11: valid_manifest_with_recommended_fields
// ============================================================================
TEST_CASE("ManifestValidator: valid_manifest_with_recommended_fields", "[pdk][validator]") {
  std::string json = R"({
    "id": "code.review",
    "name": "Code Review Agent",
    "version": "0.1.0",
    "abi_version": 1,
    "min_host_version": "2.0.0",
    "max_host_version": "3.0.0",
    "implementation_forms": ["cpp", "dsl"],
    "entry_tool": "review/run",
    "provided_tools": ["review/run", "review/suggest"],
    "interface_versions": ["IAgentV1"],
    "capabilities": ["code_review", "static_analysis"],
    "requires_isolation": true,
    "resources": {
      "timeout_ms": 60000,
      "max_concurrent": 4
    },
    "publisher": "ACME Corp",
    "trust_level": "high",
    "activation_events": ["on_session_start"],
    "tools": [
      {
        "name": "review/run",
        "description": "Run code review",
        "input_schema": "{\"type\":\"object\"}",
        "output_schema": "{\"type\":\"object\"}",
        "approval_policy": "plan"
      }
    ]
  })";
  auto result = ManifestValidator::validate(json);
  REQUIRE(result.valid == true);
  REQUIRE(result.errors.empty());
  REQUIRE(result.manifest.has_value());
  REQUIRE(result.manifest->abi_version == 1);
  REQUIRE(result.manifest->implementation_forms.size() == 2);
  REQUIRE(result.manifest->requires_isolation == true);
  REQUIRE(result.manifest->resources.timeout_ms == 60000);
  REQUIRE(result.manifest->resources.max_concurrent == 4);
  REQUIRE(result.manifest->trust_level == "high");
  REQUIRE(result.manifest->tools.size() == 1);
  REQUIRE(result.manifest->tools[0].approval_policy == "plan");
}