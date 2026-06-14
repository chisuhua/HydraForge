// tests/test_layered_context.cpp
// LayeredContext 5-层结构化上下文单元测试 (Catch2 v3)
// 覆盖 ADR-0008 + docs/specs/dsl.md §4.1 的 10 个核心场景
// + Stage 3 / Task 13: flatten() 桥接到 inja 模板的测试
#include "catch_amalgamated.hpp"
#include "agenticdsl/types/layered_context.h"
#include "agenticdsl/types/context_flatten.h" // Stage 3 / Task 13

#include <string>

using agenticdsl::LayeredContext;

// ============================================================
// 1. 默认构造: 5 个槽位均为 null JSON
// ============================================================
TEST_CASE("LayeredContext default construction", "[layered_context][stage3]") {
  LayeredContext ctx;

  // 5 个槽位都是 null (nlohmann::json 默认构造 = null)
  CHECK(ctx.system.is_null());
  CHECK(ctx.recent.is_null());
  CHECK(ctx.working.is_null());
  CHECK(ctx.archive.is_null());
  CHECK(ctx.meta.is_null());

  // 槽位类型是 object/null 而非 missing (即结构存在)
  // 注意: nlohmann::json() 是 null, 不是 object —— 第一次 at() 访问会自动创建
  // 这里只确认槽位存在, 不约束初始类型
}

// ============================================================
// 2. 路径 set/get 往返: working.data.user_input
// ============================================================
TEST_CASE("LayeredContext set and get via path", "[layered_context][stage3]") {
  LayeredContext ctx;

  // 通过直接访问结构体字段 set (模拟 node 写入流程)
  ctx.working["data"]["user_input"] = "hello";

  // 通过 at() 读取
  const auto& v = ctx.at("working.data.user_input");
  CHECK(v == "hello");
  CHECK(v.is_string());
}

// ============================================================
// 3. L1 system 写权限: can_write 返回 false
// ============================================================
TEST_CASE("LayeredContext read-only system layer", "[layered_context][stage3]") {
  LayeredContext ctx;
  CHECK(ctx.can_read("system.foo") == true);
  CHECK(ctx.can_write("system.foo") == false);
  CHECK(ctx.can_write("system.data.bar") == false);
}

// ============================================================
// 4. L2 recent 读写权限
// ============================================================
TEST_CASE("LayeredContext recent layer RW", "[layered_context][stage3]") {
  LayeredContext ctx;
  CHECK(ctx.can_read("recent.foo") == true);
  CHECK(ctx.can_write("recent.foo") == true);
  CHECK(ctx.can_write("recent.session.token") == true);
}

// ============================================================
// 5. L3 working 读写权限
// ============================================================
TEST_CASE("LayeredContext working layer RW", "[layered_context][stage3]") {
  LayeredContext ctx;
  CHECK(ctx.can_read("working.foo") == true);
  CHECK(ctx.can_write("working.foo") == true);
  CHECK(ctx.can_write("working.data.user_input") == true);
}

// ============================================================
// 6. L4 archive 读写权限 (本类型不强制 append-only)
// ============================================================
TEST_CASE("LayeredContext archive layer RW", "[layered_context][stage3]") {
  LayeredContext ctx;
  CHECK(ctx.can_read("archive.foo") == true);
  CHECK(ctx.can_write("archive.foo") == true);
  CHECK(ctx.can_write("archive.trace.history") == true);
}

// ============================================================
// 7. L5 meta 读写权限
// ============================================================
TEST_CASE("LayeredContext meta layer RW", "[layered_context][stage3]") {
  LayeredContext ctx;
  CHECK(ctx.can_read("meta.foo") == true);
  CHECK(ctx.can_write("meta.foo") == true);
  CHECK(ctx.can_write("meta.types.user_input") == true);
}

// ============================================================
// 8. 无效路径返回 null JSON
// ============================================================
TEST_CASE("LayeredContext invalid path returns null", "[layered_context][stage3]") {
  LayeredContext ctx;

  // 完整路径不存在
  const auto& v = ctx.at("nonexistent.layer.key");
  CHECK(v.is_null());

  // layer 名不在 5 个白名单中
  const auto& bad = ctx.at("unknown.foo");
  CHECK(bad.is_null());
}

