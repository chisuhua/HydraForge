# Active Status Board

> **焦点**: 当前活跃的 OpenSpec changes | **更新**: 每日
> **Master Plan**: [`docs/superpowers/plans/2026-07-03-phase5-self-bootstrapping.md`](superpowers/plans/2026-07-03-phase5-self-bootstrapping.md)
> **架构决策**: [`docs/adversarial-reviews/decisions-2026-07-07.md`](adversarial-reviews/decisions-2026-07-07.md)
> **Phase**: 5 — 自举服务化 (2026-07-03 ~ 2026-10-31)

---

## 一、快速概览

| 维度 | 状态 |
|------|------|
| **Total ctest** | **72/72 ✅** PASS (baseline 25 + Sprint 1~20 累计 47 新增) |
| **ASan** | 72/72 (100%) — `test_execute_parallel` use-after-scope 已修复 |
| **TSan** | 超时跳过 (机器性能受限, pre-existing data race 已修复) |
| **OpenSpec active** | **0** (`phase6-service-ification-v1` C20-Spike ✅ shipped + archived 2026-07-15) |
| **ADR Approved** | **20** (13 existing + 5 C17 FLIP + 1 ADR-0051 experimental; ADR-0031 Partial 不计入) |
| **ADR 🔍 Proposed** | **8** (C17 排除 7 + ADR-0051 Phase 6 Spike 2026-07-14; 详见 §四 顺延项) |
| **Completed Phase 0-4** | ✅ 100% |
| **Phase 5** | ✅ 收官 (C9-C18 全部 ✅ shipped + archived; C18 adr-0050 Oracle 推荐 Phase 6 启动 Candidate B 服务化) |
| **Phase 6** | 🟡 C20-Spike ✅ shipped (ADR-0051 ✅ Approved experimental + OpenSpec archived 2026-07-15); C20 kickoff: G2/G4/G5 teams use spike-onboarding.md |

---

## 二、活跃变更一览

### 🔵 当前活跃 (0 个)

### ✅ 已归档 (历史参考)

> Phase 5 全部 8 个 OpenSpec changes (C9-C16) 已于 2026-07-03 ~ 2026-07-09 ship + archived；C17/C18 已于 2026-07-10 ship + archived；C19 (phase6-service-ification-v1) 已于 2026-07-15 ship + archived。下表为最近归档的变更状态汇总，**仅作历史参考**。

| ID | 名称 | 阶段 | 状态 | 最后更新 |
|----|------|------|:----:|:--------:|
| **C19** | Phase 6 PDK Composition Spike (`phase6-service-ification-v1`) | ✅ Done | **✅ shipped + archived 2026-07-15** — ADR-0051 ✅ Approved (experimental) + spike-onboarding.md + ToolCoordinator RAII + 5 escalation triggers + Layer 3 dual memos + G1+G3 plugins (8 tests) + E2E (3 tests) + ctest 77/77 PASS + §12/§13 deferred to Sprint 24+ | 2026-07-15 |

| ID | 名称 | 阶段 | 状态 | 最后更新 |
|----|------|------|:----:|:--------:|
| **C18** | Phase 5 Sprint 22 Drift + Strategic Gate (`2026-07-10-phase5-sprint22-drift-strategic-gate`) | ✅ Done | **✅ shipped + archived 2026-07-10** | 2026-07-10 |
| **C17** | Phase 5 ADR States Final Sync (`2026-07-10-phase5-adr-states-final-sync`) | ✅ Done | **✅ shipped + archived 2026-07-10** | 2026-07-10 |
| **C16** | ILLMProvider Call Chain V2 (`phase5-illmprovider-call-chain-v2`) | ✅ Done | **✅ shipped + archived 2026-07-09 (§5 顺延)** | 2026-07-09 |
| **C15** | Batching Queue Plugin (`phase5-batching-queue-plugin`) | ✅ Done | **✅ shipped + archived 2026-07-07 (D2 精简版)** | 2026-07-07 |
| **C14** | Llama Engine Plugin (`phase5-llama-engine-plugin`) | ✅ Done | **✅ shipped + archived 2026-07-08** | 2026-07-08 |
| **C13** | B2 架构层 Schema (`phase5-b2-arch-schemas`) | ✅ Done | **✅ shipped + archived 2026-07-07** | 2026-07-07 |
| **C12** | Phase 5 Step 2 Yield Stream (`2026-07-04-phase5-stage1-step2-yield-stream`) | ✅ Done | **✅ shipped + archived 2026-07-04** | 2026-07-04 |
| **C11** | Phase 5 Step 1 Session Registry (`2026-07-04-phase5-stage1-step1-session-registry`) | ✅ Done | **✅ shipped + archived 2026-07-04** | 2026-07-04 |
| **C10** | Phase 5 Step 0 Lazy ModuleState (`2026-07-03-phase5-stage1-step0-lazy-modulestate`) | ✅ Done | **✅ shipped + archived 2026-07-03** | 2026-07-03 |
| **C9** | Phase 4.5 Impl-Scope Audit (`2026-07-03-phase4-5-impl-scope-audit`) | ✅ Done | **✅ shipped + archived 2026-07-03** | 2026-07-03 |

