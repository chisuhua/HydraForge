## Context

HydraForge 是内容团队的 Agent OS 平台基础设施。Phase 0-5 ship 完成 (2026-07-11, 19 ADR Approved, 7 ADR Proposed, ctest 72/72, ASan 72/72)。

Phase 6 战略由 ADR-0050 定义 (外部 MCP/OpenAI API 暴露)。本 change 是 ADR-0050 正式启动前的内部组合可行性 Spike，定位降级：验证 in-process G1→G3 composition 是否构成 ADR-0050 v1 的可行技术路径，但不兑现 Candidate B 战略目标。ADR-0051 (本 Spike) 与 ADR-0050 (战略终点) 解耦 — Spike 结果可反馈 ADR-0050 但不修改其 §决策。

用户 reframing: HydraForge 服务化的真实意图是"PDK 开发的 Agent 可以互相提供服务" (Unix 微内核哲学)，而非对外暴露 MCP/OpenAI 兼容 API。5 个领域 Agent 通过 PDK 化开发 (编程助手 / Agentic 推理引擎 / 知识库 / Agentic 记忆体 / Agentic 浏览器)，愿景"所有软件 PDK 化"。

Oracle 三轮咨询 (Round 1 `ses_0a206a23cffe1IEirU5iNaxFxC` + Round 2 二轮 + Round 3 `ses_0a17108b5ffexaXTWhF8vXot6b` Q1-Q6) 输出 v1 Spike 边界:
- v1 不引入 DECLARE_SERVICE 宏 (过早标准化, 推迟到 v2)
- v1 使用 `IToolRegistry::register_tool_function()` NOT `DECLARE_TOOL` 宏 (Q1)
- v1 仅逻辑隔离非物理隔离 (ADR-0020 限定)
- v1 in-process, transport-agnostic (为 v2 IPC transport 留 seam)
- v1 合约 `unordered_map<string,string>-in / nlohmann::json-out` (Q5)

Spike 启动条件: 4 项 critical blocker 识别 (Stage Gate unconfirmed + 5 hard prerequisites 未全 met + proposal/design/specs 需对齐 Oracle 决策 + 无 Sprint 23 capacity commitment)。Spike framing 使 W1 可推进设计对齐 (ADR-0050 不修改)，W2-W3 实现被上述 blocker 卡死。

Stakeholders:
- **内容团队**: 最终用户 (规划 5 Agent 用户)
- **PDK 作者 (G1-G3 团队)**: Agent 开发者, v1 Spike 仅 G1+G3
- **平台团队**: HydraForge 维护者, 负责 Phase 6 战略 + v1 合约设计
- **Oracle**: 架构咨询, 已提供 3 轮咨询输出

## Goals / Non-Goals

**Goals:**

1. **G1+G3 端到端 in-process 演示**: G1 (Coding Assistant) 通过 `IToolRegistry::register_tool_function()` 调用 G3 (Knowledge Base), 验证 PDK Agent 互相提供服务的最小 seam
2. **v1 合约形式化**: 定义 transport-agnostic / `unordered_map<string,string>-in` / `nlohmann::json-out` / `register_tool_function`-based / 逻辑隔离的合约规范 (ADR-0051)
3. **Awkward pattern 检测方法学落地**: 3 层 (L1 review + L2 instrumentation + L3 memo) 让 DECLARE_SERVICE 形式化时机由证据驱动
4. **Escalation trigger 监控上线**: 3 类 (runtime safety / plugin health / design review) 分层监控
5. **G2/G4/G5 onboarding 文档就绪**: Spike ship 时同步提交 `docs/service-composition/spike-onboarding.md`, 自服务决策树
6. **0 个 ADR-0050 修订 (Spike 范围)**: Oracle Round 3 决议坚守, ADR-0051 内部 Spike 不影响外部战略 ADR-0050
7. **强制 ship gate**: ADR-0051 ✅ Approved / Stage Gate 重评通过 / ctest 零回归 / 不引入 DECLARE_SERVICE / Layer 3 dual memo 存在

**Non-Goals:**

