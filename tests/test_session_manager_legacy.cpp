// tests/test_session_manager_legacy.cpp
// 功能描述：SessionManager legacy JSON → JSONL 迁移单元测试 (Task 6)
// 测试范围：migrate_legacy_json 读取线性 JSON + .backup 保留 + chain parent +
//          BranchMeta "main" 注入 + build_context_entries 等价性
// 设计依据：OpenSpec change session-manager-jsonl §4 (旧格式迁移工具) +
//          design.md Decision 4 (legacy JSON → JSONL with .backup)
// 作者：AgenticDSL Phase 5 / Session Manager JSONL Sprint
// 最后修改日期：2026-08-05

#include "catch_amalgamated.hpp"

#include <atomic>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include "core/session_manager.h"
#include "nlohmann/json.hpp"

namespace fs = std::filesystem;

namespace {

// 生成一个测试用唯一临时目录 (基于线程 id + 原子计数 + 时间戳)
// 避免并行测试间目录冲突
fs::path make_unique_temp_dir(const std::string& tag) {
  static std::atomic<uint64_t> counter{0};
  const auto n = counter.fetch_add(1);
  const auto pid = static_cast<uint64_t>(::getpid());
  const auto ts = static_cast<uint64_t>(
      std::chrono::steady_clock::now().time_since_epoch().count());
  std::ostringstream oss;
  oss << "session_legacy_test_" << tag << "_" << pid << "_" << ts << "_" << n;
  auto dir = fs::temp_directory_path() / oss.str();
  fs::create_directories(dir);
  return dir;
}

// RAII 临时目录清理器
struct TempDirGuard {
  fs::path path;
  explicit TempDirGuard(const std::string& tag) : path(make_unique_temp_dir(tag)) {}
  ~TempDirGuard() {
    std::error_code ec;
    fs::remove_all(path, ec);
  }
};

// Helper: 将 legacy JSON 写入指定路径
void write_legacy_json(const fs::path& path, const nlohmann::json& j) {
  std::ofstream out(path);
  if (!out.is_open()) {
    throw std::runtime_error("cannot open " + path.string());
  }
  out << j.dump(2) << "\n";
}

}  // namespace

// ----------------------------------------------------------------------------
// TEST_CASE 1: empty legacy → JSONL with main branch, no SessionNode
// ----------------------------------------------------------------------------
TEST_CASE("migrate empty legacy creates root node + main branch",
          "[session_manager][legacy]") {
  TempDirGuard legacy_dir("legacy_dir");
  TempDirGuard jsonl_dir("jsonl_dir");

  const auto legacy_path = legacy_dir.path / "old_session.json";
  write_legacy_json(legacy_path, nlohmann::json{{"messages", nlohmann::json::array()}});

  agenticdsl::SessionManager mgr(jsonl_dir.path);
  const std::string migrated_sid = mgr.migrate_legacy_json(legacy_path);

  // session_id 来自 legacy_path.stem()
  REQUIRE(migrated_sid == "old_session");

  // 目标 JSONL 存在
  const auto jsonl_path = jsonl_dir.path / "old_session.jsonl";
  REQUIRE(fs::exists(jsonl_path));

  // load_jsonl 后应能完整读回:主分支无 SessionNode, 仅 BranchMeta("main") 一行
  const auto nodes = mgr.load_jsonl();
  REQUIRE(nodes.empty());

  // branches_ 包含 "main"
  const auto branches = mgr.list_branches();
  bool has_main = false;
  for (const auto& bm : branches) {
    if (bm.branch_id == "main") has_main = true;
  }
  REQUIRE(has_main);

  // root_node() 返回 "" (无 SessionNode 有 parent_id == "")
  REQUIRE(mgr.get_root_node().empty());

  // build_context_entries("") 返回空 vector
  const auto empty_context = mgr.build_context_entries("");
  REQUIRE(empty_context.empty());
}

// ----------------------------------------------------------------------------
// TEST_CASE 2: single message legacy → JSONL chain, parent_id empty
// ----------------------------------------------------------------------------
TEST_CASE("migrate single message legacy to JSONL chain",
          "[session_manager][legacy]") {
  TempDirGuard legacy_dir("single_msg_legacy");
  TempDirGuard jsonl_dir("single_msg_jsonl");

  const auto legacy_path = legacy_dir.path / "single.json";
  nlohmann::json legacy;
  legacy["messages"] = nlohmann::json::array();
  legacy["messages"].push_back({{"role", "user"}, {"text", "hello"}});
  write_legacy_json(legacy_path, legacy);

  agenticdsl::SessionManager mgr(jsonl_dir.path);
  const std::string migrated_sid = mgr.migrate_legacy_json(legacy_path);

  REQUIRE(migrated_sid == "single");

  const auto jsonl_path = jsonl_dir.path / "single.jsonl";
  REQUIRE(fs::exists(jsonl_path));

  // 加载 JSONL — 应有 1 SessionNode
  const auto nodes = mgr.load_jsonl();
  REQUIRE(nodes.size() == 1);

  const auto& node = nodes[0];
  // single message → parent_id 必须为 "" (根)
  REQUIRE(node.parent_id.empty());
  REQUIRE(node.branch_id == "main");
  // content 应保留原 message
  REQUIRE(node.content.contains("role"));
  REQUIRE(node.content["role"] == "user");
  REQUIRE(node.content["text"] == "hello");

  // 同一会话二次 open 应能读到内容 (跨进程持久化正确)
  agenticdsl::SessionManager mgr2(jsonl_dir.path);
  mgr2.open("single");
  const auto nodes2 = mgr2.load_jsonl();
  REQUIRE(nodes2.size() == 1);
  REQUIRE(nodes2[0].content["text"] == "hello");
}

