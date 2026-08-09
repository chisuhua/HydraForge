#pragma once
// PDK Manifest Validator - Phase 6a step 1
// 校验 pdk_manifest.json 字符串, 返回 ManifestValidationResult
// 设计依据: openspec/changes/pdk-manifest-validation
// 关联 ADR: ADR-0052 §决策 1-3, ADR-0022 §3.2 (dual-ABI)

#include "agenticdsl/pdk/manifest.h"
#include <string>

namespace agenticdsl::pdk {

class ManifestValidator {
 public:
  // 静态方法, 接受 JSON 字符串, 返回校验结果
  // 调用方负责检查 result.valid + result.errors
  // 校验失败时 result.manifest 为 std::nullopt
  static ManifestValidationResult validate(const std::string& json_content);
};

}  // namespace agenticdsl::pdk