1. **不实现 DECLARE_SERVICE 宏**: v2 候选, 等 2+ 不同类别 awkward pattern 涌现
2. **不实现跨进程 IPC / transport layer**: v2 候选, v1 仅 in-process
3. **不实现 streaming / sync-async bridge**: G5 Browser 需要, v2 评估
4. **不实现多租户 / 鉴权 / 计费统一层**: Phase 7+ 议题
5. **不实现 OpenAI-compatible 外部 API / MCP server 暴露**: ADR-0050 §启动条件 #2 已识别为非阻塞
6. **不启动 G2/G4/G5 plugin 并行开发**: 等 v1 ship + onboarding doc 完成后
7. **不修改 ADR-0033 session hierarchy / ADR-0031 ToolCoordinator / ADR-0020 thread isolation**: 任何 amendment 按协议新建 ADR-0052+
8. **不延 W1-W3 时间线到 W4+**: drift kill 触发
9. **不引入新 namespace**: `agenticdsl::service` 留 v2
10. **不做 5 团队 kickoff**: 留 v1 ship 后单次统一 kickoff
11. **不修改 ADR-0050 §决策 / §启动条件**: Spike 不改变 ADR-0050 外部 MCP/OpenAI 战略终点；Phase 6 正式启动仍需满足 ADR-0050 §启动条件 5 项

## Decisions

### Decision 1: Spike 定位 (Oracle Q6)

**Choice**: 本 change 定位 = Spike (可行性验证), 非 Candidate B v1 实施入口

**Rationale**:
- 4 项 critical blocker: Stage Gate 2026-07-18 未确认 / ADR-0050 §启动条件 5 项 1 完全 met + 2 部分 met + 2 未 met / Sprint 23 无 capacity commitment / proposal/design/specs 需对齐 Oracle 3 轮决议
- ADR-0051 (内部 Spike) 与 ADR-0050 (外部战略终点) 解耦 — Spike 失败 ≠ ADR-0050 战略失败
- Spike 使 W1 可推进设计对齐 (ADR-0050 不修改), W2-W3 实现被 blocker 卡死

**Consequence**: W1 仅做 fix list (proposal/design/specs 重写 + ADR-0051 起草 + openspec validate + 2nd Metis review); W2-W3 实现 blocked

### Decision 2: 服务合约签名 (Oracle Q5)

**Choice**: v1 合约 = `unordered_map<string,string>-in` / `nlohmann::json-out`, transport-agnostic, 不做 "JSON-in/JSON-out" 承诺

**Rationale**:
- 实际 `IToolRegistry::call_tool()` 签名接受 `unordered_map<string,string>` — 承诺 JSON-in 会造成 mismatch
- `nlohmann::json-out` 保留结构化返回能力 (`ToolResult` 原生支持)
- transport-agnostic 保持 v2 IPC 演进的 seam
- "JSON-in/JSON-out" promise 已从文档移除

