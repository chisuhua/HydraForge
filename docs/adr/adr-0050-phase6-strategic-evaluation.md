# ADR-0050: Phase 6 战略方向评估 — 服务化 (Service-ification)

## 状态

🔍 Proposed (2026-07-10 — OpenSpec change `2026-07-10-phase5-sprint22-drift-strategic-gate` (C18) 产出; **待评审**)

## 领域

战略 / Phase 6 启动决策 / 服务化路径

## 关联

- [ADR-0019 (IInteractionBus)](./adr-0019-iinteraction-bus-mvp.md) — 事件驱动契约 (不变量)
- [ADR-0020 (Thread Model Isolation)](./adr-0020-thread-model-isolation.md) — per-agent worker 隔离 (**最危险不变量, 见 §不变量风险**)
- [ADR-0021 (PDK Design)](./adr-0021-pdk-design.md) — PDK 静态链接 (不变量)
- [ADR-0022 (Plugin Loading)](./adr-0022-plugin-loading.md) — dlopen + lifecycle (不变量)
- [ADR-0023 (ToolResult)](./adr-0023-tool-result-standard.md) — ToolResult 标准化 (不变量)
- [ADR-0031 (Execution Policy)](./adr-0031-execution-policy.md) — IExecutionPolicy + ToolCoordinator (不变量)
- [ADR-0033 (Session Hierarchy)](./adr-0033-session-hierarchy.md) — 三层会话模型 (Candidate B 并发隔离基础)
- [ADR-0042 (ILLMProvider Evolution)](./adr-0042-illmprovider-evolution-path.md) — C16 §5 Cloud plugin (🔍 Proposed, 非阻塞)
- [Master Plan `2026-07-03-phase5-self-bootstrapping.md`](../superpowers/plans/2026-07-03-phase5-self-bootstrapping.md) — Phase 5 上游
- [Master Plan `2026-07-10-phase5-remainder-adr-sync.md`](../superpowers/plans/2026-07-10-phase5-remainder-adr-sync.md) — C17+C18 范围
- [Drift Audit Report (2026-07-10)](../audits/2026-07-10-drift-gate.md) — Architecture Drift Gate ✅ PASSED
- [Stage Gate Handoff (2026-07-31)](../handoff/2026-07-31-stage-gate-evaluation.md) — Stage 1→2 推迟至 2026-07-18

## Oracle Session

**`ses_0ae4b8107ffetONLmb2Sv2wTb5`** (2026-07-11, Sisyphus 委托) — Phase 6 4 候选战略评估

---

## 背景

### Phase 5 收官状态 (2026-07-10)

| 维度 | 状态 |
|------|------|
| ctest | 72/72 ✅ PASS |
| ASan | 72/72 (100%) ✅ |
| OpenSpec ship 数 | C9-C17 (9 changes) + C18 收官中 |
| ADR Approved | 19 (含 C17 翻转的 5 个) |
| ADR 🔍 Proposed | 7 (C17 排除清单, 排除原因文档化) |
| Phase 5 实施 | ~70% (C9-C16 全 ship, 仅 C16 §5 Cloud plugin 顺延) |
| Architecture Drift Gate | ✅ PASSED (4 路 0 CRITICAL) |
| Stage 1 (C10+C11+C12) ship | ✅ (2026-07-03 ~ 2026-07-04), 但 2 周稳定窗口未到 (推迟至 2026-07-18) |

### Phase 6 启动需求

- **Phase 5 沉没成本**: Self-Bootstrapping & Service-ification 阶段已投入 9 个 change, Stage 3 (服务化) 部分路径已 ship (ADR-0019/0020/0033 + C12 YIELD/STREAM + C16 ILLMProvider v2)
- **团队约束**: 1-2 工程师, 4-6 周现实窗口
- **7 个 Phase 5 ADR 仍 🔍 Proposed**: ADR-0030 V2 / 0037 / 0038 / 0039 / 0042 / 0045 / 0046 (排除原因见 [Master Plan §十一.3](../superpowers/plans/2026-07-10-phase5-remainder-adr-sync.md))
- **C19/C20 placeholder**: 触发条件未明, 需要本 ADR 评估对齐

---

## 候选方向

### Candidate A — 自进化 (Self-Evolution)

- **聚焦**: HydraForge 自身能力随时间提升 (QualityFeedbackController + adaptive optimization + 服务化)
- **依赖**: C19 (fork-checkpoint placeholder) + C20 (analysis-service placeholder)
- **估时**: 8-12 周 (full team)
- **风险**: Quality signal noise, reward hacking, self-regression

### Candidate B — 服务化 (Service-ification) ⭐ **推荐**

