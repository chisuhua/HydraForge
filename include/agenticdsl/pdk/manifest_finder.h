#pragma once
// PDK Manifest Finder - Phase 6a step 1
// 从 .so 路径向上查找 pdk_manifest.json
// 设计依据: openspec/changes/pdk-manifest-validation
// 关联 ADR: ADR-0052 §决策 1

#include <filesystem>
#include <optional>

namespace agenticdsl::pdk {

class ManifestFinder {
 public:
  // 静态方法, 接受 .so 路径, 返回 manifest 绝对路径 (找不到时 std::nullopt)
  // 算法: weakly_canonical(.so) -> 向上 walk max 16 层 -> 找 pdk_manifest.json
  // 安全: symlink 解析 + 权限拒绝降级 + hidden 目录不跳
  static std::optional<std::filesystem::path> find(const std::filesystem::path& so_path);
};

}  // namespace agenticdsl::pdk
