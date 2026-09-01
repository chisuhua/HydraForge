// src/common/governance/cross_cutting/decorator_pattern.cpp
// 文件头注释
// 功能描述：Decorator Pattern 实现 (ADR-0085 V1)。
//          配置 ILLMProvider 装饰器链。
// 设计依据：ADR-0085 Cross-Cutting Pattern PDK v1.2
// 作者：AgenticDSL Phase 1
// 最后修改日期：2026-08-28

#include "agenticdsl/pdk/cross_cutting/decorator_pattern.h"
#include "agenticdsl/contract/i_llm_provider_decorator.h"

#include "common/log/log.h"

#include <stdexcept>

namespace hydraforge::pdk {

namespace {

class MockILLMProvider : public agenticdsl::ILLMProvider {
public:
    agenticdsl::Result<agenticdsl::GenerationResult, agenticdsl::LLMError>
    generate(const agenticdsl::GenerationRequest&, std::stop_token) override {
        return agenticdsl::Result<agenticdsl::GenerationResult, agenticdsl::LLMError>::success(
            agenticdsl::GenerationResult{});
    }
    std::unique_ptr<agenticdsl::IGenerationStream> generate_stream(const agenticdsl::GenerationRequest&, std::stop_token) override {
        return nullptr;
    }
    std::vector<agenticdsl::ILLMProvider::ModelInfo> available_models() const override {
        return {};
    }
};

} // namespace

const std::string& DecoratorPattern::name() const {
    static const std::string name = cross_cutting_pattern::Decorator;
    return name;
}

void DecoratorPattern::apply(const nlohmann::json& pattern_config,
                             CrossCuttingContext& ctx) {
    // 读取 decorators 数组
    if (!pattern_config.contains("decorators") || !pattern_config["decorators"].is_array()) {
        throw std::invalid_argument("DecoratorPattern: 'decorators' array required");
    }

    auto& decorators = pattern_config["decorators"];
    if (decorators.empty()) {
        return; // 无 decorators，直接返回
    }

    // 检查链深限制 (≤4)
    if (decorators.size() > 4) {
        throw agenticdsl::ILLMProviderDecorator::DecoratorChainTooDeep(decorators.size() + 1);
    }

    // V1 简化：创建 MockILLMProvider 作为 innermost provider
    // 实际部署时应从 DSLEngine 获取现有 provider
    auto innermost = std::make_unique<MockILLMProvider>();

    // 构造装饰器链
    std::vector<std::function<std::unique_ptr<agenticdsl::ILLMProvider>(
        std::unique_ptr<agenticdsl::ILLMProvider>)>> decorator_factories;

    for (const auto& decorator_name : decorators) {
        if (!decorator_name.is_string()) {
            throw std::invalid_argument("DecoratorPattern: decorator name must be string");
        }

        std::string name = decorator_name.get<std::string>();

        // V1 简化：仅支持 CostTracking
        if (name == "CostTracking") {
            decorator_factories.push_back(
                [](std::unique_ptr<agenticdsl::ILLMProvider> inner) {
                    // V1: 返回 inner 作为占位符，实际应创建 CostTrackingDecorator
                    return inner;
                });
        } else {
            // 未知 decorator：跳过 (FailOpen)
            LOG_WARN("DecoratorPattern: unknown decorator name='" << name << "' (fail-open skip)");
        }
    }

    // 使用 ILLMProviderDecorator::wrap_chain 构造链
    auto chain = agenticdsl::ILLMProviderDecorator::wrap_chain(
        std::move(innermost), std::move(decorator_factories));

    // 通过 set_llm_provider 回调注入
    ctx.set_llm_provider(std::move(chain));
}

} // namespace hydraforge::pdk