- **聚焦**: 暴露 HydraForge 能力为外部可消费服务 — InferenceServer MCP + OpenAI-compatible API
- **依赖**: ADR-0033 Session Hierarchy (✅ 已 ship); C16 §5 Cloud plugin **非阻塞** (本地服务即可)
- **估时**: 4-6 周 (1-2 工程师)
- **风险**: 服务稳定性成为 critical path; 无内部能力提升

### Candidate C — 第三方生态 (Third-party Ecosystem)

- **聚焦**: PDK 重心转向第三方插件贡献 (community-driven)
- **依赖**: ADR-0021 §7 Dual-Repo Policy 强化 + contribution 流程
- **估时**: 6-8 周 (含 1-2 周社区 seeding)
- **风险**: 社区可能未涌现; 高协调开销

### Candidate D — Cloud-native

- **聚焦**: Cloud plugin + Serverless 适配 (AWS Lambda / GCP Cloud Run / Azure Container Apps)
- **依赖**: ADR-0042 §C16 §5 (🔍 Proposed, 未批准)
- **估时**: 8-12 周 (基础设施 + cloud provider 集成)
- **风险**: Cloud provider 锁定; cold-start 延迟; 成本

---

## 决策

### 推荐方向: **Candidate B (服务化)**

**1-2 句话理由**:
> Phase 5 已在 service-ification 方向投入沉没成本 (ADR-0019/0020/0033 + C12 YIELD + C16 ILLMProvider v2), Candidate B 以 4-6 周 / 1-2 工程师的可负担代价完成这条路径, 产出可预测、可消费的外部接口 (MCP + OpenAI-compatible API), 边际收益最确定.

**为什么 B 是 A/D 的前置条件**:
> 服务化是自进化 (A) 和云原生 (D) 的**前置条件**——没有稳定的服务接口, 优化 (A) 和部署 (D) 都是空中楼阁. 先完成 B, 后续 A/D 才有附着点.

### 为什么**不**选其他 3 个

| 候选 | 拒绝理由 |
|------|---------|
| **A 自进化** | 8-12 周超出容量 2x; Quality signal noise + reward hacking 是研究问题而非工程问题, 小团队不适合承担高方差研究; **没有服务化基础做自进化 = 优化没人消费的东西** (过早优化) |
| **C 第三方生态** | 网络效应需要 critical mass, 社区尚不存在; 1-2 工程师无法同时建设 contribution 基础设施 AND seed 社区; **第三方插件要消费什么? 没有 B 的服务接口, 生态无附着点** |
| **D Cloud-native** | 8-12 周超出容量; LLM 推理延迟敏感 + 有状态, cloud cold-start 是灾难性匹配; ADR-0042 §C16 §5 仍 🔍 Proposed, 连批准都未完成 |

### Oracle 引用

- **Session**: `ses_0ae4b8107ffetONLmb2Sv2wTb5` (2026-07-11, Phase 6 战略评估)
- **核心论证**: 团队容量是 binding constraint, 路径依赖决定 B 优先, 外部触发因素 (C16 §5) 未到

### Solo Developer 重新评估 (2026-07-15) ⚠️ 状态补充

**触发**: 用户 2026-07-15 确认 HydraForge 由 **单一开发者**维护 (非 1-2 工程师团队), 且已规划基于 HydraForge PDK 启动下游项目 **AgentForge** 作为领域 Agent 实现载体。

**对原决策的影响**:

| §项 | 原措辞 | Solo Dev 修正 | 影响 |
|------|--------|-------------|------|
| §决策推荐 B | "1-2 工程师 × 4-6 周" | "1 人 × 6-10 日历周 (连续投入不足, 间歇性)" | B 估时翻倍; 服务化预算 vs. PDK 生产化预算 互斥 |
| §启动条件 #4 | "1-2 工程师 4-6 周无中断可用" | 重写为 solo dev 容量 (见下方修正) | 字面失效 |
| §启动条件 #5 | "≥1 个外部 agent/tool" | **部分满足**: AgentForge 由同一构建者开发, 非真正外部实体 | Oracle round 4 重新评估 |
| §不变量 ADR-0020 | "Week 4 TSan" | "Week 8-10 TSan" (日历时间扩展) | 缓解窗口拉长 |

