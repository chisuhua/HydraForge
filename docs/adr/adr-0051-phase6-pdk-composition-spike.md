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

---

## 不变量

- Spike v1 仅 **逻辑隔离**，非物理隔离 (继承 ADR-0020 + ADR-0033)。单 Agent 未捕获异常可能导致整个进程崩溃
- ToolCoordinator RAII guard **cycle detection 仅同线程有效** (Metis F4 已记录)：`thread_local` 变量 per-jthread-worker (DomainWorkerPool Sprint 3)；跨线程 cycle (如 G1 Worker A → G3 Worker B → G1 Worker A) 不可检测，接受为 v1 known limitation
- G3 `MockLLMProvider` **必须 per-test-instance** (Metis H5/A4)：`mock_provider.h:33` 声明单线程使用，`generate()` 无锁操作 `history_`/`response_queue_`。每个 test fixture 独立创建 MockLLMProvider；多线程 ctest 并行时不共享 static 实例
- **不修改** ADR-0050 §决策 / §启动条件 (Spike 范围)
- **不引入** DECLARE_SERVICE 宏 (推迟到 Phase 6 v2+；需 ≥2 不同类别 awkward pattern 涌现触发)
- **不引入** 新 namespace (`agenticdsl::service` 留 Phase 6 v2+)
- **不修改** ADR-0019/0020/0021/0022/0023/0031/0033/0034/0043/0044/0004 V2 全部 (Tier 1/2/3 fallback 协议应对暴露缺陷，新建 ADR-0052+)
- 复用 Sprint 4 PDK 头文件 (PDK 静态链接独立验证)
- 复用 Sprint 19 MockLLMProvider (单线程使用声明，v1 Spike 不要求并发)

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

1. **修 W1 fix list** → 二次 Metis 复审 → 0 CRITICAL
2. **Stage Gate 2026-07-18** → 通过则启动 W2-W3
3. **W2 D5 前** 用户确认 Layer 3 memo 模板 (5 个固定 section)
4. **W2 D10 末** 如无 E2E call 触发 HARD KILL
5. **W3 末** 如无收敛触发 DRIFT KILL (写 learnings doc，不延 W4)
6. **W3 D15** Spike ship → ADR-0051 翻 ✅ Approved (experimental)
7. **Post-Spike**: 2 周 production-like 监控 escalation triggers

---

**最后更新**: 2026-07-16 (W1 12/12 ✅ + 二次 Metis 0 CRITICAL ✅ + W2-W3 文档修订已应用; Oracle D1 议程建议 #D-4/#D-5/#D-6 已 ship)
**状态**: 🔍 Proposed (W1 12/12 ✅ 完成; 等待 Stage Gate 2026-07-18 + Sprint 23 capacity + W3 ship gate → ✅ Approved (experimental))
**关联**: [ADR-0050 Phase 6 战略评估](./adr-0050-phase6-strategic-evaluation.md)