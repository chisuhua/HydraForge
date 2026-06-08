// tests/test_simple_orchestrator.cpp
// 文件头注释
// 功能描述：SimpleCognitiveOrchestrator 集成测试（5 个 TEST_CASE）
//          覆盖 mock 成功链路 / LLM 错误 / 工具不存在 / JSON 解析失败 / 端到端
// 设计依据：plan §11
// 作者：AgenticDSL Phase 0 / Track B
// 最后修改日期：2026-06-08

#include "catch_amalgamated.hpp"

#include "core/engine.h"
#include "core/types/tool_result.h"
#include "common/llm/mock_provider.h"
#include "common/llm/llm_types.h"
#include "modules/cognitive/simple_orchestrator.h"

#include <atomic>
#include <memory>
#include <string>

using namespace agenticdsl;

namespace {

const std::string kEmptyDsl = R"(
### AgenticDSL `/main`
```yaml
# --- BEGIN AgenticDSL ---
graph_type: subgraph
nodes:
  - id: start
    type: start
    next: ["/main/end"]
  - id: end
    type: end
# --- END AgenticDSL ---
```
)";

} // namespace

// === Test 1: Mock 成功链路 ===
TEST_CASE("SimpleCognitiveOrchestrator mock success chain",
          "[cognitive][stage0]") {
  auto engine = DSLEngine::from_markdown(kEmptyDsl);
  engine->register_tool("echo", [](const std::unordered_map<std::string, std::string>& args) {
    return nlohmann::json{{"echoed", args.at("message")}};
  });
  auto* mock = dynamic_cast<MockLLMProvider*>(engine->get_llm_provider());
  REQUIRE(mock != nullptr);
  mock->set_fixed_response(R"({"tool":"echo","args":{"message":"hi"}})");

  SimpleCognitiveOrchestrator orch(&engine->get_tool_registry(),
                                   engine->get_llm_provider());

  bool done = false;
  ToolResult captured;
  orch.process("s1", [&](ToolResult r) {
    captured = std::move(r);
    done = true;
  });
  REQUIRE(done);
  REQUIRE(captured.ok);
  REQUIRE(captured.data["echoed"] == "hi");
  REQUIRE(captured.meta["tool_name"] == "echo");
}

// === Test 2: LLM 错误注入 ===
TEST_CASE("SimpleCognitiveOrchestrator LLM error injection",
          "[cognitive][stage0]") {
  auto engine = DSLEngine::from_markdown(kEmptyDsl);
  engine->register_tool("echo", [](const std::unordered_map<std::string, std::string>&) {
    return nlohmann::json::object();
  });
  auto* mock = dynamic_cast<MockLLMProvider*>(engine->get_llm_provider());
  REQUIRE(mock != nullptr);
  mock->set_simulate_error(LLMError::Code::NetworkError, "connection refused");

  SimpleCognitiveOrchestrator orch(&engine->get_tool_registry(),
                                   engine->get_llm_provider());
  bool done = false;
  ToolResult captured;
  orch.process("s2", [&](ToolResult r) {
    captured = std::move(r);
    done = true;
  });
  REQUIRE(done);
  REQUIRE_FALSE(captured.ok);
  REQUIRE(captured.meta["error_code"] == "ERR_LLM.NETWORK");
  REQUIRE(captured.meta["error_message"] == "connection refused");
}

// === Test 3: 工具不存在 ===
TEST_CASE("SimpleCognitiveOrchestrator tool not found",
          "[cognitive][stage0]") {
  auto engine = DSLEngine::from_markdown(kEmptyDsl);
  // 注意：不注册 echo 工具
  auto* mock = dynamic_cast<MockLLMProvider*>(engine->get_llm_provider());
  REQUIRE(mock != nullptr);
  mock->set_fixed_response(R"({"tool":"nonexistent","args":{}})");

  SimpleCognitiveOrchestrator orch(&engine->get_tool_registry(),
                                   engine->get_llm_provider());
  bool done = false;
  ToolResult captured;
  orch.process("s3", [&](ToolResult r) {
    captured = std::move(r);
    done = true;
  });
  REQUIRE(done);
  REQUIRE_FALSE(captured.ok);
  REQUIRE(captured.meta["error_code"] == "ERR_TOOL.NOT_FOUND");
}

// === Test 4: JSON 解析失败 ===
TEST_CASE("SimpleCognitiveOrchestrator JSON parse failure",
          "[cognitive][stage0]") {
  auto engine = DSLEngine::from_markdown(kEmptyDsl);
  engine->register_tool("echo", [](const std::unordered_map<std::string, std::string>&) {
    return nlohmann::json::object();
  });
  auto* mock = dynamic_cast<MockLLMProvider*>(engine->get_llm_provider());
  REQUIRE(mock != nullptr);
  mock->set_fixed_response("not valid json {");

  SimpleCognitiveOrchestrator orch(&engine->get_tool_registry(),
                                   engine->get_llm_provider());
  bool done = false;
  ToolResult captured;
  orch.process("s4", [&](ToolResult r) {
    captured = std::move(r);
    done = true;
  });
  REQUIRE(done);
  REQUIRE_FALSE(captured.ok);
  REQUIRE(captured.meta["error_code"] == "ERR_ORCHESTRATOR.PARSE_FAILED");
}

// === Test 5: 端到端（重复 Test 1 但验证完整 JSON 序列化）===
TEST_CASE("SimpleCognitiveOrchestrator end-to-end JSON output",
          "[cognitive][stage0][e2e]") {
  auto engine = DSLEngine::from_markdown(kEmptyDsl);
  engine->register_tool("echo", [](const std::unordered_map<std::string, std::string>& args) {
    return nlohmann::json{{"echoed", args.at("message")}, {"len", 5}};
  });
  auto* mock = dynamic_cast<MockLLMProvider*>(engine->get_llm_provider());
  REQUIRE(mock != nullptr);
  mock->set_fixed_response(R"({"tool":"echo","args":{"message":"hello"}})");

  SimpleCognitiveOrchestrator orch(&engine->get_tool_registry(),
                                   engine->get_llm_provider());
  bool done = false;
  ToolResult captured;
  orch.process("s5", [&](ToolResult r) {
    captured = std::move(r);
    done = true;
  });
  REQUIRE(done);
  REQUIRE(captured.ok);

  // 验证 to_json() 输出符合 ToolResult 格式
  auto j = captured.to_json();
  REQUIRE(j["ok"] == true);
  REQUIRE(j["data"]["echoed"] == "hello");
  REQUIRE(j["data"]["len"] == 5);
  REQUIRE(j["meta"]["tool_name"] == "echo");
}
