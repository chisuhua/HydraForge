STATUS: BLOCKED — W1 remediation
Phase 6 PDK Composition Spike (pre-strategic feasibility validation — NOT a formal Phase 6 increment)

## Why

HydraForge 内部团队计划通过 PDK 插件开发 5 个领域 Agent（编程助手 / Agentic 推理引擎 / 知识库管理 / Agentic 记忆体 / Agentic 浏览器），愿景是"所有软件 PDK 化"。但当前 PDK 工具间只能相互独立调用，无法支撑 Agent 间"互相提供服务"的组合需求（Unix 微内核哲学）。

Oracle 战略咨询（`ses_0ae4b8107ffetONLmb2Sv2wTb5`）评估 Candidate B（服务化）为唯一匹配团队容量 + 兑现 Phase 5 路径投资的方向。然而，Oracle 三轮（`ses_0a17108b5ffexaXTWhF8vXot6b`）审查发现 proposal 的 6 处偏差（Q1-Q6），指示 v1 **必须先退为 Spike 形态**：仅做 G1+G3 in-process 可行性验证，复用 `IToolRegistry::register_tool_function()` 完成演示，让 awkward patterns 涌现后再形式化 DECLARE_SERVICE，避免过早抽象锁定。

本 change 是 **Phase 6 PDK Composition Spike**（非正式 Phase 6 增量），占 W1-W3 估时。Spike 成功后由 Stage Gate + ADR-0050 §启动条件重新评估后方可进入正式 Phase 6 实施。

## What Changes

- **新增** `pdk/g1_coding_assistant/` — 编程助手 PDK plugin（DEFINE_AGENT + ReactLoop + MockLLMProvider，单 tool 调 G3）
- **新增** `pdk/g3_knowledge_base/` — 知识库 PDK plugin（`IToolRegistry::register_tool_function()` 暴露 `knowledge_base/query`，内部 session store + MockLLMProvider，支持 2-turn multi-turn + session 隔离）
- **新增** ADR-0051 `Phase 6 PDK Composition Spike`（🔍 Proposed）— 记录 Spike 合约（in-process / `unordered_map<string,string>`-in / `nlohmann::json`-out / `IToolRegistry::register_tool_function()`-based / 逻辑隔离非物理隔离）
- **新增** `tests/test_service_v1.cpp` — G1-calls-G3 端到端测试 + session 隔离测试 + error propagation 测试
- **新增** awkward pattern 3 层检测方法：Layer 1 静态 review checklist（5 项）/ Layer 2 审计日志分析（基于现有 `tool.audit.*` 事件，不修改 `ToolRegistry::call_tool()`）/ Layer 3 工程师 memo
- **新增** 5 个 escalation triggers 监控：ToolCoordinator 嵌套深度 / error-as-success 比例 / session store 增长率 / nested agent detection / cycle detection
- **允许** ToolCoordinator 修改（添加调用深度 RAII 守卫，≤2 层硬限制，检测到 >2 视为 cycle 终止并记录错误）
- **禁止** 修改 `ToolRegistry::call_tool()` 全局 instrumentation（复用现有 `tool.audit.{invoked,completed,denied}` EventBus 事件）
- **不引入** DECLARE_SERVICE 宏（Spike 阶段不形式化，推迟到 ≥2 个不同类别 awkward pattern 涌现后）
- **不引入** 新 namespace（v1 用现有 `pdk/` 目录）
- **不修改** 任何现有 ADR（0 amendments，Tier 1/2/3 fallback 协议应对）
- **Spike 范围限定**: 仅 G1+G3 in-process demo；不做 ADR-0050 §决策更新（ADR-0050 保持其外部 MCP/OpenAI 战略目标）；不做跨进程/多租户/外部暴露

## Capabilities

### New Capabilities

- `pdk-service-composition`: v1 PDK Service Composition Spike 合约 — 定义 in-process / `unordered_map<string,string>`-in / `nlohmann::json`-out / `IToolRegistry::register_tool_function()`-based 的 PDK Agent 间互相调用合约；包含 3 层 awkward pattern 检测方法 + 5 个 escalation triggers；显式声明仅逻辑隔离非物理隔离（ADR-0020 限定）；Spike 范围限定 in-process only，不做 DECLARE_SERVICE 形式化
- `coding-assistant-agent`: G1 编程助手 PDK plugin — 2 步 ReAct 循环 + 单 tool（G3）+ mock code input；实现张力最大化 MVP 暴露 nested agent + cross-tool session 语义；参考 ADR-0051 §G1 MVP scope
- `knowledge-base-agent`: G3 知识库 PDK plugin — `IToolRegistry::register_tool_function()` 暴露 `knowledge_base/query` 接口；内部 session store + MockLLMProvider；支持 2-turn multi-turn（同 session_id）+ session 隔离（不同 session_id）；实现张力最大化 MVP 暴露 stateful tool + nested agent behind tool 语义；tool handler ≤30 行；ToolCategory: **Execute**，allowed_layers: **{Workflow} only**；包含强制 error schema `{success: bool, error: string?}`（防 error flattening）

