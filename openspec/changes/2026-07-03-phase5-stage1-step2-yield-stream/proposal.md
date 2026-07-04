# Proposal: Phase 5 Stage 1 Step 2 — YIELD/STREAM Node (C12)

> **STATUS: ACTIVE** 🟡 (Oracle 深度审查完成 2026-07-03, session `ses_0d5985f3effeS1npyEV6SYk2RW`) — ready for implementation
> **Oracle 审查结果**: 5 个风险已识别 (2 P0 + 2 P1 + 1 P2), 详见 §Risks
> **关联 Oracle 决议**: Q3 — Option A (单 YieldNode + mode 枚举)
> **关联 master plan**: `docs/superpowers/plans/2026-07-03-phase5-self-bootstrapping.md` §十六.3
> **关联 IP-001**: `docs/proposals/implementation-roadmap/01-roadmap.md` §Step 2
> **关联 ADR**: ADR-0030 V2 (Phase 2 异步运行时) — **不修改**, IGenerationStream pull-based + YieldNode 正交
> **前置依赖**: C10 ✅ (module_state 持久化) + C11 ✅ (SessionVars 配置)
> **最后更新**: 2026-07-03

## Why

Phase 5 自举服务化需要支持 token-by-token 流式生成 (BOOT-001 阶段 3 目标前置)。
当前节点模型是"批量完成", 无法在 LLM 生成中途消费部分输出。

依据 IP-001 §Step 2 设计 + Oracle 决议 Q3:
- 单 YieldNode + mode 枚举 (NEXT/CONTINUE/STOP)
- 与历史 Oracle 决议一致 (master plan §十一.1)
- IGenerationStream pull-based 适配最自然 (YIELD 节点内部 `IGenerationStream::next(std::stop_token)`, 实际接口在 `src/common/llm/llm_types.h:53-61`)
- **不**用 IInteractionBus 作为推送通道 (后者是 event broker, 不适合 token 高频流)

## What Changes

### 1. 新增 NodeType::YIELD 枚举

- `src/core/types/node.h`:
  - `NodeType` 枚举加 `YIELD`
  - **Node 继承体系新增 `YieldNode` 子类** (沿用 START/END/ASSIGN/DSL/TOOL_CALL/RESOURCE/FORK/JOIN/GENERATE_SUBGRAPH/ASSERT 模式 — Node 是多态基类,非 union)
  - `YieldNode` 结构:
    ```cpp
    struct YieldNode : public Node {
        std::string yield_value;   // 模板表达式
        YieldMode mode = YieldMode::NEXT;
        NodePath stop_path;        // STOP 模式跳转目标
    };
    enum class YieldMode : uint8_t { NEXT, CONTINUE, STOP };
    ```
  - 反序列化 (MarkdownParser) 支持 yield_value/mode/stop_path 字段

### 2. NodeExecutor execute_yield() 实现

- `src/modules/executor/node_executor.h/cpp`:
  - 新增 `execute_yield(LayeredContext&, YieldNode&)` 方法
  - 内部流程:
    - 渲染 yield_value 模板 → 字符串
    - NEXT: 包装为 ToolResult, 设置 pending_yield_ 状态, 返回调用者
    - CONTINUE: 进入循环, 连续 `IGenerationStream::next(std::stop_token)` 直到 stream 结束或预算耗尽
    - STOP: 终止流, 跳到 stop_path 节点

### 3. ExecutionSession pending_yield_ 扩展

- `src/modules/scheduler/execution_session.h`:
  - 加 `std::optional<YieldState> pending_yield_` 字段
  - `YieldState` 结构: `module_path` + `resume_context` (json)

### 4. TopoScheduler yield pause/resume (Oracle Risk 8 mitigation — DAG state 持久化)

- `src/modules/scheduler/topo_scheduler.h/cpp`:
  - 新增 `enum class SchedulerState { RUNNING, YIELDED, COMPLETED, FAILED }` (Oracle Risk 8)
  - yield 暂停: **不跳出主 while 循环**, 改为在循环内检测 `pending_yield_` 后挂起 (保持 DAG state)
  - resume 回调: 外部调用 `topo_scheduler.resume_yield(session_id, token_value)` 继续执行
  - **DAG state 持久化**: resume_context 保存 `ready_queue` + `in_degree_table` + `running_nodes` (O(|V|+|E|) 避免重建)

### 5. Budget 集成 (Oracle Risk 11 mitigation — 每 token 检查)

- YIELD CONTINUE 模式 **每 pull 1 token** 检查 `is_budget_exceeded()` (非每 N tokens, Oracle Risk 11)
- 超过则立即终止流, 抛 `BudgetExceededException` **携带已消费 token 片段** (非空结果丢弃)

### 6. yield_stream_bridge 桥接 (Oracle Risk 9 mitigation)

- `src/modules/executor/yield_stream_bridge.h/cpp` 新建:
  - 封装 `IGenerationStream::next(std::stop_token)` → `YieldState` 映射 (实际接口在 `src/common/llm/llm_types.h:53-61`)
  - `pull_single()` 用于 NEXT 模式
  - `pull_loop(budget_checker, max_iter)` 用于 CONTINUE 模式
- **不**用 IInteractionBus 作为推送通道 (保持 ADR-0030 V2 决策)

### 7. 跨线程 YIELD 安全 (Oracle Risk 10 mitigation)

