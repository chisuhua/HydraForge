# Active Status Board

> **焦点**: 当前活跃的 OpenSpec changes | **更新**: 每日
> **Master Plan**: [`docs/superpowers/plans/2026-07-16-pdk-chat-demo-implementation.md`](superpowers/plans/2026-07-16-pdk-chat-demo-implementation.md)
> **架构决策**: [`docs/adr/`](adr/) — 46 ADR (含 14 子节点), 32 Approved, adr_lint 零错误
> **Phase**: 6 — Agent-as-Plugin (2026-07-15 ~ 至今, Phase 5 ✅ 收官)

---

## 一、快速概览

| 维度 | 状态 |
|------|------|
| **Total ctest** | **83/83 ✅** PASS (baseline + Sprint 1~22 累计) |
| **ASan** | **83/83** (debug mode; 0 pre-existing failures) |
| **TSan** | 超时跳过 (机器性能受限) |
| **OpenSpec active** | **0** (Phase 6 采用 plan + commit 模式, 不创建 OpenSpec changes) |
| **ADR Approved** | **33** (Phase 0-5: 18 active + Phase 6 架构评审: 14 + ADR-0067 追溯性) |
| **ADR 🔍 Proposed** | **7** (含 ADR-0050 Phase 6 战略, ADR-0030 V2 → 🟡 Partial 2026-07-23) |
| **Completed Phase 0-4** | ✅ 100% |
| **Phase 5** | ✅ 收官 (C9-C18 全部 ✅ shipped + archived) |
| **Phase 6** | 🟡 Agent-as-Plugin: pdk_chat_demo ✅ / skill_interpreter ✅ / AgentForge MVP 设计中 |

---

## 二、活跃变更一览

### 🔵 当前活跃 (0 个)

> Phase 6 决定**不采用 OpenSpec change 仪式**启动 (Solo dev 管理简化), 改用 **直接 plan + 直接 commit** 模式。ADR 文档记录决策但无 openspec/ 变更目录。详见 [`docs/GOVERNANCE.md`](GOVERNANCE.md) docs→tasks 驱动流程。

### ✅ 已归档 (历史参考)

> Phase 5 全部 8 个 OpenSpec changes (C9-C16) 已于 2026-07-03 ~ 2026-07-09 ship + archived；C17/C18 已于 2026-07-10 ship + archived；C19 (phase6-service-ification-v1) 已于 2026-07-15 ship + archived。Phase 6 (服务化) 暂缓, Phase 6-Redirect (AgentForge MVP) 不创建 OpenSpec change。下表为最近归档的变更状态汇总，**仅作历史参考**。

| ID | 名称 | 阶段 | 状态 | 最后更新 |
|----|------|------|:----:|:--------:|
| **C19** | Phase 6 PDK Composition Spike (`phase6-service-ification-v1`) | ✅ Done | **✅ shipped + archived 2026-07-15** — ADR-0051 ✅ Approved (experimental) + spike-onboarding.md + ToolCoordinator RAII + 5 escalation triggers + Layer 3 dual memos + G1+G3 plugins (8 tests) + E2E (3 tests) + ctest 77/77 PASS + §12/§13 deferred to Sprint 24+ | 2026-07-15 |

| ID | 名称 | 阶段 | 状态 | 最后更新 |
|----|------|------|:----:|:--------:|
| **C18** | Phase 5 Sprint 22 Drift + Strategic Gate (`2026-07-10-phase5-sprint22-drift-strategic-gate`) | ✅ Done | **✅ shipped + archived 2026-07-10** |
| **C17** | Phase 5 ADR States Final Sync (`2026-07-10-phase5-adr-states-final-sync`) | ✅ Done | **✅ shipped + archived 2026-07-10** |
| **C16** | ILLMProvider Call Chain V2 (`phase5-illmprovider-call-chain-v2`) | ✅ Done | **✅ shipped + archived 2026-07-09 (§5 顺延)** |
| **C15** | Batching Queue Plugin (`phase5-batching-queue-plugin`) | ✅ Done | **✅ shipped + archived 2026-07-07 (D2 精简版)** |
| **C14** | Llama Engine Plugin (`phase5-llama-engine-plugin`) | ✅ Done | **✅ shipped + archived 2026-07-08** |
| **C13** | B2 架构层 Schema (`phase5-b2-arch-schemas`) | ✅ Done | **✅ shipped + archived 2026-07-07** |
| **C12** | Phase 5 Step 2 Yield Stream (`2026-07-04-phase5-stage1-step2-yield-stream`) | ✅ Done | **✅ shipped + archived 2026-07-04** |
| **C11** | Phase 5 Step 1 Session Registry (`2026-07-04-phase5-stage1-step1-session-registry`) | ✅ Done | **✅ shipped + archived 2026-07-04** |
| **C10** | Phase 5 Step 0 Lazy ModuleState (`2026-07-03-phase5-stage1-step0-lazy-modulestate`) | ✅ Done | **✅ shipped + archived 2026-07-03** |
| **C9** | Phase 4.5 Impl-Scope Audit (`2026-07-03-phase4-5-impl-scope-audit`) | ✅ Done | **✅ shipped + archived 2026-07-03** |

