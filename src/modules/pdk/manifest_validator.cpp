// src/modules/pdk/manifest_validator.cpp
// PDK ManifestValidator 完整实现 - Phase 6a step 2 (Tasks 2.1-2.11)
// TDD: 11 test case 全部覆盖
// 设计依据: openspec/changes/pdk-manifest-validation (Phase 6a)
// 关联 ADR: ADR-0052 §决策 2-3 (9 必填 + 8 推荐)
// 作者: Sisyphus
// 日期: 2026-08-10

#include "agenticdsl/pdk/manifest_validator.h"

#include <nlohmann/json.hpp>
#include <regex>
#include <set>

namespace agenticdsl::pdk {

namespace {

constexpr uint32_t CURRENT_ABI_VERSION = 2;
constexpr std::array<uint32_t, 2> SUPPORTED_ABI_VERSIONS = {1, CURRENT_ABI_VERSION};

constexpr std::array<const char*, 4> VALID_IMPLEMENTATION_FORMS = {"skill", "dsl", "cpp", "wasm"};
constexpr std::array<const char*, 4> VALID_APPROVAL_POLICIES = {"always", "plan", "agent", "yolo"};
constexpr std::array<const char*, 4> VALID_TRUST_LEVELS = {"high", "medium", "low", "untrusted"};

bool is_valid_semver(const std::string& s) {
  static const std::regex semver_re(R"(^\d+\.\d+\.\d+(-[a-zA-Z0-9.]+)?(\+[a-zA-Z0-9.]+)?$)");
  return std::regex_match(s, semver_re);
}

bool is_supported_abi(uint32_t v) {
  for (auto supported : SUPPORTED_ABI_VERSIONS) {
    if (v == supported) return true;
  }
  return false;
}

bool is_valid_implementation_form(const std::string& form) {
  for (auto valid : VALID_IMPLEMENTATION_FORMS) {
    if (form == valid) return true;
  }
  return false;
}

bool is_valid_approval_policy(const std::string& policy) {
  for (auto valid : VALID_APPROVAL_POLICIES) {
    if (policy == valid) return true;
  }
  return false;
}

bool is_valid_trust_level(const std::string& level) {
  for (auto valid : VALID_TRUST_LEVELS) {
    if (level == valid) return true;
  }
  return false;
}

}  // namespace

ManifestValidationResult ManifestValidator::validate(const std::string& json_content) {
  ManifestValidationResult result;

  // Step 1: Parse JSON
  nlohmann::json j;
  try {
    j = nlohmann::json::parse(json_content);
  } catch (const std::exception&) {
    result.errors.push_back({"json", "parse_error", "", ""});
    return result;
  }

  // Helper: check if field exists and is string
  auto get_string = [&](const std::string& field) -> std::optional<std::string> {
    if (!j.contains(field)) {
      result.errors.push_back({field, "required", "", ""});
      return std::nullopt;
    }
    if (j[field].is_null()) {
      result.errors.push_back({field, "wrong_type", "null", "string"});
      return std::nullopt;
    }
    if (!j[field].is_string()) {
      result.errors.push_back({field, "wrong_type", j[field].dump(), "string"});
      return std::nullopt;
    }
    return j[field].get<std::string>();
  };

  // Helper: check if field exists and is unsigned integer
  auto get_uint32 = [&](const std::string& field) -> std::optional<uint32_t> {
    if (!j.contains(field)) {
      result.errors.push_back({field, "required", "", ""});
      return std::nullopt;
    }
    if (!j[field].is_number_unsigned()) {
      result.errors.push_back({field, "wrong_type", j[field].dump(), "uint32"});
      return std::nullopt;
    }
    return j[field].get<uint32_t>();
  };

  // Helper: check if field exists and is bool
  auto get_bool = [&](const std::string& field, bool default_val) -> bool {
    if (!j.contains(field)) {
      return default_val;
    }
    if (j[field].is_boolean()) {
      return j[field].get<bool>();
    }
    return default_val;
  };

  // Helper: check if field exists and is array of strings
  auto get_string_array = [&](const std::string& field) -> std::optional<std::vector<std::string>> {
    if (!j.contains(field)) {
      result.errors.push_back({field, "required", "", ""});
      return std::nullopt;
    }
    if (!j[field].is_array()) {
      result.errors.push_back({field, "wrong_type", j[field].dump(), "array"});
      return std::nullopt;
    }
    std::vector<std::string> arr;
    for (const auto& elem : j[field]) {
      if (!elem.is_string()) {
        result.errors.push_back({field, "wrong_type", elem.dump(), "string"});
        return std::nullopt;
      }
      arr.push_back(elem.get<std::string>());
    }
    return arr;
  };

  // Helper: optional string array (returns empty if missing)
  auto get_optional_string_array = [&](const std::string& field) -> std::vector<std::string> {
    if (!j.contains(field) || !j[field].is_array()) {
      return {};
    }
    std::vector<std::string> arr;
    for (const auto& elem : j[field]) {
      if (elem.is_string()) {
        arr.push_back(elem.get<std::string>());
      }
    }
    return arr;
  };

  // Helper: optional string (returns empty if missing or wrong type)
  auto get_optional_string = [&](const std::string& field) -> std::string {
    if (!j.contains(field)) {
      return "";
    }
    if (j[field].is_string()) {
      return j[field].get<std::string>();
    }
    return "";
  };

  // Step 2: Extract and validate required fields
  auto id_opt = get_string("id");
  if (!id_opt.has_value()) return result;
  std::string id = *id_opt;

  auto name_opt = get_string("name");
  if (!name_opt.has_value()) return result;
  std::string name = *name_opt;

  auto version_opt = get_string("version");
  if (!version_opt.has_value()) return result;
  std::string version = *version_opt;
  if (!is_valid_semver(version)) {
    result.errors.push_back({"version", "invalid_semver", version, ""});
    return result;
  }

  auto abi_opt = get_uint32("abi_version");
  if (!abi_opt.has_value()) return result;
  uint32_t abi_version = *abi_opt;
  if (!is_supported_abi(abi_version)) {
    result.errors.push_back({"abi_version", "mismatch", std::to_string(abi_version), "1|2"});
    return result;
  }

  auto min_host_opt = get_string("min_host_version");
  if (!min_host_opt.has_value()) return result;
  std::string min_host_version = *min_host_opt;
  if (!is_valid_semver(min_host_version)) {
    result.errors.push_back({"min_host_version", "invalid_semver", min_host_version, ""});
    return result;
  }

  auto max_host_opt = get_string("max_host_version");
  if (!max_host_opt.has_value()) return result;
  std::string max_host_version = *max_host_opt;
  if (!is_valid_semver(max_host_version)) {
    result.errors.push_back({"max_host_version", "invalid_semver", max_host_version, ""});
    return result;
  }

  auto impl_forms_opt = get_string_array("implementation_forms");
  if (!impl_forms_opt.has_value()) return result;
  std::vector<std::string> impl_forms = *impl_forms_opt;
  if (impl_forms.empty()) {
    result.errors.push_back({"implementation_forms", "required", "", "non-empty array"});
    return result;
  }
  for (size_t i = 0; i < impl_forms.size(); ++i) {
    if (!is_valid_implementation_form(impl_forms[i])) {
      result.errors.push_back({"implementation_forms[" + std::to_string(i) + "]", "invalid_enum", impl_forms[i], "skill|dsl|cpp|wasm"});
      return result;
    }
  }

  auto entry_tool_opt = get_string("entry_tool");
  if (!entry_tool_opt.has_value()) return result;
  std::string entry_tool = *entry_tool_opt;

  auto provided_tools_opt = get_string_array("provided_tools");
  if (!provided_tools_opt.has_value()) return result;
  std::vector<std::string> provided_tools = *provided_tools_opt;
  if (provided_tools.empty()) {
    result.errors.push_back({"provided_tools", "required", "", "non-empty array"});
    return result;
  }

  // Cross-field validation: entry_tool must be in provided_tools
  bool entry_found = false;
  for (const auto& tool : provided_tools) {
    if (tool == entry_tool) {
      entry_found = true;
      break;
    }
  }
  if (!entry_found) {
    result.errors.push_back({"entry_tool", "not_in_provided_tools", entry_tool, ""});
    return result;
  }

  // Step 3: Extract optional/recommended fields
  Manifest manifest;
  manifest.id = id;
  manifest.name = name;
  manifest.version = version;
  manifest.abi_version = abi_version;
  manifest.min_host_version = min_host_version;
  manifest.max_host_version = max_host_version;
  manifest.implementation_forms = impl_forms;
  manifest.entry_tool = entry_tool;
  manifest.provided_tools = provided_tools;

  // Recommended fields with defaults
  manifest.interface_versions = get_optional_string_array("interface_versions");
  manifest.capabilities = get_optional_string_array("capabilities");
  manifest.input_schema = get_optional_string("input_schema");
  manifest.output_schema = get_optional_string("output_schema");
  manifest.requires_isolation = get_bool("requires_isolation", false);
  manifest.publisher = get_optional_string("publisher");
  manifest.trust_level = get_optional_string("trust_level");
  if (manifest.trust_level.empty()) {
    manifest.trust_level = "untrusted";
  } else if (!is_valid_trust_level(manifest.trust_level)) {
    result.errors.push_back({"trust_level", "invalid_enum", manifest.trust_level, "high|medium|low|untrusted"});
    return result;
  }
  manifest.activation_events = get_optional_string_array("activation_events");

  // Resources
  if (j.contains("resources") && j["resources"].is_object()) {
    auto& res = j["resources"];
    if (res.contains("timeout_ms")) {
      if (res["timeout_ms"].is_number_unsigned()) {
        manifest.resources.timeout_ms = res["timeout_ms"].get<uint32_t>();
      } else {
        result.errors.push_back({"resources.timeout_ms", "wrong_type", res["timeout_ms"].dump(), "uint32"});
        return result;
      }
    }
    if (res.contains("max_concurrent")) {
      if (res["max_concurrent"].is_number_unsigned()) {
        manifest.resources.max_concurrent = res["max_concurrent"].get<uint32_t>();
      } else {
        result.errors.push_back({"resources.max_concurrent", "wrong_type", res["max_concurrent"].dump(), "uint32"});
        return result;
      }
    }
  }

  // Tools array
  if (j.contains("tools") && j["tools"].is_array()) {
    for (size_t i = 0; i < j["tools"].size(); ++i) {
      const auto& tool_json = j["tools"][i];
      ToolSpec tool_spec;

      if (!tool_json.contains("name") || !tool_json["name"].is_string()) {
        result.errors.push_back({"tools[" + std::to_string(i) + "].name", "required", "", ""});
        return result;
      }
      tool_spec.name = tool_json["name"].get<std::string>();

      if (tool_json.contains("description") && tool_json["description"].is_string()) {
        tool_spec.description = tool_json["description"].get<std::string>();
      }

      if (!tool_json.contains("input_schema") || !tool_json["input_schema"].is_string()) {
        result.errors.push_back({"tools[" + std::to_string(i) + "].input_schema", "required", "", ""});
        return result;
      }
      tool_spec.input_schema = tool_json["input_schema"].get<std::string>();

      if (tool_json.contains("output_schema") && tool_json["output_schema"].is_string()) {
        tool_spec.output_schema = tool_json["output_schema"].get<std::string>();
      }

      if (tool_json.contains("approval_policy") && tool_json["approval_policy"].is_string()) {
        std::string policy = tool_json["approval_policy"].get<std::string>();
        if (!is_valid_approval_policy(policy)) {
          result.errors.push_back({"tools[" + std::to_string(i) + "].approval_policy", "invalid_enum", policy, "always|plan|agent|yolo"});
          return result;
        }
        tool_spec.approval_policy = policy;
      } else {
        tool_spec.approval_policy = "plan";  // default per spec
      }

      manifest.tools.push_back(tool_spec);
    }
  }

  // All validations passed
  result.valid = true;
  result.manifest = manifest;
  return result;
}

}  // namespace agenticdsl::pdk