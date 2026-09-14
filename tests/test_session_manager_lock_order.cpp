// tests/test_session_manager_lock_order.cpp
// 功能描述：SessionManager 锁序不变量回归测试
// 测试范围：① write_mutex_ 不可递归 (编译期断言: open() 无 legacy_path 参数)
//          ② migrate_legacy_json + flush_append 并发零 TSan lock-order-inversion
//          ③ migrate + flush_append 交错后 JSONL 内容完整
// 设计依据：docs/superpowers/plans/2026-09-11-chat-session-pdk-lift-change2.md
//          L779-783 + openspec/changes/fix-session-manager-lock-order-inversion/
//          design.md D1-D4
// 作者：AgenticDSL Phase 5 / Session Manager lock-order fix
// 最后修改日期：2026-09-15

#include "catch_amalgamated.hpp"

#include <atomic>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <optional>
#include <sstream>
#include <string>
#include <thread>
#include <type_traits>
#include <unistd.h>
#include <vector>

#include "core/session_manager.h"
#include "nlohmann/json.hpp"

namespace fs = std::filesystem;

// ============================================================================
// Case 2: 编译期不变量 — open() 必须不含 legacy_path 参数
// ============================================================================
// write_mutex_ 是 std::mutex (非递归)。旧版 open(id, legacy_path) 在持
// write_mutex_ 时调 migrate_legacy_json → open(id) 重入取 write_mutex_ →
// 确定性自死锁。该参数已删除; 此断言防止未来回归。
static_assert(
    !std::is_invocable_v<decltype(&agenticdsl::SessionManager::open),
                         agenticdsl::SessionManager&, const std::string&,
                         std::optional<std::string>>,
    "open() must NOT accept legacy_path — write->write recursion risk");

namespace {

// TSan lock-graph 检测只需要两条锁序边各执行一遍即成环,
// 不需要真实死锁重叠; 少量迭代已足够 (大压力循环在 TSan+fsync 下
// 会拖慢到 ctest timeout, 无额外检测价值)。
constexpr int kStressIterations = 5;

fs::path make_unique_temp_dir(const std::string& tag) {
  static std::atomic<uint64_t> counter{0};
  const auto n = counter.fetch_add(1);
  const auto ts = static_cast<uint64_t>(
      std::chrono::steady_clock::now().time_since_epoch().count());
  std::ostringstream oss;
  oss << "session_lock_order_" << tag << "_"
      << static_cast<uint64_t>(::getpid()) << "_" << ts << "_" << n;
  auto dir = fs::temp_directory_path() / oss.str();
  fs::create_directories(dir);
  return dir;
}

struct TempDirGuard {
  fs::path path;
  explicit TempDirGuard(const std::string& tag)
      : path(make_unique_temp_dir(tag)) {}
  ~TempDirGuard() {
    std::error_code ec;
    fs::remove_all(path, ec);
  }
};

void write_legacy_json(const fs::path& path, int message_count) {
  nlohmann::json j;
  j["messages"] = nlohmann::json::array();
  for (int i = 0; i < message_count; ++i) {
    j["messages"].push_back(
        {{"role", "user"}, {"text", "msg_" + std::to_string(i)}});
  }
  std::ofstream out(path);
  out << j.dump(2) << "\n";
}

agenticdsl::SessionNode make_node(const std::string& id) {
  agenticdsl::SessionNode n;
  n.id = id;
  n.parent_id = "";
  n.branch_id = "main";
  n.content = nlohmann::json{{"role", "user"}, {"text", id}};
  return n;
}

}  // namespace

// ============================================================================
// Case 1: TSan-gate — migrate + flush_append 并发零 lock-order-inversion
// ============================================================================
// 修复前: migrate_legacy_json 末段持 index_mutex_ 调 flush_append_internal
//         (取 write_mutex_) → 与 open() 的 write→index 形成 M0↔M1 环。
// 本测试依赖 TSan lock-graph 检测 (两条边执行过即建图), 不依赖真实死锁重叠。
// 修复后: 全局统一 write→index, 环消失。
TEST_CASE("migrate_legacy_json + flush_append concurrent: no lock-order inversion",
          "[session_manager][lock_order][tsan]") {
  for (int iter = 0; iter < kStressIterations; ++iter) {
    TempDirGuard legacy_dir("legacy");
    TempDirGuard jsonl_dir("jsonl");
    const auto legacy_path = legacy_dir.path / "data.json";
    write_legacy_json(legacy_path, 3);

    agenticdsl::SessionManager mgr(jsonl_dir.path);

    std::atomic<bool> migrate_threw{false};
    std::atomic<bool> append_done{false};

    std::thread t_migrate([&] {
      try {
        mgr.migrate_legacy_json(legacy_path);
      } catch (...) {
        migrate_threw.store(true);
      }
    });

    // flush_append 需 current_path_ 由 migrate 内部 open() 设置;
    // bounded retry-until-open: 重试到成功或超时
    std::thread t_append([&] {
      const auto deadline =
          std::chrono::steady_clock::now() + std::chrono::seconds(5);
      while (std::chrono::steady_clock::now() < deadline) {
        try {
          mgr.flush_append(make_node("thread_b_node"));
          append_done.store(true);
          return;
        } catch (const std::runtime_error&) {
          std::this_thread::yield();
        }
      }
    });

    t_migrate.join();
    t_append.join();

    REQUIRE_FALSE(migrate_threw.load());
    REQUIRE(append_done.load());
    // 3 legacy nodes + >=1 from thread B
    REQUIRE(mgr.list_all_nodes().size() >= 4);
  }
}

// ============================================================================
// Case 3: functional — migrate + flush_append 交错后 JSONL 内容完整
// ============================================================================
// 验证 BranchMeta 快照改动不引入行为变化。
TEST_CASE("migrate_legacy_json + flush_append keeps JSONL intact",
          "[session_manager][lock_order][functional]") {
  TempDirGuard legacy_dir("func_legacy");
  TempDirGuard jsonl_dir("func_jsonl");
  const auto legacy_path = legacy_dir.path / "session.json";
  write_legacy_json(legacy_path, 3);

  agenticdsl::SessionManager mgr(jsonl_dir.path);
  const auto session_id = mgr.migrate_legacy_json(legacy_path);
  REQUIRE(session_id == "session");

  mgr.flush_append(make_node("extra_a"));
  mgr.flush_append(make_node("extra_b"));

  // 3 legacy + 2 extra
  REQUIRE(mgr.list_all_nodes().size() == 5);

  bool has_main = false;
  for (const auto& b : mgr.list_branches()) {
    if (b.branch_id == "main") has_main = true;
  }
  REQUIRE(has_main);

  // JSONL 行数: 3 legacy node + 1 branch meta + 2 extra node = 6
  std::ifstream in(jsonl_dir.path / (session_id + ".jsonl"));
  REQUIRE(in.is_open());
  int line_count = 0;
  std::string line;
  while (std::getline(in, line)) {
    if (!line.empty()) ++line_count;
  }
  REQUIRE(line_count == 6);
}