**Alternatives considered**:
- **(α') JSON-in/JSON-out**: 与 `call_tool()` 实际签名不符, v1 需额外序列化层 → reject
- **(α'') raw `Context` 透传**: 泄露 in-process 内部结构, v2 迁移破坏性 → reject

### Decision 3: 注册模式 (Oracle Q1)

**Choice**: v1 使用 `IToolRegistry::register_tool_function()`, NOT `DECLARE_TOOL` 宏

**Rationale**:
- `tool_macros.h:88-109` 的 `DECLARE_TOOL` 使用 `##name` token-pasting, 当 tool name = `knowledge_base/query` (ADR-0043 slash-only) 时产生非法 C++ identifier
- 所有现有 PDK plugins (model_router/llama_engine) 在实现层 bypass DECLARE_TOOL, 直接使用 `register_tool_function()`
- `register_tool_function()` 接受任意 string name (含 `/`), 不需要宏展开
- 与 Decision 2 的 `unordered_map<string,string>` 输入一致 (直接 API 调用, 无需宏包装)

**Alternatives considered**:
- **(β') DECLARE_TOOL 扩展支持 slash names**: 需修改宏 token-pasting 机制, 影响所有现有 plugin → reject (unnecessary blast radius)
- **(β'') 自建注册 helper**: 与现有 PDK pattern 分叉, 增加学习成本 → reject

### Decision 4: G3 ToolCategory 与 Layer 权限 (Oracle Q3)

**Choice**: G3 注册为 `ToolCategory::Execute` + `allowed_layers={Workflow}` only

**Rationale**:
- ADR-0004 V2 ReadOnly = "ls/cat/grep/search" 类别, 不覆盖 LLM generation + session state mutation
- G3 Knowledge Base 的 `query` tool 触发 LLM generation + 维护 session store (stateful) → 语义上超出 ReadOnly
- `allowed_layers={Workflow}` only 确保只有完整工作流图可调用 G3, 降低误用风险
- 运行时安全保持: G3 tool 经过 ToolCoordinator (layer check + approval + audit), 不 bypass 安全层

**Alternatives considered**:
- **(γ') ToolCategory::ReadOnly**: 语义不匹配 (G3 做 LLM generation + session mutation) → reject
- **(γ'') ToolCategory::Dangerous**: 过度限制 (G3 是内部 service tool, 非 destructive) → reject

### Decision 5: 核心代码修改范围 (Oracle Q4)

**Choice**: 单一 white-listed 修改 — `ToolCoordinator` nesting depth + cycle RAII guard; 禁止全局 instrumentation

**Allowed (单文件, white-listed)**:
- `ToolCoordinator` 添加 RAII guard: nesting depth > 2 → HARD KILL; cycle detected → HARD KILL
- 动机: G1 调 G3 经过 ToolCoordinator, G3 内部 Agent loop 再次调用需审批 tool → 嵌套无界递归必须防御

**Forbidden**:
- `ToolRegistry::call_tool()` 全局 instrumentation — 影响所有现有 tool 调用路径
- Layer 2 instrumentation 使用现有 `tool.audit.{invoked,completed,denied}` event payload (C4 ship, ADR-0031 §决策 4) — 无需新增 core 代码
- Plugin-internal metrics (session store 增长, error ratio) 在 G3 plugin 内部测量 — 不渗透到 core

**Rationale**: 最小侵入性; 所有新增 instrumentation 在 plugin 层或复用已有 audit bus; 只有 runtime safety (nesting/cycle) 需要 core 修改

### Decision 6: Escalation Trigger 分类 (Oracle 精炼)

**Choice**: 3 类 escalation trigger 按严重度和响应路径分层

**Runtime safety (ToolCoordinator RAII, HARD KILL)**:
- nesting depth > 2 → 立即终止, crash report
- cycle detected (G1→G3→G1) → 立即终止, crash report
- 实现: ToolCoordinator RAII guard (Decision 5)

**Plugin health (audit + G3 self-check, escalation → design review)**:
- error-as-success ratio > 10% → 触发 audit review
- session store > 1K entries → 触发 memory pressure review
- 实现: 利用 `tool.audit.{invoked,completed,denied}` event + G3 内部 health metric

**Design review (manual, ADR-0051 review)**:
- 2+ different awkward pattern categories → DECLARE_SERVICE 形式化触发 (v2)
- L1 reviewer agreement on pattern severity
- Layer 3 dual memo convergence on root cause
- 实现: 人工 review, 非自动化 trigger

## Risks / Trade-offs

### Risk 1: 嵌套 ToolCoordinator 递归 (Defect #5)
**Risk**: G1 调 G3 经 ToolCoordinator; G3 内部 Agent loop 若再调需审批 tool → 嵌套无界递归
**Mitigation**: ToolCoordinator RAII guard (nesting depth > 2 → HARD KILL, cycle → HARD KILL); Decision 5 white-listed 单一修改
**Status**: Resolution upgraded from "scope-based prohibition" → "runtime RAII guard"

### Risk 2: Error Flattening 静默缺陷 (Defect #6)
**Risk**: `IToolRegistry::register_tool_function()` wrapping 把内层 Agent-loop 错误藏在 tool-success payload 后; demo "工作" 但生产 break (G1 不重试瞬态失败)
**Mitigation**: v1 合约强制 error schema `{success: bool, error: string?}`, 禁用隐式 payload; escalation trigger #2 (plugin health) 监控 `error-as-success 比例 > 10%` via audit event monitoring
**Status**: Mitigation updated — audit event monitoring 替代 manual code review

### Risk 3: 逻辑隔离误当物理隔离 (Defect #1 from Round 1)
**Risk**: 5 团队若默认"我隔离了别人, 崩溃不影响我" 会基于错误前提设计容错
**Mitigation**: ADR-0051 §不变量 + spike-onboarding.md 显式声明"Spike isolation 是逻辑隔离, 非物理隔离; 单 Agent 未捕获异常可能导致整个进程崩溃"

### Risk 4: Transport Leak (Defect #3 from Round 1)
**Risk**: 若 v1 合约暴露 in-process 便利 (shared_ptr, &), v2 IPC 迁移时合约必须变更 → API 破坏性 → 5 Agent 全部重写
**Mitigation**: ADR-0051 + onboarding doc + 代码 review checklist 强制值语义; escalation trigger 在 code review 时拦截

### Risk 5: 5 团队过早 kickoff 信任侵蚀
**Risk**: v1 合约 ship 前教 5 团队 = 教一个会变的合约; kickoff 社交事件创造抗拒修改的组织动能 → "平台老变" 信任侵蚀
**Mitigation**: D3 决策 (c) 不做 5 团队 kickoff, 等 v1 ship 后单次统一 kickoff; ADR-0051 §后续含非规范性 onboarding 种子段

### Spike-R1: ADR-0050 §启动条件 #5 字面要求 (Oracle 识别)
**Risk**: ADR-0050 §启动条件 #5 字面要求"外部 agent/tool"; Spike 验证内部 in-process composition, 若 Stage Gate 2026-07-18 unfavourable, Spike 结果可能无法 justify ADR-0050 正式启动
**Mitigation**: Spike 在 waiver 下推进; W1 fix list 完成后 2nd Metis review 重新评估 alignment; 若 Stage Gate reject, Spike learnings 反馈 ADR-0050 但不翻 Candidate B

### Spike-R2: Spike 成功 ≠ Candidate B 可行 (Oracle 识别)
**Risk**: Spike 验证内部 in-process composition; 外部 MCP/OpenAI transport 仍是未验证领域 (Q5 `call_tool` signature 仅 in-process), Candidate B 真正的战略终点需要外部 transport
**Mitigation**: Spike scope 显式限定"内部可行性验证"; v2 (Candidate B v1) 需要重新评估 transport 方案; Spike learnings doc 区分"Spike 已验证" vs "Candidate B 未验证"

### Trade-off 1: v1 短期 vs 长期可演化
- 短期: v1 不引入 DECLARE_SERVICE, 失去"立即标准化"的工整
- 长期: v2 由证据驱动形式化, 抽象形状更贴合 5 Agent 真实需求
- 决策: 选长期 (Phase 6 是 4-6 周, v1 ship 后还有 W4-W6 评估)

### Trade-off 2: MockLLMProvider vs 真实 LLM
- MockLLMProvider: 1-2 周 ship, 不需真实 LLM 运行环境
- 真实 LLM (LlamaAdapter): 暴露真实 LLM 行为细节, 但延长 timeline + 增加依赖
- 决策: 选 MockLLMProvider (v1 demo 目的是暴露 composition 模式, 非 LLM 行为)

## Migration Plan

### Phase A: W1 Fix List (in-progress, 不依赖外部 blocker)

1. **proposal.md 重写**: 对齐 Oracle Q1-Q6 决策, Spike framing
2. **design.md 重写** (本文档): Oracle 6 决策落地
3. **specs/*.md 重写**: 3 spec 对齐新决策 (注册模式 / 合约签名 / core 修改范围)
4. **tasks.md 重写**: W1 = fix list, W2-W3 = blocked
5. **ADR-0051 起草**: 合约规范 (ADR-0050 不修改)
6. **openspec validate**: `openspec validate phase6-service-ification-v1 --strict` exit 0
7. **ADR 一致性检查**: `tools/adr_lint.py` exit 0
8. **文档漂移检查**: `tools/docs_drift_audit.py` 0 CRITICAL
9. **2nd Metis review**: 完整 change + ADR-0051 draft → 0 CRITICAL
10. **Oracle Q6 确认**: Spike → Candidate B promotion criteria 征求 Oracle
11. **W1 gate**: 全部 11 项 done → W1 sign-off

### Phase B: W2-W3 实现 (BLOCKED)

W2-W3 实现 blocked，解除条件 (全部满足):
- (i) Stage Gate 2026-07-18 重评通过
- (ii) Sprint 23 capacity commitment 确认 (1.5 eng × 2 周)
- (iii) W1 fix list 11/11 complete
- (iv) 2nd Metis review 0 CRITICAL

若 2026-07-18 Stage Gate pass → W2 启动:
- W2 D1-5: G3 Knowledge Base plugin 实现 + unit test
- W2 D6-10: G1 Coding Assistant plugin 实现 + 端到端集成
- W3 D1-5: 3 层 awkward pattern 检测方法 + escalation trigger 监控 + ADR-0051 定稿
- W3 D6-10: onboarding doc + test_service_v1.cpp + ship gate 验证 + archive

若 Stage Gate 推迟或 reject → Spike 保持 W1 complete, learnings doc 反馈 ADR-0050

### Phase C: Post-Spike 决策

Spike 结果评估:
- **若 Spike → Candidate B v1 promotion**: 基于 awkward pattern evidence (≥3 patterns from ≥2 Layer 1 categories + L1 agreement + dual memo convergence)
- **若 Spike 不足以 promote**: learnings doc 反馈 ADR-0050 → 重新评估 Candidate B vs alternative 战略方向

### Rollback Strategy

**Code rollback**:
- 删除 `pdk/g1_coding_assistant/` + `pdk/g3_knowledge_base/` 目录
- 修改根 `CMakeLists.txt` 移除 plugin 子目录
- ToolCoordinator RAII guard 保留 (runtime safety 独立价值)
- 零影响其余 codebase (插件隔离在 `pdk/` 子目录)

**Doc rollback**:
- ADR-0051 保持 🔍 Proposed (不翻 ✅ Approved)
- OpenSpec change 标记 `abandoned` + 保留 findings
- `docs/service-composition/spike-onboarding.md` 删除或标记 deprecated

**Strategic feedback**:
- findings 反馈 ADR-0050 → 评估 Spike 结果是否 justify ADR-0050 启动
- 若 Spike fail, 不自动等于 Candidate B fail (外部 transport 未验证, Spike-R2)

### Rollback Trigger (Kill Criteria)

- **HARD KILL**: crash 传播 (G3 崩 = G1 崩) OR ToolCoordinator nesting > 2 OR cycle (G1→G3→G1) OR W2 D10 末零 E2E call
- **SOFT KILL**: 2+ 不同类别 awkward pattern + 无 DECLARE_SERVICE 方向 (2 天 oracle round 后无解 → HARD)
- **DRIFT KILL**: W3 末无收敛 (写 learnings doc, 不延 W4)

## Open Questions

### Q1: ADR-0050 §决策 文字更新精确措辞 — RESOLVED (Oracle Q6)
- **Resolution**: 替换为 Decision 1 (Spike framing) — ADR-0050 §决策 不做任何修改；Spike 在 waiver 下推进
- **Rationale**: ADR-0051 内部 Spike 与 ADR-0050 外部战略解耦

### Q2: G1+G3 真实业务场景与 MVP 范围的 gap — RESOLVED (Oracle 默认)
- **Resolution**: 保持 Oracle 推荐默认 — G3 = 文档问答 (2-turn session, 硬编码片段), G1 = 编程助手 (2 步 ReAct, mock code)
- **Rationale**: Spike 目的 = surface awkward patterns, 非交付产品; 用户有异议时可在 W1 调整

### Q3: Layer 3 dual memo 模板格式 — RESOLVED (defer to W2-D5)
- **Resolution**: 不做 W1 模板, W2-D5 由 primary engineer + reviewer 定格式
- **Rationale**: Spike W1 仅 fix list; memo 模板不阻塞 Stage Gate

### Q4: ADR-0051 ship 时机 — RESOLVED (Oracle Q4)
- **Resolution**: 是, ADR-0051 ship 等 2026-07-18 Stage Gate 通过 + Sprint 23 commitment 确认
- **Rationale**: 决策 (c) 有条件推进 — 草稿不受影响, 正式 ship 卡 #2 + #4

### Q5: 7 个 Phase 5 Proposed ADR 是否影响 v1 ship — RESOLVED (defer)
- **Resolution**: 留 Sprint 24+, 不影响 Spike
- **Rationale**: 这 7 个 ADR 与 Service Composition 无直接依赖

### Q6: Spike → Candidate B v1 晋升证据门槛 (NEW)
- **Question**: 什么证据阈值能 justify Phase 6 Candidate B v1 正式启动?
- **Proposed criteria**: ≥3 awkward patterns from ≥2 different Layer 1 categories + L1 reviewer agreement on severity + Layer 3 dual memo convergence on root cause
- **需确认**: Oracle Q6 follow-up 或 team discussion 确认 threshold
- **Timeline**: W1 fix list 完成后, before Stage Gate 2026-07-18

## 后续行动

1. **W1 (in-progress)**: Fix list 11 项 — proposal/design/specs/tasks 重写 + ADR-0051 起草 + openspec validate + adr_lint + docs_drift_audit + 2nd Metis review + Oracle Q6 confirmation
2. **W2-W3 (BLOCKED)**: G1+G3 实现 + awkward pattern 检测 + escalation trigger + ADR-0051 定稿 + onboarding doc + ship gate
   - UNBLOCK 条件: Stage Gate 2026-07-18 PASS + Sprint 23 capacity confirmed + W1 fix list 11/11 + 2nd Metis 0 CRITICAL
3. **2026-07-18**: Stage Gate 重评
4. **Post-Spike**: 基于 awkward pattern evidence 决定 Spike → Candidate B v1 promotion