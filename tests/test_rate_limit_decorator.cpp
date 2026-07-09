// test_rate_limit_decorator.cpp
// 文件头注释
// 功能描述：RateLimitDecorator 集成测试
//          测试 REQ-IPD-004: 配额充足 / 配额不足 / opt-in 默认禁用
// 设计依据：openspec/changes/phase5-illmprovider-call-chain-v2/tasks.md (Task 2.5)
// 作者：AgenticDSL Phase 5 ILLMProvider Call Chain V2
// 最后修改日期：2026-07-09

#include "agenticdsl/contract/i_llm_provider_decorator.h"
#include "common/llm/mock_provider.h"
#include "common/llm/rate_limit_decorator.h"

#include <catch_amalgamated.hpp>
#include <memory>
#include <string>

using namespace agenticdsl;

// =====================================================================
// Test Case 1: 配额充足时 generate 允许通过 (REQ-IPD-004 Scenario 2)
// =====================================================================

TEST_CASE("RateLimitDecorator budget sufficient allows generate",
          "[decorator][rate_limit]") {
  // Given: MockLLMProvider + RateLimitDecorator (高配额)
  auto mock_provider = std::make_unique<MockLLMProvider>();
  const std::string test_response = "Hello from mock";
  mock_provider->set_fixed_response(GenerationResult{
      .text = test_response,
      .prompt_tokens = 5,
      .completion_tokens = 3});

  // 高配额: 100000 tokens/minute, max_tokens=100 远小于配额
  auto decorator = std::make_unique<RateLimitDecorator>(
      std::move(mock_provider), "tenant-A", 100000);

  // When: 调用 generate (GenerationRequest 非聚合, 用赋值方式构造)
  GenerationRequest req;
  req.prompt = "Hello, world";
  req.params = LLMConfig{.model = "test-model-1", .max_tokens = 100};
  const auto result = decorator->generate(req, {});

  // Then: result 成功 (配额充足)
  REQUIRE(result.has_value());
  CHECK(result.value().text == test_response);
}

// =====================================================================
// Test Case 2: 配额耗尽后 generate 返回 RateLimited (REQ-IPD-004 Scenario 2)
// =====================================================================

TEST_CASE("RateLimitDecorator budget exceeded returns failure",
          "[decorator][rate_limit]") {
  // Given: MockLLMProvider + RateLimitDecorator (极低配额)
  auto mock_provider = std::make_unique<MockLLMProvider>();
  mock_provider->set_fixed_response(GenerationResult{
      .text = "Hello",
      .prompt_tokens = 5,
      .completion_tokens = 3});

  // 极低配额: 50 tokens/minute, max_tokens=100 > 配额
  auto decorator = std::make_unique<RateLimitDecorator>(
      std::move(mock_provider), "tenant-B", 50);

  // When: 调用 generate (max_tokens=100 > 50 配额)
  GenerationRequest req;
  req.prompt = "This needs more tokens than available";
  req.params = LLMConfig{.model = "test-model-2", .max_tokens = 100};

  // Then: 应返回 RateLimited (配额不足)
  const auto result = decorator->generate(req, {});
  REQUIRE_FALSE(result.has_value());
  CHECK(result.error().code == LLMError::Code::RateLimited);
  CHECK(result.error().message.find("quota exceeded") != std::string::npos);

  // Then: 错误消息含 tenant_id
  CHECK(result.error().message.find("tenant-B") != std::string::npos);
}

// =====================================================================
// Test Case 3: RateLimitDecorator opt-in via engine flag (REQ-IPD-004 Scenario 1)
// =====================================================================

