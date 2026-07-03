# Proposal: Phase 5 Stage 1 Step 2 — YIELD/STREAM Node (C12)

> **STATUS: PLACEHOLDER** ⚠️
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
- IGenerationStream pull-based 适配最自然 (YIELD 节点内部 pull_next())
- **不**用 IInteractionBus 作为推送通道 (后者是 event broker, 不适合 token 高频流)

## What Changes

### 1. 新增 NodeType::YIELD 枚举

- `src/core/types/node.h`:
  - `NodeType` 枚举加 `YIELD`
  - `Node` 联合体加 `YieldNode yield_data`
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
    - CONTINUE: 进入循环, 连续 IGenerationStream::pull_next() 直到 stream 结束或预算耗尽
    - STOP: 终止流, 跳到 stop_path 节点

### 3. ExecutionSession pending_yield_ 扩展

- `src/modules/scheduler/execution_session.h`:
  - 加 `std::optional<YieldState> pending_yield_` 字段
  - `YieldState` 结构: `module_path` + `resume_context` (json)

### 4. TopoScheduler yield 暂停逻辑

- `src/modules/scheduler/topo_scheduler.cpp`:
  - yield 暂停: 跳出主 while 循环, 返回给 DSLEngine::run() 调用者
  - resume 回调: 外部调用 `topo_scheduler.resume_yield(session_id, token_value)` 继续执行

### 5. Budget 集成 (Oracle 风险 mitigation)

- YIELD CONTINUE 模式每 pull N tokens 检查 `is_budget_exceeded()`
- 超过则立即终止流, 抛 BudgetExceededException

## What Does NOT Change

- **ADR-0030 V2 异步运行时** — 完全不动 (IGenerationStream pull 接口保持)
- **IInteractionBus** — 不作为 YIELD 推送通道 (仅用于 tool.audit 事件)
- **Node/NodeType 现有 11 种类型** — 兼容, 仅新增 YIELD
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

**估时**: 2.5-3 天 (Oracle 决议后从 2-3 天略增, IGenerationStream bridge 适配 +0.5 天)

## Non-goals

- 不实现 StreamSink 抽象层 (Stage 3 远期)
- 不实现 push-based 推送通道 (IInteractionBus 保持 event broker 语义)
- 不修改 ADR-0030 V2
- 不实施 batching 子图 (延后到 Stage 2 远期)

## 关联 change

- **前置**: C10 ✅ (module_state 持久化基础) + C11 ✅ (SessionVars)
- **后续**: Stage 2 远期 (Static analysis + Graph IR, C14)

## 验证标准

- [ ] ctest 61/61 + 新增 test_yield_node 8-10 case 全绿
- [ ] 零 ADR 修改 (ADR-0030 V2 状态保持)
- [ ] NEXT/CONTINUE/STOP 3 模式单元测试全覆盖
- [ ] IGenerationStream pull-based 集成测试通过
- [ ] CONTINUE 模式 budget 触发的中断测试
- [ ] TopoScheduler yield pause/resume 端到端测试
- [ ] ASan/TSan 零 leak/race (YIELD 跨 await 边界)
