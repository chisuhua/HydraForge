# Agent 蒸馏与自进化 SOTA 调研笔记（2026-08）

**生成日期**: 2026-08-26
**最后验证**: 2026-08-26（v1.0，5 篇综述提炼 + ADR 映射表 + Oracle 战略评估）
**作者**: Solo Dev（架构评审输入）
**状态**: 🔍 Proposed — **证据输入文档，非决策本身**

> **定位**: HydraForge 项目文档治理惯例下，本文件是 **ADR 的证据输入**，不替代 `docs/adr/` 中任何 ADR 决策。结论由对应 ADR 起草时引用本文。
>
> **关联文档**:
> - 上下文：[`../architecture/capability-application-map-2026-08.md` §八](../architecture/capability-application-map-2026-08.md) — 蒸馏+自进化专题 + §二 Gap G11
> - 调研触发：用户梳理 5 篇综述（DSH / 11 改进空间 / 4 前沿方向 / 协同进化 / 在线教师蒸馏）
> - Oracle 战略评估 session：`ses_fc640ea84ffe0f4dyYTa4aFjiL`（2026-08-26）

---

## 一、调研目的

梳理 2026 年 Agent 蒸馏 / 自进化 SOTA 文献，提炼与 HydraForge 项目已有 ADR 体系（ADR-0061 系列 + ADR-0078 + ADR-0080/0081/0083）的**对应 / 冲突 / 超纲**关系。重点支持：

1. **G11 变异治理契约**起草（cap-map §二 Gap，🔒 Blocked）
2. **T19 GEPA R 轨 spike**（cap-map §八.3 Sprint 24 末，硬依赖 G11）
3. **R 轨任务**（T18 PASTE / T20 AFlow / T22 Fine-tune）的范围界定

---

## 二、5 篇综述来源与核心观点

### 2.1 DSH 作为蒸馏数据工厂

- **核心**：DeepSeek Harness 的 append-only 事件流天然适合蒸馏 SFT 数据
- **关键机制**：Session Fork 扩充轨迹多样性 + pre-step 钩子过滤敏感数据
- **HydraForge 对应**：[ADR-0080 v1.2 amendment](../adr/adr-0080-v1-2-amendment-d10-decouple.md) ✅ + [ADR-0081 Pre-Step Hook](../adr/adr-0081-pre-step-hook-contract.md) 🔍 + [ADR-0079 v1.1 Session 4-scope](../adr/adr-0079-unified-session-4scope.md) ✅
- **重叠度**：**95%+**（无冲突）

### 2.2 11 个蒸馏改进方向

| # | 方向 | 项目对应 | 备注 |
|---|---|---|---|
| 1 | 多轨迹采样 + 奖励排序 | (无对应) | ⚠ scope-creep，需 ADR-0083 + ADR-0078 前置 |
| 2 | 对比蒸馏 (DPO 思路) | (无对应) | ⚠ scope-creep |
| 3 | 训练侧数据多样性（多教师/跨环境） | (无对应) | 🟡 未批准方向（预设多智能体） |
| 4 | 数据清洗自动化（轨迹质量打分器） | ADR-0083 (IEvaluator 内置质量评估) ✅ | 补充 |
| 5 | 选择性 loss 权重（事件类型权重） | (无对应) | ⚠ 预设 ADR-0078 fine-tune 管线（Wave 5+ descoped） |
| 6 | 课程学习（按难度排序） | (无对应) | ⚠ scope-creep |
| 7 | 推理链压缩（摘要式蒸馏） | (无对应) | 🟡 T19 GEPA spike 时可作 v2 候选 |
| 8 | 在线蒸馏 / 迭代蒸馏 | (部分) ADR-0061-09 GEPA v2 候选 | 补充 |
| 9 | 工具泛化（元蒸馏 + 工具描述增强） | (无对应) | 🟡 ADR-0043 PDK 工具命名约定的延伸 |
| 10 | 错误恢复（主动注入故障 + 从失败轨迹学） | ADR-0083 ✅ (IEvaluator retryable 判断) | 补充 |
| 11 | 评估体系（分维度 + 泛化性） | ADR-0061-02 行为回归 ✅ + ADR-0083 ✅ | 已覆盖 |

