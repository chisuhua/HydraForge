# docs/architecture/ — 架构工作组文档

> **定位**: ADR 的**证据输入**（工作文档，非决策本身）。架构分析、分层模型、缺失能力评估在此沉淀；
> 决策结论必须提升为 `docs/adr/` 的 ADR 才具备执行效力。治理规则见 [`../GOVERNANCE.md`](../GOVERNANCE.md) §一.5。
>
> **最后更新**: 2026-07-31 | **Owner**: Architecture Working Group

---

## 一、文档索引

| 文件 | 议题 | 状态 | Last-Verified | Update-Trigger |
|------|------|------|---------------|----------------|
| ~~`agent-as-plugin-architecture-v1.2.md`~~ → [`../specs/architecture.md`](../specs/architecture.md) | Agent-as-Plugin 五层架构规范 (L0~L4 + R1~R5) | ✅ Approved — **已晋升为 specs 契约层** (2026-07-31, D1 决议) | 2026-07-31 | 层模型变更 / 新 ADR 影响 R1~R5 |
| `agent-evolution-pipeline.md` | Agent 四阶段进化管线 (SKILL→DSL→C++→Wasm) | ✅ Approved — **ADR-0061 设计附件** (2026-07-31 D6；§八 路线图已切除, 排期归 active-status) | 2026-07-31 | ADR-0061 子项实施推进 |
| `application-layer-sota-positioning-v2.md` | 应用层 SOTA 定位分析 v2 | 🟡 Proposed | 2026-07-16 | SOTA 调研刷新 / 应用层新插件 |
| `adr-implementation-status-gap-analysis.md` | ADR 实施状态基线 (**ADR 状态唯一事实源**) | 🔄 滚动更新 | 2026-07-30 | 任何 ADR 状态翻转 / ship gate |
| `layer-based-missing-capabilities-analysis.md` | 五层缺失能力分析 + Wave 1-4 执行计划 | ✅ v1.2.1 (数据修正版) | 2026-07-31 | Wave 1 完成 / 缺失能力 ship |

> 归档版本（v1.0/v1.1 架构、v1 SOTA 定位）见 [`../archive/architecture/`](../archive/architecture/)。

---

## 二、文档头元数据规范（强制）

本目录每份文档头部必须包含以下四字段，缺失视为不合规：

```markdown
**生成日期**: YYYY-MM-DD
**最后验证**: YYYY-MM-DD（数据修正版 vX.Y.Z，验证命令见文末/附录）
**作者**: Architecture Working Group
**状态**: <限定词汇>
```

- **状态词汇**：✅ Approved / 🔍 Proposed / 🟡 Proposed(禁用, 用 🔍) / 🔄 滚动更新 / ⛔ Superseded。
  与 `docs/adr-management/STATUS-GLOSSARY.md` 对齐；Superseded 文档必须当 Sprint 归档至 `docs/archive/architecture/`。
- **Last-Verified 规则**：文档中所有**计数类数据**（ADR 数、ctest 数、emit 数、百分比）必须可用命令复现，
  并在文末给出验证命令表（参考 `layer-based-missing-capabilities-analysis.md` 附录 B）。
  优先使用 `python3 tools/doc_metrics.py` 输出。
- **Update-Trigger**：每份文档在索引表中声明触发更新的事件；Sprint 收官时检查 Last-Verified 距今是否超过 30 天。

---

## 三、写作与合并条件

| 项 | 要求 |
|----|------|
| 谁可以写 | 任何人（Architecture Working Group 评审） |
| 合并条件 | ① 头部四字段齐全 ② 计数数据可复现（附验证命令）③ 不与现有 ADR 结论冲突（冲突须先改 ADR） |
| 数据引用 | ADR 状态一律引用 `adr-implementation-status-gap-analysis.md`，禁止维护状态副本表 |
| 生命周期 | 结论被 ADR 吸收 → 文档标注承接关系保留；被新分析替代 → ⛔ Superseded → 归档 |
| 审查频率 | 每月 1 次（与 GOVERNANCE.md 季审视并行，本目录单独月度档） |

---

## 四、待架构组决策清单（2026-07-31 审计产出）

| # | 决策项 | 背景 | 建议路径 |
|---|--------|------|---------|
| D1 | ~~`docs/specs/architecture.md` v2.2 处置~~ | ✅ **已决议并执行 (2026-07-31)**：采用方案①低成本变体——v2.2 归档至 `archive/specs/architecture-v2.2.md`，v1.2 晋升为 `docs/specs/architecture.md`；D1b 同步宣告"第二大脑"产品愿景归档 | 见 §五 变更记录 |
| D2 | ~~ADR-0068 (Event Emission Contract) 立项~~ | ✅ **已起草 (2026-07-31)**：[`adr-0068-event-emission-contract.md`](../adr/adr-0068-event-emission-contract.md) 🔍 Proposed — 管辖运行时生命周期事件，与 0019/0046 划界；含 22 主题 Registry + 7 幻影主题发射点 + EventBuilder + 测试契约 | 待架构组评审转 Approved |
| D3 | ~~ADR-0069 (ToolCoordinator Hook) 立项~~ | ✅ **已起草 (2026-07-31)**：[`adr-0069-tool-coordinator-hooks.md`](../adr/adr-0069-tool-coordinator-hooks.md) 🔍 Proposed — pre/post 双列表 + IToolHookRegistry L3 契约 + HookErrorPolicy；layer check/审批硬门不动 | 待架构组评审转 Approved |
| D4 | ~~ADR-0070 (DECLARE_COMMAND) 立项~~ | ✅ **已起草 (2026-07-31)**：[`adr-0070-declare-command.md`](../adr/adr-0070-declare-command.md) 🔍 Proposed — Command≠Tool 概念界定 + DECLARE_COMMAND 宏 + ICommandRegistry；shortcut 契约先行实现 defer | 待架构组评审转 Approved |
| D5 | loop_agent bypass (L4-1) 修复路径 | 分析文档建议 OpenSpec change，但 Phase 6 已决议 plan+commit 模式 | 按 Phase 6 plan+commit 修复，后补 ADR 追溯（已写入分析文档 §十三） |
| D6 | ~~`agent-evolution-pipeline.md` 定位~~ | ✅ **已决议并执行 (2026-07-31)**：选项①——保留为 ADR-0061 设计附件（✅ Approved），§八 实施路线图切除（排期归 active-status/Master Plan），双向链接补齐 | 见 §五 变更记录 |

---

## 五、变更记录

| 日期 | 变更 |
|------|------|
| 2026-07-31 | 初始化本索引（文档治理审计产出）；v1 SOTA 定位归档；元数据规范与待决策清单建立 |
| 2026-07-31 | **D1 决议执行**：v2.2 八层规范归档（`archive/specs/architecture-v2.2.md`），v1.2 晋升为 `docs/specs/architecture.md`；D1b"第二大脑"愿景归档宣告；全库 15 处引用修正 |
| 2026-07-31 | **D2/D3/D4 立项**：ADR-0068/0069/0070 草案创建（🔍 Proposed）+ 3 份改进提案注册 proposal-suggestions.md（待审查） |
| 2026-07-31 | **D6 决议执行**：`agent-evolution-pipeline.md` 保留为 ADR-0061 设计附件（✅ Approved），§八 路线图切除改指针，双向链接补齐。**D1~D6 决策清单全部关闭** |
