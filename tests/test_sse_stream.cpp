// tests/test_sse_stream.cpp
/**
 * @file test_sse_stream.cpp
 * @brief SSEDecoder 单元测试
 * @date 2026-06-07
 *
 * 覆盖用例（7+）：
 * - 单事件解析
 * - 多行 data
 * - 不完整 chunk
 * - [DONE] 哨兵
 * - Anthropic 事件类型
 * - 注释行忽略
 * - CRLF 行结束符
 * - reset() 复用
 */

#include "catch_amalgamated.hpp"
#include "common/llm/sse_stream.h"

using namespace agenticdsl;

// ================================
// 基础事件解析
// ================================

TEST_CASE("SSEDecoder parses single event", "[sse_stream]") {
  SSEDecoder d;
  d.feed("data: {\"id\": 1}\n\n");
  auto ev = d.next();
  REQUIRE(ev.has_value());
  CHECK(ev->data == "{\"id\": 1}");
  CHECK(ev->event_type.empty());
  CHECK_FALSE(ev->id.has_value());
  CHECK_FALSE(ev->retry_ms.has_value());
}

TEST_CASE("SSEDecoder parses multi-line data", "[sse_stream]") {
  SSEDecoder d;
  d.feed("data: line1\ndata: line2\ndata: line3\n\n");
  auto ev = d.next();
  REQUIRE(ev.has_value());
  CHECK(ev->data == "line1\nline2\nline3");
}

TEST_CASE("SSEDecoder handles incomplete chunk", "[sse_stream]") {
  SSEDecoder d;
  d.feed("data: par");
  auto ev1 = d.next();
  CHECK_FALSE(ev1.has_value());

  d.feed("tial\n\n");
  auto ev2 = d.next();
  REQUIRE(ev2.has_value());
  CHECK(ev2->data == "partial");
}

TEST_CASE("SSEDecoder detects [DONE] marker", "[sse_stream]") {
  SSEDecoder d;
  d.feed("data: {\"a\":1}\n\ndata: [DONE]\n\n");
  auto ev1 = d.next();
  REQUIRE(ev1.has_value());
  CHECK(ev1->data == "{\"a\":1}");

  d.feed("");  // 触发消费
  CHECK(d.is_done());

  // 后续 next() 返回 nullopt
  auto ev2 = d.next();
  CHECK_FALSE(ev2.has_value());
}

TEST_CASE("SSEDecoder parses Anthropic event types", "[sse_stream]") {
  SSEDecoder d;
  d.feed("event: content_block_delta\n"
         "data: {\"delta\":{\"text\":\"hi\"}}\n\n");
  auto ev = d.next();
  REQUIRE(ev.has_value());
  CHECK(ev->event_type == "content_block_delta");
  CHECK(ev->data == "{\"delta\":{\"text\":\"hi\"}}");
}

TEST_CASE("SSEDecoder ignores comment lines", "[sse_stream]") {
  SSEDecoder d;
  d.feed(": this is a heartbeat comment\n"
          ": another comment\n"
          "data: actual_payload\n\n");
  auto ev = d.next();
  REQUIRE(ev.has_value());
  CHECK(ev->data == "actual_payload");
}

TEST_CASE("SSEDecoder handles CRLF line endings", "[sse_stream]") {
  SSEDecoder d;
  d.feed("data: hello\r\n\r\n");
  auto ev = d.next();
  REQUIRE(ev.has_value());
  CHECK(ev->data == "hello");
}

// ================================
// 额外覆盖：id、retry、reset、error、multi-event
// ================================

TEST_CASE("SSEDecoder parses id and retry fields", "[sse_stream]") {
  SSEDecoder d;
  d.feed("id: evt-42\nretry: 3000\ndata: payload\n\n");
  auto ev = d.next();
  REQUIRE(ev.has_value());
  REQUIRE(ev->id.has_value());
  CHECK(*ev->id == "evt-42");
  REQUIRE(ev->retry_ms.has_value());
  CHECK(*ev->retry_ms == 3000);
  CHECK(ev->data == "payload");
}

TEST_CASE("SSEDecoder returns multiple events in sequence", "[sse_stream]") {
  SSEDecoder d;
  d.feed("data: first\n\ndata: second\n\ndata: third\n\n");

  auto ev1 = d.next();
  REQUIRE(ev1.has_value());
  CHECK(ev1->data == "first");

  auto ev2 = d.next();
  REQUIRE(ev2.has_value());
  CHECK(ev2->data == "second");

  auto ev3 = d.next();
  REQUIRE(ev3.has_value());
  CHECK(ev3->data == "third");

  auto ev4 = d.next();
  CHECK_FALSE(ev4.has_value());
}

TEST_CASE("SSEDecoder reset() allows reuse", "[sse_stream]") {
  SSEDecoder d;
  d.feed("data: old\n\n");
  auto old_ev = d.next();
  REQUIRE(old_ev.has_value());
  CHECK(old_ev->data == "old");

  d.reset();

  d.feed("data: new\n\n");
  auto new_ev = d.next();
  REQUIRE(new_ev.has_value());
  CHECK(new_ev->data == "new");
  CHECK_FALSE(d.is_done());
}

TEST_CASE("SSEDecoder has_error is false on clean input", "[sse_stream]") {
  SSEDecoder d;
  d.feed("data: clean\n\n");
  CHECK_FALSE(d.has_error());
  CHECK(d.error().empty());
}
