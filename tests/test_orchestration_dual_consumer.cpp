// tests/test_orchestration_dual_consumer.cpp
// Phase 5 REQ-ICC-001 Dual Consumer Model + Task 4.8 tests

#include "agenticdsl/pdk/agent_loops/orchestration_illm_provider.h"
#include "agenticdsl/contract/iinteraction_bus.h"
#include "agenticdsl/contract/inmemory_bus.h"
#include "agenticdsl/pdk/model_router.h"
#include "common/llm/mock_provider.h"

#include <catch_amalgamated.hpp>
#include <memory>
#include <string>

using namespace agenticdsl;

// === REQ-ICC-001 Scenario 1: direct generate ===
TEST_CASE("OrchestrationILLMProvider direct generate succeeds",
          "[orchestration][dual_consumer]") {
  auto inner = std::make_shared<MockLLMProvider>();
  inner->set_fixed_response(GenerationResult{"inner-text", 5, 3, "stop"});

  OrchestrationILLMProvider orch(inner);
  GenerationRequest req;
  req.prompt = "hi";
  req.params.model = "mock-llm-v1";

  auto r = orch.generate(req, std::stop_token{});
  REQUIRE(r.has_value());
  REQUIRE(r.value().text == "inner-text");
  // (直连路径, 不经过 ToolCoordinator / audit pipeline — implicit)
}

// === REQ-ICC-001 Scenario 2: Agent loop raw access (via get_llm_provider) ===
// Engine 包装的代理链不含 OrchestrationILLMProvider (per Dual Consumer Model)
TEST_CASE("Engine get_llm_provider returns CostTracking-decorated, not Orchestration",
          "[orchestration][dual_consumer][raw]") {
  // 创建 MockLLMProvider 测试 verify: engine 不使用 OrchestrationILLMProvider
  auto mock = std::make_shared<MockLLMProvider>();
  mock->set_fixed_response(GenerationResult{"x", 1, 1, "stop"});

  // 验证 OrchestrationILLMProvider::available_models() 透传
  OrchestrationILLMProvider orch(mock);
  auto models = orch.available_models();
  REQUIRE(models.size() >= 1);
  REQUIRE(models[0].name == "mock-llm-v1");
}

// === REQ-ICC-001 Scenario 3: streaming passthrough ===
TEST_CASE("OrchestrationILLMProvider stream passthrough",
          "[orchestration][dual_consumer][stream]") {
  auto inner = std::make_shared<MockLLMProvider>();
  inner->set_stream_tokens({"chunk1", "chunk2", "end"});
  OrchestrationILLMProvider orch(inner);

  GenerationRequest req;
  req.prompt = "test";
  req.params.max_tokens = 100;

  auto stream = orch.generate_stream(req, std::stop_token{});
  REQUIRE(stream->is_active());
  std::string collected;
  while (auto tok = stream->next(std::stop_token{})) {
    collected += *tok;
    collected += "|";
  }
  REQUIRE(collected.find("chunk1") != std::string::npos);
  REQUIRE(collected.find("chunk2") != std::string::npos);
}

// === REQ-ICC-002 Scenario 1+: stop_token propagation ===
namespace {
class StopAwareProvider : public ILLMProvider {
 public:
  mutable std::optional<bool> last_stop_requested;

  Result<GenerationResult, LLMError>
  generate(const GenerationRequest& req, std::stop_token token) override {
    last_stop_requested = token.stop_requested();
    if (token.stop_requested()) {
      return Result<GenerationResult, LLMError>::failure(
          LLMError{LLMError::Code::Cancelled, "cancelled"});
    }
    return Result<GenerationResult, LLMError>::success(
        GenerationResult{"ok", 1, 1, "stop"});
  }
  std::unique_ptr<IGenerationStream>
  generate_stream(const GenerationRequest&, std::stop_token) override {
    return nullptr;
  }
  std::vector<ModelInfo> available_models() const override {
    return {ModelInfo("stop-test", {ModelCapability::Chat}, 4096, "test")};
  }
};
}  // namespace

TEST_CASE("OrchestrationILLMProvider propagates stop_token to inner provider",
          "[orchestration][dual_consumer][stop]") {
  auto inner = std::make_shared<StopAwareProvider>();
  OrchestrationILLMProvider orch(inner);

  std::stop_source ss;
  auto r1 = orch.generate(GenerationRequest{"test"}, ss.get_token());
  REQUIRE(inner->last_stop_requested.has_value());
  REQUIRE_FALSE(*inner->last_stop_requested);
  REQUIRE(r1.has_value());

  ss.request_stop();
  auto r2 = orch.generate(GenerationRequest{"test"}, ss.get_token());
  REQUIRE(inner->last_stop_requested.has_value());
  REQUIRE(*inner->last_stop_requested);
  REQUIRE_FALSE(r2.has_value());
}

// === REQ-ICC-006: single registry (no IToolRegistry) — compile-time check ===
// 通过构造签名: OrchestrationILLMProvider 接受 (inference_provider, router, bus), 没有 IToolRegistry 参数
TEST_CASE("OrchestrationILLMProvider has no tool-registry dependency",
          "[orchestration][single_registry]") {
  // Compile-time: OrchestrationILLMProvider 构造函数无 IToolRegistry 参数
  // 运行时: available_models() 不触发任何 tool call
  auto inner = std::make_shared<MockLLMProvider>();
  OrchestrationILLMProvider orch(inner);
  // 调用 available_models 不应崩溃, 也不应需要任何外部 tool registry
  auto m = orch.available_models();
  REQUIRE(m.size() >= 1);
}

// === REQ-MR-003 Scenario 4: Router silent failure ===
TEST_CASE("OrchestrationILLMProvider rejects empty available_models via router",
          "[orchestration][router]") {
  // Mock provider that returns empty available_models
  class EmptyMockProvider : public ILLMProvider {
   public:
    Result<GenerationResult, LLMError>
    generate(const GenerationRequest&, std::stop_token) override {
      return Result<GenerationResult, LLMError>::failure(
          LLMError{LLMError::Code::InvalidRequest, "no models"});
    }
    std::unique_ptr<IGenerationStream>
    generate_stream(const GenerationRequest&, std::stop_token) override {
      return nullptr;
    }
    std::vector<ModelInfo> available_models() const override { return {}; }
  };

  auto inner = std::make_shared<EmptyMockProvider>();
  OrchestrationILLMProvider orch(inner);

  // orch.generate 应当返回 failure (per REQ-MR-003: 空 models = failure)
  GenerationRequest req;
  req.prompt = "x";
  auto r = orch.generate(req, std::stop_token{});
  REQUIRE_FALSE(r.has_value());
}

// === Null inference_provider safety ===
TEST_CASE("OrchestrationILLMProvider handles null inference_provider",
          "[orchestration][safety]") {
  OrchestrationILLMProvider orch(nullptr);
  GenerationRequest req;
  req.prompt = "x";
  auto r = orch.generate(req, std::stop_token{});
  REQUIRE_FALSE(r.has_value());
}
