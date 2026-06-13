// src/core/types/context.h
// ⚠️ DEPRECATION NOTE (2026-06-12, Stage 3 / Task 13):
// `Context` 是 flat nlohmann::json 别名, 仍保留以保持向后兼容。
// 新代码应优先使用 `agenticdsl::LayeredContext` (5-层结构化, ADR-0008)。
//
// 平滑迁移路径:
//   1. 新增代码直接用 LayeredContext (路径式 API: ctx.at("working.data.user_input"))
//   2. 跨 inja 模板使用 `agenticdsl::flatten(LayeredContext)` 桥接
//      (见 include/agenticdsl/types/context_flatten.h + InjaTemplateRenderer 重载)
//   3. 完全移除 flat Context 的工作留待 Stage 4 (engine.h 解耦 / core-interface-inversion)
//
// 当前不在本任务移除 `using Context = nlohmann::json;` 因为:
//   - 6 个 `virtual Context execute(Context&)` 虚函数 (node.h) 仍依赖
//   - 1 个 `ExecutionResult run(const Context&)` 公开方法 (engine.h) 仍依赖
//   - 22 个现有 Catch2 测试断言依赖 flat JSON
//   这些是 Stage 4 (breaking API change) 的范围, 不是 Stage 3 的非破坏性迁移。
//
// 作者：AgenticDSL Stage 3
// 最后修改日期：2026-06-12
#ifndef AGENTICDSL_CORE_TYPES_CONTEXT_H
#define AGENTICDSL_CORE_TYPES_CONTEXT_H

#include <nlohmann/json.hpp>

namespace agenticdsl {

// 使用 nlohmann::json 作为统一的数据类型
using Value = nlohmann::json;
using Context = nlohmann::json;

} // namespace agenticdsl::types

#endif // AGENTICDSL_TYPES_CONTEXT_H