### Modified Capabilities

无（现有 spec 不修改 — Oracle 三轮决议 0 amendments；任何 ship-block 缺陷按 Tier 2 fallback 协议新建 ADR-0052+ 而非 amend 现有 ADR）

## Impact

### Affected Code

- **新增**: `pdk/g1_coding_assistant/`（新 plugin 目录），`pdk/g3_knowledge_base/`（新 plugin 目录），`tests/test_service_v1.cpp`（新集成测试）
- **修改**: 根 `CMakeLists.txt`（添加 G1+G3 plugin 子目录），`pdk/CMakeLists.txt`（如需）
- **允许修改**: `ToolCoordinator`（添加嵌套深度 RAII 守卫 + ≤2 层硬限制；此为 Spike W3 交付物）
- **禁止修改**: `ToolRegistry::call_tool()`（不添加全局 instrumentation；复用现有 `tool.audit.*` EventBus 事件进行 Layer 2 检测）
- **不变**: 现有所有 ADR 实现代码（0 amendments）
- **参考**: `pdk/llama_engine/` 模式可参考（C14 ship，`register_tool_function()` 范式），`pdk/model_router/` 模式可参考（C7 ship）

### Affected APIs

- **复用**: `IToolRegistry::register_tool_function()`（Sprint 4 PDK 骨架），`DEFINE_AGENT(React)`（Sprint 20），`MockLLMProvider`（Sprint 19）
- **新增 contract**: 显式 G1+G3 互相调用合约（`unordered_map<string,string>` args → `nlohmann::json` result，错误 schema 强制 `{success, error}`）
- **v1 args contract 注记**: 当前使用现有 `unordered_map<string,string>` args 合约（PDK 已 ship 的默认接口）；迁移到 JSON args 需独立 ADR + 独立 change，不在本 Spike 范围
- **不引入**: `DECLARE_SERVICE` 宏（v2 候选），新 namespace（`agenticdsl::service` 留 v2）

### Affected ADRs

- **新增 ADR-0051**: Phase 6 PDK Composition Spike（🔍 Proposed, W3 ship gate 翻 ✅ Approved）
- **不修改 ADR-0050**: ADR-0050 保持其 §决策 战略目标（外部 MCP/OpenAI API 暴露），不作修订 PR；Spike 是 pre-strategic 验证，不属于 ADR-0050 的正式 Phase 6 实施
- **不修改**: ADR-0019/0020/0021/0022/0023/0031/0033/0034 全部保留（Tier 1/2/3 fallback 协议应对）
- **Spike 成功与 ADR-0050 的关系**: Spike 成功不自动推进 ADR-0050 状态；ADR-0050 §启动条件 #5 仍需满足 "external agent/tool" 方可正式启动 Phase 6

### Affected Dependencies

- **无新依赖**: 复用现有 `MockLLMProvider` / `IToolRegistry` / `DEFINE_AGENT` / `TopoScheduler` / `IInteractionBus` / ADR-0033 session hierarchy / `tool.audit.*` EventBus 事件
- **httplib::Server**（Phase 6 v2 候选，不在 Spike 范围）

### Ship Gate（W1 remediation 硬阻断）

- (a) All 4 W1 critical blockers resolved（proposal / design / specs / tasks 全部按 Q1-Q6 修正）
- (b) ADR-0051 created with 🔍 Proposed status
- (c) `openspec validate phase6-service-ification-v1 --strict` exit 0
- (d) Second Metis review with all CRITICAL → 0
- (e) Stage Gate 2026-07-18 passed + Sprint 23 capacity confirmed（**先于 W2-W3 实施**）

## Non-Goals（明确范围边界）

