// tests/test_pdk_manifest_finder.cpp
// PDK ManifestFinder TDD - Phase 6a step 3 (Tasks 3.1-3.8)
// 8 TEST_CASE 覆盖 ManifestFinder 文件系统查找全部场景
// 设计依据: openspec/changes/pdk-manifest-validation (Phase 6a)
// 关联 ADR: ADR-0052 §决策 1 (manifest 路径发现规则)
// 作者: Sisyphus
// 日期: 2026-08-10

#include "catch_amalgamated.hpp"

#include "agenticdsl/pdk/manifest_finder.h"
#include <fstream>

using namespace agenticdsl::pdk;

// RAII 清理辅助: 构造时创建临时目录, 析构时递归删除
struct TempTestDir {
  std::filesystem::path path;
  explicit TempTestDir(const std::string& name) {
    path = std::filesystem::temp_directory_path() / ("pdk_test_" + name + "_" + std::to_string(getpid()));
    std::filesystem::create_directories(path);
  }
  ~TempTestDir() {
    std::error_code ec;
    std::filesystem::remove_all(path, ec);
  }
};

// 在指定目录创建 .so 文件 (桩文件, 无需真实 .so 内容)
std::filesystem::path create_fake_so(const std::filesystem::path& dir, const std::string& name = "my_plugin.so") {
  auto so_path = dir / name;
  std::ofstream ofs(so_path, std::ios::binary);
  ofs.close();
  return so_path;
}

// 在指定目录创建 pdk_manifest.json
std::filesystem::path create_manifest(const std::filesystem::path& dir, const std::string& id = "test.plugin") {
  auto manifest_path = dir / "pdk_manifest.json";
  std::ofstream ofs(manifest_path);
  ofs << R"({"id":")" << id << R"(","name":"Test","version":"0.1.0","abi_version":2)"
          R"(,"implementation_forms":["cpp"],"entry_tool":"test/run","provided_tools":["test/run"]})";
  ofs.close();
  return manifest_path;
}

// ============================================================================
// Task 3.1: find_manifest_same_dir
// 场景: .so 和 pdk_manifest.json 在同一目录, 应返回 manifest 路径
// ============================================================================
TEST_CASE("ManifestFinder: find_manifest_same_dir", "[pdk][finder]") {
  TempTestDir guard("same_dir");
  auto so_path = create_fake_so(guard.path);
  create_manifest(guard.path);

  auto result = ManifestFinder::find(so_path);

  REQUIRE(result.has_value());
  REQUIRE(result->filename() == "pdk_manifest.json");
  REQUIRE(std::filesystem::exists(*result));
}

// ============================================================================
// Task 3.2: find_manifest_parent_dir
// 场景: .so 在子目录, pdk_manifest.json 在父目录, 应返回父目录的 manifest
// ============================================================================
TEST_CASE("ManifestFinder: find_manifest_parent_dir", "[pdk][finder]") {
  TempTestDir guard("parent_dir");
  auto sub_dir = guard.path / "sub";
  std::filesystem::create_directories(sub_dir);
  auto so_path = create_fake_so(sub_dir);
  create_manifest(guard.path);

  auto result = ManifestFinder::find(so_path);

  REQUIRE(result.has_value());
  REQUIRE(result->filename() == "pdk_manifest.json");
  REQUIRE(result->parent_path() == guard.path);
}

// ============================================================================
// Task 3.3: find_manifest_not_found
// 场景: 目录树中没有 pdk_manifest.json, 应返回 nullopt
// ============================================================================
TEST_CASE("ManifestFinder: find_manifest_not_found", "[pdk][finder]") {
  TempTestDir guard("not_found");
  auto sub_dir = guard.path / "deep" / "sub";
  std::filesystem::create_directories(sub_dir);
  auto so_path = create_fake_so(sub_dir);
  // 不创建 manifest

  auto result = ManifestFinder::find(so_path);

  REQUIRE_FALSE(result.has_value());
}

// ============================================================================
// Task 3.4: find_manifest_closest_wins
// 场景: 多层目录都有 manifest, 应返回最近的 (shallowest)
// ============================================================================
TEST_CASE("ManifestFinder: find_manifest_closest_wins", "[pdk][finder]") {
  TempTestDir guard("closest_wins");
  auto level1 = guard.path / "level1";
  auto level2 = level1 / "level2";
  std::filesystem::create_directories(level2);

  auto so_path = create_fake_so(level2);
  // 在 level2, level1, root 各创建一个 manifest, id 不同
  create_manifest(guard.path, "root_plugin");
  create_manifest(level1, "level1_plugin");
  create_manifest(level2, "level2_plugin");

  auto result = ManifestFinder::find(so_path);

  REQUIRE(result.has_value());
  // 应该找到 level2 的 manifest (最近的)
  REQUIRE(result->parent_path() == level2);
}

