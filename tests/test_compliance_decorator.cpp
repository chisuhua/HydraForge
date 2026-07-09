// test_compliance_decorator.cpp
// 文件头注释
// 功能描述：ComplianceDecorator 集成测试
//          测试 REQ-IPD-003: hash 一致性 / 不泄露原文 / opt-in 默认禁用
// 设计依据：openspec/changes/phase5-illmprovider-call-chain-v2/tasks.md (Task 2.4)
// 作者：AgenticDSL Phase 5 ILLMProvider Call Chain V2
// 最后修改日期：2026-07-09

#include "agenticdsl/contract/i_llm_provider_decorator.h"
#include "agenticdsl/contract/inmemory_bus.h"
#include "common/llm/compliance_decorator.h"
#include "common/llm/mock_provider.h"
#include "core/types/tool_result.h"

#include <atomic>
#include <catch_amalgamated.hpp>
#include <chrono>
#include <functional>
#include <mutex>
#include <nlohmann/json.hpp>
#include <memory>
#include <string>
#include <thread>
#include <vector>

using namespace agenticdsl;

// 辅助: 等待条件满足 (带超时保护)
template <typename Predicate>
static void wait_until(Predicate pred,
                       std::chrono::milliseconds timeout = std::chrono::seconds(5)) {
  const auto deadline = std::chrono::steady_clock::now() + timeout;
  while (!pred() && std::chrono::steady_clock::now() < deadline) {
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }
}

// =====================================================================
// Test Case 1: ComplianceDecorator hash 一致性 (REQ-IPD-003 Scenario 1+2)
// =====================================================================

TEST_CASE("ComplianceDecorator hash is consistent", "[decorator][compliance]") {
  // Given: MockLLMProvider + InMemoryBus
  // 注意: 必须在 std::move 之前配置 mock_provider 响应
  const std::string prompt = "Hello, test prompt";
  const std::string test_response = "This is a test completion";
  auto mock_provider = std::make_unique<MockLLMProvider>();
  mock_provider->set_fixed_response(GenerationResult{
      .text = test_response,
      .prompt_tokens = 5,
      .completion_tokens = 3});

  auto bus = std::make_shared<InMemoryBus>();

  // 订阅 compliance.log 事件以捕获 payload (异步 dispatch 后通知)
  std::mutex events_mutex;
  std::vector<nlohmann::json> captured_events;
  size_t sub_token = bus->subscribe(
      "compliance.log",
      [&](const ToolResult& payload) {
        const auto content_str = payload.meta.value("content", std::string{});
        if (!content_str.empty()) {
          std::lock_guard<std::mutex> lock(events_mutex);
          captured_events.push_back(nlohmann::json::parse(content_str));
        }
      });

  // 包装 MockLLMProvider + ComplianceDecorator
  auto decorator = std::make_unique<ComplianceDecorator>(
      std::move(mock_provider), bus);

  // When: 调用 generate (GenerationRequest 非聚合, 用赋值方式构造)
  GenerationRequest req;
  req.prompt = prompt;
  req.params = LLMConfig{.model = "test-model-1"};
  const auto result = decorator->generate(req, {});

  // Then: result 成功 (装饰器不修改业务返回值)
  REQUIRE(result.has_value());
  CHECK(result.value().text == test_response);

  // Then: 等待异步 dispatch 完成, 至少 2 个 events
  bus->wait_for_drain();
  wait_until([&] {
    std::lock_guard<std::mutex> lock(events_mutex);
    return captured_events.size() >= 2;
  });

  // Then: 验证 captured events
  {
    std::lock_guard<std::mutex> lock(events_mutex);
    REQUIRE(captured_events.size() >= 2);

    // === 第一个 event: prompt_hash (调用前 emit) ===
    const auto& prompt_payload = captured_events[0];
    CHECK(prompt_payload.contains("prompt_hash"));
    CHECK(prompt_payload["prompt_hash"].is_number_unsigned());
    CHECK(prompt_payload["model"] == "test-model-1");
    CHECK(prompt_payload.contains("timestamp"));
    CHECK(prompt_payload["timestamp"].is_string());

    // hash 一致性验证 (std::hash<std::string>)
    const auto expected_prompt_hash =
        static_cast<std::uint64_t>(std::hash<std::string>{}(prompt));
    CHECK(prompt_payload["prompt_hash"].get<std::uint64_t>() ==
          expected_prompt_hash);

    // === 第二个 event: completion_hash (调用后 emit, 仅成功时) ===
    const auto& completion_payload = captured_events[1];
    CHECK(completion_payload.contains("completion_hash"));
    CHECK(completion_payload.contains("prompt_hash"));
    CHECK(completion_payload["prompt_hash"].get<std::uint64_t>() ==
          expected_prompt_hash);

    // completion_hash 一致性验证
    const auto expected_completion_hash =
        static_cast<std::uint64_t>(std::hash<std::string>{}(test_response));
    CHECK(completion_payload["completion_hash"].get<std::uint64_t>() ==
          expected_completion_hash);
  }

  bus->unsubscribe(sub_token);
}

