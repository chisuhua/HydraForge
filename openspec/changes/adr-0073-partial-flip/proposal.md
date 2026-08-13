# adr-0073-partial-flip

## Why

ADR-0073（Tool JSON Schema 契约）创建于 2026-08-02，状态为 `🔍 Proposed`。路线图 Phase 6b W1（ADR-0073 翻牌）要求把它提升到 `🟡 Partial`。证据来自 Phase 6a PDK manifest 工作：

- `include/agenticdsl/pdk/manifest.h` 暴露 JSON Schema 2020-12 的 `input_schema`/`output_schema` 字段。
- `src/modules/pdk/manifest_validator.cpp` 对 manifest `input_schema` 做运行时强制校验。
- 现有 manifest 测试覆盖该行为。

`execution-policy.h` 中的 `ToolMetadata` 仍为 V2，没有 V3 字段（`input_schema`、`output_schema`、`ValidationMode`）。`schema_validator.*`、`ToolSchemaValidator`、`DECLARE_TOOL` 自动生成、`ToolCoordinator` 校验层均未实现，必须留给 Phase 6c C8/C9。

## What Changes

**In Scope**:

- 新建 `docs/adr/adr-0073-impl-scope-audit.md`，逐项审计 ADR-0073 D1-D6。
- 将 ADR-0073 更新为 `🟡 Partial`，追加证据段并修订复审节点。
- 更新 `docs/README.md` 的 ADR-0073 状态行。
- 修正 `roadmap.md` 的 W1 完成状态与证据来源。
- 运行 ADR lint、文档漂移、OpenSpec 和 ctest 验证。

### 关键场景

- GIVEN ADR-0073 当前状态 `🔍 Proposed` 且 D1 仅在 PDK manifest 边界部分落地
  WHEN 本提案 ship 后
  THEN ADR-0073 状态变为 `🟡 Partial`，新增的 `adr-0073-impl-scope-audit.md` 明确记录 D1 Shipped/Partial、D2/D3/D4 Deferred 至 Phase 6c C8/C9
- GIVEN 路线图 W1 行（roadmap.md:210）当前标注为待 ship
  WHEN 本提案 ship 后
  THEN W1 行标记完成，roadmap.md:397 中的 `Sprint 21 部分` 措辞被替换为真实的 Phase 6a manifest 证据
- GIVEN ADR-0073 §复审节点 与路线图 W1 时机不一致
  WHEN 本提案 ship 后
  THEN §复审节点 被修订，记录证据驱动的 Partial 翻牌且保留 `Approved` 的完整门槛

## Out of Scope

- `include/agenticdsl/policy/execution_policy.h`（ToolMetadata V3 字段属于 Phase 6c C8）
- `src/common/tools/tool_coordinator.cpp`（ToolCoordinator 校验层属于 C9）
- `include/agenticdsl/pdk/tool_macros.h`（DECLARE_TOOL V3 自动生成属于 C8）
- `docs/specs/dsl.md`（§6.2 schema 部分重写属于 C8/C9）
- 任何 C++ 代码、测试或构建配置。

## Capabilities

- MUST NOT 修改 `include/agenticdsl/policy/execution_policy.h`、`src/common/tools/tool_coordinator.cpp`、`include/agenticdsl/pdk/tool_macros.h`、`src/common/tools/schema_validator.*`。
- MUST NOT 修改 `docs/specs/dsl.md`（其 §6.2 重写属于 Phase 6c C8/C9）。
- MUST NOT 引入新依赖或调整测试数量；ctest 计数（147/147）保持不变。
- MUST 在 ADR-0073 文档中保留证据 file:line + commit 信息，并诚实指出 manifest 级证据弱于 ToolMetadata V3 实施。
- MUST 在 `roadmap.md` 中把“审阅 Sprint 21 ship 内容”替换为对应的 Phase 6a manifest 真实来源；不可保留未实现的旧措辞。

## Impact

- MUST NOT 修改 `include/agenticdsl/policy/execution_policy.h`、`src/common/tools/tool_coordinator.cpp`、`include/agenticdsl/pdk/tool_macros.h`、`src/common/tools/schema_validator.*`。
- MUST NOT 修改 `docs/specs/dsl.md`（其 §6.2 重写属于 Phase 6c C8/C9）。
- MUST NOT 引入新依赖或调整测试数量；ctest 计数（147/147）保持不变。
- MUST 在 ADR-0073 文档中保留证据 file:line + commit 信息，并诚实指出 manifest 级证据弱于 ToolMetadata V3 实施。
- MUST 在 `roadmap.md` 中把“审阅 Sprint 21 ship 内容”替换为对应的 Phase 6a manifest 真实来源；不可保留未实现的旧措辞。

## Acceptance

- [ ] 新增 `docs/adr/adr-0073-impl-scope-audit.md`，逐项记录 D1-D6 的 Shipped / Partial / Deferred / Not implemented 状态。
- [ ] `docs/adr/adr-0073-tool-json-schema-contract.md` 状态行改为 `🟡 Partial`，新增 ship 证据段，复审节点时点与 W1 一致。
- [ ] `docs/README.md` ADR-0073 行更新为 `🟡 Partial` 并附证据引用。
- [ ] `roadmap.md` 中 W1 行（line 210）标记完成；`roadmap.md` line 397 中 `Sprint 21 部分` 措辞修正为 Phase 6a manifest 来源。
- [ ] `tools/adr_lint.py` 退出码 0。
- [ ] `tools/docs_drift_audit.py` 无新增 DRIFT。
- [ ] `openspec validate --strict` 通过。
- [ ] `ctest` 输出保持 147/147，未引入代码变更。

