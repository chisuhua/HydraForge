// tests/test_decorator_pattern.cpp
// 文件头注释
// 功能描述：Decorator Pattern 单元测试 (ADR-0085 V1)。
//          2 个 TEST_CASE 覆盖:
//            1. decorator_apply_cost_tracking
//            2. decorator_apply_chain_depth_limit
// 设计依据：openspec/changes/pdk-cross-cutting-patterns (ADR-0085)
// 作者：AgenticDSL Phase 1
// 最后修改日期：2026-08-28

#include "catch_amalgamated.hpp"

#include "agenticdsl/pdk/cross_cutting/decorator_pattern.h"
#include "agenticdsl/pdk/cross_cutting/icross_cutting_pattern.h"

#include "agenticdsl/contract/iagent_registry.h"
#include "agenticdsl/contract/itool_hook_registry.h"
#include "agenticdsl/contract/iagent_hook_registry.h"
#include "agenticdsl/contract/iinteraction_bus.h"
#include "agenticdsl/contract/i_llm_provider_decorator.h"
#include "agenticdsl/policy/iapproval_handler.h"

#include <nlohmann/json.hpp>
#include <memory>

using namespace hydraforge::pdk;

// =====================================================================
// Mock 类 (简化实现，仅用于测试)
// =====================================================================

class MockILLMProvider : public agenticdsl::ILLMProvider {
public:
    agenticdsl::Result<agenticdsl::GenerationResult, agenticdsl::LLMError>
    generate(const agenticdsl::GenerationRequest&, std::stop_token) override {
        return agenticdsl::Result<agenticdsl::GenerationResult, agenticdsl::LLMError>::success(agenticdsl::GenerationResult{});
    }
    std::unique_ptr<agenticdsl::IGenerationStream> generate_stream(const agenticdsl::GenerationRequest&, std::stop_token) override {
        return nullptr;
    }
    std::vector<agenticdsl::ILLMProvider::ModelInfo> available_models() const override {
        return {};
    }
};

class MockAgentRegistry : public agenticdsl::IAgentRegistry {
public:
    bool register_agent(const std::string&, agenticdsl::AgentFactory) override { return true; }
    std::unique_ptr<agenticdsl::IAgent> create(const std::string&, const agenticdsl::AgentConfig&) override { return nullptr; }
    bool unregister(const std::string&) override { return true; }
    std::vector<std::string> list_registered() const override { return {}; }
    bool is_registered(const std::string&) const override { return false; }
    size_t size() const override { return 0; }
};

class MockToolHookRegistry : public agenticdsl::IToolHookRegistry {
public:
    void register_pre_hook(const std::string&, agenticdsl::PreHook, int, agenticdsl::HookErrorPolicy) override {}
    void register_post_hook(const std::string&, agenticdsl::PostHook, int, agenticdsl::HookErrorPolicy) override {}
    agenticdsl::PreHookResult apply_pre_hooks(const agenticdsl::ToolMetadata&, const agenticdsl::ToolCallContext&,
        const std::unordered_map<std::string, std::string>&, std::vector<std::string>&) const override {
        agenticdsl::PreHookResult result;
        result.action = agenticdsl::PreHookResult::Continue;
        return result;
    }
    agenticdsl::ToolResult apply_post_hooks(const agenticdsl::ToolMetadata&, const agenticdsl::ToolCallContext&,
        agenticdsl::ToolResult result, std::vector<std::string>&) const override {
        return result;
    }
};

class MockAgentHookRegistry : public agenticdsl::IAgentHookRegistry {
public:
    void register_pre_hook(const std::string&, agenticdsl::AgentPreHook, int, agenticdsl::HookErrorPolicy) override {}
    void register_post_hook(const std::string&, agenticdsl::AgentPostHook, int, agenticdsl::HookErrorPolicy) override {}
    agenticdsl::AgentPreHookResult apply_pre_hooks(const agenticdsl::IAgent&, const std::string&,
        std::vector<std::string>&) const override {
        agenticdsl::AgentPreHookResult result;
        result.action = agenticdsl::AgentPreHookResult::Continue;
        return result;
    }
    agenticdsl::AgentPostHookResult apply_post_hooks(const agenticdsl::IAgent&, const std::string&,
        std::vector<std::string>&) const override {
        agenticdsl::AgentPostHookResult result;
        result.modify_result = false;
        return result;
    }
};

class MockInteractionBus : public agenticdsl::IInteractionBus {
public:
    void emit(const agenticdsl::BusEvent&) override {}
    void emit(const std::string&, const std::string&) override {}
    size_t subscribe(const std::string&, std::function<void(const agenticdsl::BusEvent&)>) override { return 0; }
    void unsubscribe(size_t) override {}
};

class MockApprovalHandler : public agenticdsl::IApprovalHandler {
public:
    bool process_request(const agenticdsl::ToolMetadata&, const agenticdsl::ToolCallContext&, const agenticdsl::ToolPreview&) override {
        return true;
    }
};

// =====================================================================
// 测试用例
// =====================================================================

TEST_CASE("DecoratorPattern applies cost tracking decorator",
          "[pdk][cross_cutting][decorator]") {
    MockAgentRegistry agent_registry;
    MockToolHookRegistry tool_hook_registry;
    MockAgentHookRegistry agent_hook_registry;
    MockInteractionBus bus;
    MockApprovalHandler approval_handler;

    // set_llm_provider 回调：捕获设置的 provider
    std::unique_ptr<agenticdsl::ILLMProvider> captured_provider;
    auto set_llm_provider = [&](std::unique_ptr<agenticdsl::ILLMProvider> provider) {
        captured_provider = std::move(provider);
    };

    CrossCuttingContext ctx{
        &agent_registry,
        &agent_hook_registry,
        &tool_hook_registry,
        &bus,
        set_llm_provider,
        &approval_handler
    };

    // 创建 DecoratorPattern
    DecoratorPattern pattern;

    // 配置：使用 CostTracking decorator
    nlohmann::json config;
    config["decorators"] = nlohmann::json::array();
    config["decorators"].push_back("CostTracking");

    // 应用 pattern
    pattern.apply(config, ctx);

    // 验证：set_llm_provider 被调用，captured_provider 不为空
    REQUIRE(captured_provider != nullptr);
}

TEST_CASE("DecoratorPattern enforces chain depth limit",
          "[pdk][cross_cutting][decorator]") {
    MockAgentRegistry agent_registry;
    MockToolHookRegistry tool_hook_registry;
    MockAgentHookRegistry agent_hook_registry;
    MockInteractionBus bus;
    MockApprovalHandler approval_handler;

    std::unique_ptr<agenticdsl::ILLMProvider> captured_provider;
    auto set_llm_provider = [&](std::unique_ptr<agenticdsl::ILLMProvider> provider) {
        captured_provider = std::move(provider);
    };

    CrossCuttingContext ctx{
        &agent_registry,
        &agent_hook_registry,
        &tool_hook_registry,
        &bus,
        set_llm_provider,
        &approval_handler
    };

    DecoratorPattern pattern;

    // 配置：使用超过 4 个 decorators (链深限制)
    nlohmann::json config;
    config["decorators"] = nlohmann::json::array();
    for (int i = 0; i < 5; ++i) {
        config["decorators"].push_back("CostTracking");
    }

    // 应用 pattern 应该抛出 DecoratorChainTooDeep 异常
    REQUIRE_THROWS_AS(pattern.apply(config, ctx), agenticdsl::ILLMProviderDecorator::DecoratorChainTooDeep);
}
