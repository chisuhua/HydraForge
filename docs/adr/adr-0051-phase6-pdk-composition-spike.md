# ADR-0051: Phase 6 PDK Composition Spike

## 状态

🔍 Proposed (2026-07-14 — Oracle session `ses_0a17108b5ffexaXTWhF8vXot6b` 产出; W1 fix list 12/12 ✅ + 二次 Metis 0 CRITICAL ✅ 已完成,等待 W3 ship gate flip → ✅ Approved (experimental))

## 领域

Phase 6 / PDK 内部组合可行性 / Spike

## 关联

- [ADR-0050 (Phase 6 战略评估)](./adr-0050-phase6-strategic-evaluation.md) — **被其战略定位**：本 ADR 记录 Phase 6 正式启动前的内部组合可行性 Spike，**不兑现** ADR-0050 §决策 Candidate B 的战略目标
- [ADR-0019 (IInteractionBus)](./adr-0019-iinteraction-bus-mvp.md) — 跨模块 include 解耦约束 (engine.h 仅剩 1 modules/ include)
- [ADR-0020 (Thread Model Isolation)](./adr-0020-thread-model-isolation.md) — 线程隔离 (per-agent 隔离，物理隔离不是 v1 目标)
- [ADR-0021 (PDK Design)](./adr-0021-pdk-design.md) — PDK 头文件 API 约束
- [ADR-0022 (Plugin Loading)](./adr-0022-plugin-loading.md) — ToolCoordinator 嵌套深度约束
- [ADR-0023 (ToolResult)](./adr-0023-tool-result-standard.md) — IToolRegistry 接口契约
- [ADR-0031 (Execution Policy)](./adr-0031-execution-policy.md) — ToolCoordinator + 审计事件
- [ADR-0033 (Session Hierarchy)](./adr-0033-session-hierarchy.md) — session hierarchy
- [ADR-0043 (PDK Tool Naming)](./adr-0043-pdk-tool-naming-convention.md) — PDK 工具命名 (slash-only)
- [ADR-0044 (Inference Plugin Security)](./adr-0044-inference-plugin-security-model.md) — PDK 工具分类 (ToolCategory 矩阵)
- [ADR-0004 V2 (ToolRegistry Security)](./adr-0004-toolregistry-security.md) — ToolMetadata 4-param 注册

## Oracle Session

**`ses_0a17108b5ffexaXTWhF8vXot6b`** (2026-07-14, Oracle 复审) — Phase 6 PDK Composition Spike Q1-Q6 决策

---

## 背景

Phase 6 战略方向由 ADR-0050 定义 (Candidate B 服务化 = 外部 MCP/OpenAI API 暴露)。Oracle session `ses_0ae4b8107ffetONLmb2Sv2wTb5` 评估 Candidate B 为唯一匹配团队容量 + 兑现 Phase 5 路径投资的方向。

2026-07-10 团队 reframing: HydraForge 服务化的真实意图是 "PDK 开发的 Agent 可以互相提供服务" (Unix 微内核哲学)。但此 reframing 与 ADR-0050 §决策 字面表述 ("外部 MCP/OpenAI API") 不一致。

Oracle session `ses_0a206a23cffe1IEirU5iNaxFxC` 给出 v1 实施边界 (不引入 DECLARE_SERVICE，逻辑隔离)。Metis 审查 (`ses_0a1dd3355ffeUCmGTgLzyI1VXx`) 发现 4 个 critical blockers：DECLARE_TOOL 宏无法支持点号名称 / "不修改核心代码" 与 instrumentation 矛盾 / ADR-0050 §决策 措辞需战略重新定义 / G3 ReadOnly 分类语义不匹配。

Oracle 复审 (`ses_0a17108b5ffexaXTWhF8vXot6b`) 给出 6 项决策 (Q1-Q6)，决定本 Spike 不修改 ADR-0050 §决策，定位降级为 "Phase 6 正式启动前的内部组合可行性 Spike"。

---

## 决策

### Decision 1 — Spike Framing (Q6)

本 ADR 记录 Phase 6 内部 PDK 组合可行性 Spike。**不兑现** ADR-0050 Candidate B 战略目标。Phase 6 正式启动仍需满足 ADR-0050 §启动条件 5 项 (Phase 5 关闭 / 服务化范围文档批准 / C20 placeholder 决议 / 团队容量确认 / ≥1 外部集成目标)。

### Decision 2 — Service Contract (Q5)

