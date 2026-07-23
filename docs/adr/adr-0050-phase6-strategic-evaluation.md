# ADR-0050: Phase 6 战略方向评估 — 从服务化到 PDK 生产化

## 状态

✅ Approved (2026-07-23 — Solo Dev 重估生效: Candidate B 结构性阻塞, 转向 PDK 生产化 + AgentForge MVP 验证。原始 Oracle 评估记录保留, 重开条件见 §Candidate B 重开条件)

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

### 裁决: Candidate B (服务化) 暂缓 — Solo Dev 结构性阻塞

Phase 6 Candidate B 因以下硬前置条件不满足而**暂缓实施**：

| # | 条件 | 状态 | 阻塞判断 |
|:---:|------|:---:|------|
| 1 | Phase 5 完全关闭 | ✅ | — |
| 2 | 服务化范围文档批准 | 🔒 | 非阻塞, 可随时启动 |
| 3 | C20 placeholder 决议 | ✅ | 已决议但随 B 冻结 |
| **4** | **8-10 周连续 solo dev 投入** | 🔴 | **结构性阻塞** — solo dev 现实下不可行 |
| **5** | **≥1 个真正外部消费者** | 🔴 | **阻塞** — AgentForge 为同一构建者, 不构成外部验证 |

**结论**: Candidate B 在 Solo Dev 约束下不可行。不推翻原 Oracle 评估 (B 仍是容量匹配度最高的方向), 但承认启动条件不满足的现实。

### 新方向: PDK 生产化 + AgentForge MVP 验证

优先级从"暴露服务接口"切换为"巩固 PDK 基础 + 用 AgentForge 验收 PDK 可用性":

| 阶段 | 范围 | 估时 | 完成标准 |
|------|------|:---:|------|
| **Phase 6a** | PDK 生产化 | 2-4 周 | ① manifest 校验补全 (PluginLoader 预读 `pdk_manifest.json`) + ② SafeExec 超时/异常隔离真实测试 + ③ PDK API 文档 (`pdk.h` doxygen 注释覆盖率 ≥80%) + ④ ctest 加入 PDK 插件 manifest 校验 case |
| **Phase 6b** | AgentForge MVP | 2-4 周 | ① ≥1 个领域 agent 通过 `DEFINE_AGENT` + PDK 构建 + ② 该 agent 通过真实 LLM (非 Mock) 跑通一次完整交互 + ③ 产出 AgentForge 的 1 页 README |
| **Phase 6c** | 重新评估服务化 | N/A | ① Phase 6a + 6b 完成标准全部达到 + ② ≥1 个真实外部消费者出现 |

### 对存量 ADR 的影响

本裁决后, ADR-0052~0067 分类如下:

**Phase 6a 激活** (PDK 生产化直接需要):
| ADR | 行动 |
|-----|------|
| ADR-0052 §决策 1 (manifest 预读校验) | ✅ 实施 (P0, 2-3 天) |

**Phase 6b 待命** (AgentForge 需要时才启动):
| ADR | 行动 |
|-----|------|
| ADR-0053 AgentDescriptor 接口 | ⏸ 待 Phase 6b |
| ADR-0058 Tool Schema Validation | ⏸ 待 Phase 6b |

**⏸ 冻结** (服务化暂缓, 不做):
| ADR | 行动 |
|-----|------|
| ADR-0054 CapabilityRegistry 运行时 | ⏸ 冻结 |
| ADR-0057 Agent Lifecycle | ⏸ 冻结 |
| ADR-0059 Cross-Process Protocol | ⏸ 冻结 |
| ADR-0060 Agent Composition | ⏸ 冻结 |

**⏸ 维持** (不因本裁决改变):
| ADR | 行动 |
|-----|------|
| ADR-0061 P0 全部 6 项 | ⏸ 维持 Approved-but-unimplemented |
| ADR-0061 P2 全部 6 项 (PASTE/AFlow/GEPA 等) | ⏸ 维持 🔍 Proposed |
| ADR-0056/0062/0063/0064/0065 | ⏸ 维持 Approved-but-unimplemented |

**✅ 已完成** (不受本裁决影响):
| ADR | 行动 |
|-----|------|
| ADR-0051 PDK Composition Spike | ✅ Phase 6 W1+W2+W3 已 ship |
| ADR-0055 SKILL.md 隔离 | ✅ Sprint 22 已 ship |
| ADR-0066 Skill Interpreter 架构 | 🟡 Partial (V1 done) |
| ADR-0067 L4 分层架构 | ✅ 追溯性 Approved (代码已落地) |

---

### 历史记录: 原始 4 候选评估 (2026-07-10)

> **标注**: 以下为 Phase 5 收官时 Oracle 的原始战略评估, **已被上述 Solo Dev 重估覆盖**。保留作为决策链的完整记录。

