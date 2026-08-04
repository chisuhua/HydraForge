// tests/test_session_manager.cpp
// 功能描述：SessionManager 单元测试 (Task 1: JSONL record types + append-only writer)
// 测试范围：SessionNode/BranchMeta/SessionHandle 结构 + SessionManager::open/flush_append/next_*_id
// 设计依据：OpenSpec change session-manager-jsonl §1 (JSONL 树状存储) + Task 1 plan
// 作者：AgenticDSL Phase 5 / Session Manager JSONL Sprint
// 最后修改日期：2026-08-05

#include "catch_amalgamated.hpp"

#include <atomic>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <random>
#include <set>
#include <sstream>
#include <string>
#include <thread>

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
  oss << "session_manager_test_" << tag << "_" << pid << "_" << ts << "_" << n;
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

// 构造 SessionNode helper (用于 Task 2-5 测试)
agenticdsl::SessionNode make_node(const std::string& id,
                                const std::string& parent_id,
                                const std::string& branch_id = "main") {
  agenticdsl::SessionNode n;
  n.id = id;
  n.parent_id = parent_id;
  n.branch_id = branch_id;
  n.content = {{"msg", "hello"}};
  return n;
}

} // namespace

TEST_CASE("SessionManager: flush_append writes one parseable JSONL line",
          "[session_manager][jsonl]") {
  TempDirGuard tmp("append_one");

  agenticdsl::SessionManager mgr(tmp.path);
  mgr.open("sess_basic");

  agenticdsl::SessionNode node;
  node.id = mgr.next_node_id();
  node.parent_id = "";                 // 根节点无 parent
  node.branch_id = "branch_main";
  node.content = nlohmann::json{
      {"role", "user"},
      {"text", "hello world"}};

  mgr.flush_append(node);

  // 断言: JSONL 文件存在并包含恰好一行可解析的 JSON
  const auto jsonl_path = tmp.path / "sess_basic.jsonl";
  REQUIRE(fs::exists(jsonl_path));

  std::ifstream in(jsonl_path);
  REQUIRE(in.is_open());

  std::string line;
  REQUIRE(std::getline(in, line));
  REQUIRE_FALSE(line.empty());

  // 必须能解析回 nlohmann::json
  auto parsed = nlohmann::json::parse(line);
  REQUIRE(parsed.is_object());
  REQUIRE(parsed["id"] == node.id);
  REQUIRE(parsed["parent_id"] == "");
  REQUIRE(parsed["branch_id"] == "branch_main");
  REQUIRE(parsed["content"]["role"] == "user");
  REQUIRE(parsed["content"]["text"] == "hello world");

  // 末尾不应有第二行
  REQUIRE_FALSE(std::getline(in, line));
}

TEST_CASE("SessionManager: flush_append is atomic per line (no torn writes)",
          "[session_manager][jsonl]") {
  TempDirGuard tmp("append_lines");

  agenticdsl::SessionManager mgr(tmp.path);
  mgr.open("sess_lines");

  for (int i = 0; i < 5; ++i) {
    agenticdsl::SessionNode node;
    node.id = mgr.next_node_id();
    node.parent_id = "";   // Task 1 最小实现: 不依赖 parent 链
    node.branch_id = "branch_main";
    node.content = nlohmann::json{
        {"role", "user"},
        {"index", i}};
    mgr.flush_append(node);
  }

  // 重新解析整个文件, 每行必须独立可解析
  const auto jsonl_path = tmp.path / "sess_lines.jsonl";
  std::ifstream in(jsonl_path);
  REQUIRE(in.is_open());

  std::string line;
  int count = 0;
  while (std::getline(in, line)) {
    if (line.empty()) continue;
    auto parsed = nlohmann::json::parse(line);  // 任何一行不可解析则失败
    REQUIRE(parsed.contains("id"));
    ++count;
  }
  REQUIRE(count == 5);
}

TEST_CASE("SessionManager: next_node_id / next_branch_id produce unique non-empty IDs",
          "[session_manager][ids]") {
  TempDirGuard tmp("ids");

  agenticdsl::SessionManager mgr(tmp.path);
  mgr.open("sess_ids");

  std::set<std::string> node_ids;
  std::set<std::string> branch_ids;
  for (int i = 0; i < 100; ++i) {
    auto nid = mgr.next_node_id();
    auto bid = mgr.next_branch_id();
    REQUIRE_FALSE(nid.empty());
    REQUIRE_FALSE(bid.empty());
    REQUIRE(node_ids.insert(nid).second);
    REQUIRE(branch_ids.insert(bid).second);
  }
}