// =====================================================================
// Test Case 2: ComplianceDecorator 不泄露原始 prompt 文本 (ADR-0031 §决策 7)
// =====================================================================

TEST_CASE("ComplianceDecorator does not leak raw prompt text",
          "[decorator][compliance]") {
  // Given: MockLLMProvider + InMemoryBus
  const std::string sensitive_prompt =
      "{\"api_key\": \"sk-live-12345\", \"secret\": \"super-secret\", "
      "\"user_id\": \"user-001\"}";
  const std::string test_response = "API key protected";
  auto mock_provider = std::make_unique<MockLLMProvider>();
  mock_provider->set_fixed_response(GenerationResult{
      .text = test_response,
      .prompt_tokens = 7,
      .completion_tokens = 3});

  auto bus = std::make_shared<InMemoryBus>();

  // 订阅 compliance.log 事件以捕获 payload
  std::mutex events_mutex;
  std::vector<std::string> captured_contents;
  size_t sub_token = bus->subscribe(
      "compliance.log",
      [&](const ToolResult& payload) {
        const auto content_str = payload.meta.value("content", std::string{});
        if (!content_str.empty()) {
          std::lock_guard<std::mutex> lock(events_mutex);
          captured_contents.push_back(content_str);
        }
      });

  auto decorator = std::make_unique<ComplianceDecorator>(
      std::move(mock_provider), bus);

  // When: 调用 generate
  GenerationRequest req;
  req.prompt = sensitive_prompt;
  req.params = LLMConfig{.model = "test-model-2"};
  const auto result = decorator->generate(req, {});
  REQUIRE(result.has_value());

  // Then: 等待异步 dispatch 完成
  bus->wait_for_drain();
  wait_until([&] {
    std::lock_guard<std::mutex> lock(events_mutex);
    return captured_contents.size() >= 2;
  });

  // Then: 所有 compliance.log events payload 不包含原始 prompt 或 completion
  {
    std::lock_guard<std::mutex> lock(events_mutex);
    CHECK(captured_contents.size() >= 2);
    for (const auto& content : captured_contents) {
      // 拒绝出现任何敏感字符串 (per ADR-0031 §决策 7)
      CHECK(content.find("sk-live-12345") == std::string::npos);
      CHECK(content.find("super-secret") == std::string::npos);
      CHECK(content.find("user-001") == std::string::npos);
      // 不包含原始 prompt 整串
      CHECK(content.find(sensitive_prompt) == std::string::npos);
      // 不包含原始 completion 文本
      CHECK(content.find(test_response) == std::string::npos);
    }
  }

  bus->unsubscribe(sub_token);
}

// =====================================================================
// Test Case 3: ComplianceDecorator 是 opt-in (默认禁用, REQ-IPD-003 Scenario 4)
// =====================================================================

TEST_CASE("ComplianceDecorator is opt-in default off",
          "[decorator][compliance]") {
  // Given: MockLLMProvider + InMemoryBus (未包装 ComplianceDecorator)
  auto mock_provider = std::make_unique<MockLLMProvider>();
  mock_provider->set_fixed_response("test");
  auto bus = std::make_shared<InMemoryBus>();

  // 订阅 compliance.log 事件
  std::atomic<int> compliance_count{0};
  size_t sub_token = bus->subscribe(
      "compliance.log",
      [&](const ToolResult&) {
        compliance_count.fetch_add(1, std::memory_order_relaxed);
      });

  // When: 直接使用 MockLLMProvider (无 ComplianceDecorator 包装)
  GenerationRequest req;
  req.prompt = "Just a test";
  req.params = LLMConfig{.model = "test-model-3"};
  const auto result = mock_provider->generate(req, {});
  REQUIRE(result.has_value());

  // Then: 无 compliance.log events (ComplianceDecorator 未生效)
  bus->wait_for_drain();
  CHECK(compliance_count.load() == 0);

  // Then: 包装 ComplianceDecorator 后, 应该有 compliance.log events
  auto decorator = std::make_unique<ComplianceDecorator>(
      std::move(mock_provider), bus);
  GenerationRequest req_with_deco;
  req_with_deco.prompt = "Enable this note";
  req_with_deco.params = LLMConfig{.model = "test-model-3"};
  const auto result2 = decorator->generate(req_with_deco, {});
  REQUIRE(result2.has_value());

  // 等待异步 dispatch 完成, 验证收到 compliance.log events
  bus->wait_for_drain();
  wait_until([&] {
    return compliance_count.load() >= 1;
  });
  CHECK(compliance_count.load() >= 1);

  bus->unsubscribe(sub_token);
}
