# ADR-0081: Pre-Step Hook Contract（Agent 级拦截点）

## 状态
🔍 Proposed（推迟 — 待 ADR-0079/ADR-0080 实施后定稿）

> **注记**：本 ADR 引用由 ADR-0079/ADR-0080 v1.1 amendment 引入。
> 实际设计待 ADR-0082（Agent First-Class Registry）定稿时联动重设计：
> - 当前设计假设 LLM-scoped（ILLMProvider Decorator 链）
> - Metis 提出应为 Agent-scoped（per-agent 配置，非 per-LLM-call）
> - v1.1 推迟至 AgentRegistry 设计明确后再启动

## 上下文

### 当前拦截点格局

HydraForge 现有的拦截点：
- `ToolCoordinator::hooks_`（`tools/pre-execute` + `tools/post-execute`，per-tool 粒度）
- `TracingDecorator`（`llm.request` / `llm.response`，生命周期事件）
- `IApprovalHandler`（per-tool execution approval）

### 缺口

DSH 的核心拦截点（已在 ADR-0079/0080 v1.1 引用为前置依赖）：

| Hook | 触发时机 | 用途 |
|---|---|---|
| `agent/pre-step` | LLM 推理前 | 系统提示注入、policy injection、PII 过滤 |
| `agent/turn-stopping` | turn 中断 | 协作停止信号 |
| `tools/pre-execute` | tool call 前 | 可扩展 allow/deny/ask |
| `tools/execute` | around dispatch | 超时/重试/metrics |
| `tools/post-execute` | tool call 后 | 替换/审计 |

HydraForge 当前**仅**有 `tools/*` 三个 hook，缺失 `agent/*` 两个 hook。

## 决策

###决策 D1：Hook 分类（拟定 —待 ADR-0082 定稿联动）

| Hook | 设计原则 | 备注 |
|---|---|---|
| `agent/pre-step` | Agent-scoped（per-agent 配置）| 依赖 AgentRegistry（ADR-0082） |
| `agent/turn-stopping` | Agent-scoped（per-agent）| 同上 |
| `tools/*` | Tool-scoped（per-tool，已 ship）| ADR-0031 / ADR-0069 |

## 依赖

- **前置**：ADR-0079/ADR-0080（v1.1 已 ship）
- **阻塞**：ADR-0082（Agent First-Class Registry）定稿
- **阻塞**：ADR-0081 实施需要 AgentRegistry 决定 hooks 是 LLM-scoped 还是 Agent-scoped

## 不变量

1. **拦截点不修改 core 行为**：hook 失败/异常不阻断主流程
2. **可观察**：所有 hook 触发必须发射事件到 IInteractionBus（受 ADR-0080 v1.1 EventLog 捕获）
3. **fail-closed 安全语义**：deny 决策不可被后续 hook 覆盖（与 DSH `ToolGuard` 一致）

## 后果

### 正面后果（实施后）
- ✅ Policy 注入（system prompt 拼接）
- ✅ PII 过滤（emit 前 scrub）
- ✅ 蒸馏数据质量闸门（ADR-0080 D10 依赖）

### 负面后果
- ⚠️ 实施需与 ADR-0082 协调，**不能独立 ship**
- ⚠️ Agent-scoped vs LLM-scoped 之争未决

## 关系

- **依赖**：ADR-0079 v1.1、ADR-0080 v1.1、ADR-0082
- **被引用**：ADR-0080 v1.1 D10（scrub hook 在 capture_prompt_bytes=true 时启用）

## 推迟理由 R1

ADR-0081 推迟至 ADR-0082 定稿后，原因：
- ADR-0082 §争议 C2 决定 hooks 是 agent-scoped 还是 LLM-scoped
- 若 LLM-scoped：ADR-0081 可独立 ship（基于 ILLMProvider Decorator）
- 若 Agent-scoped：ADR-0081 必须与 AgentRegistry 协同设计

两种设计对消费者 API 影响差异大，需待 ADR-0082 决策。

---

**审批记录**：
- 提议：占位（v1.1 引用触发）
- 审批：待 ADR-0082 定稿
- 实施：未开始