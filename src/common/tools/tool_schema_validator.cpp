// src/common/tools/tool_schema_validator.cpp
// 功能描述：ADR-0073 D3 — ToolSchemaValidator 实现。
//          JSON Schema 2020-12 最小子集校验器 (type/properties/required/items/enum),
//          自包含实现 (vendor nlohmann/json 无 json-schema.hpp, 不引入新依赖)。
// 作者：from-roadmap-phase-6c-schema-complete change (Batch 1 Task 1)
// 最后修改日期：2026-08-18
#include "agenticdsl/tools/tool_schema_validator.h"

#include <stdexcept>

namespace agenticdsl::tools {

namespace {

// json::json_pointer → 字符串 (顶层为 "")
std::string ptr_to_string(const json::json_pointer& ptr) {
  return ptr.to_string();
}

// 类型匹配: schema "type" 值 vs json 实例类型
bool type_matches(const std::string& expected, const json& instance) {
  if (expected == "object") return instance.is_object();
  if (expected == "array") return instance.is_array();
  if (expected == "string") return instance.is_string();
  if (expected == "integer") return instance.is_number_integer() || instance.is_number_unsigned();
  if (expected == "number") return instance.is_number();
  if (expected == "boolean") return instance.is_boolean();
  if (expected == "null") return instance.is_null();
  return false;  // 未知 type 名称 → 视为不匹配
}

std::string instance_type_name(const json& instance) {
  if (instance.is_object()) return "object";
  if (instance.is_array()) return "array";
  if (instance.is_string()) return "string";
  if (instance.is_number_integer() || instance.is_number_unsigned()) return "integer";
  if (instance.is_number()) return "number";
  if (instance.is_boolean()) return "boolean";
  return "null";
}

void push_error(std::vector<json>& errors, const json::json_pointer& ptr,
                const std::string& message) {
  json err;
  err["path"] = ptr_to_string(ptr);
  err["message"] = message;
  errors.push_back(std::move(err));
}

// 递归校验: instance 是否满足 schema。错误路径以 base_ptr 为前缀。
void validate_node(const json& schema, const json& instance,
                   const json::json_pointer& base_ptr,
                   std::vector<json>& errors) {
  if (!schema.is_object()) return;  // 非对象 schema (true/false) → 跳过

  // "type": 字符串或字符串数组
  if (schema.contains("type")) {
    const auto& t = schema["type"];
    bool match = false;
    if (t.is_string()) {
      match = type_matches(t.get<std::string>(), instance);
    } else if (t.is_array()) {
      for (const auto& item : t) {
        if (item.is_string() && type_matches(item.get<std::string>(), instance)) {
          match = true;
          break;
        }
      }
    }
    if (!match) {
      push_error(errors, base_ptr,
                 "type mismatch: expected " + t.dump() + ", got " + instance_type_name(instance));
      return;  // 类型不符时不再深入子结构
    }
  }

  // "enum": 值枚举
  if (schema.contains("enum") && schema["enum"].is_array()) {
    bool in_enum = false;
    for (const auto& v : schema["enum"]) {
      if (v == instance) {
        in_enum = true;
        break;
      }
    }
    if (!in_enum) {
      push_error(errors, base_ptr, "value not in enum");
    }
  }

  // object: "required" + "properties"
  if (instance.is_object()) {
    if (schema.contains("required") && schema["required"].is_array()) {
      for (const auto& field : schema["required"]) {
        if (field.is_string() && !instance.contains(field.get<std::string>())) {
          push_error(errors, base_ptr / field.get<std::string>(), "required field missing");
        }
      }
    }
    if (schema.contains("properties") && schema["properties"].is_object()) {
      for (const auto& [key, subschema] : schema["properties"].items()) {
        if (instance.contains(key)) {
          validate_node(subschema, instance[key], base_ptr / key, errors);
        }
      }
    }
  }

  // array: "items"
  if (instance.is_array() && schema.contains("items")) {
    for (size_t i = 0; i < instance.size(); ++i) {
      validate_node(schema["items"], instance[i], base_ptr / i, errors);
    }
  }
}

}  // namespace

ToolSchemaValidator::ToolSchemaValidator(const std::string& schema_json) {
  schema_ = json::parse(schema_json);  // 解析失败抛 json::parse_error
}

ValidationResult ToolSchemaValidator::validate(const json& input_args) const {
  ValidationResult result;
  std::vector<json> errors;
  validate_node(schema_, input_args, json::json_pointer{}, errors);
  result.ok = errors.empty();
  result.errors = std::move(errors);
  return result;
}

}  // namespace agenticdsl::tools
