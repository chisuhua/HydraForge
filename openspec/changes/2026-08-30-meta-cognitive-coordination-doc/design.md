# Design — Cognitive-Cognitive 协调模式目录

## Context

2026-08-30 Oracle 综合评审（4 Oracle 并行, session 见 proposal.md）确认:

1. **真实缺口** (Oracle Path 1 共识): `agent-orchestration-architecture-2026-08.md` 中 cognitive-cognitive 协调原语散落 3 处无命名目录
2. **战略对齐** (Oracle Path 4 共识): 全文已 v1.3 修正 ADR-0050 Phase 6 主线 + ADR-0077 descoped + ADR-0086 信用分配待立项
3. **架构合规** (Oracle Path 2+3 共识): IAgent 扩展 (路径 2.3) 与 CognitiveOrchestrator (路径 3.3) 均 No-Go, 文档化是唯一不违反已 Approved ADR 的路径
4. **约束已明** (ADR-0085 §决策 5): Meta-Agent 自管理 V1 不实施, 文档化作为需求消化层

**任务**: 把 §十八 5 模式目录锁定为可独立验证的 OpenSpec change artifact。

## 决策

### 决策 1 — 仅文档修改, 零代码改动

**理由**: 4 Oracle 一致判断: IAgent/CognitiveOrchestrator/MetaAgent 任何新组件均违反已 Approved ADR 的字面或精神。仅文档沉淀是路径 1 (Conditional-Go) 强制条件。

**反向验证**: §九验证命令 #1-#13 全部命令仍可执行, ctest 187/187 baseline 不变 (本 change 触及 0 个 .cpp/.h 测试文件)。

### 决策 2 — 5 模式命名

参考 cap-map §三 17 类应用 + 现有原语接口:

| 模式名 | 原语依据 | 命名理由 |
|--------|----------|----------|
| **sync-delegate** | `IAgentComposition::delegate()` | 同步语义直白, 与异步模式形成对照 |
| **fan-out** | `call_async()` + `ForkJoinLoop::run(branches)` | 经典分布式 fan-out 术语, 行业认知 |
| **hierarchical-plan** | `PlanExecuteLoop::plan_phase` + 嵌套 `delegate()` | 突出 plan→delegate→verify 3 阶段层级 |
| **debate-round** | `call_async()` + `IEvaluator` + `GEPALoop.reflect` 组合 | 与 LLM agent debate literature 对齐 |
| **stream-pipeline** | `IAgentComposition::stream()` (V2 占位) | 流式处理范式名, V2 落地时直接复用 |

**替代方案**:
- ❌ "sync-delegate / async-delegate / plan-execute / fork-join / stream" — 与现有 Loop 名混淆
- ❌ "pattern-1 / pattern-2" — 无业务含义
- ✅ "mode-A / mode-B" — 违反 ADR-0085 §决策 5 SRP 哲学 (拒绝)

### 决策 3 — 强制标注策略（Oracle Path 1 3 个条件）

#### 条件 A: stream-pipeline 标 🔴 V2 占位

**理由**: `iagent_composition.h:67` 直接 `throw std::logic_error("Phase 2 - stream not yet implemented")`。任何"stream-pipeline 可用"的呈现都是 false claim。

**落地**: §十八顶部 3 个标注 + §18.1 表"落地状态"列 + §18.7 段头明示 + §九验证命令 #19 强制 grep

#### 条件 B: debate-round 标 🟡 组合配方

**理由**: `debate-round` 由 3 个独立 ship 的原语组合实现, 不是单一 contract。读者误以为存在 `IDebateRound` 接口是 false claim。

**落地**: §十八顶部 + §18.1 表 + §18.6 段头明示 + §九验证命令 #20 强制 grep

#### 条件 C: 示例代码标注（✅ / 🟡 / 🔴）

**理由**: 避免文档示例代码成为死代码参考。

