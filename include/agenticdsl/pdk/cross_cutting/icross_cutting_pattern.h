// include/agenticdsl/pdk/cross_cutting/icross_cutting_pattern.h
// 文件头注释
// 功能描述：Cross-Cutting Pattern PDK 抽象接口 (ADR-0085 V1)。
//          定义 ICrossCuttingPattern 接口和 CrossCuttingContext 结构体。
// 设计依据：ADR-0085 Cross-Cutting Pattern PDK v1.2
// 作者：AgenticDSL Phase 0
// 最后修改日期：2026-08-28

#pragma once

#include "agenticdsl/contract/ievaluator.h"
#include "agenticdsl/contract/iinteraction_bus.h"
#include "agenticdsl/contract/itool_hook_registry.h"
#include "agenticdsl/contract/iagent_hook_registry.h"
#include "agenticdsl/contract/iagent_registry.h"
#include "agenticdsl/contract/i_llm_provider_decorator.h"
#include "agenticdsl/policy/iapproval_handler.h"

#include <nlohmann/json.hpp>
#include <functional>
#include <memory>
#include <string>

namespace hydraforge::pdk {

// Cross-Cutting Context 结构体 (Oracle B3 关键不变量)
// 包含 6 个字段，用于传递所有必要的注册表和回调
struct CrossCuttingContext {
    agenticdsl::IAgentRegistry* agent_registry;
    agenticdsl::IAgentHookRegistry* agent_hook_registry;
    agenticdsl::IToolHookRegistry* tool_hook_registry;
    agenticdsl::IInteractionBus* bus;

    // set_llm_provider 回调 (Oracle B1 关键不变量)
    // 替代 v1.0 虚构的 ILLMProvider** 槽位
    std::function<void(std::unique_ptr<agenticdsl::ILLMProvider>)> set_llm_provider;

    agenticdsl::IApprovalHandler* approval_handler;
};

// Cross-Cutting Pattern 接口 (V1 最小骨架)
// 每个 Pattern 实现此接口，提供 apply() 方法
class ICrossCuttingPattern {
public:
    virtual ~ICrossCuttingPattern() = default;

    // Pattern 类型名称 (e.g. "decorator-v1", "hook-v1")
    virtual const std::string& name() const = 0;

    // 应用 Pattern 到给定上下文
    // pattern_config: JSON 配置对象
    // ctx: CrossCuttingContext 引用，包含所有注册表和回调
    virtual void apply(const nlohmann::json& pattern_config,
                       CrossCuttingContext& ctx) = 0;
};

// 内置 Pattern 类型名称常量
namespace cross_cutting_pattern {
    constexpr const char* Decorator = "decorator-v1";
    constexpr const char* Hook = "hook-v1";
    constexpr const char* Composition = "composition-v1";
    constexpr const char* Bus = "bus-v1";
}

} // namespace hydraforge::pdk