### 2.3 四个前沿方向

| 方向 | 代表工作 | 项目对应 | 备注 |
|---|---|---|---|
| 自我进化蒸馏 | EvolveR / SEED（清华陶建华） | [ADR-0061-09 GEPA](../adr/skill/adr-0061-09-gepa-loop.md) 🔍 | 对应（v2 候选）|
| 多模态蒸馏 | AoTD / VISTA | — | ⚠ **超纲**（llama.cpp 文本后端，无 vision/audio） |
| 协作式蒸馏 | AgentArk / MAGDi | — | ⚠ **未批准方向**（预设多智能体对等协作 vs 单编排器） |
| 蒸馏 + RL 混合 | SDAR（浙大）/ Distilled RL | [ADR-0078 Fine-tune](../adr/adr-0078-finetune-base-model.md) 🔍 | 对应 |

### 2.4 智能体协同进化三阶段

| 阶段 | 描述 | 项目对应 |
|---|---|---|
| Agent-Agent 互搏 | 对抗 + 协作 + 组织进化 | ⚠ **未批准方向**（当前架构为单编排器 SimpleCognitiveOrchestrator @internal） |
| Agent-Environment 共生 | 课程学习 + 奖励模型 + 世界模型 | 🟡 局部对应（[ADR-0074 Prompt Evidence Gate](../adr/adr-0074-prompt-evidence-gate.md) ✅） |
| Meta Co-Evolution | 元学习 + 进化策略 + 资源调度 | 🟡 aspirational（cap-map §六 Phase 6 候选） |

**与项目 4 阶段管线关系**：[`agent-evolution-pipeline.md`](../architecture/agent-evolution-pipeline.md)（SKILL→DSL→C++→Wasm）是**工程形态**；本综述三阶段是**运行时形态**——两者互补，不冲突。

### 2.5 在线教师蒸馏三种模式

| 模式 | 描述 | 项目对应 | 风险 |
|---|---|---|---|
| 教师即稳定器（KL 散度约束） | 防止学生偏离累积能力 | — | 🔴 **冲突**（隐含常驻大教师模型，违反 ADR-0061-04 SLM-routing-first 哲学 + 预算控制） |
| 教师即进化引擎（Bootstrap） | 学生→教师选择性吸收→蒸馏 | 🟡 间接对应（ADR-0083 IEvaluator + ADR-0061-02 行为回归作为门禁） |
| 多教师蒸馏 | 教师池动态选择/加权 | — | ⚠ **未批准方向**（与 G11 变异治理缺口叠加） |

---

## 三、ADR 映射总表

| 综述内容 | 项目 ADR | 关系 |
|---|---|---|
| DSH append-only 事件流 | ADR-0080 D10 + ADR-0080 v1.2 ✅ | 对应（95%） |
| Session Fork | ADR-0079 v1.1（4-scope + extract fork）✅ | 对应（90%） |
| Pre-step 钩子 | ADR-0081 🔍 | 对应（待 Approved） |
| 多轨迹采样 + 排序 | (新 ADR 需求) | scope-creep, 待 Wave 5+ |
| 选择性 loss 权重 | ADR-0078 🔍 | 部分对应 |
| 工具泛化 | ADR-0043 ✅ | 间接对应 |
| EvolveR / SEED 自进化蒸馏 | ADR-0061-09 GEPA 🔍 | 对应（v2 候选） |
| 多模态蒸馏（AoTD / VISTA） | (无 ADR, 无路线) | **超纲** |
| 协作式蒸馏（AgentArk / MAGDi） | (无 ADR, 与单编排器冲突) | **未批准方向** |
| 蒸馏+RL（SDAR） | ADR-0078 🔍 | 对应 |
| Agent-Agent 协同进化 | (无 ADR) | **未批准方向** |
| Agent-Environment 共生 | ADR-0074 ✅ | 部分对应 |
| Meta Co-Evolution | (cap-map §六候选) | aspirational |
| 教师即稳定器 KL | (冲突现有哲学) | 🔴 **冲突** |
| 教师即进化引擎 Bootstrap | ADR-0083 + ADR-0061-02 ✅ | 间接对应 |
| 多教师蒸馏 | (G11 缺口叠加) | **未批准方向** |