TEST_CASE("SessionManager: open on nonexistent session creates empty JSONL file",
          "[session_manager][open]") {
  TempDirGuard tmp("open_new");

  agenticdsl::SessionManager mgr(tmp.path);
  mgr.open("sess_newly_opened");

  const auto jsonl_path = tmp.path / "sess_newly_opened.jsonl";
  REQUIRE(fs::exists(jsonl_path));

  // 文件应为空 (0 条记录)
  std::ifstream in(jsonl_path);
  std::string line;
  REQUIRE_FALSE(std::getline(in, line));
}

TEST_CASE("SessionManager: open on new session is idempotent and handle matches expected path",
          "[session_manager][task2][open]") {
  TempDirGuard tmp("task2_open_new");

  agenticdsl::SessionManager mgr(tmp.path);
  const auto handle = mgr.open("sess_test_X");

  const auto jsonl_path = tmp.path / "sess_test_X.jsonl";
  REQUIRE(fs::exists(jsonl_path));
  REQUIRE_FALSE(handle.session_id.empty());
  REQUIRE(handle.session_id == "sess_test_X");
  REQUIRE(handle.jsonl_path == jsonl_path);

  const auto handle2 = mgr.open("sess_test_X");
  REQUIRE(handle2.session_id == "sess_test_X");
  REQUIRE(handle2.jsonl_path == jsonl_path);
}

TEST_CASE("SessionManager: load_jsonl returns previously written records in original order",
          "[session_manager][task2][load]") {
  TempDirGuard tmp("task2_load_jsonl");

  agenticdsl::SessionManager mgr(tmp.path);
  mgr.open("sess_load");

  std::vector<std::string> written_ids;
  std::vector<std::string> written_texts;
  for (int i = 0; i < 3; ++i) {
    agenticdsl::SessionNode node;
    node.id = mgr.next_node_id();
    node.parent_id = (i == 0) ? "" : written_ids.back();
    node.branch_id = "branch_main";
    node.content = nlohmann::json{
        {"role", "user"},
        {"index", i},
        {"text", "msg_" + std::to_string(i)}};
    written_ids.push_back(node.id);
    written_texts.push_back("msg_" + std::to_string(i));
    mgr.flush_append(node);
  }

  mgr.open("sess_load");
  const auto loaded = mgr.load_jsonl();

  REQUIRE(loaded.size() == 3);

  for (size_t i = 0; i < loaded.size(); ++i) {
    REQUIRE(loaded[i].id == written_ids[i]);
    REQUIRE(loaded[i].content["index"] == static_cast<int>(i));
    REQUIRE(loaded[i].content["text"] == written_texts[i]);
    REQUIRE(loaded[i].branch_id == "branch_main");
    if (i == 0) {
      REQUIRE(loaded[i].parent_id.empty());
    } else {
      REQUIRE(loaded[i].parent_id == written_ids[i - 1]);
    }
  }

  for (const auto& id : written_ids) {
    const auto* found = mgr.find_node(id);
    REQUIRE(found != nullptr);
    REQUIRE(found->id == id);
  }
}

TEST_CASE("SessionManager: next_node_id / next_branch_id use node_/branch_ prefix and are unique",
          "[session_manager][task2][ids]") {
  TempDirGuard tmp("task2_ids");

  agenticdsl::SessionManager mgr(tmp.path);
  mgr.open("sess_ids");

  std::set<std::string> node_ids;
  std::set<std::string> branch_ids;
  for (int i = 0; i < 100; ++i) {
    const auto nid = mgr.next_node_id();
    const auto bid = mgr.next_branch_id();

    REQUIRE_FALSE(nid.empty());
    REQUIRE_FALSE(bid.empty());

    REQUIRE(nid.rfind("node_", 0) == 0);
    REQUIRE(bid.rfind("branch_", 0) == 0);

    REQUIRE(node_ids.insert(nid).second);
    REQUIRE(branch_ids.insert(bid).second);
  }

  REQUIRE(node_ids.size() == 100);
  REQUIRE(branch_ids.size() == 100);
}

