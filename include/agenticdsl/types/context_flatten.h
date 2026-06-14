// include/agenticdsl/types/context_flatten.h
// 功能描述：将 LayeredContext (5-层结构) 扁平化为 nlohmann::json，
//          用于 inja 模板渲染等需要扁平键空间的场景。
//          本头文件是 Stage 3 / Task 13 的非破坏性迁移工具:
//          旧代码继续用 `Context = nlohmann::json`,
  //          新代码可选用 `LayeredContext`,经 flatten_layers() 桥接到 inja。
// 设计依据：ADR-0008 (5-层结构化上下文) + project-organization Stage 3 Task 13
// 作者：AgenticDSL Stage 3
// 最后修改日期：2026-06-12
#pragma once

#include <nlohmann/json.hpp>
#include "agenticdsl/types/layered_context.h"

namespace agenticdsl {

/**
 * @brief 扁平化 LayeredContext 为 nlohmann::json
 *
 * 将 5 层的顶层 JSON 拍平到单层 nlohmann::json 对象:
 *   - 路径 "working.data.user_input" 不会被下钻到嵌套键
 *     "data" -> "user_input"; 而是直接把 working 层整体作为一个 value 拍到外层
 *   - 这是 by design: inja 模板常用顶层 `{{ user_input }}` 访问,
 *     不需要嵌套路径
 *   - 若多层有同顶层 key, 后到的层覆盖先到的层 (实际不会出现,
 *     因每层用独立 json 对象, 顶层 key 天然不冲突)
 *
 * 使用示例:
 *   LayeredContext ctx;
 *   ctx.working["data"]["user_input"] = "hello";
 *   nlohmann::json flat = flatten_layers(ctx);
 *   // flat 形如: { "working": {"data": {"user_input": "hello"}} }
 *   // 渲染模板 "User said: {{ working.data.user_input }}" 即可取到 "hello"
 *
 * @param ctx 5-层结构化上下文
 * @return 拍平后的 JSON 对象 (顶层 key 是 system/recent/working/archive/meta)
 */
inline nlohmann::json flatten_layers(const LayeredContext& ctx) {
  nlohmann::json out = nlohmann::json::object();

  // 将指定层整体作为 out 的一个键值对 (仅在 src 非 null 时)
  auto merge = [&out](const std::string& key, const nlohmann::json& src) {
    if (!src.is_null()) {
      out[key] = src;
    }
  };

  // 合并顺序: system -> recent -> working -> archive -> meta
  // 后到的层覆盖先到的层 (5 层通常顶层 key 不冲突, 仅当冲突时此顺序生效)
  merge("system", ctx.system);
  merge("recent", ctx.recent);
  merge("working", ctx.working);
  merge("archive", ctx.archive);
  merge("meta", ctx.meta);

  return out;
}

} // namespace agenticdsl