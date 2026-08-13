// agenticdsl/types/context_compactor.h
// 功能描述：上下文压缩器抽象接口 (ADR-0007)
//          提供 token 计数、压缩阈值判断、历史压缩等能力
// 设计依据：ADR-0007 (上下文压缩) + .rddf/plans/context-compactor.md
// 作者：AgenticDSL context-compactor change
// 最后修改日期：2026-08-13
#pragma once

#include <memory>
#include <string>
#include <chrono>

namespace agenticdsl {

class ILLMProvider;
class IInteractionBus;

/**
 * @brief 压缩记录 (写入 context.meta.compaction_record)
 */
struct CompactionRecord {
  size_t tokens_before;
  size_t tokens_after;
  size_t summary_length;
  std::string timestamp;  // ISO 8601 or epoch seconds
};

/**
 * @brief 上下文压缩器抽象接口
 *
 * 职责：
 *   - 统计给定 JSON 上下文的 token 数量
 *   - 判断是否需要压缩（基于 token 阈值）
 *   - 执行 LLM 驱动的上下文压缩
 *   - 发射压缩前/后事件到 InteractionBus
 */
class IContextCompactor {
public:
  virtual ~IContextCompactor() = default;

  /**
   * @brief 压缩前事件回调
   * @param session_id    会话 ID
   * @param tokens_before 压缩前 token 数量
   */
  virtual void on_compact_before(const std::string& session_id,
                                size_t tokens_before) = 0;

  /**
   * @brief 执行上下文压缩
   * @param history_json 历史上下文 JSON
   * @param llm          LLM 提供者（用于生成压缩后的摘要）
   * @return 压缩后的上下文 JSON
   */
  virtual std::string compact(const std::string& history_json,
                             ILLMProvider& llm) = 0;

  /**
   * @brief 压缩后事件回调
   * @param session_id    会话 ID
   * @param tokens_before 压缩前 token 数量
   * @param tokens_after  压缩后 token 数量
   */
  virtual void on_compact_after(const std::string& session_id,
                               size_t tokens_before,
                               size_t tokens_after) = 0;

  /**
   * @brief 统计上下文的 token 数量
   * @param context_json 上下文 JSON
   * @return token 数量（估算）
   */
  virtual size_t count_tokens(const std::string& context_json) const = 0;

  /**
   * @brief 判断是否需要压缩
   * @param token_count 当前 token 数量
   * @return true 表示需要压缩
   */
  virtual bool should_compact(size_t token_count) const = 0;

  /**
   * @brief 创建压缩记录 (供 caller 写入 context metadata)
   * @param tokens_before    压缩前 token 数
   * @param tokens_after     压缩后 token 数
   * @param summary_length   摘要长度
   * @return 压缩记录
   */
  virtual CompactionRecord make_record(size_t tokens_before,
                                      size_t tokens_after,
                                      size_t summary_length) const = 0;
};

/**
 * @brief 上下文压缩器工厂函数
 * @param compact_threshold_tokens 触发压缩的 token 阈值
 * @param llm_provider            LLM 提供者（用于压缩时的摘要生成）
 * @param event_bus               事件总线（用于发射压缩事件）
 * @return 上下文压缩器实例
 */
std::unique_ptr<IContextCompactor> create_context_compactor(
    size_t compact_threshold_tokens,
    std::shared_ptr<ILLMProvider> llm_provider,
    std::shared_ptr<IInteractionBus> event_bus);

}  // namespace agenticdsl