### 状态图例

| 标记 | 含义 |
|:----:|------|
| 📋 Proposal | proposal.md 已完成 |
| 📐 Tasks | tasks.md 已完成 |
| 📝 Spec | spec.md 已完成 |
| 🔨 编码 | 正在写代码 |
| ✅ Done | 全部 ship gate 通过 |
| 🔒 阻塞 | 等待外部条件 |
| ⏸ 暂缓 | 结构性暂缓 (ADR-0050 Solo Dev 重新评估) |
| ➡️ 顺延 | 无启动触发条件 |
| 🚀 New Sprint | 新 Sprint 启动 (无 OpenSpec change 仪式) |

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
| ⏸ **Phase 6 服务化 (Candidate B)** | **结构性暂缓** — ADR-0050 §启动条件 #4 Solo Dev 容量 + #5 AgentForge 非真正"外部" 双重不满足 | (1) PDK 生产化达 Sprint 25 末里程碑; (2) AgentForge MVP 验证; (3) 服务化范围文档 ≤1 周可完成 (Solo dev 适配) | ADR-0050 Solo Dev 重新评估 (2026-07-15) + 新 plan `2026-07-15-phase6-agentforge-mvp.md` |
| ➡️ C16 §5 Cloud plugin 顺延 | 持续关注 | 外部触发 (CloudLLMProvider 实施需求) | 独立 OpenSpec change `phase5-illmprovider-call-chain-v3` 跟踪 (受 Phase 6 暂缓影响) |
| ➡️ C17 排除 ADR-0030 V2 顺延 | Fleet 实施需求 | FleetOrchestrator 解除延迟 (Oracle 2026-06-27 决议) | C19 或后续 ship 时迁移至 🟡 Partial |
| ➡️ C17 排除 ADR-0037 顺延 | 因果排序机制未实施 | Phase 7+ 自进化 | 由 AgentForge 使用情况触发 |
| ➡️ C17 排除 ADR-0038 顺延 | 推理引擎动态配置接口未实施 | 第二个推理 backend 出现时 (per ADR-0038 §增量决议) | C15 实施后由 C18 重新评估 |
| ➡️ C17 排除 ADR-0039 顺延 | JSON 查询工具 (`inference/get/status`) 未实现 | 实际外部消费者触发时 | 由 AgentForge 反馈触发 |
| ➡️ C17 排除 ADR-0042 顺延 | ILLMProvider 演进路径仅部分决策实施 | C16 §5 Cloud 插件 + 第 2 阶段重新映射交付后 | Phase 6 服务化重新评估时处理 |
| ➡️ C17 排除 ADR-0045 顺延 | 编排 Plugin 仅 step 2 部分交付 (~20% 实施) | Phase 6 实施 | 由 AgentForge 多 agent 协作需求触发 |
| ➡️ C17 排除 ADR-0046 顺延 | 4 通道架构仅通道 ① 完成 (~25% 实施) | Phase 6 实施 | 由 AgentForge 多通道需求触发 |
| 🔒 ADR-0050 §启动条件 #5 字面要求 | AgentForge = HydraForge 同人项目, 非真正"外部 agent/tool" | Oracle round 4 重新评估 (选项 A/B/C per ADR-0051 §后续 #9 注释) | Sprint 24-25 后评估 |
| ✅ ADR-0051 Phase 6 PDK Composition Spike | ✅ Approved (experimental) 2026-07-15 | 已满足 | 一 |
| 🚀 Sprint 24 AgentForge MVP | 启动中 | 不需要 (已启动) | plan 文件 `2026-07-15-phase6-agentforge-mvp.md` |