**落地**: §18.3-§18.7 每段头独立标注状态; stream-pipeline 段明示"伪代码, V2 实装前降级"

### 决策 4 — 17 类应用映射（§18.9）

按 cap-map §三 A1-C4 顺序逐行映射, 不跳类:

| cap-map 应用 | 推荐模式 | 备选 |
|--------------|----------|------|
| A1 单 agent ReAct | — (不需要) | — |
| A2 多轮对话 + fork | — (SessionManager fork) | — |
| A3 受限 LLM 应用 | hierarchical-plan | sync-delegate |
| A4 Fork-Join 聚合 | fan-out | — |
| A5 多 LoRA 路由 | — (model_router plugin) | — |
| A6 审批流 | sync-delegate | — |
| B1 Marketplace | sync-delegate | + HookPattern |
| B2 跨进程多 agent | fan-out | + sync-delegate |
| B3 分布式追踪 | — (叠加 BusPattern) | — |
| B4 Streaming Agent | stream-pipeline (V2 占位) | fan-out 降级 |
| B5 MCP Server | sync-delegate | + HookPattern |
| B6 蒸馏环境 | hierarchical-plan | — |
| B7 自进化 | debate-round | — |
| C1 跨主机联邦 | fan-out (跨进程) | + stream-pipeline |
| C2 自进化 agent | debate-round | + stream-pipeline |
| C3 WASM 沙箱 | sync-delegate | — |
| C4 Cloud-native | (服务化冻结) | — |

### 决策 5 — 升级触发条件（§18.10）

启动 ADR-0085 V2 MetaAgent 评审 + 路径 3.2 MCTS Axis6 立项的 4 个硬条件, 全部"且"语义:

1. `IAgentComposition::stream()` Phase 2 实装
2. ≥2 个真实 `IAgent` 实现类（当前 SimpleAgent 唯一 mock, AgentWorker 推迟 Sprint 24+）
3. S4 promotion criteria 全部满足（self-evolution §六, 含 ADR-0086 信用分配 ship）
4. 用户需求被识别为"运行时动态选择协调模式"

**理由**: 4 条件共同防止"文档已足, 仍过度建设"的反模式。任何一项缺失, §十八 静态选择模式足够覆盖。

## 接口

### 不变量（强制）

```cpp
// 5 模式全部基于以下 4 接口（无新增 contract）
include/agenticdsl/contract/iagent_composition.h   // 4 模式（call/call_async/delegate/stream）
include/agenticdsl/contract/iaction_registry.h       // Agent 实例管理
include/agenticdsl/contract/iaction_hook_registry.h  // Per-step pre/post hook
include/agenticdsl/contract/ievaluator.h              // 评估信号
// + 3 PDK Loop class:
include/agenticdsl/pdk/agent_loops/react_loop.h
include/agenticdsl/pdk/agent_loops/plan_execute_loop.h
include/agenticdsl/pdk/agent_loops/fork_join_loop.h
// + 2 cognitive 编排器:
include/agenticdsl/cognitive/gepa_loop.h
include/agenticdsl/cognitive/mcts_workflow_search.h
```

**反例**（Oracle 拒绝方案, 本 change 明确禁止）:
- ❌ 新增 `cognitive/cognitive_orchestrator.h`（命名冲突 `icognitive_orchestrator.h`）
- ❌ 扩展 `IAgent` 5 个方法（Oracle Path 2 No-Go 2.3）
- ❌ 新增 `IDebateRound` / `IStreamPipeline` 接口（违反"组合配方非原语"原则）
- ❌ 新增 `CrossCuttingMetaAgent`（ADR-0085 §决策 5 V1 不实施）

### 验证接口