- **不实现** DECLARE_SERVICE 宏（推迟到 v2，等 ≥2 个不同类别 awkward pattern 涌现后）
- **不实现** 跨进程 IPC / transport layer（v2 候选，Spike 仅 in-process）
- **不实现** streaming / sync-async bridge（G5 Browser 需要，留 v2）
- **不实现** 多租户 / 鉴权 / 计费统一层（Phase 7+ 议题）
- **不实现** OpenAI-compatible 外部 API / MCP server 暴露（ADR-0050 §启动条件 #2 已识别为非阻塞，留 v2）
- **不启动** G2/G4/G5 plugin 并行开发（等 Spike ship + onboarding doc 完成后）
- **不修改** ADR-0033 session hierarchy / ADR-0031 ToolCoordinator / ADR-0020 thread isolation（任何 amendment 按 Tier 2 协议新建 ADR-0052+）
- **不延** W1-W3 时间线到 W4+（drift kill 触发）
- **不引入** 新 namespace（`agenticdsl::service` 留 v2）
- **不做** 5 团队 kickoff（留 Spike ship 后单次统一 kickoff）
- **Spike 不修改 ADR-0050 §决策 / §启动条件**：Phase 6 正式启动仍需满足全部 5 项硬前置；Spike 是 pre-strategic 验证，在 ADR-0050 框架外运行

## v1 启动时已知风险

### Risk Spike-R1: ADR-0050 §启动条件 #5 未满足

- **Severity**: Medium
- **描述**: ADR-0050 §启动条件 #5 字面要求 "external agent/tool"；Spike 目标 G1+G3 是内部 Agent demo，不满足此条件。Spike 在 Waiver 下进行（pre-strategic 验证），但正式 Phase 6 启动仍需重新评估
- **缓解**: Spike 定位为 pre-strategic 验证（不触发 ADR-0050 状态翻转）；Spike ship 后由 Stage Gate + ADR-0050 框架重新评估正式 Phase 6 启动条件
- **Owner**: [用户指定]
- **解除条件**: Spike ship 后 ADR-0050 §启动条件重新评估（不要求 §决策 修订）

### Risk V1-R2: 团队容量未确认（Stage Gate ⚠️ RISKY）

- **Severity**: High
- **描述**: Stage Gate handoff §7 标记 RISKY；Sprint 22 工作已饱和；C10/C11/C12 满 2 周稳定期 2026-07-17/18 截止未到
- **缓解**: 2026-07-18 Stage Gate 重评后，Sprint 23 启动会议正式 commitment 1.5 eng × 2 周
- **Owner**: [用户指定]
- **解除条件**: Sprint 23 启动 commitment 文档合入

### Risk V1-R3: 嵌套 ToolCoordinator 递归（Defect #5）

- **Severity**: Medium（设计中已防）
- **描述**: G1 调 G3 经 ToolCoordinator；G3 内部 Agent loop 若再调需审批 tool → 再过 ToolCoordinator → 嵌套审批/审计无界递归
- **缓解**: G3 MVP scope **硬性禁止** G3 内部调需审批 tool（G3 内部只调 MockLLMProvider，无 tool 调用）；ADR-0051 §不变量显式记 "v1 G3 不嵌套调审批 tool"；**ToolCoordinator RAII 深度守卫**（≤2 层硬限制，>2 视为 cycle 终止并记录错误）
- **Owner**: 实施工程师
- **解除条件**: G3 MVP ship + escalation trigger #1（nested-ToolCoordinator depth > 2）监控上线 + ToolCoordinator RAII 守卫测试通过

### Risk V1-R4: Error Flattening 静默缺陷（Defect #6）

- **Severity**: Medium（设计中已防）
- **描述**: `IToolRegistry::register_tool_function()` wrapping 把内层 Agent-loop 错误藏在 tool-success payload 后；demo "工作" 但生产 break（G1 不重试瞬态失败）
- **缓解**: v1 合约 **强制** error schema `{success: bool, error: string?}`，禁用隐式 payload；instrumentation 记 error-as-success 比例；>10% 触发 escalation
- **Owner**: 实施工程师
- **解除条件**: G3 MVP ship + escalation trigger #2（error-as-success > 10%）监控上线 + tests/test_service_v1.cpp 覆盖 error 路径

### Risk Spike-R2: Spike 成功 ≠ Candidate B 可行

- **Severity**: Low
- **描述**: Spike 仅验证 G1+G3 in-process 组合；不验证 C++→OpenAI、跨进程 IPC、多租户等 Candidate B 核心路径
- **缓解**: Spike ship 文档显式声明 "Spike 成功不自动证明 Candidate B 可行；external MCP/OpenAI 暴露仍是 ADR-0050 §决策 目标"；Phase 6 正式启动决策权归 ADR-0050 + Stage Gate
- **Owner**: [用户指定]
- **解除条件**: Spike ship 报告中包含 §Candidate B 可行性差距分析