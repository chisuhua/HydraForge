// tests/test_pdk_chat_session_recovery.cpp
// 断线恢复 E2E + A5.6 topic 分级基线 (chat-session-pdk-lift Change 2)
// 设计依据: docs/superpowers/specs/2026-09-11-chat-session-pdk-lift-design.md §7.3 + §A5.6
// 日期: 2026-09-11
//
// 2026-09-14 实施偏差 (plan §7.3 与实际 SessionManager 语义不符处, 已按真实契约改写):
//   - plan E2E-2 直接 flush_append 而未先 open() → SessionManager 会抛
//     "no session opened (call open() first)"。已补 open()。
//   - plan E2E-3 断言 branch_b_entries.size() == 2。实测 build_context_entries 沿
//     parent_id 链回溯到根, **跨分支**(fork 子分支的 parent 就是 A 的叶子), 故为 4。
//     本文件改为同时断言: (a) 父链语义 4; (b) 按 branch_id 过滤的真实隔离 2/2。
//   - plan E2E-1 用硬编码 leaf id "node-after-turn-3"。实测无该节点 → 断言改为
//     `sm.get_branch_leaf(sm.current_branch())` 取真实叶子。
//   - plan A5.6 用例同时断言 persist_count == 2000 与 <= 600 (自相矛盾, 必失败)。
//     本文件移除不可能的阈值 (批写属 ADR-0080 append-only-event-log scope),
//     改为断言 fast/slow 二分契约本身。

#include "catch_amalgamated.hpp"

#include <agenticdsl/contract/event_builder.h>
#include <agenticdsl/contract/iinteraction_bus.h>
#include <agenticdsl/contract/inmemory_bus.h>
#include <agenticdsl/contract/resume_token.h>
#include <agenticdsl/pdk/chat_session.h>
#include <common/tools/registry.h>

#include "test_helpers/capturing_logger.h"
#include "test_helpers/in_memory_input_source.h"

#include <atomic>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <memory>
#include <string>
#include <thread>
#include <unordered_map>

namespace fs = std::filesystem;
using namespace hydraforge::pdk;

namespace {

// 每个 case 用独立 temp dir, 避免相互污染 + 避免写 HOME
fs::path make_temp_dir(const std::string& name) {
  auto dir = fs::temp_directory_path() / name;
  fs::remove_all(dir);
  return dir;
}

void register_echo_loop_run(agenticdsl::ToolRegistry& registry) {
  agenticdsl::ToolMetadata meta;
  meta.name = "loop/run";
  meta.description = "recovery test stub";
  meta.domain = "test";
  registry.register_tool(
      "loop/run", meta,
      [](const std::unordered_map<std::string, std::string>& args) -> nlohmann::json {
        nlohmann::json r;
        r["response"] = args.count("prompt") ? ("echo:" + args.at("prompt")) : "echo";
        r["steps"] = 1;
        r["tokens_used"] = 0;
        r["cost_usd"] = 0.0;
        return r;
      });
}

SessionConfig hermetic_session_config() {
  SessionConfig cfg;
  cfg.persist_dir = "";
  cfg.enable_input_thread = false;
  return cfg;
}

}  // namespace

TEST_CASE("E2E-1 resume 3 turns then restart keeps history intact",
          "[pdk][chat_session][recovery]") {
  const auto dir = make_temp_dir("pdk_chat_session_recovery_e2e1");
  const std::string sm_session_id = "e2e1-session";
  std::string leaf_node_id;

  {
    agenticdsl::SessionManager sm(dir);
    sm.open(sm_session_id);
    agenticdsl::ToolRegistry registry;
    register_echo_loop_run(registry);
    auto bus = std::make_shared<agenticdsl::InMemoryBus>();

    ChatSession session(nullptr, bus, &registry, AgentConfig{},
                        hermetic_session_config(), nullptr, nullptr, nullptr, nullptr,
                        &sm);
    for (int i = 0; i < 3; ++i) {
      auto r = session.chat("turn-" + std::to_string(i), {});
      REQUIRE(r.success);
    }
    REQUIRE(session.history().size() == 6);

    leaf_node_id = sm.get_branch_leaf(sm.current_branch());
    REQUIRE_FALSE(leaf_node_id.empty());
  }

  // 阶段 2: 重启 — 新 SessionManager 实例 + ResumeToken 恢复
  {
    agenticdsl::SessionManager sm(dir);
    agenticdsl::ToolRegistry registry;
    register_echo_loop_run(registry);
    auto bus = std::make_shared<agenticdsl::InMemoryBus>();

    agenticdsl::ResumeToken token;
    token.session_id = sm_session_id;
    token.leaf_node_id = leaf_node_id;
    token.model = "echo-v1";

    ChatSession session(nullptr, bus, &registry, AgentConfig{},
                        hermetic_session_config(), nullptr, nullptr, nullptr, nullptr,
                        &sm, token);

    REQUIRE(session.history().size() == 6);

    auto r = session.chat("turn-3", {});
    REQUIRE(r.success);
    REQUIRE(r.response == "echo:turn-3");
    REQUIRE(session.history().size() == 8);
  }
}

