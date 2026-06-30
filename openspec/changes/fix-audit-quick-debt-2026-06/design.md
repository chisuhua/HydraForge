# Design: 修复审计快速债 (Quick Audit Debt Fixes)

## Context

2026-06-30 审计完成 Sprint 17 (C.1-C.4 共 4 个严重架构债) ship 后，仍有 4 项低成本债务待清理:

- **D-3** `node.h ↔ resource.h` 循环 include (1 行修复)
- **D-4** `ToolResult::error(string)` `@deprecated` 标记但仍存在 (YAGNI)
- **D-7** `library_loader.h` 跨模块 include (PIMPL-lite 模式)
- **D-11** ADR-0032 文档漂移 (状态未同步)

这些债单个修复均 ≤50 行，1 天内可全数 ship。审计发现 Sprint 16 起的 PIMPL-lite 解耦模式已成熟 (Sprint 17 C.4 同模板)，可复用。

## Goals / Non-Goals

**Goals:**
- 清理 4 项审计识别债务，无行为变更 (D-3/D-7/D-11) 或可预测迁移 (D-4)
- 复用 Sprint 17 C.4 PIMPL-lite 模式 (前向声明 + `unique_ptr` + 析构外置)
- 保持 48/48 ctest PASS 不变
- 不引入新 ADR (所有变更在现有 ADR 框架内)

**Non-Goals:**
- 不解决 D-1 (topo_scheduler.cpp 复杂度) — 独立 change
- 不解决 D-2 (examples 不可编译) — 独立 change
- 不解决 D-5/D-8/D-9/D-10 — 独立 change
- 不引入新接口 (D-7 保持现有 API)

## Decisions

### Decision 1: D-3 修复方式 — 单行删除 + 注释修正
**选择**: 删除 `src/core/types/resource.h:5` 的 `#include "node.h"`，该 include 实际未被使用 (resource.h 仅引用 `NodePath = std::string` 别名 + 定义 `ResourceType` 枚举)。

**替代方案**:
- 改用前向声明 `class Node;` → 但 resource.h 不引用 `Node` 类，仅用字符串别名，无意义
- 完全重写 include 链 → 范围扩大，违反 P0 quick fix 范围

**结论**: 1 行删除最简洁。

### Decision 2: D-4 迁移路径 — grep + 批量替换 + 双重写
**选择**:
1. `grep -rn 'ToolResult::error("' src/ tests/ include/` 找到所有调用点
2. 逐个替换为 `ToolResult::error(ErrorCode::X, "...")` (使用合适的 ErrorCode)
3. 删除 `@deprecated` 重载声明 + 实现
4. 现有 48 测试覆盖所有 error code 路径

**替代方案**:
- 保留重载，添加 `[[deprecated("use ErrorCode overload")]]` 编译器警告 → 软迁移 (但用户要求彻底清理)
- 删除重载 + 添加 `assert` 拦截字符串调用 → 引入 runtime 开销

**结论**: 直接删除 + 调用方迁移最彻底，符合 YAGNI。

### Decision 3: D-7 PIMPL-lite 复用 Sprint 17 C.4 模板
**选择**:
```cpp
// src/modules/library/library_loader.h
#include "modules/parser/markdown_parser.h"  // ← 删除
class MarkdownParser;  // ← 前向声明
// ...
std::unique_ptr<MarkdownParser> parser_;  // unique_ptr PIMPL
// 析构外置 (out-of-line definition)
```
```cpp
// src/modules/library/library_loader.cpp
#include "modules/parser/markdown_parser.h"  // 在 .cpp include 完整类型
LibraryLoader::~LibraryLoader() = default;
```

**替代方案**:
- 抽象 `IParser` 接口依赖 → 需要修改 `MarkdownParser` 实现 `IParser`，scope 扩大
- 拆分 `LibraryLoader` 为 `Parser` + `Loader` 两个职责 → 范围过大，超出审计 scope

**结论**: PIMPL-lite 是最小侵入且符合既有模式 (Sprint 1b + Sprint 17 C.4)。

### Decision 4: D-11 文档同步 — commit 引用
**选择**: 更新 `docs/archive/adr/adr-0032-cost-collector.md`:
- 状态 `🟡 Partial` → `✅ Approved`
- 添加 commit 引用 `tests/test_cost_collector.cpp` (2026-06-14)
- 添加 ADR 编号交叉引用

**替代方案**:
- 创建新 ADR 替代归档 → 范围扩大
- 仅 PR description 提及 → 不可追溯

**结论**: 文档同步最简单，符合 docs-code-drift-audit 模式。

## Risks / Trade-offs

| Risk | Mitigation |
|------|-----------|
| D-3 删除 include 暴露其他隐式依赖 | 编译验证 + 48 ctest PASS |
| D-4 调用方替换遗漏某处 | `grep -rn 'ToolResult::error("'` 零残留验证 |
| D-7 PIMPL 增加 1 次指针解引用 | 冷路径 (启动期)，性能影响 < 1% |
| D-11 文档与代码不同步风险 | git hook `docs_drift_audit.py` 已覆盖 |

## Migration Plan

### D-3
1. 删除 1 行 include
2. 修正注释
3. `cmake --build build/tests && ctest` 验证 48/48 PASS

### D-4
1. `grep` 找到所有 string error 调用点
2. 替换为 ErrorCode enum 重载
3. 删除 `@deprecated` 重载声明 + 实现
4. 编译 + ctest 验证

### D-7
1. 前向声明 + unique_ptr
2. 析构 out-of-line 移至 .cpp
3. 编译 + ctest 验证
4. `grep -n 'library_loader.h' src/ | grep modules/parser/` 验证零引用

### D-11
1. 更新 ADR 状态 + commit 引用
2. `adr_lint.py` 验证

### Rollback
每个 D 项独立 commit，可单独 revert。如 D-4 暴露兼容性问题，revert 该 commit + 保留 `@deprecated` 标记即可。

## Open Questions

无 — 所有决策均基于现有 ADR 和 Sprint 17 C.4 模式。