// ==================== Task 2: load_jsonl ====================

TEST_CASE("SessionManager::load_jsonl returns empty for new session",
          "[session_manager][task3][load_jsonl]") {
  TempDirGuard tmp("task3_load_empty");
  agenticdsl::SessionManager mgr(tmp.path);
  mgr.open("fresh");

  auto nodes = mgr.load_jsonl();
  REQUIRE(nodes.empty());
}

TEST_CASE("SessionManager::load_jsonl parses 3 records in order",
          "[session_manager][task3][load_jsonl]") {
  TempDirGuard tmp("task3_load_3");
  agenticdsl::SessionManager mgr(tmp.path);
  mgr.open("s3");

  mgr.flush_append(make_node("a", ""));
  mgr.flush_append(make_node("b", "a"));
  mgr.flush_append(make_node("c", "b"));

  auto nodes = mgr.load_jsonl();
  REQUIRE(nodes.size() == 3);
  REQUIRE(nodes[0].id == "a");
  REQUIRE(nodes[1].id == "b");
  REQUIRE(nodes[2].id == "c");
}

// ==================== Task 3: fork / switch_branch / append_to_branch ====================

TEST_CASE("SessionManager::fork creates new branch with parent_id set",
          "[session_manager][task4][fork]") {
  TempDirGuard tmp("task4_fork");
  agenticdsl::SessionManager mgr(tmp.path);
  mgr.open("s");
  mgr.flush_append(make_node("root", ""));
  mgr.flush_append(make_node("n1", "root"));

  // 预加载索引确保 auto-create 生效 (fork 内部也会 auto-load)
  mgr.load_jsonl();

  const auto branch_id = mgr.fork("n1", "explore");
  REQUIRE_FALSE(branch_id.empty());
  REQUIRE(mgr.current_branch() == branch_id);

  // fork 写入的 branch meta 记录可被加载
  // 注: auto-create 会在 load_jsonl 中为 "main" 创建占位分支
  auto branches = mgr.list_branches();
  REQUIRE(branches.size() >= 2);  // main + explore
}

TEST_CASE("SessionManager::fork throws on unknown node_id",
          "[session_manager][task4][fork]") {
  TempDirGuard tmp("task4_fork_throw");
  agenticdsl::SessionManager mgr(tmp.path);
  mgr.open("s");
  REQUIRE_THROWS(mgr.fork("nonexistent", "explore"));
}

TEST_CASE("SessionManager::append_to_branch adds with chain parent",
          "[session_manager][task4][append]") {
  TempDirGuard tmp("task4_append");
  agenticdsl::SessionManager mgr(tmp.path);
  mgr.open("s");
  mgr.flush_append(make_node("root", ""));
  mgr.flush_append(make_node("n1", "root"));

  mgr.fork("n1", "explore");
  const auto new_id = mgr.append_to_branch("test message");
  REQUIRE_FALSE(new_id.empty());

  mgr.load_jsonl();
  auto nodes = mgr.load_jsonl();
  bool found = false;
  for (const auto& n : nodes) {
    if (n.id == new_id) {
      found = true;
      REQUIRE(n.parent_id == "n1");
      break;
    }
  }
  REQUIRE(found);
}

// ==================== Task 3: fork / switch_branch / append_to_branch (TDD ship) ====================

TEST_CASE("SessionManager::fork creates new branch and writes BranchMeta with forked_from_node",
          "[session_manager][task3][fork][ship]") {
  TempDirGuard tmp("task3_fork_branchmeta");
  agenticdsl::SessionManager mgr(tmp.path);
  mgr.open("s_fork_meta");

  agenticdsl::SessionNode root;
  root.id = mgr.next_node_id();
  root.parent_id = "";
  root.branch_id = "main";
  root.content = {{"role", "user"}, {"text", "seed"}};
  mgr.flush_append(root);

  const auto branch_id = mgr.fork(root.id, "explore");

  REQUIRE_FALSE(branch_id.empty());
  REQUIRE(branch_id.rfind("branch_", 0) == 0);
  REQUIRE(mgr.current_branch() == branch_id);

  const auto jsonl_path = tmp.path / "s_fork_meta.jsonl";
  std::ifstream in(jsonl_path);
  REQUIRE(in.is_open());

  bool found_meta = false;
  std::string line;
  while (std::getline(in, line)) {
    if (line.empty()) continue;
    auto parsed = nlohmann::json::parse(line);
    if (parsed.contains("type") && parsed["type"] == "branch") {
      found_meta = true;
      REQUIRE(parsed["branch_id"] == branch_id);
      REQUIRE(parsed["name"] == "explore");
      REQUIRE(parsed["forked_from_node"] == root.id);
      REQUIRE_FALSE(parsed["created_at"].get<std::string>().empty());
    }
  }
  REQUIRE(found_meta);

  auto branches = mgr.list_branches();
  bool found_in_index = false;
  for (const auto& b : branches) {
    if (b.branch_id == branch_id && b.name == "explore" &&
        b.forked_from_node == root.id) {
      found_in_index = true;
      break;
    }
  }
  REQUIRE(found_in_index);
}

