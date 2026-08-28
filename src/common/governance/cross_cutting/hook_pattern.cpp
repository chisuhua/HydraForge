// src/common/governance/cross_cutting/hook_pattern.cpp
// 文件头注释
// 功能描述：Hook Pattern 实现 (ADR-0085 V1)。
//          注册工具/Agent/审批 hook。
// 设计依据：ADR-0085 Cross-Cutting Pattern PDK v1.2
// 作者：AgenticDSL Phase 1
// 最后修改日期：2026-08-28

#include "agenticdsl/pdk/cross_cutting/hook_pattern.h"
#include "agenticdsl/contract/itool_hook_registry.h"
#include "agenticdsl/contract/iagent_hook_registry.h"
#include "agenticdsl/policy/iapproval_handler.h"

#include <stdexcept>

namespace hydraforge::pdk {

const std::string& HookPattern::name() const {
    static const std::string name = cross_cutting_pattern::Hook;
    return name;
}

void HookPattern::apply(const nlohmann::json& pattern_config,
                        CrossCuttingContext& ctx) {
    // 读取 hooks 数组
    if (!pattern_config.contains("hooks") || !pattern_config["hooks"].is_array()) {
        throw std::invalid_argument("HookPattern: 'hooks' array required");
    }

    auto& hooks = pattern_config["hooks"];
    for (const auto& hook_config : hooks) {
        // 检查必填字段
        if (!hook_config.contains("target") || !hook_config["target"].is_string()) {
            throw std::invalid_argument("HookPattern: 'target' string required");
        }

        std::string target = hook_config["target"].get<std::string>();

        if (target == "tool") {
            // 工具 hook
            if (!hook_config.contains("glob") || !hook_config["glob"].is_string()) {
                throw std::invalid_argument("HookPattern: tool hook requires 'glob'");
            }
            if (!hook_config.contains("type") || !hook_config["type"].is_string()) {
                throw std::invalid_argument("HookPattern: tool hook requires 'type'");
            }

            std::string glob = hook_config["glob"].get<std::string>();
            std::string type = hook_config["type"].get<std::string>();
            int priority = hook_config.value("priority", 0);
            std::string policy_str = hook_config.value("policy", "FailClosed");

            agenticdsl::HookErrorPolicy policy = (policy_str == "FailOpen") ?
                agenticdsl::HookErrorPolicy::FailOpen : agenticdsl::HookErrorPolicy::FailClosed;

            // V1 简化：注册空 hook
            if (type == "pre") {
                ctx.tool_hook_registry->register_pre_hook(glob, nullptr, priority, policy);
            } else if (type == "post") {
                ctx.tool_hook_registry->register_post_hook(glob, nullptr, priority, policy);
            }
        } else if (target == "agent") {
            // Agent hook
            if (!hook_config.contains("glob") || !hook_config["glob"].is_string()) {
                throw std::invalid_argument("HookPattern: agent hook requires 'glob'");
            }
            if (!hook_config.contains("type") || !hook_config["type"].is_string()) {
                throw std::invalid_argument("HookPattern: agent hook requires 'type'");
            }

            std::string glob = hook_config["glob"].get<std::string>();
            std::string type = hook_config["type"].get<std::string>();
            int priority = hook_config.value("priority", 0);
            std::string policy_str = hook_config.value("policy", "FailClosed");

            agenticdsl::HookErrorPolicy policy = (policy_str == "FailOpen") ?
                agenticdsl::HookErrorPolicy::FailOpen : agenticdsl::HookErrorPolicy::FailClosed;

            // V1 简化：注册空 hook
            if (type == "pre") {
                ctx.agent_hook_registry->register_pre_hook(glob, nullptr, priority, policy);
            } else if (type == "post") {
                ctx.agent_hook_registry->register_post_hook(glob, nullptr, priority, policy);
            }
        } else if (target == "approval") {
            // Approval hook
            if (!hook_config.contains("handler") || !hook_config["handler"].is_string()) {
                throw std::invalid_argument("HookPattern: approval hook requires 'handler'");
            }

            // V1 简化：调用 approval_handler->process_request
            // 实际应构造 ToolPreview 并调用
            agenticdsl::ToolMetadata meta;
            agenticdsl::ToolCallContext ctx_tool;
            agenticdsl::ToolPreview preview;

            ctx.approval_handler->process_request(meta, ctx_tool, preview);
        } else {
            // 未知 target：跳过 (FailOpen)
            // TODO: Log warning
        }
    }
}

} // namespace hydraforge::pdk
