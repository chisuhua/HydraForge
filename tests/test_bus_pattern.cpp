// tests/test_bus_pattern.cpp
// 文件头注释
// 功能描述：Bus Pattern 单元测试 (ADR-0085 V1)。
//          2 个 TEST_CASE 覆盖:
//            1. bus_apply_subscribe_topic_pattern
//            2. bus_apply_multiple_subscriptions
// 设计依据：openspec/changes/pdk-cross-cutting-patterns (ADR-0085)
// 作者：AgenticDSL Phase 1
// 最后修改日期：2026-08-28

#include "catch_amalgamated.hpp"

#include "agenticdsl/pdk/cross_cutting/bus_pattern.h"
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

TEST_CASE("BusPattern subscribes to topic pattern",
          "[pdk][cross_cutting][bus]") {
    MockAgentRegistry agent_registry;
    MockToolHookRegistry tool_hook_registry;
    MockAgentHookRegistry agent_hook_registry;
    MockInteractionBus bus;
    MockApprovalHandler approval_handler;

    CrossCuttingContext ctx{
        &agent_registry,
        &agent_hook_registry,
        &tool_hook_registry,
        &bus,
        [](std::unique_ptr<agenticdsl::ILLMProvider>) {},
        &approval_handler
    };

    BusPattern pattern;

    // 配置：订阅 topic
    nlohmann::json config;
    config["subscriptions"] = nlohmann::json::array();
    config["subscriptions"].push_back("mutation.committed");

    // 应用 pattern
    pattern.apply(config, ctx);

    // 验证：没有抛异常
    SUCCEED();
}

TEST_CASE("BusPattern handles multiple subscriptions",
          "[pdk][cross_cutting][bus]") {
    MockAgentRegistry agent_registry;
    MockToolHookRegistry tool_hook_registry;
    MockAgentHookRegistry agent_hook_registry;
    MockInteractionBus bus;
    MockApprovalHandler approval_handler;

    CrossCuttingContext ctx{
        &agent_registry,
        &agent_hook_registry,
        &tool_hook_registry,
        &bus,
        [](std::unique_ptr<agenticdsl::ILLMProvider>) {},
        &approval_handler
    };

    BusPattern pattern;

    // 配置：多个订阅
    nlohmann::json config;
    config["subscriptions"] = nlohmann::json::array();
    config["subscriptions"].push_back("mutation.committed");
    config["subscriptions"].push_back("tool.completed");
    config["subscriptions"].push_back("session.persisted");

    // 应用 pattern
    pattern.apply(config, ctx);

    // 验证：没有抛异常
    SUCCEED();
}
