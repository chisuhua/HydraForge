// tests/test_cross_cutting_orchestrator.cpp
// 文件头注释
// 功能描述：Cross-Cutting Pattern Orchestrator 单元测试 (ADR-0085 V1)。
//          5 个 TEST_CASE 覆盖:
//            1. orchestrator_dispatch_calls_correct_pattern
//            2. orchestrator_dispatch_unknown_pattern_fail_open
//            3. orchestrator_dispatch_invalid_schema_throws
//            4. orchestrator_register_pattern_adds_new_pattern
//            5. orchestrator_dispatch_multiple_patterns_in_order
// 设计依据：openspec/changes/pdk-cross-cutting-patterns (ADR-0085)
// 作者：AgenticDSL Phase 0
// 最后修改日期：2026-08-28

#include "catch_amalgamated.hpp"

// 包含尚未创建的头文件 (预期编译失败)
#include "agenticdsl/pdk/cross_cutting/icross_cutting_pattern.h"
#include "agenticdsl/pdk/cross_cutting/cross_cutting_orchestrator.h"

#include "agenticdsl/contract/iagent_registry.h"
#include "agenticdsl/contract/itool_hook_registry.h"
#include "agenticdsl/contract/iagent_hook_registry.h"
#include "agenticdsl/contract/iinteraction_bus.h"
#include "agenticdsl/contract/i_llm_provider_decorator.h"
#include "agenticdsl/policy/iapproval_handler.h"

#include <nlohmann/json.hpp>
#include <memory>
#include <vector>

using namespace hydraforge::pdk;

// =====================================================================
// Mock 类 (简化实现，仅用于测试编译)
// =====================================================================

