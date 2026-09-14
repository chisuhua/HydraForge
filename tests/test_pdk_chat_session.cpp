// tests/test_pdk_chat_session.cpp
// chat-session-pdk-lift Change 1 — mock-first 测试 (10 cases)
// 设计依据: docs/superpowers/specs/2026-09-11-chat-session-pdk-lift-design.md §7.2
// 模式: tests/AGENTS.md Pattern 1-7
// 日期: 2026-09-11
//
// 目标 target 命名: 本文件在 core 树 (tests/) 注册为 test_pdk_chat_session,
// 与 examples 树的 test_chat_session 区分 (file(GLOB) 自动注册, 同名会 CMP0002 冲突)。
//
// 2026-09-14 实施偏差 (plan §7.2 与实际源码不符处, 已按真实契约改写):
//   - plan case "request_stop mid-turn with cancellation" 断言 pre-cancelled token
//     会让 chat() 返回 success=false。实测 chat() 只把 token 存入 current_token_,
//     并不读取 token.stop_requested(); 取消需经 request_stop() → registry source。
//     本文件改为真实取消链 E2E (stub loop/run 观察 registry token + 并发 request_stop)。
//   - plan case "RecordingLLMProvider last_model empty" 断言 generate_calls >= 1。
//     实测 chat() 完全不经 LLM provider (统一走 loop/run 工具), 应为 == 0。
//     这正是既有 test_e2e_mock 的 "routes through loop/run" 契约, 故按 == 0 断言。

#include "catch_amalgamated.hpp"

#include <agenticdsl/contract/event_builder.h>
#include <agenticdsl/contract/iinteraction_bus.h>
#include <agenticdsl/contract/ilogger.h>
#include <agenticdsl/contract/iinput_source.h>
#include <agenticdsl/contract/inmemory_bus.h>
#include <agenticdsl/pdk/cancellation_registry.h>
#include <agenticdsl/pdk/chat_session.h>
#include <common/llm/llm_types.h>
#include <common/tools/registry.h>
#include <core/types/tool_result.h>

#include "test_helpers/capturing_logger.h"
#include "test_helpers/in_memory_input_source.h"

#include <atomic>
#include <chrono>
#include <memory>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

using namespace hydraforge::pdk;
using agenticdsl::IInputSource;
using agenticdsl::ILogger;
using agenticdsl::LogLevel;
using agenticdsl::test::CapturingLogger;
using agenticdsl::test::InMemoryInputSource;

namespace {

constexpr size_t kQueueCapacity = 32;

// Fixture: 真 ToolRegistry + 真 InMemoryBus + loop/run 桩。
// persist_dir 置空避免测试写 ~/.hydraforge/sessions/。
struct Fixture {
  std::shared_ptr<agenticdsl::InMemoryBus> bus =
      std::make_shared<agenticdsl::InMemoryBus>();
  agenticdsl::ToolRegistry registry;
  SessionConfig cfg;
  std::atomic<int> loop_run_calls{0};

  Fixture() {
    cfg.persist_dir = "";
    cfg.enable_input_thread = false;
    register_loop_run_stub();
  }

  void register_loop_run_stub() {
    agenticdsl::ToolMetadata meta;
    meta.name = "loop/run";
    meta.description = "test stub";
    meta.domain = "test";
    registry.register_tool(
        "loop/run", meta,
        [this](const std::unordered_map<std::string, std::string>& args)
            -> nlohmann::json {
          loop_run_calls.fetch_add(1);
          nlohmann::json r;
          r["response"] =
              args.count("prompt") ? ("echo:" + args.at("prompt")) : "echo";
          r["steps"] = 1;
          r["tokens_used"] = 0;
          r["cost_usd"] = 0.0;
          return r;
        });
  }
};

// 轮询等待队列达到期望长度 (input thread 为异步生产者)
bool wait_for_queue_size(const ChatSession& session, QueueKind kind, size_t expected,
                         std::chrono::milliseconds timeout) {
  const auto deadline = std::chrono::steady_clock::now() + timeout;
  while (std::chrono::steady_clock::now() < deadline) {
    if (session.queue_size(kind) == expected) return true;
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
  }
  return session.queue_size(kind) == expected;
}

}  // namespace

TEST_CASE("ChatSession ctor default nullptr safe no-op",
          "[pdk][chat_session][ctor]") {
  // Pattern 5 fail-safe: 全 nullptr 构造 + input_thread 默认关闭 → 不抛异常、不起线程
  ChatSession session(nullptr, nullptr, nullptr, AgentConfig{}, SessionConfig{});
  REQUIRE_FALSE(session.is_input_thread_shutdown());
  REQUIRE(session.history().empty());
}