### 状态图例

| 标记 | 含义 |
|:----:|------|
| 📋 Proposal | proposal.md 已完成 |
| 📐 Tasks | tasks.md 已完成 |
| 📝 Spec | spec.md 已完成 |
| 🔨 编码 | 正在写代码 |
| ✅ Done | 全部 ship gate 通过 |
| 🔒 阻塞 | 等待外部条件 |
| ➡️ 顺延 | 无启动触发条件 |

---

## 三、各变更详情

### C13 / `phase5-b2-arch-schemas`

| 属性 | 内容 |
|------|------|
| **目标** | 创建 4 个架构层 `.md` schema: `prefix_cache.md`, `kv_cache.md`, `decoding.md`, `cloud_engine.md` |
| **D1 已应用** | SamplerStrategy PDK 接口 **删除** — decoding.md 的 sampler 字段保持字符串选择 (5 种) |
| **D3 已应用** | 命名风格统一 `inference.*` |
| **Proposal** | ✅ 干净 (SamplerStrategy 引用全部删除) |
| **Tasks** | ✅ 干净 (§3.2 删除) |
| **Spec** | ✅ 干净 |
| **ship 状态** | ✅ **shipped 2026-07-07** — 4 个 `.md` schema 文件落地 + handoff §10.2 + master plan §C13 已标记 |
| **ship 内容** | `lib/inference/prefix_cache.md` + `kv_cache.md` + `decoding.md` + `cloud_engine.md` (PLACEHOLDER) |
| **验证** | `adr_lint` exit 0 + `openspec validate` exit 0 + `docs_drift_audit` 0 DRIFT + `grep sampler_strategy` 0 matches |

### C14 / `phase5-llama-engine-plugin`

| 属性 | 内容 |
|------|------|
| **目标** | 在 `pdk/llama_engine/` 创建 engine/model 工具实现 (`inference/engine/*`, `inference/model/*`) |
| **D1 已应用** | SamplerStrategy 相关任务删除 (采样器 clamp 内联到 generate) |
| **D3 已应用** | 8 处工具名 `llama_engine/*` → `inference/engine/*`, `llama_model/*` → `inference/model/*` |
| **Proposal** | ✅ 已更新 |
| **Tasks** | ✅ 已更新 (§4 删除, 8 处工具名替换) |
| **Spec** | ✅ 已更新 |
| **剩余工作** | C14 编码: `pdk/llama_engine/` 目录 + 8 个工具注册 + 7 测试 |
| **估时** | ~2-3 天 |
| **启动条件** | **🔒 TSan gate 100%** (`test_execute_parallel` data race 修复验证) |
| **说明** | 编码与工具名重写已分离: 重写 30min 已完成, 编码等 TSan |

### C15 / `phase5-batching-queue-plugin`

| 属性 | 内容 |
|------|------|
| **目标** | 创建 `lib/inference/batching.md` PLACEHOLDER schema (40 行) |
| **D2 已应用** | BatchingQueue PDK 接口 + LlamaBatchingQueue + 贡献流程全部 **推迟** |
| **Proposal** | ✅ 精简完成 (367→98 行) |
| **Tasks** | ✅ 精简完成 (161→70 行) |
| **Spec** | ✅ 精简完成 (214→26 行) |
| **ship 状态** | ✅ **shipped 2026-07-07 (D2 精简版)** — `lib/inference/batching.md` PLACEHOLDER 已落地 |
| **ship 内容** | `lib/inference/batching.md` (~40 行 PLACEHOLDER,batching.submit_and_wait 工具签名) + handoff §10.2 + master plan §C15 已标记 |
| **D2 验证** | 未创建 `include/agenticdsl/pdk/batching_queue.h` + 未创建 `LlamaBatchingQueue` + 未写贡献流程 |

