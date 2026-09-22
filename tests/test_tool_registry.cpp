#include "catch_amalgamated.hpp"
#include "common/tools/registry.h"
#include "common/llm/llm_tool.h"

#include <memory>
#include <nlohmann/json.hpp>

using namespace agenticdsl;
using nlohmann::json;

// Mock LLM tool for testing
class MockLLMTool : public ILLMTool {
public:
    MockLLMTool(const std::string& tool_name) : tool_name_(tool_name) {}

    LLMResult generate(const std::string& prompt, const LLMParams& params = {}) override {
        LLMResult result;
        result.success = true;
        result.text = "Mock response for: " + prompt;
        result.tokens_generated = 10;
        return result;
    }

    bool is_available() const override { return true; }
    std::string name() const override { return tool_name_; }

private:
    std::string tool_name_;
};

TEST_CASE("ToolRegistry can register LLM tool", "[tool_registry][llm_tool]") {
    ToolRegistry registry;
    
    auto llm_tool = std::make_unique<MockLLMTool>("test_llm");
    LLMParams params;
    params.temperature = 0.5f;
    params.max_tokens = 256;
    
    registry.register_llm_tool("test_llm", std::move(llm_tool), params);
    
    REQUIRE(registry.is_llm_tool("test_llm") == true);
}

TEST_CASE("ToolRegistry can check if tool is not LLM tool", "[tool_registry][llm_tool]") {
    ToolRegistry registry;
    
    REQUIRE(registry.is_llm_tool("web_search") == false);
    REQUIRE(registry.is_llm_tool("nonexistent") == false);
}

TEST_CASE("ToolRegistry can get LLM params", "[tool_registry][llm_tool]") {
    ToolRegistry registry;
    
    auto llm_tool = std::make_unique<MockLLMTool>("test_llm");
    LLMParams params;
    params.temperature = 0.5f;
    params.max_tokens = 256;
    params.model = "llama-2";
    
    registry.register_llm_tool("test_llm", std::move(llm_tool), params);
    
    const auto& retrieved_params = registry.get_llm_params("test_llm");
    REQUIRE(retrieved_params.temperature == 0.5f);
    REQUIRE(retrieved_params.max_tokens == 256);
    REQUIRE(retrieved_params.model == "llama-2");
}

TEST_CASE("ToolRegistry can call LLM tool", "[tool_registry][llm_tool]") {
    ToolRegistry registry;
    
    auto llm_tool = std::make_unique<MockLLMTool>("test_llm");
    registry.register_llm_tool("test_llm", std::move(llm_tool), LLMParams{});
    
    auto result = registry.call_llm_tool("test_llm", "Hello world", LLMParams{});
    
    REQUIRE(result.contains("success") == true);
    REQUIRE(result["success"] == true);
    REQUIRE(result.contains("text") == true);
}

TEST_CASE("ToolRegistry returns error for non-existent LLM tool", "[tool_registry][llm_tool]") {
    ToolRegistry registry;
    
    auto result = registry.call_llm_tool("nonexistent", "prompt", LLMParams{});
    
    REQUIRE(result.contains("error") == true);
}

TEST_CASE("ToolRegistry list_tools includes LLM tools", "[tool_registry][llm_tool]") {
    ToolRegistry registry;
    
    auto llm_tool = std::make_unique<MockLLMTool>("test_llm");
    registry.register_llm_tool("test_llm", std::move(llm_tool), LLMParams{});
    
    auto tools = registry.list_tools();
    
    bool found = false;
    for (const auto& t : tools) {
        if (t == "test_llm") {
            found = true;
            break;
        }
    }
    REQUIRE(found == true);
}

TEST_CASE("ToolRegistry has_tool works for LLM tools", "[tool_registry][llm_tool]") {
    ToolRegistry registry;
    
    auto llm_tool = std::make_unique<MockLLMTool>("test_llm");
    registry.register_llm_tool("test_llm", std::move(llm_tool), LLMParams{});
    
    REQUIRE(registry.has_tool("test_llm") == true);
    REQUIRE(registry.has_tool("nonexistent") == false);
}

TEST_CASE("ToolRegistry get_llm_params throws for non-LLM tool", "[tool_registry][llm_tool]") {
    ToolRegistry registry;

    REQUIRE_THROWS_AS(registry.get_llm_params("web_search"), std::runtime_error);
}

// ============================================================================
// R3 scenario 1 (Oracle bg_8237a316 SHIP-with-fixes Major #2):
// Spec R3 "并发 register + unregister 无数据竞争 (TSan 干净)"
// 2 线程并发不同工具名 register/unregister, 写-写通过 mutation_mutex_ 串行化.
// ============================================================================
#include <atomic>
#include <thread>

TEST_CASE("ToolRegistry concurrent register+unregister write-write safe",
          "[tool_registry][concurrency][mutex][oracle-ship-with-fixes]") {
    ToolRegistry registry;
    // Pre-register 2 个工具: a (后续不操作), c (被 t2 unregister)
    struct DummyFn {
        json operator()(const std::unordered_map<std::string, std::string>&) const {
            return json{{"ok", true}};
        }
    };
    ToolMetadata dummy_meta{};
    registry.register_tool("a", dummy_meta, DummyFn{});
    registry.register_tool("c", dummy_meta, DummyFn{});
    REQUIRE(registry.has_tool("a"));
    REQUIRE(registry.has_tool("c"));

    // 启动屏障: 双线程等到 barrier 后并发执行
    std::atomic<bool> start{false};
    std::atomic<int> done_count{0};
    std::atomic<bool> register_ok{false};

    std::thread t_register([&]() {
        while (!start.load(std::memory_order_acquire)) { /* spin wait */ }
        struct NewTool {
            json operator()(const std::unordered_map<std::string, std::string>&) const {
                return json{{"new", true}};
            }
        };
        registry.register_tool("b", dummy_meta, NewTool{});
        register_ok.store(true, std::memory_order_release);
        done_count.fetch_add(1, std::memory_order_release);
    });

    std::thread t_unregister([&]() {
        while (!start.load(std::memory_order_acquire)) { /* spin wait */ }
        // unregister 'c' — 不同 tool_name (与 register 路径不冲突)
        registry.unregister_tool_function("c");
        done_count.fetch_add(1, std::memory_order_release);
    });

    // 同步释放
    start.store(true, std::memory_order_release);
    t_register.join();
    t_unregister.join();

    // 终态断言
    REQUIRE(done_count.load() == 2);
    REQUIRE(register_ok.load());
    REQUIRE(registry.has_tool("a"));
    REQUIRE(registry.has_tool("b"));
    REQUIRE_FALSE(registry.has_tool("c"));
    // TSan cleanliness 在 tsan preset 下验证 (per spec R3 scenario 1)
}
