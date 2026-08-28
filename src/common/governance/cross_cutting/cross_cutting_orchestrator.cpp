// src/common/governance/cross_cutting/cross_cutting_orchestrator.cpp
// 文件头注释
// 功能描述：Cross-Cutting Pattern Orchestrator 实现 (ADR-0085 V1)。
//          管理 Pattern 注册和分发。
// 设计依据：ADR-0085 Cross-Cutting Pattern PDK v1.2
// 作者：AgenticDSL Phase 2
// 最后修改日期：2026-08-28

#include "agenticdsl/pdk/cross_cutting/cross_cutting_orchestrator.h"
#include "agenticdsl/pdk/cross_cutting/decorator_pattern.h"
#include "agenticdsl/pdk/cross_cutting/hook_pattern.h"
#include "agenticdsl/pdk/cross_cutting/composition_pattern.h"
#include "agenticdsl/pdk/cross_cutting/bus_pattern.h"

#include <stdexcept>

namespace hydraforge::pdk {

CrossCuttingOrchestrator::CrossCuttingOrchestrator(
    agenticdsl::IAgentRegistry& agent_registry,
    agenticdsl::IAgentHookRegistry& agent_hook_registry,
    agenticdsl::IToolHookRegistry& tool_hook_registry,
    agenticdsl::IInteractionBus& bus,
    std::function<void(std::unique_ptr<agenticdsl::ILLMProvider>)> set_llm_provider,
    agenticdsl::IApprovalHandler* approval_handler,
    std::vector<std::unique_ptr<ICrossCuttingPattern>> patterns)
    : patterns_(std::move(patterns))
    , ctx_{&agent_registry, &agent_hook_registry, &tool_hook_registry, &bus, std::move(set_llm_provider), approval_handler} {
    // 默认注册 4 个内置 Pattern (M7)
    if (patterns_.empty()) {
        patterns_.push_back(std::make_unique<DecoratorPattern>());
        patterns_.push_back(std::make_unique<HookPattern>());
        patterns_.push_back(std::make_unique<CompositionPattern>());
        patterns_.push_back(std::make_unique<BusPattern>());
    }
}

void CrossCuttingOrchestrator::dispatch(const nlohmann::json& config) {
    // 检查 patterns 数组
    if (!config.contains("patterns") || !config["patterns"].is_array()) {
        throw std::invalid_argument("CrossCuttingOrchestrator: 'patterns' array required");
    }

    auto& patterns = config["patterns"];
    for (const auto& pattern_config : patterns) {
        // 检查 type 字段
        if (!pattern_config.contains("type") || !pattern_config["type"].is_string()) {
            throw std::invalid_argument("CrossCuttingOrchestrator: pattern 'type' required");
        }

        std::string type = pattern_config["type"].get<std::string>();
        nlohmann::json config_obj = pattern_config.contains("config") ? pattern_config["config"] : nlohmann::json::object();

        // 查找 pattern
        ICrossCuttingPattern* pattern = find_pattern(type);
        if (pattern) {
            // 异常隔离: 每个 pattern apply() 用 try-catch 包裹
            try {
                pattern->apply(config_obj, ctx_);
            } catch (const std::exception& e) {
                // FailOpen: 记录警告但继续
                // TODO: Log warning
            }
        } else {
            // 未知 pattern: FailOpen (log warning + skip)
            // TODO: Log warning
        }
    }
}

void CrossCuttingOrchestrator::register_pattern(std::unique_ptr<ICrossCuttingPattern> pattern) {
    if (pattern) {
        patterns_.push_back(std::move(pattern));
    }
}

ICrossCuttingPattern* CrossCuttingOrchestrator::find_pattern(const std::string& name) const {
    for (const auto& pattern : patterns_) {
        if (pattern->name() == name) {
            return pattern.get();
        }
    }
    return nullptr;
}

} // namespace hydraforge::pdk
