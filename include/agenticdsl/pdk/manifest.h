#pragma once
// PDK Manifest 数据结构 - Phase 6a step 1
// 完整履行 ADR-0052 §决策 1-3: 9 必填 + 8 推荐 + 1 可选字段
// 设计依据: openspec/changes/pdk-manifest-validation (Phase 6a)

#include <cstdint>
#include <string>
#include <vector>
#include <optional>

namespace agenticdsl::pdk {

// 单个 tool 声明 (per ADR-0052 推荐字段)
struct ToolSpec {
  std::string name;                          // kebab-case per ADR-0043
  std::string description;
  std::string input_schema;                  // JSON Schema 2020-12
  std::string output_schema;                 // JSON Schema 2020-12
  std::string approval_policy = "plan";      // always|plan|agent|yolo
};

// 资源声明 (per ADR-0052)
struct Resources {
  uint32_t timeout_ms = 30000;
  uint32_t max_concurrent = 1;
};

// 完整 manifest (9 必填 + 8 推荐 + 1 可选)
struct Manifest {
  // 必填 (ADR-0052 §决策 2)
  std::string id;                            // kebab-case, max 64
  std::string name;                          // human-readable, max 128
  std::string version;                       // semver
  uint32_t abi_version = 0;                 // 1 or 2 (dual-ABI per SUPPORTED_ABI_VERSIONS)
  std::string min_host_version;              // semver, soft constraint
  std::string max_host_version;              // semver, soft constraint
  std::vector<std::string> implementation_forms;  // {skill,dsl,wasm}
  std::string entry_tool;                    // MUST ∈ provided_tools
  std::vector<std::string> provided_tools;   // non-empty, kebab-case

  // 推荐 (ADR-0052 §决策 3)
  std::vector<std::string> interface_versions;
  std::vector<std::string> capabilities;
  std::string input_schema;                  // JSON Schema 2020-12 (raw JSON string)
  std::string output_schema;
  bool requires_isolation = false;
  Resources resources;
  std::string publisher;
  std::string trust_level = "untrusted";     // high|medium|low|untrusted
  std::vector<std::string> activation_events;
  std::vector<ToolSpec> tools;               // 工具详细 schema

  // 可选 (ADR-0052 §决策 7)
  std::optional<std::string> signature;      // Phase 6a 仅记录不验签
};

// 校验错误
struct ValidationError {
  std::string field;                         // 字段路径 e.g. "tools[0].input_schema"
  std::string reason;                        // required|invalid_semver|mismatch|wrong_type|invalid_enum|...
  std::string value;                         // 实际值 (optional)
  std::string expected;                      // 期望值/类型/enum (optional)
};

// 校验结果
struct ManifestValidationResult {
  bool valid = false;
  std::optional<Manifest> manifest;          // valid=true 时填充
  std::vector<ValidationError> errors;       // valid=false 时填充
};

}  // namespace agenticdsl::pdk
