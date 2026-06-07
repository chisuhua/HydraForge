// sse_stream.cpp
// 功能描述：SSE 解析器实现，状态机驱动
//          处理 \n / \r\n / \r 三种行结束符，跨 chunk 累积缓冲
//          支持 OpenAI（data: ... + [DONE]）和 Anthropic（event: type + data: ...）
// 设计依据：track-01-cloud-llm.md M1.7
// 作者：AgenticDSL Track 0.1
// 最后修改日期：2026-06-07

#include "sse_stream.h"

#include <cstdlib>
#include <deque>
#include <string>
#include <utility>

namespace agenticdsl {

// PIMPL 实现：避免在头文件中暴露 std::deque 等 STL 类型
struct SSEDecoder::Impl {
  std::deque<SSEEvent> queue;   ///< 已完成待消费的事件队列
  std::string buffer;           ///< 未消费的原始字节
  SSEEvent current;             ///< 当前正在构建的事件
  std::string current_data;     ///< 当前事件的 data 字段（多行拼接）
  bool done = false;            ///< 是否遇到 [DONE] 标记
  std::string error;            ///< 解析错误信息
};

SSEDecoder::SSEDecoder() : impl_(std::make_unique<Impl>()) {}

SSEDecoder::~SSEDecoder() = default;

void SSEDecoder::set_error(std::string msg) {
  if (impl_->error.empty()) {
    impl_->error = std::move(msg);
  }
}

void SSEDecoder::reset() {
  impl_ = std::make_unique<Impl>();
}

void SSEDecoder::feed(std::string_view chunk) {
  if (chunk.empty()) {
    return;
  }
  impl_->buffer.append(chunk);
  // 持续消费 buffer 中所有完整行
  while (true) {
    auto nl_pos = impl_->buffer.find('\n');
    if (nl_pos == std::string::npos) {
      break;
    }
    // 提取行内容（不含 '\n'），处理 \r\n 与 \r
    std::string_view line{impl_->buffer.data(), nl_pos};
    if (!line.empty() && line.back() == '\r') {
      line.remove_suffix(1);
    }
    process_line(line);
    // 移除已处理的字节
    impl_->buffer.erase(0, nl_pos + 1);
  }
}

void SSEDecoder::process_line(std::string_view line) {
  // 空行 → 事件分派边界
  if (line.empty()) {
    dispatch_event();
    return;
  }

  // 注释行（以 ':' 开头）忽略
  if (line.front() == ':') {
    return;
  }

  // 解析 "field: value" 格式
  auto colon = line.find(':');
  std::string_view field;
  std::string_view value;
  if (colon == std::string::npos) {
    // 无冒号：整行作为字段名，值为空
    field = line;
    value = std::string_view{};
  } else {
    field = line.substr(0, colon);
    value = line.substr(colon + 1);
    // 跳过 value 开头的单个可选空格
    if (!value.empty() && value.front() == ' ') {
      value.remove_prefix(1);
    }
  }

  if (field == "event") {
    impl_->current.event_type = std::string(value);
  } else if (field == "data") {
    // 多行 data: 用 '\n' 拼接
    if (!impl_->current_data.empty()) {
      impl_->current_data.push_back('\n');
    }
    impl_->current_data.append(value);
  } else if (field == "id") {
    impl_->current.id = std::string(value);
  } else if (field == "retry") {
    // 解析为整数（毫秒）
    try {
      std::string s(value);
      int retry = std::stoi(s);
      if (retry >= 0) {
        impl_->current.retry_ms = retry;
      }
    } catch (...) {
      // 忽略非法 retry 值
    }
  }
  // 其他字段忽略
}

void SSEDecoder::dispatch_event() {
  // 仅在有 data 或 id/retry 时才视为有效事件
  bool has_payload = !impl_->current_data.empty() ||
                     impl_->current.id.has_value() ||
                     impl_->current.retry_ms.has_value();

  if (!has_payload && impl_->current.event_type.empty()) {
    // 空事件，跳过（连续空行或开头空行）
    return;
  }

  // 写入 data
  if (!impl_->current_data.empty()) {
    impl_->current.data = std::move(impl_->current_data);
    impl_->current_data.clear();
  }

  // 检测 OpenAI [DONE] 哨兵：标记流结束，不入队
  // 消费者通过 is_done() 检测流终止，而非收到 [DONE] 事件
  if (impl_->current.data == "[DONE]") {
    impl_->done = true;
    impl_->current = SSEEvent{};
    return;
  }

  // 仅当 data 非空时才入队（空 data 的 event_type-only 不构成可消费事件）
  if (!impl_->current.data.empty()) {
    impl_->queue.push_back(std::move(impl_->current));
  } else {
    impl_->current = SSEEvent{};
  }
}

std::optional<SSEEvent> SSEDecoder::next() {
  if (!impl_->queue.empty()) {
    SSEEvent ev = std::move(impl_->queue.front());
    impl_->queue.pop_front();
    return ev;
  }
  return std::nullopt;
}

bool SSEDecoder::is_done() const { return impl_->done; }

bool SSEDecoder::has_error() const { return !impl_->error.empty(); }

const std::string& SSEDecoder::error() const { return impl_->error; }

} // namespace agenticdsl
