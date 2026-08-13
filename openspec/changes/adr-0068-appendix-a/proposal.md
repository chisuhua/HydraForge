# adr-0068-appendix-a

## Why

ADR-0068 (事件发射契约) 在 2026-08-03 已晋升为 ✅ Approved。Wave 1 §2-§5 完成了幻影主题强制发射点落地 + EventBuilder V2 扩展。

Appendix A (lines 196-209) 中有 **14 个主题**标记为 `📡`（已发射但无注册订阅方）。这些主题实际上已经在代码中发射（有 emit 调用点），只是缺少正式注册为"已注册主题"。

路线图 W6 要求"ADR-0068 §附录 A amendment PR 起草 (14 候选主题注册), P0, ~4h"。

## What Changes

**In Scope**:

- 修改 `docs/adr/adr-0068-event-emission-contract.md` Appendix A，将 14 个 `📡` 主题状态更新为 `✅ registered`（注册完成），并补充 evidence 引用。
- 更新 `docs/README.md` 中 ADR-0068 行（如需要同步说明）。
- 更新 `docs/active-status.md` §一 表格 W6 完成状态。
- 运行 ADR lint、文档漂移、OpenSpec 验证。

### 关键场景

- GIVEN ADR-0068 Appendix A 中 14 个主题当前状态为 `📡`（已发射但无注册订阅方）
  WHEN 本提案 ship 后
  THEN 这 14 个主题状态更新为 `✅ registered`，每行附有 evidence 引用（source file:line）

- GIVEN 路线图 W6 行当前标注为待 ship
  WHEN 本提案 ship 后
  THEN W6 行标记完成

## Out of Scope

- 修改任何 C++ 代码（`src/`、`include/`、`pdk/`）。
- 修改现有 `✅` 或 `👻` 行的状态。
- 修改 EventBuilder 行为或 topic payload。
- 修改其他 ADR 文档。
- 创建测试或修改测试代码。

## Capabilities

- MUST NOT 修改 `src/`、`include/`、`pdk/`、`tests/` 目录下的任何文件。
- MUST NOT 修改现有 `✅` 或 `👻` 状态的行。
- MUST NOT 修改任何 topic 的 payload schema。
- MUST 在每个 `📡` → `✅ registered` 变更行附加 evidence 引用（file:line）。
- MUST 运行 `tools/adr_lint.py`、`tools/docs_drift_audit.py`、`openspec validate --strict`。

## Impact

- **零代码变更**：纯文档状态同步。
- **零测试变更**：ctest 计数保持不变（147/147）。

## Acceptance

- [ ] ADR-0068 Appendix A 中 14 个 `📡` 主题更新为 `✅ registered`，附 evidence 引用
- [ ] `docs/README.md` ADR-0068 行保持一致
- [ ] `docs/active-status.md` W6 行标记完成
- [ ] `tools/adr_lint.py` 退出码 0
- [ ] `tools/docs_drift_audit.py` 无新增 DRIFT
- [ ] `openspec validate --strict` 通过
- [ ] `ctest` 输出保持 147/147（无代码变更）