> **C17 排除原因明细**: 详见 [`docs/superpowers/plans/2026-07-10-phase5-remainder-adr-sync.md` §十一.3](superpowers/plans/2026-07-10-phase5-remainder-adr-sync.md) (Metis 审查 `ses_0b02706b7ffepKdYy3qxnmOzXy` 裁决后用户选项 A 决策 2026-07-10, 范围 12 → 5)
>

> **2026-07-15 新增**: Phase 6 服务化结构性暂缓, 由 `2026-07-15-phase6-agentforge-mvp.md` 替代. 所有原 Phase 6 顺延项 (ADR-0037/0038/0039/0042/0045/0046 + ADR-0050 #5) 启动条件统一改为 "AgentForge 使用反馈触发".

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

> **2026-07-15 方向重定向**: Phase 6 (服务化) 结构性暂缓 (见 ADR-0050 §决策 Solo Developer 重新评估段); 转向 PDK 生产化 + AgentForge MVP 路径 (新 plan: `docs/superpowers/plans/2026-07-15-phase6-agentforge-mvp.md`).

### Sprint 24 (2026-07-15 ~ 2026-07-29, 2 周)

1. **本周 (Day 1-2)**: 创建 `agentforge/` 独立项目脚手架
   - 拷贝 `examples/agent_basic/` 作为种子
   - 实现 1 个微型领域 agent (任何领域都行, 目标是建立 PDK 连接)
   - 验证 `DSLEngine::set_llm_provider()` + DECLARE_TOOL 调用成功
   - **边界**: 不超过 4 小时; 不要追求完美
2. **Week 1**: SafeExec 重写 (PDK 最高风险修复)
   - `std::async` + `wait_for` → `std::jthread` + `stop_token` 或复用 `DomainWorkerPool`
   - TDD: 先写测试验证线程不泄漏
3. **Week 2**: PDK README + 真实 LLM 路由示例
   - `include/agenticdsl/pdk/README.md` (~2 页贯穿示例)
   - Demo: AgentForge agent 通过 `ILLMProvider` (非 Mock) 调用

### Sprint 25 (2026-07-29 ~ 2026-08-12, 2 周)

4. **PDK 开发者指南完整化**: 文档 + 3 个 agent loop 示例
5. **AgentForge 第 2 个领域 agent**: 验证 PDK 复用性
6. **Sprint 24 末决策点**: 评估 Phase 6 服务化是否重新启动

### 进化管线服务 (中长期, 不阻塞 Phase A)

> 详见 [`docs/architecture/agent-evolution-pipeline.md` §八](architecture/agent-evolution-pipeline.md)。启动条件由实际需求驱动, 不预设 timeline。

| 服务 | 阶段 | 当前状态 | 启动条件 |
|------|:----:|---------|:--------:|
| `SkillInterpreter` V2 完善 | 1 | 🟡 Partial (V1 done) | Sprint 24-25, 3 项 defer 范围 |
| `DSLValidator` 增强 | 2 | ❌ 无代码 | Sprint 24, 靠 DSLEngine (已有) |
| Wasm 技术栈预研 | 4 | ❌ 无代码 | Sprint 24-25, ADR-0056 已定义 |
| `SolidificationEngine` | 1→2 | ❌ 无代码 | DSLValidator + SkillInterpreter V2 完成后 |
| `WasmRuntime` PoC | 4 | ❌ 无代码 | wasi-sdk 通路验证 + AgentForge MVP 完成 |
| `RegressionSuite` | 全阶段 | ❌ 无代码 | SolidificationEngine V1 完成后 |
| `WasmCompiler` | 2→4 | ❌ 无代码 | WasmRuntime 完成后 |
| `C++CodeGenerator` | 2→3 | ❌ 无代码 | SolidificationEngine 完成后 |

### 已归档的 Phase 6 服务化路径

- ⏸ **Phase 6 服务化 (Candidate B)** 暂缓, 启动条件重新定义 (见 §四)
- ⏸ **C20 (analysis-service)** 暂停, 等 Sprint 24-25 末评估 (ADR-0050 §决策 Solo 重新评估段)
- ⏸ **C16 §5 Cloud plugin** 顺延不变 (独立 change 跟踪)
- ⏸ **C19 (fork-checkpoint)** 推迟不变, 触发条件 = Phase 7+ 自进化
- ✅ **ADR-0051 ✅ Approved (experimental)** 状态保留, 不影响其他 ADR

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