TEST_CASE("ChatSession explicit InMemoryInputSource classifies steering vs follow-up",
          "[pdk][chat_session][ctor]") {
  Fixture f;
  auto input = std::make_unique<InMemoryInputSource>();
  input->enqueue_input("plain follow-up");
  input->enqueue_input("/help");  // '/' 前缀 → steering
  f.cfg.enable_input_thread = true;

  ChatSession session(nullptr, f.bus, &f.registry, AgentConfig{}, f.cfg, nullptr,
                      nullptr, std::move(input),
                      std::make_unique<CapturingLogger>());

  REQUIRE(wait_for_queue_size(session, QueueKind::Steering, 1,
                              std::chrono::milliseconds(1000)));
  REQUIRE(wait_for_queue_size(session, QueueKind::FollowUp, 1,
                              std::chrono::milliseconds(1000)));
}

TEST_CASE("ChatSession steering priority is steering before follow-up",
          "[pdk][chat_session][steering]") {
  Fixture f;
  ChatSession session(nullptr, f.bus, &f.registry, AgentConfig{}, f.cfg);

  REQUIRE(session.try_push_follow_up_for_test("follow-1"));
  REQUIRE(session.try_push_steering_for_test("/steer-1"));

  auto first = session.try_pop_input();
  REQUIRE(first.has_value());
  REQUIRE(first->kind == QueueKind::Steering);
  REQUIRE(first->text == "/steer-1");

  auto second = session.try_pop_input();
  REQUIRE(second.has_value());
  REQUIRE(second->kind == QueueKind::FollowUp);
  REQUIRE(second->text == "follow-1");
}

TEST_CASE("ChatSession request_stop is no-op when no turn in flight",
          "[pdk][chat_session][cancel]") {
  Fixture f;
  ChatSession session(nullptr, f.bus, &f.registry, AgentConfig{}, f.cfg);
  // current_cancellation_id_ 为空 → request_stop 早返回, 不 deref null registry
  REQUIRE_NOTHROW(session.request_stop());
  REQUIRE_FALSE(session.is_input_thread_shutdown());
}

TEST_CASE("ChatSession overflow rejects at capacity 32 and logs a warn",
          "[pdk][chat_session][queue]") {
  Fixture f;
  auto logger = std::make_unique<CapturingLogger>();
  auto* raw_logger = logger.get();
  ChatSession session(nullptr, f.bus, &f.registry, AgentConfig{}, f.cfg, nullptr,
                      nullptr, nullptr, std::move(logger));

  for (size_t i = 0; i < kQueueCapacity; ++i) {
    REQUIRE(session.try_push_follow_up_for_test("msg-" + std::to_string(i)));
  }
  // 第 33 条被拒 (overflow-reject 契约)
  REQUIRE_FALSE(session.try_push_follow_up_for_test("overflow-msg"));
  REQUIRE(session.queue_size(QueueKind::FollowUp) == kQueueCapacity);
  REQUIRE(raw_logger->count(LogLevel::kWarn) >= 1);
}

TEST_CASE("ChatSession chat routes through loop/run and returns its response",
          "[pdk][chat_session][chat]") {
  Fixture f;
  ChatSession session(nullptr, f.bus, &f.registry, AgentConfig{}, f.cfg);

  auto result = session.chat("hello", {});
  REQUIRE(result.success);
  REQUIRE(result.error_message.empty());
  REQUIRE(result.response == "echo:hello");
  REQUIRE(f.loop_run_calls.load() == 1);
}

TEST_CASE("ChatSession 5 sequential turns accumulate history",
          "[pdk][chat_session][concurrency]") {
  Fixture f;
  ChatSession session(nullptr, f.bus, &f.registry, AgentConfig{}, f.cfg);

  for (int i = 0; i < 5; ++i) {
    auto result = session.chat("turn-" + std::to_string(i), {});
    REQUIRE(result.success);
    REQUIRE(result.response == "echo:turn-" + std::to_string(i));
  }
  // 5 轮 × (user + assistant)
  REQUIRE(session.history().size() == 10);
  REQUIRE(f.loop_run_calls.load() == 5);
}

