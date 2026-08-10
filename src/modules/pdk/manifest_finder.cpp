// src/modules/pdk/manifest_finder.cpp
// PDK ManifestFinder - Phase 6a step 3 (Tasks 3.1-3.8)
// 真实文件系统查找逻辑实现
// 设计依据: openspec/changes/pdk-manifest-validation (Phase 6a)
// 关联 ADR: ADR-0052 §决策 1 (manifest 路径发现规则)
// 作者: Sisyphus
// 日期: 2026-08-10

#include "agenticdsl/pdk/manifest_finder.h"

namespace agenticdsl::pdk {

namespace {

// 检查指定目录是否有 pdk_manifest.json, 有则返回路径, 无则返回 nullopt
// 权限不足时返回 nullopt (不抛异常)
std::optional<std::filesystem::path> check_dir_for_manifest(
    const std::filesystem::path& dir) {
  auto manifest_path = dir / "pdk_manifest.json";
  std::error_code ec;
  if (std::filesystem::exists(manifest_path, ec) && !ec) {
    return manifest_path;
  }
  return std::nullopt;
}

// 检查目录是否可读 (可进入/枚举)
// 如果权限不足, 返回 false
bool is_dir_readable(const std::filesystem::path& dir) {
  std::error_code ec;
  auto status = std::filesystem::status(dir, ec);
  if (ec) return false;
  // 检查目录类型
  if (!std::filesystem::is_directory(status)) return false;
  // 检查读权限
  auto perms = status.permissions();
  using p = std::filesystem::perms;
  bool readable = ((perms & p::others_read) != p::none) ||
                  ((perms & p::owner_read) != p::none);
  return readable;
}

}  // namespace

std::optional<std::filesystem::path> ManifestFinder::find(
    const std::filesystem::path& so_path) {
  std::error_code ec;
  auto resolved = std::filesystem::weakly_canonical(so_path, ec);
  if (ec) {
    return std::nullopt;
  }

  auto current_dir = resolved.parent_path();

  // 向上查找, 最多 16 层 (包括 current_dir 自己)
  // depth=0: current_dir (so 所在目录)
  // depth=1: parent of current_dir
  // ...
  // depth=16: 16th ancestor
  constexpr int kMaxDepth = 16;
  int depth = 0;

  while (depth <= kMaxDepth) {
    auto result = check_dir_for_manifest(current_dir);
    if (result.has_value()) {
      return result;
    }

    auto parent = current_dir.parent_path();
    if (parent == current_dir) {
      break;
    }

    if (!is_dir_readable(parent)) {
      current_dir = parent;
      ++depth;
      continue;
    }

    current_dir = parent;
    ++depth;
  }

  return std::nullopt;
}

}  // namespace agenticdsl::pdk
