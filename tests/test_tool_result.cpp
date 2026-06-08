// tests/test_tool_result.cpp
// 文件头注释
// 功能描述：ToolResult MVP 信封的单元测试。
//          覆盖 success/error 工厂、JSON 序列化/反序列化、缺失字段处理。
// 设计依据：plan §4 X 阶段单元测试
// 作者：AgenticDSL Phase 0 / Track X
// 最后修改日期：2026-06-08

#include "catch_amalgamated.hpp"

#include "core/types/tool_result.h"

using namespace agenticdsl;

TEST_CASE("ToolResult success factory", "[types][tool_result]") {
  nlohmann::json data = {{"echoed", "hello"}, {"count", 3}};
  auto r = ToolResult::success(data);

  REQUIRE(r.ok == true);
  REQUIRE(r.data == data);
  // meta 应默认为空对象（不是 null）
  REQUIRE(r.meta.is_object());
  REQUIRE(r.meta.empty());
}

TEST_CASE("ToolResult error factory", "[types][tool_result]") {
  auto r = ToolResult::error("ERR_LLM.NETWORK", "timeout");

  REQUIRE(r.ok == false);
  // error_code 与 error_message 必须注入到 meta
  REQUIRE(r.meta.contains("error_code"));
  REQUIRE(r.meta["error_code"] == "ERR_LLM.NETWORK");
  REQUIRE(r.meta.contains("error_message"));
  REQUIRE(r.meta["error_message"] == "timeout");
  // data 应保持为空对象（错误结果不应携带工具输出）
  REQUIRE(r.data.is_object());
  REQUIRE(r.data.empty());
}

TEST_CASE("ToolResult JSON roundtrip", "[types][tool_result]") {
  // 1) success 往返
  ToolResult success_r;
  success_r.ok = true;
  success_r.data = {{"k", "v"}, {"n", 42}};
  success_r.meta = {{"trace_id", "abc-123"}};

  auto j1 = success_r.to_json();
  auto roundtrip1 = ToolResult::from_json(j1);
  REQUIRE(roundtrip1.ok == success_r.ok);
  REQUIRE(roundtrip1.data == success_r.data);
  REQUIRE(roundtrip1.meta == success_r.meta);

  // 2) error 往返
  ToolResult error_r = ToolResult::error("ERR_PARSE", "bad json");
  auto j2 = error_r.to_json();
  auto roundtrip2 = ToolResult::from_json(j2);
  REQUIRE(roundtrip2.ok == error_r.ok);
  REQUIRE(roundtrip2.meta["error_code"] == "ERR_PARSE");
  REQUIRE(roundtrip2.meta["error_message"] == "bad json");
}

TEST_CASE("ToolResult handles missing fields gracefully",
          "[types][tool_result]") {
  // 完整缺失（空对象） -> ok=false
  nlohmann::json empty_j = nlohmann::json::object();
  auto r1 = ToolResult::from_json(empty_j);
  REQUIRE(r1.ok == false);
  REQUIRE((r1.data.is_null() || r1.data.is_object()));
  REQUIRE((r1.meta.is_null() || r1.meta.is_object()));

  // 仅 ok 字段
  nlohmann::json partial_j = {{"ok", true}};
  auto r2 = ToolResult::from_json(partial_j);
  REQUIRE(r2.ok == true);
  // data/meta 缺失时回退默认（空对象）
  REQUIRE(r2.data.is_object());
  REQUIRE(r2.meta.is_object());

  // 非对象 JSON（数组）-> 应返回默认值
  nlohmann::json arr_j = nlohmann::json::array();
  auto r3 = ToolResult::from_json(arr_j);
  REQUIRE(r3.ok == false);
}