**修正后的方向**: 见 [§决策后续](#solo-dev-重新评估后续-action-2026-07-15) — 暂缓 Phase 6 服务化, 优先 PDK 生产化 + AgentForge MVP 验证 (单一开发者路径).

---

## 启动条件 (Phase 6 硬前置)

> **5 项硬前置**: 全部满足后才可启动 Phase 6 实施

1. **Phase 5 完全关闭**: C17 OpenSpec change 已 archive (✅), active OpenSpec changes = 0 (C18 收官后满足)
2. **服务化范围文档批准**: 明确 in-scope (MCP server + OpenAI-compatible `/v1/chat/completions` + `/v1/models`) 和 out-of-scope (cloud deployment → Candidate D follow-up), 使 C16 §5 成为可选而非阻塞依赖
3. **C20 placeholder 决议**: analysis-service placeholder 激活 (见下文 §C19/C20 决策)
4. ~~**团队容量确认**: 1-2 工程师 4-6 周无中断可用~~ → **修正 (2026-07-15 Solo Dev 重新评估)**: Solo dev 容量受日常事项 + 其他项目 (含 AgentForge) 挤压. Phase 6 启动条件 #4 **重写为** "Solo dev 连续 8-10 周不投入其他大幅工作的承诺". 实务上 = 先推 Sprint 24-25 PDK 生产化 + AgentForge MVP (可衡量), 待 PDK 成熟后再评估服务化启动窗口.
5. **≥1 个具体集成目标**: 至少识别 1 个会消费 MCP/OpenAI API 的外部 agent/tool (避免"建了没人用"). **修正 (2026-07-15)**: AgentForge 作为 HydraForge 同一构建者的下游项目, 不构成字面"外部"消费者. Oracle round 4 (ADR-0051 §后续 #9) 需重新评估 #5 是否可放宽至"内部跨项目消费" 或 是否需引入真正外部触发后再启动服务化.

> **Phase 6 启动条件当前状态**:
> - #1 ✅
> - #2 🔒 未启动 (Phase 6 整体暂停中)
> - #3 ✅ (analysis-service placeholder 激活决策已记录, 但 OpenSpec change 不启动)
> - #4 🔴 **修正后仍不可行** (8-10 周连续 solo dev 不现实)
> - #5 🟡 **部分满足但需重新评估**
>
> **结论**: Phase 6 Candidate B (服务化) 在 Solo Dev 现实下 **结构性阻塞**, 不应再 push. 转向 "PDK 生产化 + AgentForge MVP 验证" 路径 (新 plan: `docs/superpowers/plans/2026-07-15-phase6-agentforge-mvp.md`).

---

## 不变量风险评估

| ADR | 风险 | 缓解 |
|-----|:----:|------|
| 0019 (IInteractionBus) | 🟢 低 | 服务层 wrap 现有 engine, 不改 bus 契约 |
| **0020 (Thread 隔离)** | 🟠 **中高** | **每个请求 request-scoped DSLEngine 实例 或 ADR-0033 session 隔离; Week 4 强制 TSan 并发压测** |
| 0021 (PDK 静态链接) | 🟢 低 | 服务层是 PDK consumer 不修改 PDK |
| 0022 (Plugin loading) | 🟢 低 | 服务启动时 load plugin, lifecycle 不变; 确保 service shutdown 先卸载 plugin |
| 0023 (ToolResult) | 🟡 中 | OpenAI API 需 ToolResult → OpenAI 格式映射, 注意信息丢失; 映射层放 service adapter, ToolResult 原始结构不动; contract test 覆盖 round-trip |

### 🚨 最危险不变量: ADR-0020 (Thread Model Isolation)

**风险描述**: 服务化首次引入**真正的并发请求路径**, 之前的测试都是单线程或受控并发 (per-agent worker 隔离). 必须 Week 4 做并发压测 + ASan/TSan.

**缓解策略**:
- **实现**: 每个请求 request-scoped DSLEngine 实例 (最简单) 或共享 DSLEngine + ADR-0033 session 隔离
- **测试**: Week 4 TSan 并发压测, 1000+ 并发请求无 data race
- **Gate**: TSan 0 errors = ship gate 硬阻断项

---

## C19 / C20 决策

### C20 (analysis-service placeholder) → ✅ **激活**

**与 Candidate B 直接对齐**. analysis-service 就是 service-ification 的核心交付物之一.

**触发条件**: Phase 6 kickoff 立即激活. C20 从 placeholder 升级为正式 OpenSpec change, scope = "暴露 DSLEngine 分析能力为 MCP tools + OpenAI API endpoints".

### C19 (fork-checkpoint placeholder) → ⏸ **推迟 (非归档)**

**与 Candidate B 不直接对齐** — fork-checkpoint 是自进化的回滚安全网, 服务化不需要 fork-rollback.

**不归档理由**: ADR-0033 session hierarchy 已覆盖请求级隔离, 但 C19 的 "checkpoint" 语义在 Phase 7+ 自进化仍有价值.

**触发条件**: 重新评估时机 = Candidate A (自进化) 正式启动时. 若 Phase 7 仍不启动, 且 ADR-0033 证明足够, 则归档 C19.

---

### Solo Dev 重新评估后续 Action (2026-07-15)

**Phase 6 服务化 (Candidate B) → ⏸ 暂缓, 结构性降级为 Path-dependent 启动条件.**

原 C20 决策 (analysis-service 激活) 暂停. 等以下条件重新满足时再评估:

1. PDK 生产化达 Sprint 25 末里程碑 (SafeExec 重写 + 文档 + 真实 LLM 集成完成)
2. AgentForge MVP 验证 (≥1 个领域 agent 通过 PDK 调用成功)
3. **同时**, Phase 6 服务化范围文档可压缩到 ≤1 周完成 (Solo dev 适配)

满足上述 3 项后, 可重新评估 C20 OpenSpec change 创建 (估时窗口: Sprint 26 末).

---

## 风险评估 (Top 3 + 缓解)

### Risk 1: 服务稳定性成为 critical path

- **影响**: 外部消费者一旦依赖 API, 回归即破坏下游
- **缓解**: API 版本化 (`/v1/`) + contract test 锁定 + canary 发布模式 + SemVer

### Risk 2: 并发请求隔离违反 ADR-0020 🚨 **硬阻断项**

- **影响**: 首次引入真实多请求并发, 可能 data race
- **缓解**: request-scoped DSLEngine 实例 (最简单) 或 session 隔离; **Week 4 TSan 强制 gate**

### Risk 3: OpenAI API 兼容性漂移

- **影响**: 上游 API 演进, 维护成本持续
- **缓解**: pin 到特定 API 快照版本; HydraForge 扩展放独立 namespace (`x-hydraforge-*`); 文档明确标注 deviations

---

## 估时细化 (4-6 周, 1-2 工程师)

| 周 | 交付物 | 工程师 |
|----|--------|--------|
| **W1** | 服务化范围文档 + 本 ADR 草稿 + C20 激活 + 集成目标确认 | 1 (设计) |
| **W2** | InferenceServer MCP server skeleton (`list_models` + `generate`) + DSLEngine 集成 + MCP 协议 unit test | 1-2 |
| **W3** | OpenAI-compatible `/v1/chat/completions` + `/v1/models` + ToolResult → OpenAI 映射层 | 2 (并行: endpoints + 映射) |
| **W4** | 并发隔离实现 (request-scoped) + TSan/ASan 并发压测 + ADR-0020 gate | 2 |
| **W5** | E2E 集成测试 (模拟外部消费者) + API 文档 + migration guide | 2 |
| **W6** | Buffer: bug 修复 + 性能 baseline + ship gate (ctest + ASan + openspec validate) | 2 |

### Ship Gate (硬阻断)

- ctest 72+N/N PASS
- ASan 全绿
- **TSan 并发 0 errors** (ADR-0020 gate)
- openspec validate exit 0
- ≥1 个外部消费者 E2E demo

### 已有基础设施降低风险

- `tests/test_http_adapter.cpp` 证明 httplib::Server 可用 (OpenAI API 的 HTTP 层无需新依赖)
- ToolRegistry + DSLEngine 提供 backend
- ADR-0033 session hierarchy 已支持请求级隔离
- `tests/test_helpers/http_mock_server.h` 可复用于 OpenAI API E2E 测试

---

## 后续行动

1. **Sprint 22 (2026-07-10 ~ 2026-07-31)**: C18 ship + archived; 等待 Stage Gate 重新评估 (2026-07-18)
2. **Sprint 23 (2026-07-19 ~ 2026-07-25)** (若决议启动 Phase 6):
   - 创建 OpenSpec change `phase6-service-ification` (W1 设计)
   - C20 placeholder 升级为正式 change
3. **Master Plan 更新**: `2026-07-03-phase5-self-bootstrapping.md` §十一 Adjustment Log 追加本 ADR 引用 + §十二 Strategic Pivots Log 追加 Phase 6 启动决议
4. **本 ADR 状态**: 待评审 → 若 Sprint 23 启动 Phase 6 实施, 状态升级为 `✅ Approved`

---

**最后更新**: 2026-07-10 (C18 Day 2, Oracle session `ses_0ae4b8107ffetONLmb2Sv2wTb5` 输出合成)
**状态**: 🔍 Proposed (待 Sprint 23 启动评审)
**关联**: [Drift Gate ✅](../audits/2026-07-10-drift-gate.md), [Stage Gate 推迟决议](../handoff/2026-07-31-stage-gate-evaluation.md)