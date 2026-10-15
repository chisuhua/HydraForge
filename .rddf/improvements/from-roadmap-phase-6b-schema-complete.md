# from-roadmap-phase-6b-schema-complete

> **status: SUPERSEDED by from-roadmap-phase-6c-schema-complete (2026-09-02)** — 2026-09-02 cleanup.
> Phase 6c 版已 archived + ADR-0073 D2+D3+D4 ship (per ADR-0073 ✅ Approved 2026-08-18).
> 本文件保留作历史设计意图记录，详见 `proposal-suggestions.md` §3.2.

**优先级**: P0 | **来源**: from-roadmap (phase-6b/schema-complete, ADR-0073 D3)
**阶段**: phase-6b | **分类**: schema-complete
**类型**: feature
**主题**: DECLARE_TOOL V3自动生成；ToolCoordinator校验层

## 架构依据

ADR-0073 Tool JSON Schema 契约 D3 部分（D2/D4 已 2026-08-14 提前 ship）：

- D2/D4 Partial 已 ship：`include/agenticdsl/pdk/tool_macros.h` DECLARE_TOOL V3 自动生成 inputSchema + `ToolMetadata` V3 字段。
- D3 仍待 C9：运行时校验（ToolCoordinator 4 步 sanitization pipeline）。
- D5/D6 留 follow-up：output_schema + ValidationMode 强制。
- 与 ADR-0073 D2 配套：D2 自动生成 schema 静态部分，D3 注入动态校验层。

## 范围

- **In Scope**:
  - `src/common/tools/tool_coordinator.{h,cpp}` 新增 4 步 sanitization pipeline：(1) 参数类型校验 → (2) 必填字段检查 → (3) enum/literal 校验 → (4) custom validator 钩子。
  - `include/agenticdsl/contract/ischema_validator.h` L3 契约（与 ADR-0073 D4 衔接）。
  - `src/common/tools/schema_validator.cpp` L1 实现（基于 DECLARE_TOOL V3 生成 schema）。
  - `tests/test_tool_coordinator_validation.cpp` 4 步 pipeline 测试（合法 / 类型错 / 缺字段 / enum 不符）。
  - `tests/test_decure_tool_v3_runtime.cpp` DECLARE_TOOL V3 工具运行时校验 E2E。
- **Out of Scope**:
  - output_schema 强制（D5 defer）。
  - ValidationMode 字段实施（D6 defer）。
  - 自定义 validator 业务逻辑（仅契约层）。

## 关键场景

- GIVEN DECLARE_TOOL V3 定义 `shell/exec` 工具 inputSchema：`{cmd: string, timeout: int}`
  WHEN 调用 `shell/exec` 缺 `cmd`
  THEN 4 步 pipeline 拒绝（步骤 2 必填字段失败），返回 SchemaError。

- GIVEN 调用 `shell/exec` 传 `timeout="abc"`（类型错误）
  WHEN 调用
  THEN 4 步 pipeline 拒绝（步骤 1 类型校验失败），返回 TypeError。

- GIVEN 调用 `mode/select` enum 字段传非法值
  WHEN 调用
  THEN 步骤 3 enum/literal 校验失败，返回 EnumError。

- GIVEN 自定义 validator hook 注册（plugin 提供）
  WHEN 调用
  THEN 步骤 4 调用 hook 校验（失败返回 CustomValidatorError）。

## 技术约束

- MUST 4 步 pipeline 顺序固定（类型 → 必填 → enum → custom），失败早返回。
- MUST 校验失败时仍 emit `tool.audit.denied` 事件（reason 含失败步骤）。
- MUST schema validator 与 ADR-0073 D2 自动生成的 schema 字节一致（避免二次构造）。
- MUST NOT 在校验层引入 LLM 调用（纯静态校验）。
- SHOULD pipeline 性能 < 1ms（P95），不阻塞工具执行。

## 验收标准

- 4 步 pipeline 单元测试通过（合法 + 3 类失败）。
- DECLARE_TOOL V3 工具 E2E 测试通过。
- tool.audit.denied 事件 emit 验证（reason 含失败步骤）。
- ctest 全量零回归。
- ADR-0073 状态可从 🟡 Partial 提升 ✅ Approved。

## Status: ⛔ SUPERSEDED by from-roadmap-phase-6c-schema-complete (2026-09-02)

**Status 段补全** (per Pattern #10 hygiene, 2026-10-15 audit verification):

**5-维判定**:
- 维度 1 self-declared: header `> **status: SUPERSEDED by from-roadmap-phase-6c-schema-complete (2026-09-02)**` 已标注
- 维度 2 archive: ✅ `openspec/changes/archive/2026-09-01-from-roadmap-phase-6c-schema-complete/` (per commit `911d19f`)
- 维度 3 commit: ✅ commit `911d19f` (mark 6 Phase 6b proposals SUPERSEDED)
- 维度 4 ADR: 关联 ADR-0073 Tool JSON Schema 契约 (状态 🟡 Partial per ADR-0073 自身承认)
- 维度 5 fallback: 6c-schema-complete 已 🟡 PARTIAL ship (per本会话 868f67f hygiene commit)

**关联 ship 提案**:
- `from-roadmap-phase-6c-schema-complete` 🟡 PARTIAL (本会话 2026-10-15 audit 实证, commit 868f67f)
- 6c 版替代 6b 版 schema-complete (per `iteration.json` 8/8 archived + Phase 6c 2026-09-02 ship 收官)
- ADR-0073 自身 🟡 Partial 状态: D2/D4 ship, D3/D5/D6 待 ship (与本提案 SUPERSEDED 一致)

**判定结论**: ⛔ SUPERSEDED (本提案 6b schema-complete 被 6c-schema-complete 替代 ship, 本提案保留作历史设计意图记录, 详见 `proposal-suggestions.md` §3.2)

**Pattern 沉淀**: 6b 头注 `> **status: SUPERSEDED by from-roadmap-phase-6c-schema-complete (2026-09-02)**` 已标注 (commit `911d19f`), 但**缺标准 ## Status 段** (本日 hygiene 模式 5 字段格式). 本 commit 补全标准化 Status 段.