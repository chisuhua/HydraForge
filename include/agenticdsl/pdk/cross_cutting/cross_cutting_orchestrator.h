// include/agenticdsl/pdk/cross_cutting/cross_cutting_orchestrator.h
// 文件头注释
// 功能描述：Cross-Cutting Pattern Orchestrator (ADR-0085 V1)。
//          管理 Pattern 注册和分发。
// 设计依据：ADR-0085 Cross-Cutting Pattern PDK v1.2
// 作者：AgenticDSL Phase 0
// 最后修改日期：2026-08-28

#pragma once

#include "agenticdsl/pdk/cross_cutting/icross_cutting_pattern.h"
#include <nlohmann/json.hpp>
#include <memory>
#include <vector>

namespace hydraforge::pdk {

// Cross-Cutting Pattern Orchestrator
// 管理 Pattern 注册和分发
class CrossCuttingOrchestrator {
public:
    // 构造函数：接收 6 个引用 + 可选 patterns 列表
    CrossCuttingOrchestrator(
        agenticdsl::IAgentRegistry& agent_registry,
        agenticdsl::IAgentHookRegistry& agent_hook_registry,
        agenticdsl::IToolHookRegistry& tool_hook_registry,
        agenticdsl::IInteractionBus& bus,
        std::function<void(std::unique_ptr<agenticdsl::ILLMProvider>)> set_llm_provider,
        agenticdsl::IApprovalHandler* approval_handler = nullptr,
        std::vector<std::unique_ptr<ICrossCuttingPattern>> patterns = {});

    // 分发配置到所有匹配的 patterns
    void dispatch(const nlohmann::json& config);

    // 注册新 pattern
    void register_pattern(std::unique_ptr<ICrossCuttingPattern> pattern);

private:
    // 查找 pattern by name
    ICrossCuttingPattern* find_pattern(const std::string& name) const;

    // 内置 patterns (默认注册 4 个)
    std::vector<std::unique_ptr<ICrossCuttingPattern>> patterns_;

    // CrossCuttingContext (传递给 patterns)
    CrossCuttingContext ctx_;
};

} // namespace hydraforge::pdk
