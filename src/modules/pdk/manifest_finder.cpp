// src/modules/pdk/manifest_finder.cpp
// PDK Manifest Finder 桩实现 (Task 1.4 stub)
// 真实逻辑由 Task 3.x TDD 循环逐步填充
// 设计依据: openspec/changes/pdk-manifest-validation (Phase 6a)

#include "agenticdsl/pdk/manifest_finder.h"

namespace agenticdsl::pdk {

std::optional<std::filesystem::path> ManifestFinder::find(const std::filesystem::path& /*so_path*/) {
  // 桩实现: 总是返回 nullopt, 等待 Task 3.x TDD 循环实现
  return std::nullopt;
}

}  // namespace agenticdsl::pdk
