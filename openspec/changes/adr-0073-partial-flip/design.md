# design.md — adr-0073-partial-flip

## Why

ADR-0073 (Tool JSON Schema 契约) 在 `docs/adr/` 中仍标记为 `🔍 Proposed`。
路线图 Phase 6b W1 要求把它翻牌到 `🟡 Partial`，但需要先审计实际代码中哪些决策已部分落地、哪些必须延期至 Phase 6c C8/C9。

Phase 6a PDK manifest 工作（commit `69f9183`）已经在 manifest 边界落地了 D1 的核心承诺：
- `include/agenticdsl/pdk/manifest.h` 暴露 `input_schema` / `output_schema` 字段，标注 JSON Schema 2020-12。
- `src/modules/pdk/manifest_validator.cpp` 在运行时**强制**要求 `input_schema`。
- 现有 manifest 测试覆盖该行为。

这些证据足以支撑 Partial 翻牌，但远未覆盖 ToolMetadata V3（D2）、ToolCoordinator 校验层（D3）或 DECLARE_TOOL 自动生成（D4）。

## What Changes

| 产物 | 动作 |
|------|------|
| `docs/adr/adr-0073-impl-scope-audit.md` | **新建** — 逐决策（D1-D6）分类 Shipped / Partial / Deferred / Not implemented，附 file:line + commit 证据 |
| `docs/adr/adr-0073-tool-json-schema-contract.md` | **修改** — 状态行 `🔍 Proposed → 🟡 Partial`；追加 ship 证据段；修订 §复审节点 使翻牌时点与 W1 一致 |
| `docs/README.md` | **修改** — ADR-0073 行更新为 `🟡 Partial` |
| `roadmap.md` | **修改** — W1 行（line 210）标记完成；line 397 "Sprint 21 部分" 措辞修正为 Phase 6a manifest 来源 |

**Out of Scope**（禁止变更）：
- `include/agenticdsl/policy/execution_policy.h`（ToolMetadata V3 = C8）
- `src/common/tools/tool_coordinator.cpp`（C9）
- `include/agenticdsl/pdk/tool_macros.h`（C8）
- `docs/specs/dsl.md`（§6.2 重写 = C8/C9）

## Impact

- **零代码变更**：纯文档审计与同步。
- **零测试变更**：ctest 147/147 保持不变。
- **依赖链**：Phase 6c C8 (`C8 依赖 W1 (Phase 6b ship)`) 由本 change 的 ship 满足。

## Acceptance

- [ ] `tools/adr_lint.py` exit 0
- [ ] `tools/docs_drift_audit.py` 0 new DRIFT
- [ ] `openspec validate --strict` exit 0
- [ ] `ctest` 输出不变
