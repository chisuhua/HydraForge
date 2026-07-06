# yield-stream Specification

## Purpose
Phase 5 Stage 1 Step 2 (C12, IP-001 §Step 2 + Oracle Q3 Option A) — 新增 `NodeType::YIELD` (第 11 个节点类型),单 `YieldNode` + `mode` 枚举 (`NEXT` / `CONTINUE` / `STOP`),支持 token-by-token 流式生成(BOOT-001 阶段 3 自举服务化前置);IGenerationStream pull-based 适配最自然 (`IGenerationStream::next(std::stop_token)` 实际接口 `src/common/llm/llm_types.h:53-61`),**不**用 IInteractionBus 推送(token 高频不适用);CONTINUE 模式每 pull 1 token 检查 `is_budget_exceeded()` + `BudgetExceededException` 携带已消费 token 片段;与 ADR-0030 V2 (Phase 2 异步运行时) 正交,不修改 V2。
## Requirements
### Requirement: yield-node-type-added

`NodeType` 枚举 MUST 包含 `YIELD` 值。

#### Scenario: 枚举值存在

- **WHEN** 检查 `src/core/types/node.h` NodeType enum
- **THEN** 包含 `YIELD` 枚举值 (与现有 10 类型并列)

#### Scenario: Node 继承体系扩展

- **WHEN** 检查 Node struct (多态基类, `virtual execute` + `virtual clone`,见 `src/core/types/node.h:35-55`)
- **THEN** 继承体系新增 `YieldNode : public Node` 子类 (沿用 START/END/ASSIGN/DSL/TOOL_CALL/RESOURCE/FORK/JOIN/GENERATE_SUBGRAPH/ASSERT 模式)
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
- **THEN** execute_yield() 循环 pull `IGenerationStream::next(std::stop_token)`
- **AND** 每 pull **1 token** 检查 `is_budget_exceeded()` (Oracle Risk 11 mitigation, N=1 可配置)
- **AND** 超过预算立即终止流 + 抛 BudgetExceededException
- **AND** stream 结束 (`IGenerationStream::next(token)` 返回 nullopt 或 is_active()=false) 时退出循环

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

CONTINUE 模式 MUST 每 N tokens 检查 budget (**默认 N=1**, 可配置;Oracle Risk 11 mitigation 决议)。

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