Spike v1 服务合约使用现有 `IToolRegistry::call_tool` 签名 (`std::unordered_map<std::string, std::string> args` → `nlohmann::json result`)。**禁止**宣称 "JSON-in/JSON-out"。复杂输入值 JSON 编码到 string value 中。`call_tool_json()` overload 推迟到 Phase 6 正式实施 (满足 ADR-0050 §启动条件 #2 之后)。

### Decision 3 — Registration Pattern (Q1)

Spike 插件使用现有 `IToolRegistry::register_tool_function(name, metadata, lambda)` 模式 (与 `pdk/llama_engine/` / `pdk/model_router/` 一致)。**不**使用 `DECLARE_TOOL` 宏 (宏的 `##name` token-pasting 不支持 `.` / `/` 等非 C++ 标识符字符；所有现有 PDK 插件 100% 绕过 DECLARE_TOOL)。工具名遵循 ADR-0043 slash-only 规则：`knowledge_base/query`。

### Decision 4 — G3 ToolCategory (Q3)

G3 `knowledge_base/query` 工具分类为 `ToolCategory::Execute`，`allowed_layers = {LayerProfile::Workflow}`。理由：ADR-0004 V2 定义 `ReadOnly = ls/cat/grep/search` (纯只读操作)，不覆盖 LLM 生成 + session 状态管理。运行时安全保留 (G3 仅调 `MockLLMProvider::generate()`，不走 ToolCoordinator)。Layer 矩阵中 `check_layer_permission(Workflow, Execute) = true` 允许 Workflow 层调用 G3。

### Decision 5 — Core Code Modification Scope (Q4)

核心代码修改白名单化：
- **允许**：`ToolCoordinator` 嵌套深度 + 环检测 RAII guard (单一修改，behavioral change)
- **禁止**：`ToolRegistry::call_tool()` 全局 instrumentation
- **Layer 2 instrumentation**：复用现有 `tool.audit.{invoked,completed,denied}` 事件 (C4 ship)
- **Plugin-internal metrics**：G3 内部 metrics (session store 大小、error-as-success 比例) 通过 audit event self-report，不走核心代码

### Decision 6 — Escalation Trigger Re-classification (Q2)

5 个 escalation trigger 重新分类：
- **Runtime safety** (ToolCoordinator RAII guard): 嵌套深度 > 2 / 环检测 → HARD KILL
- **Plugin health** (audit + G3 self-check): error-as-success 比例 > 10% / session store > 1K → escalation log
- **Design review** (manual, ADR-0051 review): 2+ awkward pattern 类别 (来自 Layer 1 + Layer 3 dual memos) → DECLARE_SERVICE 形式化触发 (Phase 6 v2+)

### Decision 7 — Finalized v1 Contract (W3 Ship Gate)

Spike v1 实施完成后的最终合约：

- **注册模式**: `IToolRegistry::register_tool_function(name, metadata, lambda)` (与 `pdk/llama_engine/` / `pdk/model_router/` 一致, 无新宏)
- **工具命名**: ADR-0043 slash-only (`knowledge_base/query`, `coding_assistant/review`)
- **Args 签名**: `std::unordered_map<std::string, std::string> → nlohmann::json result` (Decision 2 落地)
- **Error Schema**: 强制 `{success: bool, answer?: string, error?: string}` 格式, 所有 return path 统一
- **Transport**: 进程内 `IToolRegistry::call_tool()` (非网络 / 非 MCP / 非 OpenAI API)
- **G3 ToolCategory**: `ToolCategory::Execute`, `allowed_layers = {LayerProfile::Workflow}`
- **G1 循环**: `AgentLoopType::React`, 2-step (invoke G3 → synthesize comment)
- **核心代码修改**: 仅 ToolCoordinator RAII guard (nesting depth + cycle detection + thread_local state)
- **Layer 2 审计**: 复用 C4 ship 的 `tool.audit.{invoked,completed,denied}` events; G3 自报告 5 个审计字段 (caller_session_id / callee_tool_name / args_keys_only / return_latency_ms / callee_internally_invoked_llm)

---

## 不变量

- **逻辑隔离非物理隔离** (继承 ADR-0020 per-agent 线程隔离 + ADR-0033 会话层级)：Spike v1 的所有 agent plugin 运行在同一进程中, 共享同一地址空间. 单 Agent 的未捕获异常或内存越界写入会导致整个进程崩溃, **不是**进程级沙箱。G1 和 G3 之间通过 `IToolRegistry::call_tool()` 进行 in-process 函数调用, 非 IPC/网络。这是 v1 接受的风险, 记录在此供 Phase 6 Candidate B v2 评估是否需要 `fork()` 或进程池隔离
- **ToolCoordinator RAII guard cycle detection 仅同线程有效** (Metis F4 已记录)：`thread_local` 变量绑定到 DomainWorkerPool (Sprint 3) 的 jthread worker 线程。跨线程 cycle (如 G1 on Worker A → G3 on Worker B → G1 on Worker A) 不可检测, 接受为 v1 known limitation。v2 需评估全局 cycle graph 或 per-process cycle counter
- G3 `MockLLMProvider` **必须 per-test-instance** (Metis H5/A4)：`mock_provider.h:33` 声明单线程使用，`generate()` 无锁操作 `history_`/`response_queue_`。每个 test fixture 独立创建 MockLLMProvider；多线程 ctest 并行时不共享 static 实例
- **不修改** ADR-0050 §决策 / §启动条件 (Spike 范围)
- **不引入** DECLARE_SERVICE 宏 (推迟到 Phase 6 v2+；需 ≥2 不同类别 awkward pattern 涌现触发)
- **不引入** 新 namespace (`agenticdsl::service` 留 Phase 6 v2+)
- **不修改** ADR-0019/0020/0021/0022/0023/0031/0033/0034/0043/0044/0004 V2 全部 (Tier 1/2/3 fallback 协议应对暴露缺陷，新建 ADR-0052+)
- 复用 Sprint 4 PDK 头文件 (PDK 静态链接独立验证)
- 复用 Sprint 19 MockLLMProvider (单线程使用声明，v1 Spike 不要求并发)

---

## 观察 (Layer 3 Dual Memo Findings)

> **来源**: Primary + Reviewer 独立 Layer 3 1-page memo (2026-07-15), 参见:
> - [`docs/service-composition/layer3-memo-primary.md`](../service-composition/layer3-memo-primary.md)
> - [`docs/service-composition/layer3-memo-reviewer.md`](../service-composition/layer3-memo-reviewer.md)
> - [`docs/service-composition/layer3-comparison.md`](../service-composition/layer3-comparison.md) (分歧分析)

### ✅ 双审查一致发现 (6 项 — 高置信度)

两个独立审查者同时捕获的 pattern:

| # | 发现 | 严重度 |
|---|------|--------|
| **O-1** | LLM callback 签名 `string→string` 不支持 error return — G3 回调无法表达 failure mode (Defect #6 根因) | 🔴 P0 |
| **O-2** | 魔术字符串 "G3: no response queued" 伪装为有效 answer — 流入正常 data flow 被 G1 synthesis 处理 | 🔴 P0 |
| **O-3** | Hardcoded tool name `"knowledge_base/query"` 无编译时检查 — G1 字符串匹配 G3, 改名仅运行时发现 | 🟠 P1 |
| **O-4** | 缺失 shared contract header — G1/G3 合约全在 README.md 而非 `.h` 文件, 无编译时 schema 验证 | 🟠 P1 |
| **O-5** | LLM callback pattern copy-paste — G1 和 G3 各自实现 `set_llm_callback()`/`enqueue_response()`, 代码几乎一致 | 🟡 P2 |
| **O-6** | `call_tool` 签名保真度不足 — `string→json` 将所有类型信息退化为字符串, 无编译时类型验证 | 🟠 P1 |

### 🔀 互补性发现 (4 项 — 正交信号)

| # | Primary 独见 | Reviewer 独见 | 互补性 |
|---|-------------|--------------|--------|
| **D-1** | 30 行 handler 约束限制错误处理 (无空间加 try-catch) | handler 无 try-catch 保护 LLM 调用 (异常穿透致进程崩溃) | 🟢 约束→后果 |
| **D-2** | MockLLMProvider 多线程不安全性 (Worker pool 并发数据竞争) | G1 registry 指针 data race (写带锁读不带锁) | 🟢 两个独立 race |
| **D-3** | 错误传播丢失根因 (G1 替换 G3 error 为通用消息) | callback 异常未捕获 (`g3_internal_llm()` 异常穿透整个调用栈) | 🟡 数据流+控制流 |
| **D-4** | Agent 间缺少类型级区分 (看起来都一样) | Agent 角色分类缺失 (不区分 orchestrator vs compute) | 🟢 同一根因不同角度 |

### 📊 净信号

- **共识发现**: 6 项 (2 P0 + 3 P1 + 1 P2)
- **互补发现**: 4 项 (零冲突)
- **触及 ADR-0051 决策**: 4/6 决策受影响 (D2 contract / D3 registration / D4 G3 ToolCategory / D5 core code)
- **Layer 3 收敛评估**: ✅ 满足 ADR-0051 §提升标准 #3 (primary + reviewer ≥1 major awkward pattern 共识)

### 🚦 形式化触发评估

当前满足 DECLARE_SERVICE 形式化触发条件 (ADR-0051 §决策 6):
1. **≥2 不同类别**: 4/5 Layer 1 checklist 类别出现 P0/P1 (Contract Drift, Lifecycle Coupling, Error Propagation, Resource Lifetime) ✓
2. **Layer 1 reviewer agreement**: 6 项 AGREE + 零 CONFLICT ✓
3. **Layer 3 convergence**: 6 项一致 (含 2 P0) ✓

**建议**: 在 Spike ship 时同步创建 ADR-0052 DECLARE_SERVICE 提案草稿, 但**不立即兑现** (样本仅 2 agent, G2/G4/G5 可能揭示新 pattern 类别)。

---

## 启动条件

> Spike 自身的启动条件 (非 Phase 6 启动条件)

1. **W1 fix list 12/12 完成** (2026-07-14, per `openspec/changes/phase6-service-ification-v1/tasks.md` §1 ✅ + active-status.md §二): proposal/design/specs/tasks/ADR-0051 全部修正 + openspec validate exit 0 + 二次 Metis 复审 0 CRITICAL
2. **Stage Gate 2026-07-18 通过**: Risk V1-R2 解除
3. **Sprint 23 启动 commitment**: 1.5 eng × 2 周

## Ship Gate (硬阻断)

- W1 fix list 12/12 完成 (per `openspec/changes/phase6-service-ification-v1/tasks.md` §1 ✅)
- ctest 72+N/72+N PASS 零回归 (基线 + 新增 Spike 测试)
- ASan 72+N/72+N PASS 零回归
- Layer 3 dual memos (primary + reviewer) 提交到 `docs/service-composition/observations/`
- ToolCoordinator RAII guard 实现 + 单元测试通过
- 5 escalation triggers 全部 wired + tested
- ADR-0050 §决策 / §启动条件 未修改
- G3 tool handler ≤30 行
- error schema 强制 `{success, error}`

---

## 触发条件 (Escalation Triggers — W3 实施后)

> 5 个 escalation triggers 全部 wired + tested (per tasks.md §6.6, test_escalation_triggers.cpp 6/6 PASS)

### Runtime Safety (ToolCoordinator RAII guard)

| Trigger | 条件 | 响应 | 测试 |
|---------|------|------|------|
| **T-1** | 嵌套深度 > 2 (G1→G3→G3 再加一层) | HARD KILL (`std::runtime_error` throw) | `nesting_depth_exceeds_2_kills` |
| **T-2** | 环检测 (同一工具名已在 `thread_local` call stack 中) | HARD KILL (`std::runtime_error` throw) + `cycle_detected_log` audit event | `cycle_detection_kills` |

Per ADR-0051 §不变量: `thread_local` 变量绑定 DomainWorkerPool jthread worker, 跨线程 cycle 不可检测 (v1 接受限制)。

### Plugin Health (Audit + G3 Self-Check)

| Trigger | 条件 | 响应 | 测试 |
|---------|------|------|------|
| **T-3** | G3 session store size > 1K entries | Escalation log + session 清理警告 | `session_store_size_triggers_1k` |
| **T-4** | G3 error-as-success ratio > 10% | Escalation log + 健康检查告警 | `error_ratio_triggers_10_percent` |

### Design Review (Manual)

| Trigger | 条件 | 响应 | 测试 |
|---------|------|------|------|
| **T-5** | 2+ awkward pattern 类别 (来自 Layer 1 + Layer 3 dual memos) | ADR-0052 draft proposal (DECLARE_SERVICE 形式化) | `design_review_trigger` |

### Normal Regression Safeguard (R4)

| Test | 条件 | 响应 |
|------|------|------|
| **R4** | G1→G3 composition (depth=2, no cycle, no escalation trigger fired) | Assert successful return; RAII guard does NOT误杀 legitimate nested calls |

---

## 风险评估

| 风险 | 等级 | 描述 | 缓解 |
|------|:----:|------|------|
| **Spike-R1** | 🟠 中高 | ADR-0050 §启动条件 #5 字面要求 "外部 agent/tool"。Spike 在 waiver 下推进 | 若 Stage Gate 2026-07-18 不利，Spike 结果可能不构成 ADR-0050 launch 的充分证据 |
| **Spike-R2** | 🟠 中高 | Spike 成功 ≠ Candidate B 可行。Spike 验证内部 in-process composition；外部 MCP/OpenAI transport 仍是未验证领域 | Spike 结果标注为 "internal only"，不声称外部可行性 |
| **Spike-R3** | 🟡 中 | MockLLMProvider 单线程声明 vs ctest 默认并行测试。G3 多 session 测试可能间歇性失败 | H2 from Metis；测试加 `--order rand` 暴露 flaky |
| **Spike-R4** | 🟡 中 | 1.5 工程师 × 2 周 与 100+ task list 容量不匹配 | drift kill 触发 — 写 learnings doc，不延 W4 |
| **Spike-R5** | 🟢 低 | G3 tool handler ≤30 行约束可能导致 golfed/awkward 代码，与 "暴露 awkward pattern" 目标矛盾 | Layer 3 memo 记录 golfed 模式作为观察数据点 |

---

## 估时 (W1-W3)

| 周 | 交付物 | 工程师 |
|----|--------|--------|
| **W1** (现在) | 11 项 fix list | 1.5 |
| **W2** (实施) | G3 + G1 plugin + integration | 1.5 |
| **W3** (收尾) | ToolCoordinator RAII + 5 escalation triggers + ADR-0051 ship gate + archive | 1.5 |

**总计**: 1.5 eng × 3 周 ≈ 22.5 人天

---

## Spike → Candidate B 提升标准

满足以下 **5 项全部** 后，**才**可提议 ADR-0052 启动 Phase 6 Candidate B v1：

1. ≥3 awkward patterns 从 ≥2 不同 Layer 1 类别观察到
2. Layer 1 reviewer agreement: ≥2 reviewers 独立识别
3. Layer 3 dual memos convergence: primary + reviewer 在 ≥1 major awkward pattern 上达成共识
4. Oracle round 4 确认内部 Spike 证据支持 "外部 agent/tool" 需求 (ADR-0050 §启动条件 #5)
5. ADR-0050 §启动条件 #2/#4/#5 重新评估通过

---

## 后续行动

### 已完成 (W1-W3)
1. ✅ **W1 fix list** → 12/12 完成 → 二次 Metis 复审 0 CRITICAL
2. ✅ **W2 G3 + G1 plugin** → 实施 + 集成 (per tasks.md §2-§4)
3. ✅ **W2 Layer 3 dual memos** → primary + reviewer + comparison 归档
4. ✅ **W3 ToolCoordinator RAII** → nesting depth + cycle detection + thread_local state
5. ✅ **W3 5 escalation triggers** → 全部 wired + tested (6/6 PASS)
6. ✅ **W3 ADR-0051 finalization** → §决策/§不变量/§观察/§触发条件/§后续 全部更新

### Spike Onboarding (G2/G4/G5 启动材料)
- 📖 **`docs/service-composition/spike-onboarding.md`** — G2/G4/G5 团队 kickoff 参考文档 (2-3 页, ~15 分钟阅读)
  - §1 "What Spike IS" — in-process / `register_tool_function`-based / `unordered_map<string,string> → nlohmann::json` / no new macros
  - §2 "What Spike IS NOT" — not networked / not async / not streaming / not multi-tenant / not Candidate B v1
  - §3 "Spike Contract" — 规范性契约 (~1 page: tool name format / args schema / return schema / error schema)
  - §4 "Does your Agent fit Spike?" — 决策树 (4 questions: stateless / session / streaming / cross-agent)
  - §5 Trigger thresholds — DECLARE_SERVICE push 触发阈值 (交叉引用 §触发条件)
  - §6 Reference implementations — G1 + G3 插件链接
  - ⚠️ **RED BANNER**: Spike 代码是 tension-maximizing MVP, **非** G2/G4/G5 的生产参考

### Post-Ship (Deferred to Sprint 24+)
7. **Post-Spike**: 2 周 production-like 监控 escalation triggers
8. **ADR-0052 draft**: 仅当 2+ different-category awkward patterns 触发 (per §触发条件 T-5)
9. **Oracle round 4**: 评估内部 Spike 证据是否支持 ADR-0050 §启动条件 #5 (外部 agent/tool 需求)

---

**最后更新**: 2026-07-15 (W3 ship gate — §7.1-§7.5 ADR-0051 finalization 完成)
**状态**: 🔍 Proposed (W1 12/12 ✅ + W2 实施 ✅ + W3 收尾 ship gate 验证中; 等待 commit 4 → ✅ Approved (experimental))
**关联**: [ADR-0050 Phase 6 战略评估](./adr-0050-phase6-strategic-evaluation.md)