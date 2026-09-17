# Master Plan: pdk-chat-demo Evolution & Self-Evolution Agent Framework

> **驱动诊断**: pdk_chat_demo 跑真实 LLM 模式返回 0 步 + 空 Assistant
> **驱动愿景**: 在 HydraForge "DSL 执行引擎" 核心使命内搭建 Harness-RSI 闭环骨架
> **覆盖**: Sprint 34-36 (3 Sprints, ~3-4 周)
> **生成日期**: 2026-09-16
> **最后验证**: 2026-09-16（基于 Phase 6c 收官 + Sprint 33+ 收官 baseline）
> **作者**: Architecture Working Group + Oracle 评审 (`task_id=ses_f55f307f6ffeRJ9SIny8iUbZ8Y`)
> **状态**: 🔍 Proposed (Master Plan 草案，待 24h cooling-off + GitHub Issue self-review)

---

## 一、Baseline (项目当前状态)

### 1.1 Phase 进度
- **Phase 6c** (2026-08-19 ~ 09-09, ~80h) — 🟢 实质 ship 完成 (11/13 任务)
- **Sprint 33+** (Day 1-5 2026-09-16) — real-LLM 验证 + 收盘，7 changes ship + archived
- **Phase 7** — ⏸ Gated (3/6 启动条件 FAIL，结构性不满足 Solo Dev ~27h/周)
- **Phase 8a/b** — ⏸ Gated by Phase 7a ship ≥3 月

