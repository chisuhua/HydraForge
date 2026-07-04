# yield-stream Specification

> **Purpose**: 追踪 C12 YIELD/STREAM 节点实施范围
> **关联 proposal**: `../proposal.md`
> **最后更新**: 2026-07-03

## ADDED Requirements

### Requirement: yield-node-type-added

`NodeType` 枚举 MUST 包含 `YIELD` 值。

#### Scenario: 枚举值存在

- **WHEN** 检查 `src/core/types/node.h` NodeType enum
- **THEN** 包含 `YIELD` 枚举值 (与现有 11 类型并列)

#### Scenario: Node 联合体扩展

- **WHEN** 检查 Node struct
- **THEN** 包含 `YieldNode yield_data` 成员
- **AND** YieldNode 包含 yield_value (string) + mode (YieldMode) + stop_path (NodePath)

---

### Requirement: yield-mode-enum-defined

`YieldMode` MUST 定义 NEXT/CONTINUE/STOP 3 个值。

#### Scenario: 3 模式值存在

- **WHEN** 检查 `enum class YieldMode`
- **THEN** 包含 `NEXT = 0`, `CONTINUE = 1`, `STOP = 2`
- **AND** 默认值 = NEXT

---

### Requirement: yield-execution-mode-support

`execute_yield()` MUST 支持 NEXT/CONTINUE/STOP 3 种模式。

#### Scenario: NEXT 模式

- **WHEN** YieldNode.mode == NEXT 且 `session.get_pending_yield()` 为空
- **THEN** execute_yield() 渲染 yield_value 模板, 包装为 ToolResult 返回
- **AND** 设置 `session.pending_yield_` = YieldState{module_path, resume_context}
- **AND** 调用者通过 ToolResult 接收 token

#### Scenario: CONTINUE 模式

- **WHEN** YieldNode.mode == CONTINUE 且 IGenerationStream 活跃
- **THEN** execute_yield() 循环 pull IGenerationStream::pull_next()
- **AND** 每 pull 10 tokens 检查 `is_budget_exceeded()`
- **AND** 超过预算立即终止流 + 抛 BudgetExceededException
- **AND** stream 结束 (pull_next() 返回 nullopt) 时退出循环

#### Scenario: STOP 模式

- **WHEN** YieldNode.mode == STOP
- **THEN** execute_yield() 终止当前 IGenerationStream (如有)
- **AND** TopoScheduler 跳到 stop_path 节点
- **AND** clear `session.pending_yield_` (设为 std::nullopt)

---

### Requirement: yield-pending-state-persisted

`pending_yield_` MUST 在 ExecutionSession 中持久化。

#### Scenario: 跨 await 持久化

- **WHEN** YIELD NEXT 模式设置 pending_yield_ 后, scheduler yield pause
- **AND** 外部调用 `topo_scheduler.resume_yield(session_id, token)` 恢复
- **THEN** ExecutionSession.pending_yield_ 仍包含原 YieldState
- **AND** resume 行为从 resume_context 恢复

---

### Requirement: yield-budget-checked

CONTINUE 模式 MUST 每 N tokens 检查 budget (默认 N=10, 可配置)。

#### Scenario: budget 超限中断

- **WHEN** CONTINUE 模式已 pull 50 tokens, 第 51 次 pull 前 `is_budget_exceeded()` 返回 true
- **THEN** 立即终止流
- **AND** 抛 `BudgetExceededException` 含具体 token 计数
- **AND** DSLEngine::run() 捕获后返回 ExecutionResult{status=failed, error_code=budget_exceeded}

---

### Requirement: yield-resume-supported

TopoScheduler MUST 支持从 `pending_yield_` 恢复执行。

#### Scenario: 端到端 pause/resume

- **WHEN** YieldNode.mode=NEXT 触发 yield pause
- **AND** 外部调用 `topo_scheduler.resume_yield(session_id, "next_token_value")`
- **THEN** scheduler 从 pending_yield_.resume_context 恢复
- **AND** "next_token_value" 注入到 LayeredContext (按 resume_context 指定路径)
- **AND** 继续执行后续节点

## 备注 (Oracle 深度审查 2026-07-03)

本 change 不修改 ADR-0030 V2 异步运行时设计。IGenerationStream pull-based + YieldNode consume 是正交关系, YIELD 节点内部 bridge IGenerationStream 拉取 token (YieldStreamBridge 辅助类, Oracle Risk 9 mitigation)。IInteractionBus 不作为 YIELD 推送通道 (后者是 event broker 不适合 token 高频流)。

### Requirement: oracle-r8-resume-dag-state-persisted (P0)

`resume_yield()` MUST 从持久化的 DAG state 恢复 (不重建)。

#### Scenario: DAG state 持久化

- **WHEN** YIELD NEXT 暂停执行, TopoScheduler 进入 YIELDED 状态
- **THEN** resume_context 保存 `ready_queue` + `in_degree_table`
- **AND** `resume_yield()` 恢复就绪队列与入度表 (不重新计算)
- **AND** 恢复后 DAG 继续执行正确的后续节点

### Requirement: oracle-r11-budget-per-token (P1)

CONTINUE MUST 每 pull 1 token 检查 budget (默认 N=1, 可配置)。

#### Scenario: token 级 budget 检查

- **WHEN** CONTINUE 模式已 pull 50 tokens, 第 51 次 pull 前 `is_budget_exceeded()` 返回 true
- **THEN** 立即终止流 (不等到下一个 batch)
- **AND** `BudgetExceededException` 携带 `consumed_tokens: [token1, token2, ..., token50]`
- **AND** DSLEngine 转换为 ExecutionResult{status=failed, partial_result=<已消费 tokens>}

### Requirement: oracle-r10-cross-thread-yield-safe (P1)

`pending_yield_` 访问 MUST 跨线程安全。

#### Scenario: 跨 worker thread resume

- **WHEN** YIELD NEXT 在 Worker A 设置 pending_yield_
- **AND** Worker B 调用 `resume_yield()` 从 pending_yield_ 恢复
- **THEN** `resume_yield()` 原子操作 (check → clear → continue)
- **AND** TSan 0 data race