---

## 四、G11 变异治理契约起草要点（来自 Oracle session `ses_fc640ea84ffe0f4dyYTa4aFjiL`）

### 4.1 6 维度契约骨架

1. **变异对象分级**：L1 prompt 资产（[ADR-0074](../adr/adr-0074-prompt-evidence-gate.md)）/ L2 DSL 图（[ADR-0061-06](../adr/skill/adr-0061-06-trajectory-ir.md)）/ L3 SKILL.md（[ADR-0061-03](../adr/skill/adr-0061-03-skill-compiler.md)）/ L4 权重（[ADR-0078](../adr/adr-0078-finetune-base-model.md)）— 权限逐级收紧
2. **授权绑定**：复用 [ADR-0004 ApprovalPolicy](../adr/adr-0004-toolregistry-security.md) ✅ + [ADR-0031 ExecutionPolicy](../adr/adr-0031-execution-policy.md) ✅ 模式（yolo/plan/agent），L3/L4 强制 plan 以上
3. **治理流程**：propose → IEvaluator 评估（[ADR-0083](../adr/adr-0083-evaluator-reward-contract.md)）→ 行为回归门（[ADR-0061-02](../adr/skill/adr-0061-02-behavioral-regression.md)）→ commit → 版本固定
4. **审计轨迹**：变异事件写入 [ADR-0080](../adr/adr-0080-append-only-event-log.md) append-only log（复用 D10 CaptureMode 三态）
5. **失败回滚**：prompt/DSL 层用版本钉住 + session fork（ADR-0079 v1.1 ✅）；**权重层 V1 显式禁止自动变异**
6. **攻击面约束**：变异来源白名单（仅 R 轨任务上下文），外部输入永不可触发自修改

### 4.2 起草顺序建议

- **估时**：2 sprint（与 cap-map §八.4 一致）
- **前置**：T17 SkillCompiler ship（提供 L3 变异对象的具体形态）
- **倒推**：T19 GEPA spike 排 Sprint 24 末 → G11 须 Sprint 25 W1 起草 🔍 Proposed
- **编号**：预估 `adr-0084-mutation-governance-contract.md`（最终由 `tools/adr_lint.py` 确认）

---

## 五、风险点与冲突标注

### 5.1 直接冲突（需在 G11 ADR 中显式处理）

- **"教师即稳定器" KL 散度机制** 隐含常驻大教师模型，与 ADR-0061-04 SLM-routing-first 哲学 + IBudgetController 直接冲突
  - **缓解**：G11 ADR §决策 显式声明"在线教师蒸馏仅适用于训练期，非 serving 形态"；serving 路径必须经 SLM-routing

### 5.2 未批准方向（需新架构决议才可纳入）

- **多智能体对等协作**（综述 3 AgentArk/MAGDi + 综述 4 Agent-Agent 阶段）
  - **当前架构**：单编排器（SimpleCognitiveOrchestrator @internal，Phase 1+ 由 CognitiveWorker + ReactLoop 封装）
  - **缓解**：在 G11 ADR §决策 中标注"协作式蒸馏暂不支持，需 ADR-0051 Phase 6 PDK 组合 spike promotion 后独立评估"

### 5.3 超纲内容（项目技术栈不覆盖）

- **多模态蒸馏**（AoTD / VISTA）
  - **理由**：llama.cpp 文本后端，无 vision/audio 引擎
  - **缓解**：本笔记仅做文献存档，不进入 ADR 起草

### 5.4 scope-creep 风险（需前置条件才可推进）

- **多轨迹采样 + 排序、对比蒸馏、选择性 loss 权重、课程学习、推理链压缩**
  - **理由**：预设 ADR-0078 fine-tune 训练管线（Wave 5+ descoped）+ AgenticMind 外部项目（4-6 周）
  - **缓解**：作为 ADR-0078 v2 候选或新 ADR 提案前置文档

---

## 六、与现有文档的协调