```bash
# §九 验证命令 #18-#22 (强制每 Sprint 收官运行)
for pattern in "sync-delegate" "fan-out" "hierarchical-plan" "debate-round" "stream-pipeline"; do
  grep -c "\*\*${pattern}\*\*" docs/architecture/agent-orchestration-architecture-2026-08.md
done
# 预期: 每个 pattern = 2 (§十八 目录表 1 + §四决策树分支 1)

grep "stream-pipeline.*V2 占位" docs/architecture/agent-orchestration-architecture-2026-08.md | head -3
# 预期: 至少 1 行

grep "debate-round.*组合配方" docs/architecture/agent-orchestration-architecture-2026-08.md | head -3
# 预期: 至少 1 行
```

## 反例（明确不做）

| 反例 | 拒绝理由 |
|------|----------|
| 新建 `CrossCuttingMetaAgent` (cross-cutting-hooks-architecture §4.6 设计稿) | ADR-0085 §决策 5 V1 不实施; Oracle Path 4 No-Go 4.3 |
| 扩展 `IAgent` 加 `shared_context()` / `current_goal()` 等 5 方法 | Oracle Path 2 No-Go 2.3 (重复发明 + 真空设计) |
| 新建 `cognitive/cognitive_orchestrator.h` (类比 CrossCuttingOrchestrator) | 命名冲突 (`icognitive_orchestrator.h` 已存在) + Oracle Path 3 No-Go 3.3 |
| 实施 `debate-round` 单一原语 (新增 `IDebateRound` 接口) | 违反"组合配方非原语"原则 + SRP |
| 实施 `stream-pipeline` V2 (填 `stream()` 实际逻辑) | 不在本 change 范围 (ADR-0060 V2 amendment 单独流程) |
| 修改 ADR-0085 §决策 5 文本 | 不在本 change 范围 (ADR-0085 V2 amendment 单独流程) |

## 跨 change 依赖

### 前置依赖（全部已 ship）
- ✅ ADR-0060 (Agent Composition) Approved
- ✅ ADR-0021 (PDK Design) Approved + Sprint 5 ship
- ✅ ADR-0085 (Cross-Cutting Pattern PDK) Approved 2026-08-28
- ✅ ADR-0082 (Agent First-Class Registry) Approved V1 骨架
- ✅ ADR-0083 (IEvaluator) Approved V2 ship 2026-08-27
- ✅ ADR-0084 (Mutation Governance) Approved V1 ship 2026-08-26
- ✅ T19 GEPALoop V1 ship 2026-08-27
- ✅ T20 MCTSWorkflowSearch V1 ship 2026-08-28

### 后续依赖（不在本 change 范围）
- **ADR-0086 credit-assignment-contract.md** — Step 2 (Oracle Path 4 建议), 单独 OpenSpec change
- **ADR-0085 V2 MetaAgent amendment** — 触发条件 §18.10, 后续流程
- **路径 3.2 MCTS Axis6** — 触发条件 §18.10, 后续 OpenSpec change
- **路径 2.2 ICognitiveAgent 子接口** — 挂 AgentWorker change (Sprint 24+, G3 open)
- **ADR-0060 V2 stream Phase 2 实装** — 触发条件 §18.10 #1

## ADR 兼容性

| ADR | 兼容性 | 验证 |
|-----|--------|------|
| ADR-0060 | ✅ 不修改（仅引用现有 4 模式）| `git diff` 0 行 |
| ADR-0021 | ✅ 不修改（仅引用 3 Loop class）| `git diff` 0 行 |
| ADR-0085 | ✅ 不修改 §决策 5 文字（§十八顶部明确引用）| 顶部"与 ADR-0085 §决策 5 的关系" 段存在 |
| ADR-0082 | ✅ 不修改（IAgent V1 骨架不触碰）| `git diff` 0 行 |
| ADR-0083 | ✅ 不修改（IEvaluator V2 仅引用）| `git diff` 0 行 |
| ADR-0050 | ✅ 已 v1.3 同步（§十六 Phase 6 战略对齐 note）| 头部引用 "🔒 冻结" |
| ADR-0077 | ✅ 已 v1.3 同步（§五 C1 标注 Wave 4 descoped）| C1 行 grep 命中 |
