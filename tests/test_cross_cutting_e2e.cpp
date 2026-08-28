// tests/test_cross_cutting_e2e.cpp
// 文件头注释
// 功能描述：Cross-Cutting E2E 测试 (ADR-0085 V1)。
//          2 个 TEST_CASE 覆盖:
//            1. e2e_high_security_mode_full_pipeline
//            2. e2e_cost_optimization_mode_minimal
// 设计依据：openspec/changes/pdk-cross-cutting-patterns (ADR-0085)
// 作者：AgenticDSL Phase 3
// 最后修改日期：2026-08-28

#include "catch_amalgamated.hpp"

#include "agenticdsl/pdk/cross_cutting/cross_cutting_orchestrator.h"
#include "agenticdsl/pdk/cross_cutting/cross_cutting_config.h"

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

TEST_CASE("E2E high security mode full pipeline",
          "[pdk][cross_cutting][e2e]") {
    MockAgentRegistry agent_registry;
    MockToolHookRegistry tool_hook_registry;
    MockAgentHookRegistry agent_hook_registry;
    MockInteractionBus bus;
    MockApprovalHandler approval_handler;

    std::unique_ptr<agenticdsl::ILLMProvider> captured_provider;
    auto set_llm_provider = [&](std::unique_ptr<agenticdsl::ILLMProvider> provider) {
        captured_provider = std::move(provider);
    };

    CrossCuttingOrchestrator orchestrator(
        agent_registry, agent_hook_registry, tool_hook_registry, bus,
        set_llm_provider, &approval_handler);

    // 创建配置
    nlohmann::json config;
    config["patterns"] = nlohmann::json::array();

    // Pattern 1: decorator
    nlohmann::json decorator_config;
    decorator_config["type"] = "decorator-v1";
    decorator_config["config"] = nlohmann::json::object();
    decorator_config["config"]["decorators"] = nlohmann::json::array();
    decorator_config["config"]["decorators"].push_back("CostTracking");
    config["patterns"].push_back(decorator_config);

    // Pattern 2: hook
    nlohmann::json hook_config;
    hook_config["type"] = "hook-v1";
    hook_config["config"] = nlohmann::json::object();
    hook_config["config"]["hooks"] = nlohmann::json::array();
    nlohmann::json hook_item;
    hook_item["target"] = "tool";
    hook_item["glob"] = "*";
    hook_item["type"] = "pre";
    hook_item["priority"] = 1000;
    hook_item["policy"] = "FailClosed";
    hook_config["config"]["hooks"].push_back(hook_item);
    config["patterns"].push_back(hook_config);

    // Pattern 3: composition
    nlohmann::json composition_config;
    composition_config["type"] = "composition-v1";
    composition_config["config"] = nlohmann::json::object();
    composition_config["config"]["agents"] = nlohmann::json::array();
    nlohmann::json agent_item;
    agent_item["name"] = "privacy-policy-v1";
    agent_item["scope"] = "react-loop/*";
    composition_config["config"]["agents"].push_back(agent_item);
    config["patterns"].push_back(composition_config);

    // Pattern 4: bus
    nlohmann::json bus_config;
    bus_config["type"] = "bus-v1";
    bus_config["config"] = nlohmann::json::object();
    bus_config["config"]["subscriptions"] = nlohmann::json::array();
    bus_config["config"]["subscriptions"].push_back("mutation.committed");
    config["patterns"].push_back(bus_config);

    // Dispatch
    orchestrator.dispatch(config);

    // 验证
    REQUIRE(captured_provider != nullptr);
}

TEST_CASE("E2E cost optimization mode minimal",
          "[pdk][cross_cutting][e2e]") {
    MockAgentRegistry agent_registry;
    MockToolHookRegistry tool_hook_registry;
    MockAgentHookRegistry agent_hook_registry;
    MockInteractionBus bus;
    MockApprovalHandler approval_handler;

    std::unique_ptr<agenticdsl::ILLMProvider> captured_provider;
    auto set_llm_provider = [&](std::unique_ptr<agenticdsl::ILLMProvider> provider) {
        captured_provider = std::move(provider);
    };

    CrossCuttingOrchestrator orchestrator(
        agent_registry, agent_hook_registry, tool_hook_registry, bus,
        set_llm_provider, &approval_handler);

    // 创建配置
    nlohmann::json config;
    config["patterns"] = nlohmann::json::array();

    // Pattern 1: decorator
    nlohmann::json decorator_config;
    decorator_config["type"] = "decorator-v1";
    decorator_config["config"] = nlohmann::json::object();
    decorator_config["config"]["decorators"] = nlohmann::json::array();
    decorator_config["config"]["decorators"].push_back("CostTracking");
    config["patterns"].push_back(decorator_config);

    // Pattern 2: bus
    nlohmann::json bus_config;
    bus_config["type"] = "bus-v1";
    bus_config["config"] = nlohmann::json::object();
    bus_config["config"]["subscriptions"] = nlohmann::json::array();
    bus_config["config"]["subscriptions"].push_back("tool.completed");
    bus_config["config"]["subscriptions"].push_back("llm.response");
    config["patterns"].push_back(bus_config);

    // Dispatch
    orchestrator.dispatch(config);

    // 验证
    REQUIRE(captured_provider != nullptr);
}