| 现有文档 | 协调动作 |
|---|---|
| [`capability-application-map-2026-08.md` §八](../architecture/capability-application-map-2026-08.md) | 不动（v1.3.1 已 ship 2026-08-25） |
| [`pdk-chat-demo-distill-source-survey-2026-08.md`](../architecture/pdk-chat-demo-distill-source-survey-2026-08.md) | 本文 §二.1 引用其结论（SessionWriter JSONL 过渡） |
| [`agent-evolution-pipeline.md`](../architecture/agent-evolution-pipeline.md) | 本文 §二.4 标注工程形态 vs 运行时形态区分 |
| ADR-0061-08 / 0061-09 | 本文 §三 标注 v2 候选机会，不立即合并 |
| ADR-0078 / ADR-0081 / ADR-0083 | 本文 §三 标注重叠度（75%-95%） |
| G11 issue（本会话新建） | 本文 §四 6 维度作 issue body 自审清单种子 |

---

## 七、下一阶段

1. **本会话内**（已 ship）：
   - [x] 本笔记 ship（单 commit，零决策污染）
   - [x] G11 GitHub issue 创建（启动 24h cooling-off）

2. **Sprint 24 中**（cooling-off 期满 + T17 骨架 ship 后）：
   - 在 G11 issue 中勾选 Self-Review Checklist
   - 决策 ✅ / ⏸ / ❌

3. **Sprint 25 W1**：
   - 起草 `adr-0084-mutation-governance-contract.md` 🔍 Proposed
   - 估 2 sprint，预计 Sprint 26 末评审

4. **Sprint 25 末**：
   - 随下次 cap-map 版本同步 G11 状态更新（🔒 Blocked → 🔍 Proposed）

---

## 八、参考来源

### 8.1 5 篇综述（用户 2026-08-26 梳理）

1. DSH 作为 Agent 蒸馏数据生产工厂
2. Agent 蒸馏的 11 个改进方向
3. 四个前沿方向详解（自我进化 / 多模态 / 协作式 / 蒸馏+RL）
4. 智能体协同进化三阶段（Agent-Agent / Agent-Environment / Meta）
5. 在线教师蒸馏三种模式

### 8.2 引用的项目 ADR

- [ADR-0061 父](../adr/adr-0061-agent-evolution-and-solidification.md) ✅
- [ADR-0061-02 行为回归](../adr/skill/adr-0061-02-behavioral-regression.md) ✅
- [ADR-0061-03 SkillCompiler](../adr/skill/adr-0061-03-skill-compiler.md) ✅
- [ADR-0061-06 Trajectory IR](../adr/skill/adr-0061-06-trajectory-ir.md) ✅ + [v1.1 amendment](../adr/skill/adr-0061-06-v1-1-amendment-trajectory-ir-decouple.md) ✅
- [ADR-0061-09 GEPA](../adr/skill/adr-0061-09-gepa-loop.md) 🔍
- [ADR-0074 Prompt Evidence Gate](../adr/adr-0074-prompt-evidence-gate.md) ✅
- [ADR-0078 Fine-tune](../adr/adr-0078-finetune-base-model.md) 🔍
- [ADR-0079 Session 4-scope](../adr/adr-0079-unified-session-4scope.md) ✅
- [ADR-0080 Append-Only Event Log](../adr/adr-0080-append-only-event-log.md) ✅
- [ADR-0080 v1.2 amendment](../adr/adr-0080-v1-2-amendment-d10-decouple.md) ✅
- [ADR-0081 Pre-Step Hook](../adr/adr-0081-pre-step-hook-contract.md) 🔍
- [ADR-0083 IEvaluator](../adr/adr-0083-evaluator-reward-contract.md) ✅

### 8.3 Oracle session

- `ses_fc640ea84ffe0f4dyYTa4aFjiL`（2026-08-26）— 战略评估输入 + G11 6 维度契约骨架

---

**笔记状态**: 🔍 Proposed — 证据输入文档
**作者**: Solo Dev（架构评审输入）
**关联 Sprint**: Sprint 24（启动周）
**笔记创建日期**: 2026-08-26