#ifndef AGENTICDSL_LLM_SSE_STREAM_H
#define AGENTICDSL_LLM_SSE_STREAM_H

// 文件头注释
// 功能描述：通用 SSE（Server-Sent Events）解析器
//          支持 OpenAI（data: {json}\n\n + [DONE]）和 Anthropic（event: type\ndata: {json}\n\n）
//          增量解析（处理分块字节流），状态机驱动
// 设计依据：track-01-cloud-llm.md M1.6、MDN EventSource 规范
// 作者：AgenticDSL Track 0.1
// 最后修改日期：2026-06-07

#include <memory>
#include <optional>
#include <string>
#include <string_view>

namespace agenticdsl {

/**
 * @brief 单个 SSE 事件
 *
 * 字段对应 SSE 规范（https://html.spec.whatwg.org/multipage/server-sent-events.html）：
 * - event: 事件类型（OpenAI 留空，Anthropic 用 content_block_delta / message_stop 等）
 * - data:  数据载荷；多行 data: 字段以 '\n' 拼接
 * - id:    事件 ID（可选）
 * - retry: 客户端重试间隔（毫秒，可选）
 */
struct SSEEvent {
  std::string event_type;        ///< 事件类型；空字符串表示默认 "message"
  std::string data;              ///< 数据载荷
  std::optional<std::string> id; ///< 事件 ID（last-event-id）
  std::optional<int> retry_ms;   ///< 客户端重试间隔（毫秒）
};

/**
 * @brief 通用 SSE 解析器（状态机）
 *
 * 用法：
 *   SSEDecoder decoder;
 *   decoder.feed(chunk1);
 *   while (auto ev = decoder.next()) {
 *     handle(*ev);
 *     decoder.feed(chunk2);
 *   }
 *
 * 线程安全：单线程使用，跨线程访问需自行加锁
 */
class SSEDecoder {
public:
  SSEDecoder();
  ~SSEDecoder();

  // 禁止拷贝（持有内部状态）
  SSEDecoder(const SSEDecoder&) = delete;
  SSEDecoder& operator=(const SSEDecoder&) = delete;

  /**
   * @brief 注入原始字节（可多次调用，处理跨 chunk 边界）
   * @param chunk 来自 HTTP chunked transfer 的字节片段
   */
  void feed(std::string_view chunk);

  /**
   * @brief 拉取下一个已解析的完整事件
   * @return 若有事件则返回事件；否则返回 std::nullopt
   * @note 返回 std::nullopt 不代表结束，可能仅是当前 chunk 数据不足
   */
  std::optional<SSEEvent> next();

  /// 流是否遇到 [DONE] 标记
  bool is_done() const;

  /// 是否遇到解析错误
  bool has_error() const;

  /// 获取错误信息
  const std::string& error() const;

  /// 重置内部状态（可复用同一实例）
  void reset();

private:
  void process_line(std::string_view line);
  void dispatch_event();
  void set_error(std::string msg);

  // PIMPL：隐藏 std::deque 等 STL 类型
  struct Impl;
  std::unique_ptr<Impl> impl_;
};

} // namespace agenticdsl

#endif // AGENTICDSL_LLM_SSE_STREAM_H