**Oracle 推荐方向: Candidate B (服务化)**

**1-2 句话理由**:
> Phase 5 已在 service-ification 方向投入沉没成本 (ADR-0019/0020/0033 + C12 YIELD + C16 ILLMProvider v2), Candidate B 以 4-6 周 / 1-2 工程师的可负担代价完成这条路径, 产出可预测、可消费的外部接口 (MCP + OpenAI-compatible API), 边际收益最确定.

**为什么不选其他 3 个**:

| 候选 | 拒绝理由 |
|------|---------|
| **A 自进化** | 8-12 周超出容量 2x; Quality signal noise + reward hacking 是研究问题而非工程问题, 小团队不适合承担高方差研究 |
| **C 第三方生态** | 网络效应需要 critical mass, 社区尚不存在; 1-2 工程师无法同时建设 contribution 基础设施 AND seed 社区 |
| **D Cloud-native** | 8-12 周超出容量; LLM 推理延迟敏感 + 有状态, cloud cold-start 是灾难性匹配; ADR-0042 §C16 §5 仍 🔍 Proposed |

**Oracle 引用**:
- **Session**: `ses_0ae4b8107ffetONLmb2Sv2wTb5` (2026-07-11, Phase 6 战略评估)
- **核心论证**: 团队容量是 binding constraint, 路径依赖决定 B 优先, 外部触发因素 (C16 §5) 未到

---

### Solo Developer 重新评估 (2026-07-15) — 导致本次裁决的直接依据

**触发**: 用户 2026-07-15 确认 HydraForge 由 **单一开发者**维护, 且已规划基于 HydraForge PDK 启动下游项目 **AgentForge** 作为领域 Agent 实现载体。

**对原决策的影响**:

| §项 | 原措辞 | Solo Dev 修正 | 影响 |
|------|--------|-------------|------|
| §决策推荐 B | "1-2 工程师 × 4-6 周" | "1 人 × 6-10 日历周" | B 估时翻倍; 服务化预算 vs. PDK 生产化预算互斥 |
| §启动条件 #4 | "1-2 工程师 4-6 周无中断可用" | 重写为 solo dev 容量 | 字面失效 |
| §启动条件 #5 | "≥1 个外部 agent/tool" | AgentForge 非真正外部实体 | 不满足 |
| §不变量 ADR-0020 | "Week 4 TSan" | "Week 8-10 TSan" (日历时间扩展) | 缓解窗口拉长 |

**修正后的方向**: 暂缓 Phase 6 服务化, 优先 PDK 生产化 + AgentForge MVP 验证 (单一开发者路径)。即为本裁决的主决策。

---

## Candidate B 重开条件

Phase 6 服务化在以下条件**全部**满足时可重新评估:

| # | 条件 | 当前状态 |
|:---:|------|:---:|
| 1 | PDK 生产化完成 + AgentForge MVP 验证通过 (Phase 6a + 6b) | 🔒 |
| 2 | Solo dev 有 ≥6 周连续可用窗口 (非原估 8-10 周) | 🔒 |
| 3 | 识别到 ≥1 个真正的、独立于 HydraForge 的外部消费者 | 🔒 |
| 4 | C16 §5 Cloud plugin 状态不阻塞 (仍为可选依赖) | 🔒 |

> **当前状态**: 🔒 全部未满足, 服务化暂缓。Phase 6c 为自然触发点。

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

1. **Sprint 24+**: 启动 Phase 6a — PDK 生产化 (manifest 校验补全 + SafeExec 测试 + doxygen 文档, 2-4 周)
2. **Sprint 25+**: 启动 Phase 6b — AgentForge MVP (首个领域 agent PDK 验证, 2-4 周)
3. **Sprint 26+**: 启动 Phase 6c — 重新评估服务化 (基于 Phase 6a/6b 完成状态 + 重开条件)
4. **文档同步**:
   - `docs/active-status.md` Phase 6 行更新为 "✅ Approved (PDK 生产化)"
   - `docs/README.md` ADR-0052~0060 表中标注 `⚠` (Approved-but-unimplemented) 标签区分已验证契约
   - `docs/adr/adr-0051-phase6-pdk-composition-spike.md` 补充 G1/G3 spike 转正条件 (若尚未写入)
5. **Master Plan 更新**: `docs/superpowers/plans/` 新建 Phase 6a PDK 生产化 plan

---

**最后更新**: 2026-07-23 (ADR-0050 裁决: Solo Dev 重估生效, Candidate B 暂缓, 转向 PDK 生产化)
**状态**: ✅ Approved (Solo Dev 重估裁决)
**关联**: [Drift Gate ✅](../audits/2026-07-10-drift-gate.md), [Stage Gate 推迟决议](../handoff/2026-07-31-stage-gate-evaluation.md), [ADR-0051 Spike](./adr-0051-phase6-pdk-composition-spike.md)