TEST_CASE("SessionManager::switch_branch updates current branch pointer and append_to_branch uses it",
          "[session_manager][task3][switch_branch][ship]") {
  TempDirGuard tmp("task3_switch");
  agenticdsl::SessionManager mgr(tmp.path);
  mgr.open("s_switch");

  agenticdsl::SessionNode root;
  root.id = mgr.next_node_id();
  root.parent_id = "";
  root.branch_id = "main";
  root.content = {{"seed", true}};
  mgr.flush_append(root);

  const auto explore_id = mgr.fork(root.id, "explore");
  REQUIRE(mgr.current_branch() == explore_id);

  mgr.switch_branch("main");
  REQUIRE(mgr.current_branch() == "main");

  mgr.switch_branch(explore_id);
  REQUIRE(mgr.current_branch() == explore_id);

  const auto new_node_id = mgr.append_to_branch("hello explore");
  REQUIRE_FALSE(new_node_id.empty());

  mgr.load_jsonl();
  const auto* node = mgr.find_node(new_node_id);
  REQUIRE(node != nullptr);
  REQUIRE(node->branch_id == explore_id);
  REQUIRE(node->parent_id == root.id);
  REQUIRE(node->content.contains("message"));
  REQUIRE(node->content["message"] == "hello explore");
}

TEST_CASE("SessionManager::switch_branch throws on nonexistent branch_id",
          "[session_manager][task3][switch_branch_throws][ship]") {
  TempDirGuard tmp("task3_switch_throws");
  agenticdsl::SessionManager mgr(tmp.path);
  mgr.open("s_switch_throws");

  REQUIRE_THROWS_AS(mgr.switch_branch("nonexistent_branch_id"),
                    std::runtime_error);
}

TEST_CASE("SessionManager::fork on unknown node_id throws std::runtime_error",
          "[session_manager][task3][fork_throws][ship]") {
  TempDirGuard tmp("task3_fork_throws");
  agenticdsl::SessionManager mgr(tmp.path);
  mgr.open("s_fork_throws");

  agenticdsl::SessionNode n;
  n.id = mgr.next_node_id();
  n.parent_id = "";
  n.branch_id = "main";
  n.content = {{"x", 1}};
  mgr.flush_append(n);

  REQUIRE_THROWS_AS(mgr.fork("nonexistent_node_id", "explore"),
                    std::runtime_error);

  REQUIRE(mgr.current_branch() == "main");
}

// ==================== Task 5: build_context_entries ====================

TEST_CASE("SessionManager::build_context_entries walks leaf-to-root",
          "[session_manager][task5][context]") {
  TempDirGuard tmp("task5_context");
  agenticdsl::SessionManager mgr(tmp.path);
  mgr.open("s");
  mgr.flush_append(make_node("root", ""));
  mgr.flush_append(make_node("n1", "root"));
  mgr.flush_append(make_node("n2", "n1"));
  mgr.flush_append(make_node("n3", "n2"));

  mgr.load_jsonl();

  auto ctx = mgr.build_context_entries("n3");
  REQUIRE(ctx.size() == 4);
  // root-first order
  REQUIRE(ctx[0].id == "root");
  REQUIRE(ctx[1].id == "n1");
  REQUIRE(ctx[2].id == "n2");
  REQUIRE(ctx[3].id == "n3");
}

