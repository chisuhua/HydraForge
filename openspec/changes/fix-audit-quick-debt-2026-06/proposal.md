# Proposal: 修复审计快速债 (Quick Audit Debt Fixes)

> **STATUS: PROPOSAL** — 2026-06-30 全项目审计 (`docs/superpowers/plans/2026-06-30-audit-remediation-roadmap.md`) 续集

## Why

2026-06-30 审计完成 Sprint 17 C.1-C.4 (4 个严重架构债 + 1 个 P0 测试失败) 后，仍有 4 项低成本债务待清理:

1. **D-3 `node.h ↔ resource.h` 循环 include** — 1 行修复，但违反"include what you use"原则且 `resource.h:5` 注释错误
2. **D-4 `ToolResult::error(string)` `@deprecated` 标记但未删除** — YAGNI 债
3. **D-7 `library_loader.h` 跨模块 include** — 与 Sprint 17 C.4 同模式 PIMPL-lite 解耦
4. **D-11 ADR-0032 文档漂移** — 状态未同步到实际 (test_cost_collector.cpp 已 ship)

这些债单个修复均 ≤50 行，1 天内可全数 ship，且零行为变更。

## What Changes

### 1. D-3 修复 `node.h ↔ resource.h` 循环 include
- 删除 `src/core/types/resource.h:5` 的 `#include "node.h"` (实际不需要 — 仅用 `NodePath = std::string` 别名)
- 修正该行注释错误 ("引入 Context/Value" → 删除)
- **BREAKING**: 无 (编译输出不变)

### 2. D-4 删除 `@deprecated` 重载 `ToolResult::error(string, string)`
- 移除 `src/core/types/tool_result.h:90-92` 的 `error(string, string, json)` 重载
- 移除 `src/core/types/tool_result.cpp` 对应实现
- 全代码库替换调用点 `error("...", "...")` → `error(ErrorCode::..., "...")`
- **BREAKING**: 是 — 公共 API 签名变化，但调用方需要更新为 enum 版本 (已在文档推荐)

### 3. D-7 `library_loader.h` PIMPL-lite 解耦
- `src/modules/library/library_loader.h` 移除 `#include "modules/parser/markdown_parser.h"`
- 前向声明 `class MarkdownParser;`
- 成员 `std::unique_ptr<MarkdownParser>` 替换直接依赖
- 析构函数 out-of-line 定义 (与 Sprint 1b 模式一致)
- **BREAKING**: 无

### 4. D-11 ADR-0032 文档状态同步
- `docs/archive/adr/adr-0032-cost-collector.md` 状态 `🟡 Partial` → `✅ Approved`
- 添加 commit 引用: `tests/test_cost_collector.cpp` 已 ship (2026-06-14)
- **BREAKING**: 无 (纯文档)

## Capabilities

### New Capabilities
无新增能力。

### Modified Capabilities
- `arch-refactor`: 追加 4 项新 requirement (D-3/D-4/D-7/D-11)，作为 Sprint 17 4 任务的延续

## Impact

| 维度 | 影响 |
|------|------|
| 源代码变更 | 4 文件，~30 行修改 + ~10 行删除 |
| 测试变更 | 全现有 48 个 ctest 保持 100% PASS |
| API 变更 | D-4 移除 1 个 `@deprecated` 公共方法 |
| 性能影响 | 无 |
| 文档更新 | ADR-0032 状态同步 |
| 兼容性 | D-3/D-7/D-11 完全向后兼容；D-4 需调用方迁移到 enum 重载 |

## Non-goals

- **不解决 D-1 (topo_scheduler.cpp 复杂度)** — 留独立 change `reduce-topo-scheduler-complexity`
- **不解决 D-2 (examples 不可编译)** — 留独立 change `examples-mockllm-migration`
- **不解决 D-5 (context.h DEPRECATION NOTE)** — Phase 4 engine.h 解耦工作，超出审计范围
- **不解决 D-8 (execution_session.h god header)** — 中型重构，独立 scope
- **不解决 D-9 (node_executor.h cross-module)** — 需要新抽象 `IApprovalHandler`，独立 ADR
- **不解决 D-10 (httplib 模板重复)** — 测试可读性优化，可选 P3

## Estimated Effort

- D-3: 5 分钟
- D-4: 1 小时 (grep + 替换 + 验证)
- D-7: 0.5 天 (PIMPL-lite 模式已成熟)
- D-11: 5 分钟 (文档同步)

**总计**: ~0.75 天 (1 个工作日)

## Test Strategy

- 现有 48 个 ctest 保持 100% PASS
- D-4 调用点替换: `grep -rn 'ToolResult::error("' src/ tests/` 验证零残留
- D-7 PIMPL-lite: 验证 `library_loader.h` 不再 `#include "modules/parser/"`
- D-3 循环 include: 验证 `resource.h` 不再 `#include "node.h"`
- 无需新增测试 (现有测试覆盖所有改动路径)