### C16 / `phase5-illmprovider-call-chain-v2`

| 属性 | 内容 |
|------|------|
| **目标** | ILLMProvider 调用链 v2 架构（D2' Dual Consumer Model + D3 ILLMProviderDecorator + D1 Cloud plugin 化 + D5 available_models pure virtual） |
| **依赖** | ADR-0035 (inference engine plugin spec) ✅ |
| **依赖** | ADR-0042 (ILLMProvider evolution path) ✅ |
| **依赖** | ADR-0045 (orchestration plugin spec) ✅ |
| **Proposal** | ✅ v2 修订完成 |
| **§1 Decorator** | ✅ CostTrackingDecorator + 链深度限制 + 流式精度 |
| **§2 Compliance/RateLimit** | ✅ ComplianceDecorator + RateLimitDecorator + DSLEngine opt-in flags |
| **§3 pure virtual** | ✅ `available_models()` =0 + 5 个 override |
| **§4 OrchestrationILLMProvider** | ✅ Dual Consumer Model 直连 + `test_orchestration_dual_consumer` 7 TC PASS |
| **§5 Cloud plugin** | 🔴 顺延（独立 change `phase5-illmprovider-call-chain-v3`） |
| **§6 PluginLoader** | ✅ 5 符号查找 + lifecycle + ABI v2 |
| **§7 ADR 文档** | ✅ ADR-0001/0035/0038/0042/0045/0005 修订 |
| **§8 Deprecate** | ✅ LlamaAdapter + LlamaAdapterProvider `[[deprecated]]` |
| **§9 Engine 集成** | ✅ `decorate_provider()` + 3 处直调路径全部经过装饰器链 |
| **§10 测试** | ✅ 72/72 ctest, ASan 72/72, `test_execute_parallel` fix |
| **ship 状态** | ✅ **可 ship**（§5 顺延） |
| **ASan** | ✅ 72/72 (test_execute_parallel use-after-scope 已修复) |

---

## 四、顺延项（无启动触发条件）

| 顺延项 | 影响 | 启动条件 | 处理方式 |
|--------|------|:--------:|---------|
| ➡️ C16 §5 Cloud plugin 顺延 | 持续关注 | 外部触发 (CloudLLMProvider 实施需求) | 独立 OpenSpec change `phase5-illmprovider-call-chain-v3` 跟踪 |
| ➡️ C17 排除 ADR-0030 V2 顺延 | Fleet 实施需求 | FleetOrchestrator 解除延迟 (Oracle 2026-06-27 决议) | C19 或后续 ship 时迁移至 🟡 Partial |
| ➡️ C17 排除 ADR-0037 顺延 | 因果排序机制未实施 | Phase 6 实施 | 由 C19/C20 范围评估 |
| ➡️ C17 排除 ADR-0038 顺延 | 推理引擎动态配置接口未实施 | 第二个推理 backend 出现时 (per ADR-0038 §增量决议) | C15 实施后由 C18 重新评估 |
| ➡️ C17 排除 ADR-0039 顺延 | JSON 查询工具 (`inference/get/status`) 未实现 | C19 实施 (Phase 6) | C19 plan 时纳入 |
| ➡️ C17 排除 ADR-0042 顺延 | ILLMProvider 演进路径仅部分决策实施 | C16 §5 Cloud 插件 + 第 2 阶段重新映射交付后 | C20 处理 |
| ➡️ C17 排除 ADR-0045 顺延 | 编排 Plugin 仅 step 2 部分交付 (~20% 实施) | Phase 6 实施 | C19/C20 范围评估 |
| ➡️ C17 排除 ADR-0046 顺延 | 4 通道架构仅通道 ① 完成 (~25% 实施) | Phase 6 实施 | C19/C20 范围评估 |
| 🔒 ADR-0050 §启动条件 #5 字面要求 | C20-Spike 内部 PDK 互调 vs ADR-0050 "外部 agent/tool" 字面冲突 | Oracle round 4 re-evaluation 确认内部 Spike 证据支持 (Spike→Candidate B 提升条件 #4) | C20-Spike → C20 正式启动时 |
| 🔒 ADR-0051 Phase 6 PDK Composition Spike | 🔍 Proposed 2026-07-14 (Phase 6 内部组合可行性 Spike; **不兑现** ADR-0050 Candidate B 战略目标) | Spike ship gate (W3 D14) + Oracle Q6 follow-up confirmation | C20-Spike W3 → archive 后翻 ✅ Approved (experimental) |
| 🔒 C20-Spike W2-W3 实施 | W2-W3 BLOCKED on (i) Stage Gate 2026-07-18 通过; (ii) Sprint 23 capacity 1.5 eng × 2 周; (iii) Oracle Q6 confirmation | 3 项 unlock 全部满足 | C20-Spike → W2-W3 启动 |