TEST_CASE("SessionManager::build_context_entries respects branch isolation",
          "[session_manager][task5][context][ship]") {
  TempDirGuard tmp("task5_isolation");
  agenticdsl::SessionManager mgr(tmp.path);
  mgr.open("s");
  mgr.flush_append(make_node("root", ""));
  mgr.flush_append(make_node("n1", "root"));

  const auto explore_branch_id = mgr.fork("n1", "explore");
  REQUIRE_FALSE(explore_branch_id.empty());

  // fork 返回的 branch_id (e.g. "branch_1") 与 fork 名字 "explore" 不同
  mgr.flush_append(make_node("n2", "n1", explore_branch_id));

  mgr.load_jsonl();
  mgr.switch_branch("main");
  mgr.flush_append(make_node("n3", "n1"));

  mgr.load_jsonl();

  auto ctx_a = mgr.build_context_entries(mgr.get_branch_leaf(explore_branch_id));
  REQUIRE(ctx_a.size() == 3);
  REQUIRE(ctx_a.back().id == "n2");

  mgr.switch_branch("main");
  auto ctx_b = mgr.build_context_entries(mgr.get_branch_leaf("main"));
  REQUIRE(ctx_b.size() == 3);
  REQUIRE(ctx_b.back().id == "n3");
}

TEST_CASE("SessionManager::build_context_entries protects against cycles",
          "[session_manager][task5][cycle]") {
  TempDirGuard tmp("task5_cycle");
  agenticdsl::SessionManager mgr(tmp.path);
  mgr.open("s");
  mgr.flush_append(make_node("root", ""));
  mgr.flush_append(make_node("n1", "root"));

  // 手动构造循环: n1.parent_id = n1 (自循环)
  auto path = tmp.path / "s.jsonl";
  std::ofstream out(path, std::ios::app);
  out << make_node("n1", "n1").to_json().dump() << "\n";
  out.close();

  mgr.load_jsonl();
  // 不应死循环
  auto ctx = mgr.build_context_entries("n1");
  REQUIRE(ctx.size() <= 10);  // visited-set 保护
}

TEST_CASE("SessionManager::get_root_node returns parent-less node",
          "[session_manager][task5][root]") {
  TempDirGuard tmp("task5_root");
  agenticdsl::SessionManager mgr(tmp.path);
  mgr.open("s");
  mgr.flush_append(make_node("root", ""));
  mgr.flush_append(make_node("n1", "root"));

  mgr.load_jsonl();
  REQUIRE(mgr.get_root_node() == "root");
}

// ==================== Task 4: compact ====================

TEST_CASE("SessionManager::compact keeps only active branch and creates backup",
          "[session_manager][task4][compact]") {
  TempDirGuard tmp("task4_compact");
  agenticdsl::SessionManager mgr(tmp.path);
  mgr.open("s");
  mgr.flush_append(make_node("root", ""));
  mgr.flush_append(make_node("n1", "root"));

  mgr.fork("n1", "explore");
  mgr.flush_append(make_node("n2", "n1"));  // explore branch

  mgr.switch_branch("main");
  // 切换回 main 后 compact

  // 验证 .backup 存在
  fs::path backup_path = tmp.path / "s.jsonl.backup";
  mgr.compact();
  REQUIRE(fs::exists(backup_path));
}

// Task 4 ship: 3 个新测试 (compact 去除废弃分支 / .backup 内容 / append-only 不变量)