TEST_CASE("ChatSession cancellation propagates to in-flight loop/run",
          "[pdk][chat_session][cancel]") {
  Fixture f;
  auto registry = std::make_shared<CancellationRegistry>();
  // 独立 registry: Fixture 已注册同名 loop/run, ToolRegistry 拒绝重复注册
  agenticdsl::ToolRegistry blocking_registry;

  // stub 观察 registry 里自己的 cancellation_id, 被 request_stop 后立即返回
  agenticdsl::ToolMetadata meta;
  meta.name = "loop/run";
  meta.description = "blocking stub observing cancellation";
  meta.domain = "test";
  auto* registry_raw = registry.get();
  blocking_registry.register_tool(
      "loop/run", meta,
      [registry_raw](const std::unordered_map<std::string, std::string>& args)
          -> nlohmann::json {
        const std::string cid = args.count("cancellation_id")
                                    ? args.at("cancellation_id")
                                    : std::string{};
        nlohmann::json r;
        r["steps"] = 1;
        r["tokens_used"] = 0;
        r["cost_usd"] = 0.0;
        if (cid.empty()) {
          r["response"] = "no-cancellation-id";
          return r;
        }
        const auto deadline =
            std::chrono::steady_clock::now() + std::chrono::milliseconds(2000);
        while (std::chrono::steady_clock::now() < deadline) {
          if (registry_raw->resolve_token(cid).stop_requested()) {
            r["response"] = "cancelled";
            return r;
          }
          std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
        r["response"] = "timeout";
        return r;
      });

  ChatSession session(nullptr, f.bus, &blocking_registry, AgentConfig{}, f.cfg, registry);

  std::thread canceller([&session]() {
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    session.request_stop();
  });
  auto result = session.chat("long-running", {});
  canceller.join();

  REQUIRE(result.success);
  REQUIRE(result.response == "cancelled");
}

TEST_CASE("ChatSession does not call the LLM provider directly (loop/run is the only path)",
          "[pdk][chat_session][realllm-guard]") {
  // Pattern 2 守卫: RecordingLLMProvider 记录 generate 次数。
  // 契约: chat() 统一经 loop/run 工具, 不直接依赖 ILLMProvider。
  class RecordingLLMProvider : public agenticdsl::ILLMProvider {
   public:
    std::string last_model;
    int generate_calls = 0;

    agenticdsl::Result<agenticdsl::GenerationResult, agenticdsl::LLMError> generate(
        const agenticdsl::GenerationRequest& req, std::stop_token) override {
      last_model = req.params.model;
      ++generate_calls;
      agenticdsl::GenerationResult r;
      r.text = "direct-provider-response";
      return agenticdsl::Result<agenticdsl::GenerationResult,
                                agenticdsl::LLMError>::success(r);
    }
    std::unique_ptr<agenticdsl::IGenerationStream> generate_stream(
        const agenticdsl::GenerationRequest&, std::stop_token) override {
      return nullptr;
    }
    std::vector<agenticdsl::ILLMProvider::ModelInfo> available_models() const override {
      return {};
    }
  };

  Fixture f;
  // 真 engine (空 graph, 与 test_e2e_mock 同构) + recorder provider
  auto engine = std::make_unique<agenticdsl::DSLEngine>(
      std::vector<agenticdsl::ParsedGraph>{});
  auto recorder = std::make_unique<RecordingLLMProvider>();
  auto* raw = recorder.get();
  engine->set_llm_provider(std::move(recorder));

  ChatSession session(engine.get(), f.bus, &f.registry, AgentConfig{}, f.cfg);

  auto result = session.chat("test", {});

  REQUIRE(result.success);
  REQUIRE(result.response == "echo:test");   // 来自 loop/run 桩
  REQUIRE(result.response != "direct-provider-response");
  REQUIRE(f.loop_run_calls.load() == 1);
  // Pattern 2 契约: ChatSession 不直接调用 ILLMProvider (统一经 loop/run)
  REQUIRE(raw->generate_calls == 0);
}

TEST_CASE("ChatSession with nullptr CancellationRegistry falls back gracefully",
          "[pdk][chat_session][fallback]") {
  Fixture f;
  auto logger = std::make_unique<CapturingLogger>();
  auto* raw_logger = logger.get();
  ChatSession session(nullptr, f.bus, &f.registry, AgentConfig{}, f.cfg,
                      nullptr,  // nullptr registry → Impl 自持 fallback
                      nullptr, nullptr, std::move(logger));

  // fallback registry 自持 → 取消链完整可用, 不 deref null
  REQUIRE_NOTHROW(session.request_stop());
  auto result = session.chat("fallback-turn", {});
  REQUIRE(result.success);
  REQUIRE(result.response == "echo:fallback-turn");
  REQUIRE(raw_logger->count(LogLevel::kWarn) == 0);
}