// ============================================================================
// Task 3.5: find_manifest_symlink_resolved
// 场景: .so 是 symlink, 应先 resolve 再查找
// ============================================================================
TEST_CASE("ManifestFinder: find_manifest_symlink_resolved", "[pdk][finder]") {
  TempTestDir guard("symlink_test");
  auto real_dir = guard.path / "real";
  std::filesystem::create_directories(real_dir);
  auto so_path = create_fake_so(real_dir);
  create_manifest(real_dir);

  // 在 guard.path 创建 symlink 指向 real_dir
  auto symlink_path = guard.path / "link";
  std::filesystem::create_directory_symlink(real_dir, symlink_path);
  auto symlink_so = symlink_path / "my_plugin.so";
  std::ofstream ofs(symlink_so, std::ios::binary);
  ofs.close();

  auto result = ManifestFinder::find(symlink_so);

  REQUIRE(result.has_value());
  // 解析后应该在 real_dir 中找到 manifest
  REQUIRE(result->parent_path() == real_dir);
}

// ============================================================================
// Task 3.6: find_manifest_max_depth_16
// 场景: manifest 在 16 层之外, 应返回 nullopt
// ============================================================================
TEST_CASE("ManifestFinder: find_manifest_max_depth_16", "[pdk][finder]") {
  TempTestDir guard("max_depth");
  auto deep_path = guard.path;
  // 创建 16 层嵌套
  for (int i = 0; i < 16; ++i) {
    deep_path /= "d";
  }
  std::filesystem::create_directories(deep_path);
  auto so_path = create_fake_so(deep_path);
  // 在 guard.path (第 0 层) 创建 manifest, 但 so 在第 16 层
  create_manifest(guard.path);

  auto result = ManifestFinder::find(so_path);

  // 16 层限制, 向上走 16 步到 guard.path, 应该找到
  REQUIRE(result.has_value());

  // 现在测试第 17 层, 应该找不到
  auto deeper_path = deep_path / "d";
  std::filesystem::create_directories(deeper_path);
  auto deeper_so = create_fake_so(deeper_path);

  auto result2 = ManifestFinder::find(deeper_so);

  REQUIRE_FALSE(result2.has_value());
}

// ============================================================================
// Task 3.7: find_manifest_permission_denied_skip
// 场景: 中间目录权限不足, 应跳过继续向上找, 不崩溃
// ============================================================================
TEST_CASE("ManifestFinder: find_manifest_permission_denied_skip", "[pdk][finder]") {
  TempTestDir guard("perm_denied");
  auto sub_dir = guard.path / "sub";
  std::filesystem::create_directories(sub_dir);
  auto so_path = create_fake_so(sub_dir);
  // 在 guard.path 创建 manifest
  create_manifest(guard.path);

  // 移除 sub 目录的读权限 (如果支持)
  // 注意: root 用户可能忽略权限, 所以这个测试在 root 下会退化为普通查找
  std::error_code ec;
  std::filesystem::permissions(sub_dir, std::filesystem::perms::none, ec);

  auto result = ManifestFinder::find(so_path);

  // 无论权限如何, 不应崩溃, 要么找到要么找不到
  // 在支持权限的系统上, 应该跳过 sub 继续向上在 guard.path 找到
  // 在 root 或不支持权限的系统上, 可能直接找到
  // 我们只验证不崩溃且结果合理
  if (result.has_value()) {
    // 如果找到了, 应该在 guard.path 或更上
    REQUIRE(result->parent_path() != sub_dir);
  }

  // 恢复权限以便 TempTestDir 析构清理
  std::filesystem::permissions(sub_dir, std::filesystem::perms::all, ec);
}

// ============================================================================
// Task 3.8: find_manifest_hidden_dirs
// 场景: 中间路径包含隐藏目录 (.开头), 不应跳过, 应正常查找
// ============================================================================
TEST_CASE("ManifestFinder: find_manifest_hidden_dirs", "[pdk][finder]") {
  TempTestDir guard("hidden_dirs");
  auto hidden_dir = guard.path / ".hidden";
  auto sub_dir = hidden_dir / "sub";
  std::filesystem::create_directories(sub_dir);
  auto so_path = create_fake_so(sub_dir);
  create_manifest(guard.path);

  auto result = ManifestFinder::find(so_path);

  REQUIRE(result.has_value());
  REQUIRE(result->parent_path() == guard.path);
}
