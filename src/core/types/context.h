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
// C2 Day 2-3 (2026-06-27, Sprint 12 P1, ADR-0030 V2 §决策 2):
//   添加 fork()/merge() 自由函数 — Context 深拷贝 + 子覆盖父 merge
//   解决 ADR-0030 V2 §风险表 "共享可变 Context 🔴 高风险"
//   不可变分支: DAG 节点派发前 fork(), 节点完成时 merge()
//
// 作者：AgenticDSL Stage 3 / C2 Day 2-3
// 最后修改日期：2026-06-27
#ifndef AGENTICDSL_CORE_TYPES_CONTEXT_H
#define AGENTICDSL_CORE_TYPES_CONTEXT_H

#include <nlohmann/json.hpp>

namespace agenticdsl {

// 使用 nlohmann::json 作为统一的数据类型
using Value = nlohmann::json;
using Context = nlohmann::json;

inline Context fork(const Context& parent) {
    return parent;
}

inline Context merge(const Context& child, const Context& parent) {
    Context result = parent;
    result.merge_patch(child);
    return result;
}

} // namespace agenticdsl

#endif // AGENTICDSL_CORE_TYPES_CONTEXT_H
