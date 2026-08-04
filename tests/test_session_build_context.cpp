// tests/test_session_build_context.cpp
// 功能描述：SessionManager::build_context_entries 单元测试 (Task 5)
// 测试范围：叶到根 parent 链 walk + branch 隔离 + 循环 parent 防御
// 设计依据：OpenSpec change session-manager-jsonl §3 + Task 5 plan
//         + Decision 3 (build_context_entries leaf-to-root via parent_id)
// 作者：AgenticDSL Phase 5 / Session Manager JSONL Sprint
// 最后修改日期：2026-08-05

#include "catch_amalgamated.hpp"

#include <atomic>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <set>
#include <sstream>
#include <string>
#include <thread>
#include <unordered_set>
#include <vector>

#include "core/session_manager.h"
#include "nlohmann/json.hpp"

namespace fs = std::filesystem;

namespace {

// 生成一个测试用唯一临时目录 (基于线程 id + 原子计数 + 时间戳)
// 避免并行测试间目录冲突，也避免依赖 std::tmpnam (POSIX 已弃用)
fs::path make_unique_temp_dir(const std::string& tag) {
  static std::atomic<uint64_t> counter{0};
  const auto n = counter.fetch_add(1);
  const auto pid = static_cast<uint64_t>(::getpid());
  const auto ts = static_cast<uint64_t>(
      std::chrono::steady_clock::now().time_since_epoch().count());
  std::ostringstream oss;
  oss << "session_build_context_test_" << tag << "_" << pid << "_" << ts << "_" << n;
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

}  // namespace

// ==================== Task 5: build_context_entries ====================

TEST_CASE("build_context_entries returns root-first chain",
          "[session_manager][task5][context][chain]") {
  TempDirGuard tmp("task5_chain");

  agenticdsl::SessionManager mgr(tmp.path);
  mgr.open("s_chain");

  // 通过 append_to_branch 构建链 root → n1 → n2 → n3
  // (append_to_branch 基于 current_branch_, 第一个调用因 branches_["main"].forked_from_node == ""
  //  会创建 parent_id 为空的根节点)
  const auto root_id = mgr.append_to_branch("root msg");
  const auto n1_id = mgr.append_to_branch("n1 msg");
  const auto n2_id = mgr.append_to_branch("n2 msg");
  const auto n3_id = mgr.append_to_branch("n3 msg");

  REQUIRE_FALSE(root_id.empty());
  REQUIRE_FALSE(n1_id.empty());
  REQUIRE_FALSE(n2_id.empty());
  REQUIRE_FALSE(n3_id.empty());

  // 重新加载索引 (flush_append 已填, 这里显式 load 防御性确保索引同步)
  mgr.load_jsonl();

  // build_context_entries 从叶子 (n3) 沿 parent_id 链回到根, root-first 顺序
  const auto ctx = mgr.build_context_entries(n3_id);
  REQUIRE(ctx.size() == 4);
  REQUIRE(ctx[0].id == root_id);
  REQUIRE(ctx[1].id == n1_id);
  REQUIRE(ctx[2].id == n2_id);
  REQUIRE(ctx[3].id == n3_id);
}

TEST_CASE("build_context_entries respects branch isolation",
          "[session_manager][task5][context][isolation]") {
  TempDirGuard tmp("task5_isolation");

  agenticdsl::SessionManager mgr(tmp.path);
  mgr.open("s_isolation");

  agenticdsl::SessionNode root;
  root.id = mgr.next_node_id();
  root.parent_id = "";
  root.branch_id = "main";
  root.content = {{"role", "user"}, {"text", "root"}};
  mgr.flush_append(root);

  agenticdsl::SessionNode n1;
  n1.id = mgr.next_node_id();
  n1.parent_id = root.id;
  n1.branch_id = "main";
  n1.content = {{"role", "user"}, {"text", "n1"}};
  mgr.flush_append(n1);

  const auto explore_branch = mgr.fork(n1.id, "explore");
  REQUIRE_FALSE(explore_branch.empty());
  REQUIRE(mgr.current_branch() == explore_branch);

  agenticdsl::SessionNode n2;
  n2.id = mgr.next_node_id();
  n2.parent_id = n1.id;
  n2.branch_id = explore_branch;
  n2.content = {{"role", "user"}, {"text", "n2 in explore"}};
  mgr.flush_append(n2);

  mgr.switch_branch("main");
  REQUIRE(mgr.current_branch() == "main");

  agenticdsl::SessionNode n3;
  n3.id = mgr.next_node_id();
  n3.parent_id = n1.id;
  n3.branch_id = "main";
  n3.content = {{"role", "user"}, {"text", "n3 in main"}};
  mgr.flush_append(n3);

  mgr.load_jsonl();

  const auto explore_leaf = mgr.get_branch_leaf(explore_branch);
  REQUIRE(explore_leaf == n2.id);

  const auto ctx_explore = mgr.build_context_entries(explore_leaf);
  REQUIRE(ctx_explore.size() == 3);
  REQUIRE(ctx_explore[0].id == root.id);
  REQUIRE(ctx_explore[1].id == n1.id);
  REQUIRE(ctx_explore[2].id == n2.id);

  const auto main_leaf = mgr.get_branch_leaf("main");
  REQUIRE(main_leaf == n3.id);

  const auto ctx_main = mgr.build_context_entries(main_leaf);
  REQUIRE(ctx_main.size() == 3);
  REQUIRE(ctx_main[0].id == root.id);
  REQUIRE(ctx_main[1].id == n1.id);
  REQUIRE(ctx_main[2].id == n3.id);

  for (const auto& n : ctx_explore) {
    REQUIRE(n.id != n3.id);
  }
  for (const auto& n : ctx_main) {
    REQUIRE(n.id != n2.id);
  }
}

TEST_CASE("build_context_entries handles cyclic parent_id without infinite loop",
          "[session_manager][task5][context][cycle]") {
  TempDirGuard tmp("task5_cycle");

  agenticdsl::SessionManager mgr(tmp.path);
  mgr.open("s_cycle");

  agenticdsl::SessionNode root;
  root.id = mgr.next_node_id();
  root.parent_id = "";
  root.branch_id = "main";
  root.content = nlohmann::json{{"role", "user"}, {"text", "seed"}};
  mgr.flush_append(root);

  agenticdsl::SessionNode n1;
  n1.id = mgr.next_node_id();
  n1.parent_id = root.id;
  n1.branch_id = "main";
  n1.content = nlohmann::json{{"role", "user"}, {"text", "n1"}};
  mgr.flush_append(n1);

  // 手动注入循环: 直接向 JSONL 追加自循环节点, 模拟 corrupt 文件场景
  const auto jsonl_path = tmp.path / "s_cycle.jsonl";
  std::ofstream out(jsonl_path, std::ios::app);
  REQUIRE(out.is_open());

  agenticdsl::SessionNode corrupt;
  corrupt.id = n1.id;  // 复用 n1 的 id
  corrupt.parent_id = n1.id;  // 自循环
  corrupt.branch_id = "main";
  corrupt.content = nlohmann::json{{"corrupt", true}};
  out << corrupt.to_json().dump() << "\n";
  out.close();

  mgr.load_jsonl();

  // 死循环检测: 用 std::thread 包装 build_context_entries, 超时则视为失败
  std::atomic<bool> finished{false};
  std::vector<agenticdsl::SessionNode> ctx;
  std::thread t([&]() {
    ctx = mgr.build_context_entries(n1.id);
    finished.store(true);
  });

  const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
  while (!finished.load() && std::chrono::steady_clock::now() < deadline) {
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }

  REQUIRE(finished.load());
  if (t.joinable()) t.join();

  REQUIRE(ctx.size() <= 10);
  REQUIRE(ctx.size() >= 2);
  REQUIRE(ctx[0].id == root.id);
  REQUIRE(ctx[1].id == n1.id);
}

TEST_CASE("build_context_entries returns empty for unknown leaf",
          "[session_manager][task5][context][unknown]") {
  TempDirGuard tmp("task5_unknown");
  agenticdsl::SessionManager mgr(tmp.path);
  mgr.open("s_unknown");
  mgr.append_to_branch("seed");

  mgr.load_jsonl();

  // 未知 leaf_id 返回空 (不抛异常 — build_context_entries 是只读 walk)
  const auto ctx = mgr.build_context_entries("nonexistent_node_id");
  REQUIRE(ctx.empty());
}

TEST_CASE("build_context_entries on root returns single-node chain",
          "[session_manager][task5][context][root]") {
  TempDirGuard tmp("task5_root");
  agenticdsl::SessionManager mgr(tmp.path);
  mgr.open("s_root_only");

  const auto root_id = mgr.append_to_branch("only message");
  REQUIRE_FALSE(root_id.empty());

  mgr.load_jsonl();

  // 根节点的 parent_id 为空, walk 终止 — 只返回根本身
  const auto ctx = mgr.build_context_entries(root_id);
  REQUIRE(ctx.size() == 1);
  REQUIRE(ctx[0].id == root_id);
  REQUIRE(ctx[0].parent_id.empty());
}