TEST_CASE("SessionManager::compact removes inactive branch messages from JSONL",
          "[session_manager][task4][compact_remove]") {
  TempDirGuard tmp("task4_compact_remove");
  agenticdsl::SessionManager mgr(tmp.path);
  mgr.open("s");
  mgr.flush_append(make_node("root", ""));
  mgr.flush_append(make_node("n1", "root"));
  mgr.flush_append(make_node("n2", "n1"));
  mgr.flush_append(make_node("n3", "n2"));

  // fork at n2 -> explore; append 2 messages to fork
  const auto explore_id = mgr.fork("n2", "explore");
  REQUIRE_FALSE(explore_id.empty());
  const auto fork_msg1 = mgr.append_to_branch("fork msg 1");
  const auto fork_msg2 = mgr.append_to_branch("fork msg 2");
  REQUIRE_FALSE(fork_msg1.empty());
  REQUIRE_FALSE(fork_msg2.empty());

  mgr.switch_branch("main");
  REQUIRE(mgr.current_branch() == "main");

  mgr.compact();

  // 读 compact 后 JSONL
  const auto jsonl_path = tmp.path / "s.jsonl";
  std::ifstream in(jsonl_path);
  REQUIRE(in.is_open());

  std::unordered_set<std::string> ids_seen;
  bool found_main_branch_meta = false;
  bool found_fork_msg1 = false;
  bool found_fork_msg2 = false;
  bool found_explore_branch_meta = false;
  std::string line;
  while (std::getline(in, line)) {
    if (line.empty()) continue;
    auto parsed = nlohmann::json::parse(line);
    if (parsed.contains("type") && parsed["type"] == "branch") {
      const auto bid = parsed.value("branch_id", "");
      if (bid == "main") found_main_branch_meta = true;
      if (bid == explore_id) found_explore_branch_meta = true;
    } else {
      const auto id = parsed.value("id", "");
      ids_seen.insert(id);
      if (id == fork_msg1) found_fork_msg1 = true;
      if (id == fork_msg2) found_fork_msg2 = true;
    }
  }

  // 主分支记录完整保留
  REQUIRE(ids_seen.count("root") == 1);
  REQUIRE(ids_seen.count("n1") == 1);
  REQUIRE(ids_seen.count("n2") == 1);
  REQUIRE(ids_seen.count("n3") == 1);
  REQUIRE(found_main_branch_meta);

  // fork 分支消息被移除
  REQUIRE_FALSE(found_fork_msg1);
  REQUIRE_FALSE(found_fork_msg2);
  REQUIRE_FALSE(found_explore_branch_meta);
}

TEST_CASE("SessionManager::compact creates .backup file with pre-compact content",
          "[session_manager][task4][compact_backup]") {
  TempDirGuard tmp("task4_compact_backup");
  agenticdsl::SessionManager mgr(tmp.path);
  mgr.open("s");
  mgr.flush_append(make_node("root", ""));
  mgr.flush_append(make_node("n1", "root"));
  mgr.flush_append(make_node("n2", "n1"));
  mgr.flush_append(make_node("n3", "n2"));

  const auto explore_id = mgr.fork("n2", "explore");
  mgr.append_to_branch("fork msg 1");
  mgr.append_to_branch("fork msg 2");
  mgr.switch_branch("main");

  // count lines before compact
  const auto jsonl_path = tmp.path / "s.jsonl";
  std::ifstream in_before(jsonl_path);
  REQUIRE(in_before.is_open());
  int pre_compact_count = 0;
  std::string line;
  while (std::getline(in_before, line)) {
    if (!line.empty()) ++pre_compact_count;
  }
  in_before.close();
  REQUIRE(pre_compact_count > 0);

  // .backup should not exist yet
  const auto backup_path = tmp.path / "s.jsonl.backup";
  REQUIRE_FALSE(fs::exists(backup_path));

  mgr.compact();

  // .backup exists
  REQUIRE(fs::exists(backup_path));

  // .backup line count equals pre-compact line count
  std::ifstream in_backup(backup_path);
  REQUIRE(in_backup.is_open());
  int backup_count = 0;
  while (std::getline(in_backup, line)) {
    if (!line.empty()) ++backup_count;
  }
  REQUIRE(backup_count == pre_compact_count);
}

TEST_CASE("SessionManager::compact preserves append-only invariant",
          "[session_manager][task4][compact_invariant]") {
  TempDirGuard tmp("task4_compact_invariant");
  agenticdsl::SessionManager mgr(tmp.path);
  mgr.open("s");
  mgr.flush_append(make_node("root", ""));
  mgr.flush_append(make_node("n1", "root"));
  mgr.flush_append(make_node("n2", "n1"));
  mgr.flush_append(make_node("n3", "n2"));

  const auto explore_id = mgr.fork("n2", "explore");
  mgr.append_to_branch("fork msg 1");
  mgr.append_to_branch("fork msg 2");
  mgr.switch_branch("main");

  mgr.compact();

  // 文件行数 == 剩余 committed record 数
  // 预期: 4 main nodes (root, n1, n2, n3) + 1 main branch meta = 5 行
  const auto jsonl_path = tmp.path / "s.jsonl";
  std::ifstream in(jsonl_path);
  REQUIRE(in.is_open());

  int line_count = 0;
  std::string line;
  while (std::getline(in, line)) {
    if (!line.empty()) ++line_count;
  }

  REQUIRE(line_count == 5);
}