// tests/test_hook_pattern.cpp
// 文件头注释
// 功能描述：Hook Pattern 单元测试 (ADR-0085 V1)。
//          3 个 TEST_CASE 覆盖:
//            1. hook_apply_tool_pre_global
//            2. hook_apply_agent_pre_scoped
//            3. hook_apply_approval_l4
// 设计依据：openspec/changes/pdk-cross-cutting-patterns (ADR-0085)
// 作者：AgenticDSL Phase 1
// 最后修改日期：2026-08-28

#include "catch_amalgamated.hpp"

#include "agenticdsl/pdk/cross_cutting/hook_pattern.h"
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

TEST_CASE("HookPattern applies tool pre-hook globally",
          "[pdk][cross_cutting][hook]") {
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

    HookPattern pattern;

    // 配置：tool pre-hook 全局
    nlohmann::json config;
    config["hooks"] = nlohmann::json::array();
    nlohmann::json hook_config;
    hook_config["target"] = "tool";
    hook_config["glob"] = "*";
    hook_config["type"] = "pre";
    hook_config["priority"] = 1000;
    hook_config["policy"] = "FailClosed";
    config["hooks"].push_back(hook_config);

    // 应用 pattern
    pattern.apply(config, ctx);

    // 验证：没有抛异常
    SUCCEED();
}

TEST_CASE("HookPattern applies agent pre-hook scoped",
          "[pdk][cross_cutting][hook]") {
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

    HookPattern pattern;

    // 配置：agent pre-hook scoped
    nlohmann::json config;
    config["hooks"] = nlohmann::json::array();
    nlohmann::json hook_config;
    hook_config["target"] = "agent";
    hook_config["glob"] = "react-loop/*";
    hook_config["type"] = "pre";
    hook_config["priority"] = 500;
    hook_config["policy"] = "FailOpen";
    config["hooks"].push_back(hook_config);

    // 应用 pattern
    pattern.apply(config, ctx);

    // 验证：没有抛异常
    SUCCEED();
}

TEST_CASE("HookPattern applies approval hook L4",
          "[pdk][cross_cutting][hook]") {
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

    HookPattern pattern;

    // 配置：approval hook
    nlohmann::json config;
    config["hooks"] = nlohmann::json::array();
    nlohmann::json hook_config;
    hook_config["target"] = "approval";
    hook_config["handler"] = "human-approval-v1";
    config["hooks"].push_back(hook_config);

    // 应用 pattern
    pattern.apply(config, ctx);

    // 验证：没有抛异常
    SUCCEED();
}