// ----------------------------------------------------------------------------
// TEST_CASE 3: multi-message legacy preserves order via build_context
// ----------------------------------------------------------------------------
TEST_CASE("migrate multi-message preserves order via build_context",
          "[session_manager][legacy]") {
  TempDirGuard legacy_dir("multi_msg_legacy");
  TempDirGuard jsonl_dir("multi_msg_jsonl");

  const auto legacy_path = legacy_dir.path / "chat.json";
  nlohmann::json legacy;
  legacy["messages"] = nlohmann::json::array();
  legacy["messages"].push_back({{"i", 0}, {"text", "first"}});
  legacy["messages"].push_back({{"i", 1}, {"text", "second"}});
  legacy["messages"].push_back({{"i", 2}, {"text", "third"}});
  write_legacy_json(legacy_path, legacy);

  agenticdsl::SessionManager mgr(jsonl_dir.path);
  const std::string migrated_sid = mgr.migrate_legacy_json(legacy_path);

  REQUIRE(migrated_sid == "chat");

  const auto nodes = mgr.load_jsonl();
  REQUIRE(nodes.size() == 3);

  // 链结构: n1.parent_id="" (root) → n2.parent_id=n1.id → n3.parent_id=n2.id
  REQUIRE(nodes[0].parent_id.empty());
  REQUIRE(nodes[1].parent_id == nodes[0].id);
  REQUIRE(nodes[2].parent_id == nodes[1].id);

  // 全部 "main" 分支
  for (const auto& n : nodes) REQUIRE(n.branch_id == "main");

  // build_context_entries(leaves) → 顺序 chain
  const auto ctx = mgr.build_context_entries(nodes[2].id);
  REQUIRE(ctx.size() == 3);
  REQUIRE(ctx[0].content["text"] == "first");
  REQUIRE(ctx[1].content["text"] == "second");
  REQUIRE(ctx[2].content["text"] == "third");

  // 等价性: build_context_entries 后顺序应与原 legacy["messages"] 一致
  for (size_t i = 0; i < ctx.size(); ++i) {
    REQUIRE(ctx[i].content["i"] == legacy["messages"][i]["i"]);
    REQUIRE(ctx[i].content["text"] == legacy["messages"][i]["text"]);
  }
}

// ----------------------------------------------------------------------------
// TEST_CASE 4: migration creates .backup file
// ----------------------------------------------------------------------------
TEST_CASE("migrate creates .backup file",
          "[session_manager][legacy]") {
  TempDirGuard legacy_dir("backup_legacy");
  TempDirGuard jsonl_dir("backup_jsonl");

  const auto legacy_path = legacy_dir.path / "data.json";
  nlohmann::json legacy;
  legacy["messages"] = nlohmann::json::array();
  legacy["messages"].push_back({{"x", "abc"}});
  write_legacy_json(legacy_path, legacy);

  REQUIRE(fs::exists(legacy_path));
  REQUIRE_FALSE(fs::exists(legacy_path.string() + ".backup"));

  agenticdsl::SessionManager mgr(jsonl_dir.path);
  mgr.migrate_legacy_json(legacy_path);

  // .backup 必须存在
  const auto backup_path = legacy_path.string() + ".backup";
  REQUIRE(fs::exists(backup_path));

  // .backup 内容应 == 原 legacy (字节级)
  std::ifstream in_orig(legacy_path);
  std::ifstream in_backup(backup_path);
  std::stringstream orig_ss, backup_ss;
  orig_ss << in_orig.rdbuf();
  backup_ss << in_backup.rdbuf();
  REQUIRE(orig_ss.str() == backup_ss.str());

  // overwrite_existing 语义: 改 legacy 后再次迁移 → backup 被新内容覆盖
  nlohmann::json legacy2;
  legacy2["messages"] = nlohmann::json::array();
  legacy2["messages"].push_back({{"x", "def"}});
  write_legacy_json(legacy_path, legacy2);

  const auto jsonl_dir2_str = jsonl_dir.path.string() + "_v2";
  fs::create_directories(jsonl_dir2_str);
  agenticdsl::SessionManager mgr2{fs::path(jsonl_dir2_str)};
  mgr2.migrate_legacy_json(legacy_path);

  std::ifstream in_backup2(backup_path);
  std::stringstream backup2_ss;
  backup2_ss << in_backup2.rdbuf();
  REQUIRE(backup2_ss.str() != orig_ss.str());
  REQUIRE(backup2_ss.str().find("def") != std::string::npos);
}