### 1.2 Active vs Archive
- **openspec/changes/** 当前 active: **0** (空，干净起点)
- **最近 archive** (2026-09-16): 7 changes
  - adr-0087-root-cause-upgrade, real-llm-core-coverage, chat-real-llm-coverage
  - cloud-adapter-threading-root-cause, fix-timer-callback-dtor-race
  - skill-interpreter-ipc-rellm, pdk-chat-session-shim-cleanup

### 1.3 关键 Baseline 数据
- **ctest baseline**: 243/243 PASS（2026-09-16 verified, per `AGENTS.md` "Recent Changes"）
- **adr_lint.py**: 0 errors
- **docs_drift_audit**: 0 DRIFT items
- **openspec validate**: clean

### 1.4 关键 bug（驱动本 plan 存在）
- **Bug 1**: `pdk/loop_agent/src/pdk_entry.cpp:166-329` `loop/run` 工具返回契约缺 `ok/error_code` 字段
- **Bug 2**: `pdk/chat_session/src/chat_session.cpp:500-503` `result.success = true` 无条件覆盖吞错误
- **Bug 3**: `pdk/loop_agent` 仅注册 2 工具（`loop/set_parent_provider`, `loop/run`），但 `lib/loop/*.agent.md` 引用 3 个未实现工具
  - `loop/decide_react` (react.agent.md L25)
  - `loop/execute_plan` (plan_execute.agent.md L23)
  - `loop/process_task` (fork_join.agent.md L21/29/37)

### 1.5 既有契约栈（Wave 2 复用基础）
- **ADR-0083** IEvaluator/RewardSignal Contract (✅ V2 Shipped 2026-08-27)
- **ADR-0084** Mutation Governance Contract (✅ V1 Shipped 2026-08-26)
- **ADR-0086** Credit Assignment Contract (🔍 Proposed 2026-08-31)
- **ADR-0080** AppendOnlyEventLog (✅ Approved v1.1)
- **ADR-0061-13** Distillation Output Format (✅ Approved + Shipped 2026-08-29)

---

## 二、Dependency Graph

```
[Wave 1 - 修 chat demo, P0 必要]
═══════════════════════════════════════════════════════════════
   C0 (fix-loop-run-return-contract) ⊥ C1 (loop-agent-tools)
              │                            │
              └─────────┬──────────────────┘
                        ▼
                 [Sprint 34 ship gate]
                        │
                        ▼
[Wave 2 - 自进化骨架, P1 中期]
═══════════════════════════════════════════════════════════════
              C2 (genome-registry) ──── hard dep on C0+C1
                        │
                        ▼
              C3 (h-d-m-transition-guard) ──── hard dep on C2
                        │
                        ▼
                 [Sprint 35 ship gate]
                        │
                        ▼
[Wave 2.5 - Pilot 实验, P2 紧跟]
═══════════════════════════════════════════════════════════════
              C4 (harness-rsi-pilot) ──── hard dep on C3
                        │
                        ▼
              [Go / No-Go Decision Gate]
                        │
            ┌───────────┴───────────┐
            ▼                       ▼
         [Go]                  [No-Go]
    扩展 Model-RSI 方向       归档 Wave 2 skeleton
    (立项 ADR-0078 pilot)    等待需求驱动
```

### 依赖关系详细

| 关系 | 类型 | 含义 | 调度 |
|------|------|------|------|
| C0 → C1 | **parallel (none)** | 两者独立 | Sprint 34 同 Sprint 并行 |
| C0 + C1 → C2 | **hard** | C2 需要 chat demo 能跑通才能采集真实事件流 | Sprint 35 起 |
| C2 → C3 | **hard** | C3 状态机需要 Genome 版本号才能判断"过期数据" | Sprint 35 内 |
| C3 → C4 | **hard** | Pilot 需要 H→D→M 守卫 | Sprint 36 |

---

## 三、Change Overview

| # | Slug | 类型 | 估时 | 依赖 | 状态 | Sprint |
|---|------|------|------|------|------|--------|
| **C0** | `fix-loop-run-return-contract` | immediate-placeholder | 1-2h | None | ✅ | 34 |
| **C1** | `loop-agent-tools` | immediate-placeholder | 3-5h | None (∥ C0) | ✅ | 34 |
| **C2** | `genome-registry` | hard-placeholder | 1 周 | C0+C1 | ⚪ | 35 |
| **C3** | `h-d-m-transition-guard` | hard-placeholder | 2-3 天 | C2 | ⚪ | 35 |
| **C4** | `harness-rsi-pilot` | hard-placeholder | 1-2 周 | C3 | ⚪ | 36 |

**总估时**: 3-4 周（基于 Oracle `ses_f55f307f6ffeRJ9SIny8iUbZ8Y` 评审）

### 类型说明
- **immediate-placeholder**: Wave 1 修复 bug 的高优先级 change，待写完整 proposal/design/tasks/specs
- **hard-placeholder**: 依赖上游 change ship + 接口稳定后才能详细制定

---

## 四、Detailed Tracking

### C0: `fix-loop-run-return-contract`

**类型**: immediate-placeholder
**估时**: 1-2h
**Sprint**: 34 (Wave 1)

**目标**: 让 `loop/run` 工具返回 `{"ok": false, "error_code": ..., "response": ""}` 契约 + ChatSession 消费 `ok` 字段

**Why（精简）**:
- 现有 `loop/run` 返回 `{"success", "response", "steps", "tokens_used", "cost_usd"}` 5 字段
- 错误路径返回 `{"success": false, "error", "steps": 0}` 但 ChatSession 看不到 success 字段（只读 response/steps 等）
- `chat_session.cpp:503` `result.success = true` 无条件覆盖
- Oracle 评审 (C1): 治标不治本，**应改契约而非改消费方**

**What（精简）**:
1. `pdk_entry.cpp` 改 `loop/run` 返回契约：新增 `ok` 字段（语义对齐 ToolResult.ok），所有返回路径统一
2. `chat_session.cpp:500-503` 改消费 `ok` 字段而非 success 字段
3. 加 regression test：mock 错误响应时 ChatResult.success=false + error_message 非空

**Out of Scope**: tool 内部错误处理逻辑（保持现状）

**Verification**: 真实 LLM 模式 loop 内部失败 → Assistant 显示 error_message 而非空

**详细制定 TODO** (待起草 proposal 时填充):
- [ ] 1. Oracle 咨询: `ok` 字段是否对齐 ToolResult.ok 语义还是新设计
- [ ] 2. 写完整 design.md (契约 diff + 兼容性矩阵)
- [ ] 3. 写完整 tasks.md (RED-GREEN-REFACTOR 5 步)
- [ ] 4. 写完整 spec.md (R1: loop/run 返回 ok 字段; R2: ChatSession 消费 ok; R3: 错误路径透传 error_code)
- [ ] 5. 移除 PLACEHOLDER 标记 → openspec validate → ship
- [ ] 6. 启动 Sprint 34 实施

---

### C1: `loop-agent-tools`

**类型**: immediate-placeholder
**估时**: 3-5h
**Sprint**: 34 (Wave 1, ∥ C0)

**目标**: 实现 3 个 `lib/loop/*.agent.md` 引用的 loop-specific 工具 + C++/DSL 双循环分工决策文档化

**Why（精简）**:
- `react.agent.md` 引用 `loop/decide_react`（从未实现）
- `plan_execute.agent.md` 引用 `loop/execute_plan`（从未实现）
- `fork_join.agent.md` 引用 `loop/process_task`（从未实现）
- 导致所有 loop 走 0 步 + 空 Assistant
- Oracle 评审 (M1): 必须写明 C++/DSL 双循环功能重叠的分工决策

**What（精简）**:
1. 实现 `loop/decide_react`: 接受 LLM response → 解析 OpenAI function_call JSON / XML tool tags → 返回 `{action_tool, action_args, final, success}`
2. 实现 `loop/execute_plan`: 接受 plan markdown + context → DSLEngine 追加子图 → 返回 `{success, results, total_steps}`
3. 实现 `loop/process_task`: 接受 `{branch_id, query, tools}` → 单线程 react-style 执行 → 返回 `{success, branch_id, result}` (std::async 并行 fan-out 由 DSL `fork` 节点编排)
4. `+ loop/set_capture_mode`: 接受 mode ∈ {None, Training} → 返回成功（让外部切换 Data-RSI 采集）
5. **必须** 在 proposal/design 写明：C++ `agent_loops/react_loop.h` (Sprint 4) 与 DSL `lib/loop/react.agent.md` 是**两套并行实现**，分工 = C++ 为原语层（性能敏感）/ DSL 为编排层（可热更新）
6. **必须** 收窄 `bus_ptr` 字符串裸指针透传：加注释 + 审计点，新工具一律显式注入 (Oracle B2)

**Out of Scope**: Model-RSI、ADR-0078 LoRA 训练

**Verification**: `react.agent.md` 端到端 total_steps ≥ 1 + 3 个新工具单测 + 双循环分工决策文档

**详细制定 TODO** (待起草 proposal 时填充):
- [ ] 1. 决策前置: 3 工具的输入输出 schema (D1-D3) + 解析规则 (JSON function_call vs XML tags vs 自然语言)
- [ ] 2. 写完整 design.md (3 工具实现 + 双循环分工决策 + bus_ptr 边界规则)
- [ ] 3. 写完整 tasks.md (3 工具 × 5 步 = 15 步)
- [ ] 4. 写完整 spec.md (R1: decide_react 协议; R2: execute_plan 协议; R3: process_task 协议; R4: set_capture_mode 协议; R5: bus_ptr 边界规则)
- [ ] 5. 移除 PLACEHOLDER → openspec validate → ship
- [ ] 6. 启动 Sprint 34 实施 (与 C0 并行)

---

### C2: `genome-registry`

**类型**: hard-placeholder
**估时**: 1 周
**Sprint**: 35 (Wave 2)
**依赖**: C0 + C1 ship (chat demo 端到端跑通)

**目标**: 把 `config.json` + `lib/loop/*.agent.md` + `ChatConfig` 抽象为可版本化的 Genome 对象 + 文件系统 Registry

**Why（精简）**:
- MetaRSI-v1 "Genome" 概念：Harness 完整配置打包为可版本化、可分享对象
- HydraForge 现状：配置散落 3 个地方（config.json, .agent.md, ChatConfig 隐式）
- 没有可版本化抽象，Harness-RSI 变异没有 diff/rollback/fork 锚点
- 自进化架构 §一明确"Genome 是 Harness 资产的可追溯性基础"

**What（精简）**:
1. `pdk/genome/spec/genome-v1.yaml` — Genome CRD schema (apiVersion, metadata{name,version,parent,created_by,capture_mode}, spec{harness{system_prompt,loop_type,workflow}, tools[], budget, model_routing, prompt_cache_prefix})
2. `include/agenticdsl/genome/registry.h` — `IGenomeRegistry` 接口 (load(name@version) / commit(genome) / fork(name, parent, mutations) / list_versions(name) / diff(v1, v2))
3. `src/core/genome/registry.cpp` — 文件系统后端 (`~/.hydraforge/genomes/<name>/<version>/genome.yaml`)
4. 谱系追踪：commit 强制 `parent` 字段必填
5. 完整性校验：HMAC 签名 + schema validation
6. 8 个 test case (load/commit/fork/diff/version_listing/parent_validation/HMAC/invalid_schema)

**Out of Scope**:
- Git-LFS 后端（filesystem 优先）
- Genome-aware ChatSession/DSLEngine 构造参数（避免 BREAKING，留作 follow-up）

**Verification**: Genome commit/fork/rollback 往返 + 版本 diff 确定性测试

**详细制定 TODO** (待 C0+C1 ship 后):
- [ ] 1. 决策前置: storage backend (D9 filesystem vs SQLite vs Git-LFS) + signature scheme (D10 HMAC vs ed25519)
- [ ] 2. 写完整 design.md (Genome schema + IGenomeRegistry 接口 + filesystem layout + 谱系图)
- [ ] 3. 写完整 tasks.md (schema 8 case × 5 步 + registry 6 case × 5 步)
- [ ] 4. 写完整 spec.md (R1: Genome CRD schema; R2: IGenomeRegistry 接口; R3: 谱系追踪; R4: HMAC 完整性; R5: 错误传播契约)
- [ ] 5. 移除 PLACEHOLDER → openspec validate → ship
- [ ] 6. 启动 Sprint 35 实施

---

### C3: `h-d-m-transition-guard`

**类型**: hard-placeholder
**估时**: 2-3 天
**Sprint**: 35 (Wave 2)
**依赖**: C2 ship (Genome Registry 提供版本号)

**目标**: ~200 行轻量状态机 + can_transition() 强制 H→D→M 顺序，**取消** 3 算子接口（映射既有 ADR 栈）

**Why（精简）**:
- MetaRSI-v1 关键规则：禁止 H→M 直跳（用旧数据训练新能力导致目标混乱）
- 必须 H→D→M（用新 Harness 跑一遍生成数据，再训练）
- 验证逻辑与生成分离（确定性代码，模型无权改）
- Oracle 评审 (M2): 取消原计划的 3 算子接口框架 (IDataRSI/IHarnessRSI/IModelRSI) — **与 ADR-0083/0084/0086 契约栈重复**
- YAGNI 原则：pilot 验证价值前不造重型框架

**What（精简）**:
1. `include/agenticdsl/evolution/transition_guard.h` — `TransitionGuard` 类
2. `can_transition(from_state, to_state, current_genome_version, last_harness_change_version) -> EvolutionVerdict`
3. 核心规则: `H → M` 直跳返回 `{can_proceed: false, reason: "H→M forbidden, must run H→D→M"}`
4. 复用既有契约:
   - 评估侧 → ADR-0083 IEvaluator 接口
   - 变异侧 → ADR-0084 MutationGovernance (gate-and-audit)
   - 归因侧 → ADR-0086 CreditAssignment (Proposed, 降级为可选)
5. `evaluate_readiness()` 4 条件矩阵: 归因 Attributed + 回归门 PASS + 预算充足 + 无未控制混杂
6. 12 个 test case: H→D ✓, H→M ✗, D→M ✓, D→H ✓, M→D ✓, M→H ✓, evaluate_readiness 4×2 矩阵

**Out of Scope**:
- 3 算子接口 (IDataRSI/IHarnessRSI/IModelRSI) — Oracle 评审取消
- Model-RSI 实际执行（依赖 ADR-0078 LoRA 训练管线）
- IModelRSI 仅在 ADR-0078 下登记占位，不写代码

**Verification**: `can_transition(H→M)` 返回 false 编译期+运行期双重断言 + H→D→M 全路径通过

**详细制定 TODO** (待 C2 ship 后):
- [ ] 1. 决策前置: 状态机范围 (4 状态 H/D/M/Done vs 简单 Ready/NotReady) + 与 ADR-0086 集成方式
- [ ] 2. 写完整 design.md (can_transition 算法 + 4 条件矩阵 + 复用现有契约)
- [ ] 3. 写完整 tasks.md (~200 行实现 + 12 case 测试)
- [ ] 4. 写完整 spec.md (R1: H→M 禁止; R2: H→D→M 强制; R3: evaluate_readiness 4 条件; R4: 与 ADR-0083/0084/0086 集成点)
- [ ] 5. 移除 PLACEHOLDER → openspec validate → ship
- [ ] 6. 启动 Sprint 35 实施

---

### C4: `harness-rsi-pilot`

**类型**: hard-placeholder (Go/No-Go Decision Gate)
**估时**: 1-2 周
**Sprint**: 36 (Wave 2.5)
**依赖**: C3 ship (transition guard 就绪)

**目标**: IHarnessRSI 首个真实实现 + 端到端 mock 闭环 + 真实 LLM 1 turn 验证

**Why（精简）**:
- YAGNI 原则：pilot 验证 Harness-RSI 价值前不造重型调度框架
- Oracle 评审：pilot 结果决定后续是否需要更重的 Model-RSI 方向
- 必须证明 H→D→M 守卫有效 + Harness 变异经 ApprovalPolicy 拦截

**What（精简）**:
1. `src/modules/evolution/harness_rsi.cpp` — IHarnessRSI 首个实现 (直接调 ChatConfig::override_* 方法)
2. 接受 `GenomeMutations{prompt_delta, tools_add/remove, workflow_patch}` → 产出新 Genome
3. 强制走 `evaluate_readiness()` + `MutationGovernanceVerdict` 双重门禁
4. Mock 闭环: Genome 变异 → ApprovalPolicy 通过 → 重新加载到 ChatSession → 1 turn 验证响应
5. 真实 LLM 1 turn: D2 配置 deepseek, 1 个 prompt_delta 验证应用
6. 6 个 test case (mock 闭环 3 + 真实 LLM 1 + ApprovalPolicy 拦截 2)

**Go 决策标准** (pilot 完成后):
- Mock 闭环全部通过
- 真实 LLM 1 turn 验证 prompt delta 生效
- ApprovalPolicy 拦截测试通过
- ctest 零回归

**No-Go 决策**: 归档 Wave 2 skeleton，等待需求驱动 (e.g., 真实训练数据 / 评估基线就绪)

**Out of Scope**:
- Model-RSI 实际执行
- 真实 LoRA 训练
- 多 Agent 协同进化

**Verification**: mock 闭环 + 真实 LLM 1 turn + 变异经 ApprovalPolicy 拦截

**详细制定 TODO** (待 C3 ship 后):
- [ ] 1. 决策前置: IHarnessRSI 输入 schema (mutation 表达力) + 双门禁交互协议
- [ ] 2. 写完整 design.md (HarnessRSI 实现 + 端到端 mock 流程 + ApprovalPolicy 交互)
- [ ] 3. 写完整 tasks.md (mock 闭环 5 case × 5 步 + 真实 LLM 1 case)
- [ ] 4. 写完整 spec.md (R1: IHarnessRSI 接口; R2: MutationGovernance 集成; R3: 端到端 mock 流程; R4: 真实 LLM 1 turn 契约; R5: ApprovalPolicy 拦截)
- [ ] 5. 移除 PLACEHOLDER → openspec validate → ship
- [ ] 6. 启动 Sprint 36 实施 + Go/No-Go 决策

---

## 五、Sprint Breakdown

### Sprint 34 (Wave 1: 修 chat demo, ~5-7h, P0 必要)

| Change | 估时 | Type | 任务 |
|--------|------|------|------|
| **C0** `fix-loop-run-return-contract` | 1-2h | immediate | 改 loop/run 契约 + ChatSession 消费 ok 字段 |
| **C1** `loop-agent-tools` | 3-5h | immediate | 3 工具实现 + 双循环分工决策 + bus_ptr 边界 |

**并行执行**: C0 和 C1 由 2 个独立子 agent 并行

**Ship Gate (必须全部通过)**:
- [ ] 真实 LLM 模式跑通：输入消息 → Assistant 显示非空文本 + total_steps ≥ 1
- [ ] 3 工具单测全部 PASS
- [ ] ctest 全量 243/243 零回归
- [ ] adr_lint 0 errors
- [ ] docs_drift_audit 0 DRIFT
- [ ] openspec validate clean
- [ ] dual-agent review (Metis + Oracle) 通过

**变更依据**: 本 master plan + 每个 change 的 OpenSpec artifacts

---

### Sprint 35 (Wave 2: 自进化骨架, ~1.5 周, P1 中期)

| Change | 估时 | Type | 任务 |
|--------|------|------|------|
| **C2** `genome-registry` | 1 周 | hard | Genome CRD + IGenomeRegistry + filesystem 后端 |
| **C3** `h-d-m-transition-guard` | 2-3 天 | hard | ~200 行状态机 + can_transition H→D→M 强制 |

**并行**: C2 和 C3 顺序（C3 依赖 C2 的 Genome 版本号接口）

**Ship Gate**:
- [ ] Genome commit/fork/rollback 往返测试通过
- [ ] can_transition(H→M) 编译期+运行期双重断言通过
- [ ] 12 个 transition guard test case 全部 PASS
- [ ] ctest 零回归
- [ ] dual-agent review (Metis + Oracle) 通过

---

### Sprint 36 (Wave 2.5: Pilot 实验, ~1-2 周, P2 紧跟)

| Change | 估时 | Type | 任务 |
|--------|------|------|------|
| **C4** `harness-rsi-pilot` | 1-2 周 | hard | IHarnessRSI 首个实现 + 端到端 mock 闭环 + 真实 LLM 1 turn |

**Go/No-Go Decision Gate** (pilot 完成后):
- **Go** → 立项 ADR-0078 Model-RSI pilot（Wave 3）
- **No-Go** → 归档 Wave 2 skeleton，等待需求驱动

**Ship Gate (Go 路径)**:
- [ ] Mock 闭环 3 case 通过
- [ ] 真实 LLM 1 turn 验证 prompt delta 生效
- [ ] ApprovalPolicy 拦截 2 case 通过
- [ ] ctest 零回归
- [ ] dual-agent review 通过
- [ ] **Go/No-Go 决策记录入 §10 Drift Log**

---

## 六、Risks

| # | 风险 | 影响 | 缓解 |
|---|------|------|------|
| R1 | C0 的 `ok` 字段契约与 ToolResult.ok 语义冲突 | ChatSession 消费侧需调整 | 起草 proposal 时先 Oracle 咨询 D1 |
| R2 | C1 的 3 工具实现 + 解析规则歧义 | react agent 可能误判 LLM 输出 | 起草时决策 D1-D3 (JSON function_call vs XML tags vs 自然语言) |
| R3 | C++/DSL 双循环长期二选一时机未明 | 重复实现难收敛 | Change 2 proposal 写明分工决策 + 触发条件: 任一循环出现第 3 个消费者才统一重构 |
| R4 | `bus_ptr` 字符串裸指针透传扩大使用面 | 内存安全风险 | C1 必须加注释+审计点；新工具一律显式注入 (Oracle B2) |
| R5 | `chat_session.cpp:485` 修复后 chat_session 测试需重测 | 浪费 0.5 天 | TSan 跑 test_chat_session_recovery 验证锁顺序契约 |
| R6 | C2 的 Genome schema 过度设计 | 后续调整成本高 | 起草时决策 D9 (filesystem) + D10 (HMAC)；过度设计倾向 → 简化为最小可用集 |
| R7 | C3 的状态机与 ADR-0086 集成未确定 | CreditAssignment 是 Proposed，未 ship | 起草时降级为可选依赖 |
| R8 | C4 pilot No-Go 决策后 Wave 2 skeleton 浪费 | 投入沉没 | Sprint 35 收官时预审，如果 pilot 假设不成立提前终止 |
| R9 | Single-Dev 流程成本未计入排期 | 5 changes × issue + 24h cooling-off + checklist = 2-3h | 排期 + 0.5h 流程缓冲 |
| R10 | MetaRSI-v1 论文真实性未验证 (Oracle 评审声明) | 设计依据弱 | OpenSpec artifacts 引用时标注 "external framework reference, unverified" |

---

## 七、Maintenance Rules

### 7.1 本 plan 的更新触发
- 任何 change 状态变化 → 更新 §四 + §三
- 任何 Sprint ship gate 决策 → 更新 §五
- 任何风险实现 → 更新 §六
- 任何 Go/No-Go 决策 → 更新 §五 + §十 Drift Log
- 任何漂移检测 → 更新 §十
- 任何调整 → 更新 §十一
- 任何战略方向变化 → 更新 §十二

### 7.2 占位符填充触发
- 任何 hard-placeholder 的依赖 ship 后 → 调用 `open-spec-placeholder-fill` 子技能
- 任何 immediate-placeholder 进入实施 → 写完整 proposal/design/tasks/specs

### 7.3 Review Gates 强制
- Sprint 收官必须更新 §10 Drift Log
- 每 2-3 Sprint 必须做 §9 架构漂移 gate
- 每个 placeholder 启动前必须做 §9 依赖刷新 gate
- 季度必须做 §9 战略对齐 gate

### 7.4 状态标签规范
- ⚪ placeholder (未起草)
- 🟡 active (已起草 proposal/spec/tasks，实施中)
- ✅ done (shipped + archived)
- ⛔ blocked (依赖未 ship)
- ❌ abandoned (Go/No-Go 决策 No-Go)

---

## 八、References

### 8.1 关键 ADR 契约栈（Wave 2 复用）
- **ADR-0083** IEvaluator/RewardSignal Contract: `docs/adr/adr-0083-evaluator-reward-contract.md`
- **ADR-0084** Mutation Governance Contract: `docs/adr/adr-0084-mutation-governance-contract.md`
- **ADR-0086** Credit Assignment Contract: `docs/adr/adr-0086-credit-assignment-contract.md` (🔍 Proposed)
- **ADR-0080** AppendOnlyEventLog: `docs/adr/adr-0080-append-only-event-log.md`
- **ADR-0061-13** Distillation Output Format: `docs/adr/skill/adr-0061-13-distillation-output-format.md`
- **ADR-0078** Fine-tune Base Model: `docs/adr/adr-0078-finetune-base-model.md` (🔍 Proposed, Model-RSI 依赖)

### 8.2 关键架构文档
- `docs/architecture/self-evolution-architecture-2026-08.md` (🔍 Proposed, 边界定义)
- `docs/architecture/agent-evolution-pipeline.md` (✅ Approved, 4 阶段管线)
- `docs/architecture/axis6-chain-workflow-architecture-2026-08.md` (🔍 Proposed v1.1, 7 缺口)
- `docs/architecture/capability-application-map-2026-08.md` (✅ Active v2.5, 31 项能力)

### 8.3 关键既有 OpenSpec archive (pattern 参照)
- `openspec/changes/archive/2026-07-20-loop-agent-dsl-execution/` (loop_agent DSL 实施, Change 2 模板)
- `openspec/changes/archive/2026-09-15-pdk-chat-session-shim-cleanup/` (PDK shim 清理, Change 1/2 模板)
- `openspec/changes/archive/2026-08-03-promote-event-builder-fulltoolresult-support/` (EventBuilder V2, 契约模式)

### 8.4 关键 recent AGENTS.md 段
- `AGENTS.md` §模式 6 (Contract-layer utility tool pattern) — Change 1/2 设计参考
- `AGENTS.md` §模式 7 (Concurrent ctest race detection) — Change 0 TSan 验收参考
- `AGENTS.md` §模式 8 (OpenSpec dual-agent review) — Wave 1/2/2.5 review 模式

### 8.5 外部参考 (unverified, 引用需标注)
- MetaRSI-v1 论文 (清华/北大/斯坦福, 2026-XX, 论文真实性待用户/团队验证)
  - 3 算子 RSI 框架: Data-RSI / Harness-RSI / Model-RSI
  - 关键规则: 禁止 H→M 直跳，必须 H→D→M
  - Genome 概念: Harness 完整配置可版本化对象
  - 验证与生成分离原则

### 8.6 Oracle 评审 session
- `task_id=ses_f55f307f6ffeRJ9SIny8iUbZ8Y` (2026-09-16, 修订本 plan)

---

## 九、Review Gates

### 9.1 🔄 Sprint Review Gate（每个 Sprint 收官）
**Trigger**: Sprint 收官时
**Check**:
- 行为符合预期？
- bug 数？
- 假设错误？
- 时间漂移 > 30%？
**Output**: 添加行到 §十 Drift Log

### 9.2 🧭 Architecture Drift Gate（每 2-3 Sprint）
**Trigger**: 每 2-3 Sprint 自动触发
**Check**:
- ADR 描述仍匹配实现？
- ADR 状态需要更新？
- 文档/代码漂移？
**Output**: ADR-fix sub-change (类似 archive `2026-09-16-adr-0087-root-cause-upgrade`)

### 9.3 🔗 Dependency Refresh Gate（每个 placeholder 启动前）
**Trigger**: 任何 hard-placeholder 进入实施前
**Check**:
- 依赖真 ship 了？
- 接口匹配？
- 谱系是否变化？
**Output**: 调整 placeholder 内容 → §十一 Adjustment Log

### 9.4 🎯 Strategic Alignment Gate（季度）
**Trigger**: 季度自动触发
**Check**:
- backlog 仍服务 goals？
- 新 ADR？
- Phase 7 启动条件复评？
- 路线图大方向变化？
**Output**: 重大方向调整 → §十二 Strategic Pivots + 新 master plan

---

## 十、Drift Log

| 日期 | Sprint | Drift 描述 | 解决 | Commit |
|------|--------|-----------|------|--------|
| 2026-09-17 | 34 | **C0 ship-with-fixes**: Oracle review (session `ses_f54ef2010ffeLK90Y0OQp1DuxJ`) 发现 4 项 (Critical commit 顺序, Major spec R3/R2 producer-correction, Minor test 名 + spec text alignment). 全部修正 ship. | 应用 Critical/Major/Minor; 3 atomic commits `f84dbb3` + `d21ac6f` + `f3fbb9d`; archive 5 文件完整 | `f84dbb3` + `d21ac6f` + `f3fbb9d` |
| 2026-09-17 | 34 | **C1 ship-with-fixes**: Metis dual-agent review (session `ses_f53731302ffegMN8KTsrzqrPOB`) 发现 4 Major (M1 merge_patch 语义未定义, M2 子图 registry 隔离 NOTE 缺失, M3 thread_local per-thread 声明缺失, M4 E2E 自动化缺失). 全部修正 ship. | design.md D2/D4 加 NOTE, tasks.md §6.1 改自动化 E2E; deep agent `bg_9a5f7c89` 实施 22 case PASS; atomic commit `f4766be`; archive 5 文件完整 (路径 `2026-09-17-2026-09-16-loop-agent-tools` 因当前日期 9-17 + 创建日期 9-16 双前缀) | `f4766be` |

---

## 十一、Adjustment Log

| 日期 | Change | 调整 | 原因 |
|------|--------|------|------|
| 2026-09-17 | C1 | tools 字段 v1 忽略（留 future field） | Metis N1 minor: design.md 隐式接受, tasks §3 未列 |
| 2026-09-17 | C1 | `loop/decide_react` response 字段保持原文 | design.md D1 L1/L2/L3 统一返回原文, spec Scenario 强化 (Metis N3) |
| 2026-09-17 | C1 | 错误码仅断言活跃值 (InvalidParams/Unknown) | C1 仅产 2 个值, Cancelled/ToolNotRegistered 是保留值 (Metis N5) |

---

## 十二、Strategic Pivots

| 日期 | Pivot | 触发 | 影响 |
|------|-------|------|------|
| (空) | - | - | - |

---

## 十三、Response Change Types

| 类型 | 触发 | 工作流 |
|------|------|--------|
| **fix** | ship 后发现 bug | 创建 `fix-<name>` OpenSpec change → ship-with-fixes |
| **retro** | sprint 收官复盘发现系统性改进 | 创建 `<name>-retro` OpenSpec change |
| **redirect** | 战略对齐发现方向需调整 | 创建 `<name>-redirect` OpenSpec change + 更新本 plan §十二 |

---

## 附录 A: Physical Layout

```
docs/superpowers/plans/
└── 2026-09-16-pdk-chat-demo-evolution-roadmap.md     # 本文件 (Master Plan tracker)

openspec/changes/
├── 2026-09-16-fix-loop-run-return-contract/         # C0 immediate-placeholder
│   ├── .openspec.yaml
│   ├── proposal.md          # STATUS: PLACEHOLDER
│   ├── tasks.md             # 5-10 sections TBD
│   └── specs/
│       └── loop-run-contract/spec.md
├── 2026-09-16-loop-agent-tools/                    # C1 immediate-placeholder
│   ├── .openspec.yaml
│   ├── proposal.md          # STATUS: PLACEHOLDER
│   ├── tasks.md
│   └── specs/
│       └── loop-agent-tools/spec.md
├── 2026-09-16-genome-registry/                      # C2 hard-placeholder
│   ├── .openspec.yaml
│   ├── proposal.md          # STATUS: PLACEHOLDER + dep on C0+C1
│   ├── tasks.md
│   └── specs/
│       └── genome-registry/spec.md
├── 2026-09-16-h-d-m-transition-guard/               # C3 hard-placeholder
│   ├── .openspec.yaml
│   ├── proposal.md          # STATUS: PLACEHOLDER + dep on C2
│   ├── tasks.md
│   └── specs/
│       └── transition-guard/spec.md
└── 2026-09-16-harness-rsi-pilot/                    # C4 hard-placeholder (Go/No-Go)
    ├── .openspec.yaml
    ├── proposal.md          # STATUS: PLACEHOLDER + dep on C3
    ├── tasks.md
    └── specs/
        └── harness-rsi-pilot/spec.md
```

---

## 附录 B: 实施路径变更依据

| 来源 | 关键决策 |
|------|---------|
| `openspec/changes/archive/2026-08-03-promote-event-builder-fulltoolresult-support/` | EventBuilder 契约模式 (C0 参考) |
| `openspec/changes/archive/2026-07-20-loop-agent-dsl-execution/` | loop_agent DSL 实施 (C1 参考) |
| `openspec/changes/archive/2026-09-15-pdk-chat-session-shim-cleanup/` | PDK shim 清理模式 (C1 bus_ptr 边界) |
| Oracle session `ses_f55f307f6ffeRJ9SIny8iUbZ8Y` | C1 修正 (契约对齐)、M1 修正 (双循环分工)、M2 修正 (取消 3 算子接口) |

---

**Last Updated**: 2026-09-16
**Next Review**: 24h cooling-off 后 self-review
**Maintainer**: Architecture Working Group + Solo Dev