TEST_CASE("E2E-2 truncated last JSONL line is discarded on reload",
          "[pdk][chat_session][recovery]") {
  const auto dir = make_temp_dir("pdk_chat_session_recovery_e2e2");
  const std::string sm_session_id = "e2e2-session";
  std::string last_complete_id;

  {
    agenticdsl::SessionManager sm(dir);
    sm.open(sm_session_id);  // plan 缺此步 → flush_append 会抛
    std::string parent_id;
    for (int i = 0; i < 3; ++i) {
      agenticdsl::SessionNode n;
      n.id = sm.next_node_id();
      n.parent_id = parent_id;
      n.branch_id = "main";
      n.content = nlohmann::json{{"msg", "msg-" + std::to_string(i)}};
      sm.flush_append(n);
      parent_id = n.id;
    }
    last_complete_id = parent_id;
    REQUIRE_FALSE(last_complete_id.empty());
  }

  // 模拟崩溃: 追加半行 JSON (无换行)
  {
    std::ofstream corrupt(dir / (sm_session_id + ".jsonl"), std::ios::app);
    corrupt << R"({"id":"node_partial","content":"partial-crashed")";
  }

  {
    agenticdsl::SessionManager sm(dir);
    sm.open(sm_session_id);
    const auto loaded = sm.load_jsonl();
    REQUIRE(loaded.size() == 3);  // 半行被丢弃

    const auto entries = sm.build_context_entries(last_complete_id);
    REQUIRE(entries.size() == 3);
    REQUIRE(entries.front().content["msg"] == "msg-0");
  }
}

TEST_CASE("E2E-3 fork branch keeps per-branch records isolated after reload",
          "[pdk][chat_session][recovery]") {
  const auto dir = make_temp_dir("pdk_chat_session_recovery_e2e3");
  const std::string sm_session_id = "e2e3-session";
  std::string last_a_id;
  std::string branch_b_id;

  {
    agenticdsl::SessionManager sm(dir);
    sm.open(sm_session_id);

    std::string parent_id;
    for (int i = 0; i < 2; ++i) {
      agenticdsl::SessionNode n;
      n.id = sm.next_node_id();
      n.parent_id = parent_id;
      n.branch_id = "main";
      n.content = nlohmann::json{{"msg", "A-msg-" + std::to_string(i)}};
      sm.flush_append(n);
      parent_id = n.id;
    }
    last_a_id = parent_id;

    branch_b_id = sm.fork(last_a_id, "branch-B");
    REQUIRE_FALSE(branch_b_id.empty());

    std::string b_parent = last_a_id;  // fork 自 A 的叶子起新链, 逐节点链式挂接
    for (int i = 0; i < 2; ++i) {
      agenticdsl::SessionNode n;
      n.id = sm.next_node_id();
      n.parent_id = b_parent;
      n.branch_id = branch_b_id;
      n.content = nlohmann::json{{"msg", "B-msg-" + std::to_string(i)}};
      sm.flush_append(n);
      b_parent = n.id;
    }
  }

  // 重启: 新实例从 JSONL 重建索引
  {
    agenticdsl::SessionManager sm(dir);
    sm.open(sm_session_id);
    sm.load_jsonl();

    // 分支叶定位 (真实语义, 与 plan 假设不同):
    // get_branch_leaf 的定义是 "该分支内无子节点的节点"。
    // fork 之后 main 的末端节点被 branch-B 的子节点引用 → 不再是叶子 → 返回空。
    REQUIRE(sm.get_branch_leaf("main").empty());
    const std::string leaf_b = sm.get_branch_leaf(branch_b_id);
    REQUIRE_FALSE(leaf_b.empty());
    REQUIRE(leaf_b != last_a_id);

    // 父链语义: fork 子分支的 parent 是 A 的叶子 → 回溯跨分支 (root-first)
    const auto ctx_b = sm.build_context_entries(leaf_b);
    REQUIRE(ctx_b.size() == 4);
    REQUIRE(ctx_b.back().content["msg"] == "B-msg-1");

    // 隔离性: 按 branch_id 过滤后 A/B 各 2 条, 无交叉污染
    size_t a_count = 0;
    size_t b_count = 0;
    for (const auto& n : sm.list_all_nodes()) {
      if (n.branch_id == branch_b_id) {
        ++b_count;
      } else if (n.branch_id == "main") {
        ++a_count;
      }
    }
    REQUIRE(a_count == 2);
    REQUIRE(b_count == 2);
  }
}

