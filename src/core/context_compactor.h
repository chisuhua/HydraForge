// src/core/context_compactor.h
// 功能描述：上下文压缩器实现 PIMPL-lite 头文件
//          完整类型定义在 .cpp，前向声明在 .h 以减少编译依赖
// 设计依据：ADR-0007 (上下文压缩) + .rddf/plans/context-compactor.md
// 作者：AgenticDSL context-compactor change
// 最后修改日期：2026-08-13
#ifndef AGENTICDSL_CORE_CONTEXT_COMPACTOR_H
#define AGENTICDSL_CORE_CONTEXT_COMPACTOR_H

#include "agenticdsl/types/context_compactor.h"

#include <memory>
#include <string>

namespace agenticdsl {

// 前向声明 - 完整类型在 .cpp 中定义
class ContextCompactorImpl;

/**
 * @brief 上下文压缩器实现类 (PIMPL-lite)
 *
 * 使用 PIMPL 模式将完整实现隐藏在 .cpp 中，减少编译依赖
 */
class ContextCompactorImpl final : public IContextCompactor {
public:
  /**
   * @brief 构造函数
   * @param threshold          触发压缩的 token 阈值
   * @param llm                LLM 提供者
   * @param bus                事件总线
   */
  ContextCompactorImpl(size_t threshold,
                      std::shared_ptr<ILLMProvider> llm,
                      std::shared_ptr<IInteractionBus> bus);

  ~ContextCompactorImpl() override;

  void on_compact_before(const std::string& session_id,
                         size_t tokens_before) override;

  std::string compact(const std::string& history_json,
                      ILLMProvider& llm) override;

  void on_compact_after(const std::string& session_id,
                        size_t tokens_before,
                        size_t tokens_after) override;

  size_t count_tokens(const std::string& context_json) const override;

  bool should_compact(size_t token_count) const override;

  CompactionRecord make_record(size_t tokens_before,
                              size_t tokens_after,
                              size_t summary_length) const override;

private:
  // PIMPL-lite: 完整实现细节在 .cpp
  struct Impl;
  std::unique_ptr<Impl> impl_;
};

}  // namespace agenticdsl

#endif  // AGENTICDSL_CORE_CONTEXT_COMPACTOR_H