- `ExecutionSession.pending_yield_` 访问加 `std::mutex yield_mutex_` (字段级别)
- `resume_yield()` 原子操作: 检查 → 清除 pending_yield_ → 继续 DAG
- TSan 必验证 cross-thread resume 无 data race

## What Does NOT Change

- **ADR-0030 V2 异步运行时** — 完全不动 (IGenerationStream pull 接口保持)
- **IInteractionBus** — 不作为 YIELD 推送通道 (仅用于 tool.audit 事件)
- **Node/NodeType 现有 10 种类型** (START/END/ASSIGN/DSL_CALL/TOOL_CALL/RESOURCE/FORK/JOIN/GENERATE_SUBGRAPH/ASSERT,见 `src/core/types/node.h:22-33`) — 兼容, 仅新增 YIELD
- **DSL 解析器现有功能** — 兼容, 增量支持 YIELD 字段

## Capabilities

### ADDED Requirements

- `yield-node-type-added`: NodeType MUST 包含 YIELD
- `yield-mode-enum-defined`: YieldMode MUST 包含 NEXT/CONTINUE/STOP 3 值
- `yield-execution-mode-support`: execute_yield() MUST 支持 NEXT/CONTINUE/STOP 3 种模式
- `yield-pending-state-persisted`: pending_yield_ MUST 在 ExecutionSession 中持久化
- `yield-budget-checked`: CONTINUE 模式 MUST 每 N tokens 检查 budget
- `yield-resume-supported`: TopoScheduler MUST 支持从 pending_yield_ 恢复执行

## Impact

**修改文件** (估):
- `src/core/types/node.h` (+30 行: enum + struct)
- `src/modules/executor/node_executor.h/cpp` (+80 行 execute_yield)
- `src/modules/scheduler/execution_session.h` (+20 行 YieldState)
- `src/modules/scheduler/topo_scheduler.cpp` (+30 行 yield pause/resume)
- `src/modules/parser/markdown_parser.cpp` (+15 行 YIELD JSON 解析)
- `tests/test_yield_node.cpp` (新, 8-10 test case)

**API 兼容性**: 零 breaking change (NodeType 增量, Node 子类仅新增)

**估时**: 5-6 天 (Oracle 深度审查后从 2.5-3 天调整: +1.5d DAG state + +0.5d stream bridge + +0.3d budget per-token + +0.2d cross-thread + +0.2d exhaust switch + **+1.0d examples/phase5_yield_token_generator/ 示例程序** master plan §四 ship gate 要求 + **+0.3d BudgetExceededException 新异常类型声明** tasks §6.0)

## Risks (Oracle 深度审查 2026-07-03, session `ses_0d5985f3effeS1npyEV6SYk2RW`)

| # | Risk | Severitya | Mitigation | Effort |
|---|---|---|:---:|---|:---:|
| 8 | TopoScheduler DAG state 在 YIELD pause 期间丢失 (topo_scheduler.cpp 跳出循环后 finalize() 清理) | **P0** | §4 DAG state 持久化 (保存 ready_queue + in_degree + running_nodes) | +1.5d |
| 9 | IGenerationStream pull-based 到 YIELD 桥接缺失 | **P0** | §6 YieldStreamBridge 辅助类封装 pull→context | +0.5d |
| 11 | Budget 与 CONTINUE 循环超预算检测延迟 (每 N tokens 才检查) | P1 | §5 每 pull 1 token 检查 budget + 异常携带已消费 token | +0.3d |
| 10 | pending_yield_ 跨 await 线程安全 (DomainWorkerPool 多 worker) | P1 | §7 yield_mutex_ 字段级 + TSan verify | +0.2d |
| 12 | NodeType::YIELD 新增导致 exhaust switch 编译错误 | P2 | tasks.md §1.5 grep 全库 switch (NodeType) 审计 | +0.2d |

**Effort Delta**: +2.7 天 (vs 原始 2.5-3 天估时)

## user decision resolved (Oracle Q1-Q4)

- Q2: STOP stop_path 跳转目标为**已定义后续节点** (DAG 连续性保证, 非任意节点)
- Q3: resume_yield 调用者为**异步回调** (通过 IInteractionBus 订阅 yield token, TUI/AI 消费)
- Q4: ToolCoordinator audit 集成 — yield.* 事件也走 tool.audit 通道 (tool.audit.yield.{started,completed,cancelled})

## Non-goals

- 不实现 StreamSink 抽象层 (Stage 3 远期)
- 不实现 push-based 推送通道 (IInteractionBus 保持 event broker 语义)
- 不修改 ADR-0030 V2
- 不实施 batching 子图 (延后到 Stage 2 远期)

## 关联 change

- **前置**: C9 `2026-07-03-2026-07-03-phase4-5-impl-scope-audit` (audit ✅, archived 2026-07-03) + C10 ✅ (module_state 持久化基础) + C11 ✅ (SessionVars)
- **后续**: Stage 2 远期 (Static analysis + Graph IR, C14)

## 验证标准

- [ ] ctest 61/61 + 新增 test_yield_node 8-10 case 全绿
- [ ] 零 ADR 修改 (ADR-0030 V2 状态保持)
- [ ] NEXT/CONTINUE/STOP 3 模式单元测试全覆盖
- [ ] IGenerationStream pull-based 集成测试通过
- [ ] CONTINUE 模式 budget 触发的中断测试
- [ ] TopoScheduler yield pause/resume 端到端测试
- [ ] ASan/TSan 零 leak/race (YIELD 跨 await 边界)