// ============================================================
// 9. dump / load 往返
// ============================================================
TEST_CASE("LayeredContext dump and load round-trip", "[layered_context][stage3]") {
  LayeredContext original;
  original.system["env"]["version"] = "1.0";
  original.recent["session"]["token"] = "abc123";
  original.working["data"]["user_input"] = "hello";
  original.archive["trace"][0] = "step1";
  original.archive["trace"][1] = "step2";
  original.meta["types"]["user_input"] = "string";

  // dump -> 顶层 5 键
  nlohmann::json dumped = original.dump();
  CHECK(dumped.is_object());
  CHECK(dumped.contains("system"));
  CHECK(dumped.contains("recent"));
  CHECK(dumped.contains("working"));
  CHECK(dumped.contains("archive"));
  CHECK(dumped.contains("meta"));
  CHECK(dumped["working"]["data"]["user_input"] == "hello");

  // load -> 还原
  LayeredContext restored = LayeredContext::load(dumped);
  CHECK(restored.at("system.env.version") == "1.0");
  CHECK(restored.at("recent.session.token") == "abc123");
  CHECK(restored.at("working.data.user_input") == "hello");
  CHECK(restored.at("archive.trace")[0] == "step1");
  CHECK(restored.at("archive.trace")[1] == "step2");
  CHECK(restored.at("meta.types.user_input") == "string");
}

// ============================================================
// 10. L1 system 通过 at() 写入时抛异常 (RW 权限拦截)
// ============================================================
TEST_CASE("LayeredContext system layer cannot be written via at()",
          "[layered_context][stage3]") {
  LayeredContext ctx;

  // 通过 at() 写入 L1 必须抛 std::runtime_error
  CHECK_THROWS_AS(
      (ctx.at("system.foo") = "v"),
      std::runtime_error);

  // 其他 layer 不抛
  CHECK_NOTHROW((ctx.at("working.foo") = "v"));
  CHECK_NOTHROW((ctx.at("recent.foo") = "v"));
  CHECK_NOTHROW((ctx.at("archive.foo") = "v"));
  CHECK_NOTHROW((ctx.at("meta.foo") = "v"));
}

// ============================================================
// 11. flatten() 桥接 LayeredContext -> nlohmann::json (Stage 3 / Task 13)
// 用于跨 inja 模板渲染, 验证拍平结果保留各层顶层 key
// ============================================================
TEST_CASE("LayeredContext flatten to JSON for templates", "[layered_context][stage3]") {
  LayeredContext ctx;
  ctx.working["data"]["user_input"] = "hello";
  ctx.meta["permissions"] = nlohmann::json::array({"read", "write"});

  nlohmann::json flat = agenticdsl::flatten_layers(ctx);

  // 结果必须是顶层 object
  CHECK(flat.is_object());

  // 5 层的顶层 key 都暴露出来
  CHECK(flat.contains("working"));
  CHECK(flat.contains("meta"));

  // working 层的子结构完整保留 (便于 inja {{ working.data.user_input }} 访问)
  CHECK(flat["working"].is_object());
  CHECK(flat["working"]["data"]["user_input"] == "hello");

  // meta 层的 array 类型也保留
  CHECK(flat["meta"].is_object());
  CHECK(flat["meta"]["permissions"].is_array());
  CHECK(flat["meta"]["permissions"].size() == 2);
  CHECK(flat["meta"]["permissions"][0] == "read");
  CHECK(flat["meta"]["permissions"][1] == "write");
}

// ============================================================
// 12. flatten() 对空 LayeredContext 返回空 object (Stage 3 / Task 13)
// 边界情况: 默认构造的 LayeredContext 5 层均为 null JSON,
// flatten 必须正常返回 (而非抛异常或返回 null)
// ============================================================
TEST_CASE("LayeredContext flatten empty context", "[layered_context][stage3]") {
  LayeredContext ctx; // 5 层均为 null JSON
  nlohmann::json flat = agenticdsl::flatten_layers(ctx);

  // 返回空 object (不是 null, 不是 array)
  CHECK(flat.is_object());
  CHECK(flat.empty());
  CHECK_FALSE(flat.is_null());
}