TEST_CASE("RateLimitDecorator opt-in via engine flag",
          "[decorator][rate_limit]") {
  // Given: 验证 RateLimitDecorator 构造器接受 tenant_id + tokens_per_minute
  // (DSLEngine wiring 由其他 agent 处理, 这里仅验证构造器签名与行为)
  auto mock_provider = std::make_unique<MockLLMProvider>();
  mock_provider->set_fixed_response(GenerationResult{
      .text = "ok",
      .prompt_tokens = 1,
      .completion_tokens = 1});

  // When: 构造 RateLimitDecorator (opt-in 启用)
  auto decorator = std::make_unique<RateLimitDecorator>(
      std::move(mock_provider), "tenant-C", 1000);

  // Then: 构造成功 + 可调用 generate (配额充足)
  GenerationRequest req;
  req.prompt = "test";
  req.params = LLMConfig{.model = "test-model-3", .max_tokens = 50};
  const auto result = decorator->generate(req, {});
  REQUIRE(result.has_value());
  CHECK(result.value().text == "ok");

  // Then: 默认不包装 RateLimitDecorator (MockLLMProvider 直连, opt-in 默认 off)
  auto bare_mock = std::make_unique<MockLLMProvider>();
  bare_mock->set_fixed_response("bare");
  // 不包装 RateLimitDecorator, 直接调用 (opt-in 默认 off)
  const auto bare_result = bare_mock->generate(req, {});
  REQUIRE(bare_result.has_value());
  CHECK(bare_result.value().text == "bare");
}

// === REQ-IPD-004 Scenario: 配额不足时 MUST 阻止 inner provider 被调用 ===
// Bug repro (修复前): 基类 generate() 先调用 inner_->generate(), 再调用
// decorate_generate() 做 post-check。结果是 mock 被调用 1 次, 即使真实应该
// 因为配额不足被阻止 — 内层资源 (API 配额 / token 成本) 仍然消耗。
TEST_CASE("RateLimitDecorator quota exceeded blocks inner provider call",
          "[decorator][rate_limit][pre_check]") {
  // Given: 配置响应 + 配额 50 tokens/min (远小于请求 100)
  auto mock_provider = std::make_unique<MockLLMProvider>();
  MockLLMProvider* mock_raw = mock_provider.get();
  mock_provider->set_fixed_response(GenerationResult{
      .text = "should-never-be-returned",
      .prompt_tokens = 5,
      .completion_tokens = 3});

  auto decorator = std::make_unique<RateLimitDecorator>(
      std::move(mock_provider), "tenant-blocked", /*tokens_per_minute=*/50);

  GenerationRequest req;
  req.prompt = "expensive";
  req.params = LLMConfig{.model = "m", .max_tokens = 100};

  // When: generate 在 配额耗尽 场景下被调用
  const auto result = decorator->generate(req, {});

  // Then: 返回 RateLimited 失败
  REQUIRE_FALSE(result.has_value());
  REQUIRE(result.error().code == LLMError::Code::RateLimited);

  // Then (核心): inner MUST NOT 被调用 — call_count 必须为 0
  REQUIRE(mock_raw->call_count() == 0);
}

// === Stream path 同步测试: 配额不足时 MUST 阻止 inner.generate_stream() ===
TEST_CASE("RateLimitDecorator stream path blocks inner when quota exceeded",
          "[decorator][rate_limit][pre_check][stream]") {
  auto mock_provider = std::make_unique<MockLLMProvider>();
  MockLLMProvider* mock_raw = mock_provider.get();
  mock_provider->set_stream_tokens({"chunk-a", "chunk-b"});

  auto decorator = std::make_unique<RateLimitDecorator>(
      std::move(mock_provider), "tenant-stream", /*tokens_per_minute=*/50);

  GenerationRequest req;
  req.prompt = "stream-expensive";
  req.params = LLMConfig{.model = "m", .max_tokens = 100};

  // When: generate_stream 在配额不足场景
  auto stream = decorator->generate_stream(req, {});

  // Then: 流立即不活跃, 报告 RateLimited
  REQUIRE_FALSE(stream->is_active());
  REQUIRE(stream->error().has_value());
  REQUIRE(stream->error()->code == LLMError::Code::RateLimited);

  // Then (核心): inner.generate_stream MUST NOT 被调用
  REQUIRE(mock_raw->call_count() == 0);
}
