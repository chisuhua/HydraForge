// src/modules/pdk/manifest_validator.cpp
// PDK Manifest Validator 桩实现 (Task 1.4 stub)
// 真实逻辑由 Task 2.x TDD 循环逐步填充
// 设计依据: openspec/changes/pdk-manifest-validation (Phase 6a)

#include "agenticdsl/pdk/manifest_validator.h"

namespace agenticdsl::pdk {

ManifestValidationResult ManifestValidator::validate(const std::string& /*json_content*/) {
  // 桩实现: 返回 invalid result, 等待 Task 2.x TDD 循环实现
  ManifestValidationResult result;
  result.valid = false;
  result.errors.push_back({"", "not_implemented", "", ""});
  return result;
}

}  // namespace agenticdsl::pdk
