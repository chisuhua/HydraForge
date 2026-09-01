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
| `adr-implementation-status-gap-analysis.md` | ADR 实施状态基线 (**ADR 状态权威参照**，最终以 `docs/adr/*.md` `## 状态` 为准；本表为滚动视图层) | 🔄 滚动更新 | 2026-09-01 | 任何 ADR 状态翻转 / ship gate |
| `layer-based-missing-capabilities-analysis.md` | 五层缺失能力分析 + Wave 1-4 执行计划 | ✅ v1.2.1 (数据修正版) | 2026-07-31 | Wave 1 完成 / 缺失能力 ship |
| ~~`defect-truth-table-2026-08.md`~~ → [`archive/architecture/defect-truth-table-2026-08.md`](../../archive/architecture/defect-truth-table-2026-08.md) | 架构缺陷真相表（**已归档** 2026-08-24 — 11 真实缺陷 + 3 盲点 × 代码 × ADR 真相） | ⛛ Superseded → `capability-application-map-2026-08.md` | 2026-08-24 | 历史审计追溯，不再维护 |
| `capability-application-map-2026-08.md` | 架构能力-应用地图（**31 项已 ship 能力** + 9 项 open gap + 17 类应用场景 + 23 个工程任务 T1-T22 + T26 解锁映射 + Oracle 评审蒸馏+自进化专题 §八） | ✅ Active (v2.5) | 2026-09-01 | 新增能力 ship / open gap 闭合 / 新应用类型立项 / Oracle 评审输入 / 自进化架构边界与 T14-T22 推进 / ADR-0083 状态修正 + ADR-0084 文件创建 / Sprint 24 审计闭环 5 commits (20735dc/29c5b7c/45db64f/5e0bc7c/07b81ca) |
| `self-evolution-architecture-2026-08.md` | 自进化与协同进化架构定义（证据输入：闭环、支撑平面、阶段边界与当前禁止行为） | 🔍 Proposed (v1.1) | 2026-08-26 | ADR-0084 / T15 / T19 / T20 / T22 状态变化，或协同进化 spike promotion |
| `pdk-chat-demo-distill-source-survey-2026-08.md` | pdk_chat_demo Session JSONL 临时数据源调研（推荐 SessionWriter JSONL 而非 pdk JSON） | ✅ Active (v1.0) | 2026-08-24 | SessionWriter 升级 / D10 v1.2 ship |
| `defect-fix-roadmap-2026-08.md` | 架构缺陷修复路线图（12 个 rdd-workflow 提案节点 + Mermaid 依赖图） | ✅ Active (v1.0) | 2026-08-20 | 提案 ship / Sprint 收官交叉检查 |
| `agent-orchestration-architecture-2026-08.md` | **多智能体编排架构总览**（5 层编排模型 + 5 种编排单元 + Loop×Pattern 行为矩阵 + 认知/领域协同 + 17 类应用场景映射 + LLM 编排蓝图 + 断链清单） | 🔍 Proposed (v1.5) | 2026-08-30 | 横切架构 / PDK Agent 家族 / Cognitive-Domain 协作演进（断链修复 P0 落地 / 场景智能体 P1 ship / Axis6 Phase 0+1 ship）|
| `axis6-chain-workflow-architecture-2026-08.md` | **Axis6 Chain 完整工作流程架构**（双图宇宙第一性原理 + 6 阶段工作流程 + EvolutionReadinessGate + WorkflowMaterializer + GenerateSubGraph×Axis6 关系 + 7 项缺口 G1-G7） | 🔍 Proposed (v1.0) | 2026-08-31 | G1-G7 缺口修复 / ADR-0061-08 v1.1 Approved / GenerateSubGraph 断链修复 |
| `multi-domain-agent-architecture.md` | **多领域智能体架构与服务协作**（Cognitive/Domain 分层 + IInteractionBus 事件驱动 + PDK 工具注册 + DSL DAG 直传） | ✅ Active (v1.0, 2026-08-30 从 `docs/guides/` 迁入) | 2026-08-30 | ADR-0020 / 0082 状态变化, 或 Cognitive-Domain 协作机制演进 |

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
| 2026-08-20 | 新增 `defect-truth-table-2026-08.md`（架构缺陷真相表 v1.0）：14 项缺陷与已有 ADR 全文交叉审计；识别 5 项已通过 ADR-0020/0022/0069 解决，1 项属工程债；修正 4.1/5.1/6.1 状态，合并 1.2/1.3/1.5 进 ADR-0079 v1.2 修订计划 |
| 2026-08-24 | **架构治理转型**: `defect-truth-table-2026-08.md` 归档至 `archive/architecture/`（⛛ Superseded by `capability-application-map-2026-08.md`）；新建 `capability-application-map-2026-08.md`（22 项已 ship 能力 + 9 项 open gap + A/B/C 三类应用场景矩阵 + T1-T13 工程任务解锁映射）；视角从"缺陷清单"转向"能力-应用地图"。P1-P12 + 7.1-7.3 全部 ship 后文档完成其历史使命 |
| 2026-08-24 | **v1.1 Oracle 评审输入**: `capability-application-map-2026-08.md` §八 新增 Agent 蒸馏+自进化专题（Oracle session ses_fcba5e477ffeG9wEBHVhU64J0o 识别 6 项架构层缺口 G10-G15）；§一/§二/§三/§四/§五 全部对齐 v1.1；新增 T14-T22 任务映射（E 轨工程 + R 轨研究双轨） |
| 2026-08-24 | **v1.1.2-1.1.3 任务推进**: T14 行为回归套件 ship（6 cases PASS, 183/183 ctest 0 回归, OpenSpec validate --strict PASS）；3 个新 ADR 草案（ADR-0083 IEvaluator + ADR-0080 v1.2 amendment + ADR-0061-13 蒸馏输出格式）；新建 pdk-chat-demo-distill-source-survey 调研报告（推荐 SessionWriter JSONL 作为过渡数据源） |
