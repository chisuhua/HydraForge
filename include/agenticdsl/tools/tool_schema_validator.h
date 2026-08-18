// include/agenticdsl/tools/tool_schema_validator.h
// 功能描述：ADR-0073 D3 — JSON Schema 2020-12 校验包装层。
//          复用 ToolMetadata::input_schema (single source of truth),
//          不重新生成 schema。
// 实现说明：原规划使用 nlohmann json_schema::json_validator
//          (pboettch/json-schema-validator), 但项目 vendor 的
//          external/nlohmann_json (nlohmann/json v3.11.2+) 并不包含
//          json-schema.hpp, 且本 change 禁止引入新外部依赖。
//          因此 .cpp 内实现 JSON Schema 2020-12 最小子集
//          (type/properties/required/items/enum), 保持相同公开 API。
// 作者：from-roadmap-phase-6c-schema-complete change (Batch 1 Task 1)
// 最后修改日期：2026-08-18
#pragma once

#include <nlohmann/json.hpp>
#include <string>
#include <vector>

namespace agenticdsl::tools {

using nlohmann::json;

// 校验结果: ok=false 时 errors 携带结构化字段路径
// 每个 error = {{"path": "/foo/bar"}, {"message": "..."}}
struct ValidationResult {
  bool ok = true;
  std::vector<json> errors;
};

class ToolSchemaValidator {
 public:
  // schema_json: JSON Schema 2020-12 字符串 (来自 ToolMetadata::input_schema)
  explicit ToolSchemaValidator(const std::string& schema_json);

  // 校验输入参数。返回 ValidationResult。
  ValidationResult validate(const json& input_args) const;

 private:
  json schema_;
};

}  // namespace agenticdsl::tools