class MockAgentRegistry : public agenticdsl::IAgentRegistry {
public:
    bool register_agent(const std::string&, agenticdsl::AgentFactory) override {
        return true;
    }
    std::unique_ptr<agenticdsl::IAgent> create(const std::string&, const agenticdsl::AgentConfig&) override {
        return nullptr;
    }
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
    size_t subscribe(const std::string&, std::function<void(const agenticdsl::BusEvent&)>) override {
        return 0;
    }
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

TEST_CASE("CrossCuttingOrchestrator dispatch calls correct pattern",
          "[pdk][cross_cutting][orchestrator]") {
    MockAgentRegistry agent_registry;
    MockToolHookRegistry tool_hook_registry;
    MockAgentHookRegistry agent_hook_registry;
    MockInteractionBus bus;
    MockApprovalHandler approval_handler;

    // set_llm_provider 回调 (空操作)
    auto set_llm_provider = [](std::unique_ptr<agenticdsl::ILLMProvider>) {};

    // 创建 Orchestrator (无 patterns)
    CrossCuttingOrchestrator orchestrator(
        agent_registry, agent_hook_registry, tool_hook_registry, bus,
        set_llm_provider, &approval_handler);

    // 注册自定义 pattern
    class MockPattern : public ICrossCuttingPattern {
    public:
        const std::string& name() const override { return name_; }
        void apply(const nlohmann::json&, CrossCuttingContext&) override {
            applied_ = true;
        }
        bool applied_ = false;
    private:
        std::string name_ = "mock-v1";
    };

    auto mock_pattern = std::make_unique<MockPattern>();
    auto* mock_ptr = mock_pattern.get();
    orchestrator.register_pattern(std::move(mock_pattern));

    // Dispatch config
    nlohmann::json config;
    config["patterns"] = nlohmann::json::array();
    nlohmann::json pattern_config;
    pattern_config["type"] = "mock-v1";
    pattern_config["config"] = nlohmann::json::object();
    config["patterns"].push_back(pattern_config);

    orchestrator.dispatch(config);

    REQUIRE(mock_ptr->applied_);
}

TEST_CASE("CrossCuttingOrchestrator dispatch unknown pattern fail open",
          "[pdk][cross_cutting][orchestrator]") {
    MockAgentRegistry agent_registry;
    MockToolHookRegistry tool_hook_registry;
    MockAgentHookRegistry agent_hook_registry;
    MockInteractionBus bus;
    MockApprovalHandler approval_handler;

    auto set_llm_provider = [](std::unique_ptr<agenticdsl::ILLMProvider>) {};

    CrossCuttingOrchestrator orchestrator(
        agent_registry, agent_hook_registry, tool_hook_registry, bus,
        set_llm_provider, &approval_handler);

    // Dispatch unknown pattern
    nlohmann::json config;
    config["patterns"] = nlohmann::json::array();
    nlohmann::json pattern_config;
    pattern_config["type"] = "unknown-v999";
    pattern_config["config"] = nlohmann::json::object();
    config["patterns"].push_back(pattern_config);

    // 不应该抛异常 (FailOpen)
    REQUIRE_NOTHROW(orchestrator.dispatch(config));
}

TEST_CASE("CrossCuttingOrchestrator dispatch invalid schema throws",
          "[pdk][cross_cutting][orchestrator]") {
    MockAgentRegistry agent_registry;
    MockToolHookRegistry tool_hook_registry;
    MockAgentHookRegistry agent_hook_registry;
    MockInteractionBus bus;
    MockApprovalHandler approval_handler;

    auto set_llm_provider = [](std::unique_ptr<agenticdsl::ILLMProvider>) {};

    CrossCuttingOrchestrator orchestrator(
        agent_registry, agent_hook_registry, tool_hook_registry, bus,
        set_llm_provider, &approval_handler);

    // 无效 schema: patterns 数组项缺少 type
    nlohmann::json config;
    config["patterns"] = nlohmann::json::array();
    nlohmann::json pattern_config;
    pattern_config["config"] = nlohmann::json::object();
    config["patterns"].push_back(pattern_config);

    REQUIRE_THROWS_AS(orchestrator.dispatch(config), std::invalid_argument);
}

TEST_CASE("CrossCuttingOrchestrator register pattern adds new pattern",
          "[pdk][cross_cutting][orchestrator]") {
    MockAgentRegistry agent_registry;
    MockToolHookRegistry tool_hook_registry;
    MockAgentHookRegistry agent_hook_registry;
    MockInteractionBus bus;
    MockApprovalHandler approval_handler;

    auto set_llm_provider = [](std::unique_ptr<agenticdsl::ILLMProvider>) {};

    CrossCuttingOrchestrator orchestrator(
        agent_registry, agent_hook_registry, tool_hook_registry, bus,
        set_llm_provider, &approval_handler);

    // 注册前: dispatch 未知 pattern
    nlohmann::json config;
    config["patterns"] = nlohmann::json::array();
    nlohmann::json pattern_config;
    pattern_config["type"] = "test-pattern-v1";
    pattern_config["config"] = nlohmann::json::object();
    config["patterns"].push_back(pattern_config);

    orchestrator.dispatch(config); // 不抛异常

    // 注册新 pattern
    class TestPattern : public ICrossCuttingPattern {
    public:
        const std::string& name() const override { return name_; }
        void apply(const nlohmann::json&, CrossCuttingContext&) override {
            applied_ = true;
        }
        bool applied_ = false;
    private:
        std::string name_ = "test-pattern-v1";
    };

    auto test_pattern = std::make_unique<TestPattern>();
    auto* test_ptr = test_pattern.get();
    orchestrator.register_pattern(std::move(test_pattern));

    // Dispatch 再次: 现在应该调用 pattern
    orchestrator.dispatch(config);

    REQUIRE(test_ptr->applied_);
}

TEST_CASE("CrossCuttingOrchestrator dispatch multiple patterns in order",
          "[pdk][cross_cutting][orchestrator]") {
    MockAgentRegistry agent_registry;
    MockToolHookRegistry tool_hook_registry;
    MockAgentHookRegistry agent_hook_registry;
    MockInteractionBus bus;
    MockApprovalHandler approval_handler;

    auto set_llm_provider = [](std::unique_ptr<agenticdsl::ILLMProvider>) {};

    CrossCuttingOrchestrator orchestrator(
        agent_registry, agent_hook_registry, tool_hook_registry, bus,
        set_llm_provider, &approval_handler);

    // 注册两个 patterns
    class OrderTracker : public ICrossCuttingPattern {
    public:
        OrderTracker(std::string name, std::vector<int>& order, int id)
            : name_(std::move(name)), order_(order), id_(id) {}
        const std::string& name() const override { return name_; }
        void apply(const nlohmann::json&, CrossCuttingContext&) override {
            order_.push_back(id_);
        }
    private:
        std::string name_;
        std::vector<int>& order_;
        int id_;
    };

    std::vector<int> order;
    orchestrator.register_pattern(std::make_unique<OrderTracker>("pattern-a-v1", order, 1));
    orchestrator.register_pattern(std::make_unique<OrderTracker>("pattern-b-v1", order, 2));

    // Dispatch 两个 patterns
    nlohmann::json config;
    config["patterns"] = nlohmann::json::array();
    nlohmann::json pattern_a;
    pattern_a["type"] = "pattern-a-v1";
    pattern_a["config"] = nlohmann::json::object();
    config["patterns"].push_back(pattern_a);
    nlohmann::json pattern_b;
    pattern_b["type"] = "pattern-b-v1";
    pattern_b["config"] = nlohmann::json::object();
    config["patterns"].push_back(pattern_b);

    orchestrator.dispatch(config);

    // 验证顺序
    REQUIRE(order.size() == 2);
    REQUIRE(order[0] == 1);
    REQUIRE(order[1] == 2);
}