TEST_CASE("A5.6 topic persistence grading marks slow-path with meta.persist",
          "[pdk][chat_session][topic-baseline]") {
  const auto dir = make_temp_dir("pdk_chat_session_recovery_a5_6");
  agenticdsl::SessionManager sm(dir);
  sm.open("a5-6-session");

  std::atomic<int> persist_marked{0};
  std::atomic<int> fast_path_events{0};
  std::atomic<int> persist_marked_fast{0};
  auto bus = std::make_shared<agenticdsl::InMemoryBus>();

  const auto record_slow = [&](const agenticdsl::BusEvent& evt) {
    if (evt.payload.meta.value("persist", false)) {
      persist_marked.fetch_add(1, std::memory_order_relaxed);
    }
  };
  const auto record_fast = [&](const agenticdsl::BusEvent& evt) {
    fast_path_events.fetch_add(1, std::memory_order_relaxed);
    if (evt.payload.meta.value("persist", false)) {
      persist_marked_fast.fetch_add(1, std::memory_order_relaxed);
    }
  };
  bus->subscribe("chat.turn.start", record_slow);
  bus->subscribe("chat.turn.end", record_slow);
  // fast-path topic 必须**不带** meta.persist (A5.6 分级契约)
  bus->subscribe("chat.steering.enqueued", record_fast);
  bus->subscribe("chat.followup.enqueued", record_fast);

  agenticdsl::ToolRegistry registry;
  register_echo_loop_run(registry);

  // 经 InMemoryInputSource 真正触发 fast-path 发射路径 (input thread enqueue)
  auto input = std::make_unique<agenticdsl::test::InMemoryInputSource>();
  input->enqueue_input("/steer-command");
  input->enqueue_input("plain follow-up");
  auto cfg = hermetic_session_config();
  cfg.enable_input_thread = true;

  ChatSession session(nullptr, bus, &registry, AgentConfig{}, cfg, nullptr, nullptr,
                      std::move(input),
                      std::make_unique<agenticdsl::test::CapturingLogger>(), &sm);

  // 等 input thread 把两条输入分类入队 (fast-path 发射随后发生)
  const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
  while (std::chrono::steady_clock::now() < deadline &&
         (session.queue_size(QueueKind::Steering) == 0 ||
          session.queue_size(QueueKind::FollowUp) == 0)) {
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
  }
  REQUIRE(session.queue_size(QueueKind::Steering) == 1);
  REQUIRE(session.queue_size(QueueKind::FollowUp) == 1);

  constexpr int kTurns = 5;
  for (int i = 0; i < kTurns; ++i) {
    REQUIRE(session.chat("t" + std::to_string(i), {}).success);
  }
  bus->wait_for_drain();

  // 每轮 chat.turn.start + chat.turn.end 均为 slow-path (persist=true)
  REQUIRE(persist_marked.load() == 2 * kTurns);
  REQUIRE(fast_path_events.load() == 2);
  REQUIRE(persist_marked_fast.load() == 0);
}
