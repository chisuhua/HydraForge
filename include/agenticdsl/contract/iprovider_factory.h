// agenticdsl/contract/iprovider_factory.h
// 文件头注释
// 功能描述：IProviderFactory 抽象接口 — Phase 1 P1 引擎解耦 (ADR-0019 §1.4 退出标准)
//          替代 engine.h 直接 include common/llm/mock_provider.h
//          提供方创建抽象：屏蔽具体 ILLMProvider 实现的 include 依赖
// 设计依据：ADR-0005 (LLM 后端配置与工厂) §3 + ADR-0019 §1.4
//          + openspec/changes/2026-06-15-residual-engine-h-decoupling T1
// 作者：AgenticDSL Phase 1 P1.T1
// 最后修改日期：2026-06-18
#pragma once

#include <memory>

namespace agenticdsl {

class ILLMProvider;  // 前向声明 (来自 common/llm/llm_types.h)
struct LLMConfig;    // 前向声明 (来自 common/llm/llm_config.h)

/**
 * @brief ILLMProvider 工厂抽象接口
 *
 * 工厂方法基于 LLMConfig::provider 字段选择具体实现:
 *   - "mock"      → MockLLMProvider
 *   - "openai"    → CloudLLMAdapter (兼容 DeepSeek/Qwen/月之暗面等)
 *   - "anthropic" → CloudLLMAdapter (后续扩展)
 *   - "local"     → LlamaAdapterProvider (本地 llama.cpp)
 *   - 默认 (空/未识别) → MockLLMProvider (CI 永远可运行)
 *
 * 命名空间: agenticdsl (扁平, 与现有 contract 头一致)
 *
 * 线程安全: 实现类必须保证 create() 可并发调用 (P1.T1 多线程 1000x 测试要求)
 */
class IProviderFactory {
 public:
  virtual ~IProviderFactory() = default;

  /**
   * @brief 根据 config 创建 ILLMProvider 实例
   * @param config LLM 配置 (含 provider/api_key/model 等)
   * @return unique_ptr<ILLMProvider> — 失败时返回 nullptr (配置错误)
   *
   * 默认实现由具体工厂类提供; 调用方负责持有返回的 unique_ptr
   */
  virtual std::unique_ptr<ILLMProvider> create(const LLMConfig& config) = 0;
};

}  // namespace agenticdsl
