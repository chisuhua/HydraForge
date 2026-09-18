// tests/test_dsl_engine_ctx_bridge.cpp
// F1 fix-react-decide-empty-response — DSL node ctx bridge 空响应诊断与回归守卫
// Oracle ses_f4d05cdb0ffe0BhMdEADyfsdTz 纠正根因: flatten_layers 不在 react.agent.md 执行路径上,
// 真实根因是 think 节点 (llm_call) 写入 output_key 的值为空字符串, 且链路缺少空值校验.
// 本文件 5 个 cases 验证 (Cases 1-3 inja 行为文档 + Case 4-5 真 NodeExecutor 级 GREEN guard):

#include "catch_amalgamated.hpp"

#include "agenticdsl/contract/iinteraction_bus.h"
#include "agenticdsl/contract/inmemory_bus.h"
#include "common/tools/registry.h"
#include "common/llm/llm_tool.h"
#include "common/llm/llm_types.h"
#include "core/types/context.h"
#include "core/types/node.h"
#include "core/types/tool_result.h"
#include "modules/executor/node_executor.h"

#include <nlohmann/json.hpp>
#include <inja/inja.hpp>
#include <memory>
#include <stdexcept>
#include <string>

using namespace nlohmann;
using agenticdsl::Context;
using agenticdsl::DSLNode;
using agenticdsl::ILLMTool;
using agenticdsl::LLMParams;
using agenticdsl::LLMResult;
using agenticdsl::NodeExecutor;
using agenticdsl::ToolRegistry;

namespace {

// MockLLMEmptyTool: 实现 ILLMTool, generate 返回空 text 但 success=true.
// 模拟真实 LLM provider 因配置错误或模型遮蔽返回空响应.
class MockLLMEmptyTool : public ILLMTool {
 public:
    explicit MockLLMEmptyTool(std::string name) : name_(std::move(name)) {}
    LLMResult generate(const std::string& /*prompt*/,
                       const LLMParams& /*params*/) override {
        LLMResult r;
        r.success = true;
        r.text = "";  // 关键: 空 text 模拟 F1 bug 场景
        r.tokens_generated = 0;
        return r;
    }
    bool is_available() const override { return true; }
    std::string name() const override { return name_; }
 private:
    std::string name_;
};

// MockLLMNonEmptyTool: 对照组 — 返回非空 text, 应不抛错.
class MockLLMNonEmptyTool : public ILLMTool {
 public:
    explicit MockLLMNonEmptyTool(std::string name, std::string fixed)
        : name_(std::move(name)), fixed_(std::move(fixed)) {}
    LLMResult generate(const std::string& /*prompt*/,
                       const LLMParams& /*params*/) override {
        LLMResult r;
        r.success = true;
        r.text = fixed_;
        r.tokens_generated = 1;
        return r;
    }
    bool is_available() const override { return true; }
    std::string name() const override { return name_; }
 private:
    std::string name_;
    std::string fixed_;
};

}  // namespace

TEST_CASE("Flat ctx with empty llm_response: inja renders empty string (not throw)",
          "[ctx_bridge][f1]") {
    json flat = json::object();
    flat["llm_response"] = "";
    flat["user_input"] = "hello";
    flat["history"] = "";

    inja::Environment env;

    SECTION("{{llm_response}} renders empty string") {
        std::string result;
        REQUIRE_NOTHROW(result = env.render("{{llm_response}}", flat));
        REQUIRE(result == "");
        REQUIRE(result.empty());
    }

    SECTION("{{nonexistent_key}} throws on missing key") {
        std::string result;
        REQUIRE_THROWS_AS(env.render("{{nonexistent_key}}", flat), inja::RenderError);
    }
}

TEST_CASE("Mock LLM empty text: think node writes empty llm_response without validation",
          "[ctx_bridge][f1]") {
    json flat;
    flat["llm_response"] = "";

    SECTION("decide args with empty llm_response: safe but useless") {
        inja::Environment env;
        std::string rendered;
        REQUIRE_NOTHROW(rendered = env.render("{{llm_response}}", flat));
        REQUIRE(rendered.empty());
    }
}

TEST_CASE("ctx.contains(key) short-circuit skip on empty string",
          "[ctx_bridge][f1][regression]") {
    json flat;
    flat["llm_response"] = "";
    flat["user_input"] = "test";

    SECTION("existing empty key: passes contains check, value is empty") {
        REQUIRE(flat.contains("llm_response"));
        REQUIRE(flat["llm_response"].is_string());
        REQUIRE(flat["llm_response"].get<std::string>().empty());
    }
}

TEST_CASE("NodeExecutor llm_call with empty text throws descriptive error (F1 fix)",
          "[ctx_bridge][f1][green]") {
    ToolRegistry registry;
    registry.register_llm_tool("mock_empty",
                              std::make_unique<MockLLMEmptyTool>("mock_empty"),
                              LLMParams{});

    auto bus = std::make_shared<agenticdsl::InMemoryBus>();
    NodeExecutor executor(registry, nullptr, bus.get());

    DSLNode node(
        "/main/think",
        "User: {{user_input}}",
        "mock_empty",
        LLMParams{},
        {"llm_response"},
        {"/main/end"});

    Context ctx;
    ctx["user_input"] = "hello";

    SECTION("Empty LLM text triggers F1 validation throw") {
        REQUIRE_THROWS_WITH(
            executor.execute_node(&node, ctx),
            Catch::Matchers::ContainsSubstring("empty text") &&
            Catch::Matchers::ContainsSubstring("llm_response"));
    }
}

TEST_CASE("NodeExecutor llm_call with non-empty text passes through (F1 fix regression)",
          "[ctx_bridge][f1][green]") {
    ToolRegistry registry;
    registry.register_llm_tool("mock_ok",
                              std::make_unique<MockLLMNonEmptyTool>("mock_ok",
                                                                    "Hello from LLM"),
                              LLMParams{});

    auto bus = std::make_shared<agenticdsl::InMemoryBus>();
    NodeExecutor executor(registry, nullptr, bus.get());

    DSLNode node(
        "/main/think",
        "User: {{user_input}}",
        "mock_ok",
        LLMParams{},
        {"llm_response"},
        {"/main/end"});

    Context ctx;
    ctx["user_input"] = "hello";

    SECTION("Non-empty LLM text passes validation, written to ctx") {
        Context result;
        REQUIRE_NOTHROW(result = executor.execute_node(&node, ctx));
        REQUIRE(result.contains("llm_response"));
        REQUIRE(result["llm_response"] == "Hello from LLM");
    }
}