> **C17 排除原因明细**: 详见 [`docs/superpowers/plans/2026-07-10-phase5-remainder-adr-sync.md` §十一.3](superpowers/plans/2026-07-10-phase5-remainder-adr-sync.md) (Metis 审查 `ses_0b02706b7ffepKdYy3qxnmOzXy` 裁决后用户选项 A 决策 2026-07-10, 范围 12 → 5)
>
> **C20-Spike reframing**: Oracle session `ses_0a206a23cffe1IEirU5iNaxFxC` (二轮) + `ses_0a17108b5ffexaXTWhF8vXot6b` (Q1-Q6 决策) 共同裁定 C20 内部 PDK 组合与 ADR-0050 §决策 "外部 MCP/OpenAI API" 战略目标存在 reframing；Spike 保留 ADR-0050 §决策不动，新建 ADR-0051 作为 Phase 6 内部组合 Spike；Phase 6 正式 Candidate B v1 仍需满足 ADR-0050 §启动条件 5 项。

---

## 五、最近完成的变更

| 日期 | ID | 名称 | 关键 Ship |
|:----:|:--:|------|-----------|
| 2026-07-15 | C19 | phase6-service-ification-v1 Spike ship | ADR-0051 ✅ Approved (experimental) + spike-onboarding.md + ToolCoordinator RAII + 5 escalation triggers (6 tests) + G1+G3 plugins (8 tests) + E2E (3 tests) + ctest 77/77 PASS + ASan documented skip + `openspec validate --strict` EXIT 0 + OpenSpec archived. 5 commits. §12 (5 items) + §13 (7 items) deferred to Sprint 24+. C20 kickoff: G2/G4/G5 teams use spike-onboarding.md. |
| 2026-07-14 | C20-Spike | phase6-service-ification-v1 W1 fix list | Oracle Q1-Q6 决策应用 + ADR-0051 创建 (🔍 Proposed) + Spike framing (不兑现 ADR-0050 Candidate B) + DECLARE_TOOL→register_tool_function + slash 命名 (knowledge_base/query) + G3 ToolCategory::Execute + audit events 替代 ToolRegistry 注入 + W1 fix list 11/12 ✅ + 2nd Metis 0 CRITICAL + `openspec validate --strict` EXIT 0 + adr_lint EXIT 0 + docs_drift_audit 0 DRIFT. W2-W3 BLOCKED awaiting Stage Gate 2026-07-18 + Sprint 23 capacity. |
| 2026-07-10 | C18 | phase5-sprint22-drift-strategic-gate | Architecture Drift Gate (4 路 0 CRITICAL) + Strategic Alignment Gate (Oracle 推荐 Candidate B 服务化) + Stage Gate 推迟至 2026-07-18 + ADR-0050 🔍 Proposed 创建 + C20 placeholder 激活 + C19 推迟 |
| 2026-07-09 | C16 | illmprovider-call-chain-v2 | ILLMProvider v2: Decorator chain (CostTracking/Compliance/RateLimit) + Dual Consumer Model (OrchestrationILLMProvider) + available_models() pure virtual + PluginLoader V2 + DSLEngine opt-in flags. 72/72 ctest, ASan 72/72. §5 Cloud plugin deferred. |
| 2026-07-07 | — | c16-patches | C16 三处文档不一致 patch (active-status/proposal/specs) + D5 决策草稿 + proposal-v2.md (313 行, 5 项歧义消除) |
| 2026-07-07 | C15 | batching-queue-plugin | `lib/inference/batching.md` PLACEHOLDER (~40 行) + D2 精简 ship + handoff/master plan 同步 + openspec validate exit 0 |
| 2026-07-07 | C13 | b2-arch-schemas | 4 个 `lib/inference/{prefix_cache,kv_cache,decoding,cloud_engine}.md` schema 文件 + D1 SamplerStrategy 删除 + D3 命名统一 + handoff/master plan 同步 + 全部验证 exit 0 |
| 2026-07-06 | — | docs-cleanup-phase-2 | 5 个已 ship plan 归档, 22 个 openspec spec 补全, ADR 状态同步 |
| 2026-07-04 | C12 | yield-stream | 64/64 ctest, 9 个测试, YIELD 3 节点模式 + Budget 每 token 检查 |
| 2026-07-04 | C11 | session-registry | 63/63 ctest, SessionRegistry 5 方法, 4 个 session.* 工具 |
| 2026-07-03 | C10 | lazy-modulestate | module_states_ map lazy init |
| 2026-07-03 | C9 | adr-impl-scope-audit | 11 个 ADR 实施审计文档 |
| 2026-07-02 | C6 | tool-metadata-v2 | IToolRegistry + ToolMetadata V2 BREAKING |
| 2026-07-02 | C7 | model-router-plugin | 3 路由策略 .so, 4 个 registry 工具, 61/61 ctest |
| 2026-07-01 | — | http-mock-server-helper | HttpMockServer RAII helper, 消除 4 处模板重复 |
| 2026-06-30 | — | decompose-execution-session-h | execution_session.h 14→11 includes, 7→0 modules/ |
| 2026-06-30 | — | fix-audit-quick-debt-2026-06 | ToolResult::error BREAKING, PIMPL-lite MarkdownParser |
| 2026-06-25 | — | engine-include-final-decoupling | engine.cpp cross-module 10→3 |

