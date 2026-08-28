// src/common/governance/cross_cutting/composition_pattern.cpp
// 文件头注释
// 功能描述：Composition Pattern 实现 (ADR-0085 V1)。
//          注册 Agent 并注入 hook。
// 设计依据：ADR-0085 Cross-Cutting Pattern PDK v1.2
// 作者：AgenticDSL Phase 1
// 最后修改日期：2026-08-28

#include "agenticdsl/pdk/cross_cutting/composition_pattern.h"
#include "agenticdsl/contract/iagent_registry.h"
#include "agenticdsl/contract/iagent_hook_registry.h"

#include <stdexcept>

namespace hydraforge::pdk {

const std::string& CompositionPattern::name() const {
    static const std::string name = cross_cutting_pattern::Composition;
    return name;
}

void CompositionPattern::apply(const nlohmann::json& pattern_config,
                               CrossCuttingContext& ctx) {
    // 读取 agents 数组
    if (!pattern_config.contains("agents") || !pattern_config["agents"].is_array()) {
        throw std::invalid_argument("CompositionPattern: 'agents' array required");
    }

    auto& agents = pattern_config["agents"];
    for (const auto& agent_config : agents) {
        // 检查必填字段
        if (!agent_config.contains("name") || !agent_config["name"].is_string()) {
            throw std::invalid_argument("CompositionPattern: agent 'name' required");
        }

        std::string name = agent_config["name"].get<std::string>();
        std::string scope = agent_config.value("scope", "*");
        std::string instance_id = agent_config.value("instance_id", "");

        // V1 简化：注册空工厂
        agenticdsl::AgentFactory factory = [](const agenticdsl::AgentConfig&) -> std::unique_ptr<agenticdsl::IAgent> {
            return nullptr;
        };

        ctx.agent_registry->register_agent(name, factory);

        // 自动注入 AgentPreHook
        ctx.agent_hook_registry->register_pre_hook(
            scope,
            nullptr, // V1 简化：空 hook
            0, // 默认优先级
            agenticdsl::HookErrorPolicy::FailClosed
        );
    }
}

} // namespace hydraforge::pdk
