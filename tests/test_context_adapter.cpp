// tests/test_context_adapter.cpp
// Sprint 20 (2026-07-01) / OpenSpec change migrate-context-to-layered
// 桥接适配测试: 验证 agenticdsl::to_context / from_context 双向桥接正确性
// 5 个 TEST_CASE 覆盖: 空 / 嵌套 / 数组 / path 访问 / inja 模板
#include "catch_amalgamated.hpp"
#include "agenticdsl/types/layered_context.h"
#include "agenticdsl/types/context_flatten.h"
#include "common/utils/template_renderer.h"
#include <nlohmann/json.hpp>
#include <string>

using agenticdsl::LayeredContext;
using agenticdsl::to_context;
using agenticdsl::from_context;

// ============================================================
// 5.2.1 to_context(empty json) 返回空 LayeredContext
// 验证 to_context 对空 JSON 的行为: L3 working 默认为空, 其他 4 层也空
// ============================================================
TEST_CASE("to_context with empty JSON returns empty LayeredContext",
          "[context_adapter][migrate_context]") {
    nlohmann::json empty = nlohmann::json::object();
    LayeredContext ctx = to_context(empty);

    // L3 working 应该是空 object (等于输入)
    CHECK(ctx.working.is_object());
    CHECK(ctx.working.empty());

    // 其他 4 层应该为 null (默认构造 LayeredContext 的初始状态)
    CHECK(ctx.system.is_null());
    CHECK(ctx.recent.is_null());
    CHECK(ctx.archive.is_null());
    CHECK(ctx.meta.is_null());
}

// ============================================================
// 5.2.2 to_context(nested) 嵌套结构保留
// 验证 to_context 对嵌套 JSON 的行为: 嵌套对象结构原样保留在 L3 working
// ============================================================
TEST_CASE("to_context preserves nested structure in working layer",
          "[context_adapter][migrate_context]") {
    nlohmann::json nested = {
        {"user", {{"name", "Alice"}, {"age", 30}}},
        {"settings", {{"theme", "dark"}, {"lang", "zh-CN"}}}
    };

    LayeredContext ctx = to_context(nested);

    // L3 working 应该完整保留嵌套结构
    CHECK(ctx.working.is_object());
    CHECK(ctx.working.contains("user"));
    CHECK(ctx.working["user"]["name"] == "Alice");
    CHECK(ctx.working["user"]["age"] == 30);
    CHECK(ctx.working["settings"]["theme"] == "dark");
    CHECK(ctx.working["settings"]["lang"] == "zh-CN");

    // 其他层仍然为 null
    CHECK(ctx.system.is_null());
    CHECK(ctx.recent.is_null());
    CHECK(ctx.archive.is_null());
    CHECK(ctx.meta.is_null());
}

// ============================================================
// 5.2.3 to_context(array) 数组保留
// 验证 to_context 对 JSON 数组的行为: 数组原样保留在 L3 working
// ============================================================
TEST_CASE("to_context preserves array data in working layer",
          "[context_adapter][migrate_context]") {
    nlohmann::json with_array = {
        {"items", nlohmann::json::array({"apple", "banana", "cherry"})},
        {"numbers", nlohmann::json::array({1, 2, 3, 4, 5})}
    };

    LayeredContext ctx = to_context(with_array);

    // 数组数据应该完整保留
    CHECK(ctx.working.contains("items"));
    CHECK(ctx.working["items"].is_array());
    CHECK(ctx.working["items"].size() == 3);
    CHECK(ctx.working["items"][0] == "apple");
    CHECK(ctx.working["items"][2] == "cherry");

    CHECK(ctx.working.contains("numbers"));
    CHECK(ctx.working["numbers"].is_array());
    CHECK(ctx.working["numbers"].size() == 5);
    CHECK(ctx.working["numbers"][2] == 3);
}

// ============================================================
// 5.2.4 from_context(lc) 路径访问
// 验证 from_context 返回的 JSON 可以通过 at("working.<key>") 访问
// 体现 Sprint 20 双语义: 5-层结构 (LayeredContext.at) 适配跨层路径访问
// ============================================================
TEST_CASE("from_context enables path access via LayeredContext::at",
          "[context_adapter][migrate_context]") {
    LayeredContext lc;
    lc.working["data"]["user_input"] = "hello world";
    lc.working["data"]["count"] = 42;
    lc.recent["session_id"] = "sess_12345";

    // 从 LayeredContext 拍平回 flat JSON
    nlohmann::json flat = from_context(lc);
    CHECK(flat.is_object());

    // 通过 LayeredContext.at 路径访问
    CHECK(lc.at("working.data.user_input") == "hello world");
    CHECK(lc.at("working.data.count") == 42);
    CHECK(lc.at("recent.session_id") == "sess_12345");

    // 拍平后的 JSON 也应该能访问
    CHECK(flat["working"]["data"]["user_input"] == "hello world");
    CHECK(flat["working"]["data"]["count"] == 42);
    CHECK(flat["recent"]["session_id"] == "sess_12345");

    // 路径不存在时返回 null (LayeredContext at 行为)
    CHECK(lc.at("working.nonexistent.key").is_null());
}

// ============================================================
// 5.2.5 inja_render(to_context(flat)) 模板渲染
// 验证 inja 模板可以通过 LayeredContext 重载渲染
// 覆盖 InjaTemplateRenderer::render(string_view, const LayeredContext&)
// ============================================================
TEST_CASE("inja render works with to_context bridged LayeredContext",
          "[context_adapter][migrate_context]") {
    nlohmann::json flat = {
        {"user", {{"name", "Bob"}}},
        {"count", 7}
    };

    // 桥接到 LayeredContext
    LayeredContext ctx = to_context(flat);

    // 通过 LayeredContext 重载渲染模板
    // 模板使用 {{ working.user.name }} 路径访问 (flatten_layers 后 5 层作顶层 key)
    std::string tmpl = "Hello, {{ working.user.name }}! You have {{ working.count }} items.";
    std::string expected = "Hello, Bob! You have 7 items.";
    std::string result = agenticdsl::InjaTemplateRenderer::render(tmpl, ctx);

    CHECK(result == expected);
}