---

## 六、下一步行动 (按当前焦点)

1. **C20-Spike ✅ shipped + archived**: ADR-0051 ✅ Approved (experimental), OpenSpec change `phase6-service-ification-v1` archived to `2026-07-15-phase6-service-ification-v1`. §12 (5 Post-Ship items) + §13 (7 Promotion Criteria) deferred to Sprint 24+.
2. **C20 kickoff (Sprint 24+)**: G2/G4/G5 teams use `docs/service-composition/spike-onboarding.md` as onboarding material. C20 scope: Phase 6 Candidate B v1 (ADR-0050 §决策 正式实施).
3. **C19 (fork-checkpoint) 推迟**: 与 Candidate A (自进化) 重新对齐, ADR-0033 已覆盖请求级隔离; C19 触发条件 = Candidate A 正式启动时
3. **C16 §5 Cloud plugin 顺延**: 独立 OpenSpec change `phase5-illmprovider-call-chain-v3` 跟踪 (C17 ship 后 ADR-0035 ✅ Approved 满足实施依赖)
4. **ADR-0051 状态翻 ✅ Approved (experimental)**: C20-Spike W3 ship gate 通过后自动翻; 不影响 ADR-0050 🔍 Proposed 状态
5. **ADR-0050 保持不动**: §决策 / §启动条件 全部保留; Phase 6 Candidate B v1 正式启动仍需满足全部 5 项硬前置

---

## 七、存档说明

> 以下历史看板已归档: 它们的 Phase 0-4 追踪已由 `docs/active-status.md` 替代。
>
> - **`docs/roadmap-status.md`** → `docs/archive/roadmap-status.md` (Phase 0-4 Sprint 日志, 最后更新 2026-06, 463 行)
> - **`docs/implementation-roadmap.md`** → `docs/archive/implementation-roadmap.md` (2016-06-03 旧蓝图, 829 行)

---

## 八、参考

| 文档 | 用途 |
|------|------|
| `docs/superpowers/plans/2026-07-03-phase5-self-bootstrapping.md` | 详细执行计划、依赖图、每 Change 精确实施步骤 |
| `docs/adversarial-reviews/decisions-2026-07-07.md` | B2 架构决策 D1-D4 正式记录 |
| `docs/adversarial-reviews/main-report.md` | Adversarial Review 完整报告 (5 大发现 + 4 方案对比) |
| `openspec/changes/` | 活跃 OpenSpec change 目录 (proposal/tasks/spec) |
| `docs/archive/roadmap-status.md` | Phase 0-4 历史 Sprint 追踪 (已归档) |
| `docs/archive/implementation-roadmap.md` | 旧实施路线图 (已归档) |
