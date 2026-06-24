// tests/test_engine_factory.cpp
// 验证 DSLEngine 默认/自定义/依赖注入构造路径
// 覆盖 P2.A 删除 engine.cpp 工厂后的直接构造路径

#include "catch_amalgamated.hpp"
#include "core/engine.h"
#include "common/llm/mock_provider.h"
#include "agenticdsl/contract/inmemory_bus.h"

using namespace agenticdsl;

static const char* kMinimalMain = R"(
### AgenticDSL `/main`
```yaml
# --- BEGIN AgenticDSL ---
graph_type: subgraph
entry: start
nodes:
  - id: start
    type: assign
    assign:
      ok: true
    next: /main/end
  - id: end
    type: end
    termination_mode: hard
# --- END AgenticDSL ---
```
)";

TEST_CASE("test_engine_create_with_default_config", "[engine][factory][default]") {
    auto engine = DSLEngine::from_markdown(kMinimalMain);
    REQUIRE(engine != nullptr);

    auto result = engine->run();
    REQUIRE(result.success);
    REQUIRE(result.final_context.contains("ok"));
    // AssignNode 通过 inja 模板渲染写入, 所有值被强制为字符串
    // (与 node_factory.cpp make_assign 行为一致: bool → "true"/"false")
    REQUIRE(result.final_context["ok"] == "true");
}

TEST_CASE("test_engine_create_with_custom_config", "[engine][factory][custom]") {
    auto engine = DSLEngine::from_markdown(kMinimalMain);
    REQUIRE(engine != nullptr);

    // 自定义配置：注入一个固定响应的 MockLLMProvider
    auto mock = std::make_unique<MockLLMProvider>();
    mock->set_fixed_response("hello");
    engine->set_llm_provider(std::move(mock));

    REQUIRE(engine->get_llm_provider() != nullptr);
}

TEST_CASE("test_engine_create_with_dependencies", "[engine][factory][di]") {
    auto engine = DSLEngine::from_markdown(kMinimalMain);
    REQUIRE(engine != nullptr);

    // 依赖注入 1：自定义 LLM provider
    auto mock = std::make_unique<MockLLMProvider>();
    mock->set_fixed_response("mocked");
    engine->set_llm_provider(std::move(mock));
    REQUIRE(engine->get_llm_provider() != nullptr);

    // 依赖注入 2：交互总线
    auto bus = std::make_shared<InMemoryBus>();
    engine->set_interaction_bus(bus);
    REQUIRE(engine->get_interaction_bus() == bus);

    // 依赖注入 3：通过 get_tool_registry() 注册工具（IToolRegistry 抽象接口）
    engine->get_tool_registry().register_tool_function(
        "echo",
        [](const std::unordered_map<std::string, std::string>& args) -> nlohmann::json {
            auto it = args.find("value");
            return nlohmann::json{{"value", it != args.end() ? it->second : "empty"}};
        });
    REQUIRE(engine->get_tool_registry().has_tool("echo"));

    // 预算控制器可用（由 BudgetController 工厂创建）
    REQUIRE(engine->get_session_cost() == 0.0);
}
