# Batch 2 ship gate — 7 defects resolved, 7 changes archived

**生成日期**: 2026-08-21
**最后验证**: 2026-08-21（ctest 180/180 PASS, 0 failures）
**作者**: Batch 2 ship automation
**状态**: ✅ Batch 2 ship gate 完成

---

## 一、Batch 2 ship 概览

| # | 变更 | Commit | 提案 | 缺陷表覆盖 |
|---|------|--------|------|-----------|
| 1 | P6 ADR-0079 v1.2 amend | `c4a3659` | `adr-0079-v1-2-amend` | 缺陷 1.2/1.3/1.5 |
| 2 | P5 SessionWriter 基础设施 | `9f07347` | `session-writer-bridge` | 缺陷 1.1 |
| 3 | P2 agent.* 生命周期 emit | `1f24ff3` | `emit-agent-lifecycle-events` | 缺陷 3.2 |
| 4 | P9 ExecutionResult 统一 | `dca4916` | `error-taxonomy-execution-boundary` | 盲点 7.1 |
| 5 | P8 Agent 编排 3 模式接口 | `87f55f8` | `adr-0060-p2-p3-patterns` | 缺陷 3.3 (partial) |
| 6 | P11 OTel exporter skeleton | `c47d568` | `otel-exporter-skeleton` | 盲点 7.3 |
| 7 | P7 ADR-0082 first-class AgentRegistry | `3119d16` | `adr-0082-promote-to-approved` | 缺陷 3.1 |
| 8 | P3 ADR-0081 pre-step hook | (本次 commit) | `adr-0081-promote-to-approved` | 缺陷 4.2 |

**Batch 1 (commit `0da23d0`)**: P12 mock-bus / P1 adr-0057 事件 / P10 compact-events / P4 event-log-query

---

## 二、缺陷表 v1.1 → v1.2 状态翻转

| 缺陷 | 旧状态 | 新状态 | 修复 commit |
|------|-------|-------|------------|
| 1.1 4 套会话存储 | P0（SessionWriter 0%） | ✅ P5 ship SessionWriter | `9f07347` |
| 1.2 message-index 寻址 | P1 | ✅ P6 v1.2 amend | `c4a3659` |
| 1.3 branch cursor 语义 | P1 | ✅ P6 v1.2 amend | `c4a3659` |
| 1.5 path-extraction fork | P1 | ✅ P6 v1.2 amend | `c4a3659` |
| 2.1 EventLog query | P0（query 缺失） | ✅ P4 ship (Batch 1) | `39bb25e` |
| 3.1 Agent 非 first-class | P1（搁置） | ✅ P7 ship skeleton (IAgentRegistry) | `3119d16` |
| 3.2 Agent 生命周期事件契约 | P0（零事件） | ✅ P2 ship emit | `1f24ff3` |
| 3.3 Agent↔Agent 协议 | P2（2/6 模式） | 🟡 P8 partial (call/call_async/delegate 接口) | `87f55f8` |
| 4.2 缺 pre-step hook | P0（待 ADR-0082） | ✅ P3 ship skeleton (IAgentHookRegistry) | 本次 commit |
| 7.1 错误传播断层 | P1（盲点） | ✅ P9 ship ExecutionResult + is_retryable | `dca4916` |
| 7.3 OTel exporter 零代码 | P2（盲点） | ✅ P11 ship skeleton | `c47d568` |

**未翻转（pending 实施期推迟）**：
- 缺陷 3.1 完整 AgentWorker + spawn_agent + YAML 配置（Sprint 24+ Agent hook 实施 change）
- 缺陷 4.2 Agent loop (ReactLoop/PlanExecuteLoop/ForkJoinLoop) 集成 hook（Sprint 24+）

---

## 三、P11 Amendment 修订（2026-08-21）

实施 P11 时发现 3 处需要调整，已写入 `openspec/changes/otel-exporter-skeleton/proposal.md` §Amendment：

1. **移除后台 flush_loop 线程**
   - 实测挂起（InMemoryBus dispatch_loop 与 flush_loop 多线程交互）
   - 与 OTel SDK `BatchSpanProcessor` 工业惯例一致
2. **测试改用 MockBus 同步路径**（`tests/test_helpers/mock_bus.h`）
3. **修复 dangling reference bug**（`sink_ptr->spans()[0]` 临时对象）

未改变：`ISpanSink` 抽象 / OtelConfig 字段 / 4 类事件订阅 / 3 属性 / opt-in + fail-closed

## 三.5、P7+P3 Amendment 修订（2026-08-21）

实施 P7（IAgentRegistry）和 P3（IAgentHookRegistry）时简化范围：

1. **V1 简化 unregister**：同步删除，pending 语义留 Sprint 24+
2. **IAgent 最小集**：仅 `name()` + `id()`，完整生命周期接口留 Agent hook 实施 change
3. **AgentPreHookResult::ModifyContext 简化为字符串替换**：复杂 step_input 类型推迟
4. **不发射 IInteractionBus 事件**：V1 骨架，事件集成留给 Sprint 24+
5. **Agent loop 集成推迟**：ReactLoop/PlanExecuteLoop/ForkJoinLoop 调用 hook 留 Sprint 24+

---

## 四、验证命令

```bash
# ctest 全量
cd build && timeout 300 ctest --output-on-failure -j$(nproc)
# 预期：100% tests passed, 182/182, 0 failed (~10 sec)
# 含：P11 test_otel_exporter (7 cases / 25 assertions)
#      P7 test_agent_registry (5 cases / 29 assertions)
#      P3 test_agent_hook_registry_contract (4 cases / 18 assertions)

# 单测试
ctest -R "test_otel_exporter|test_agent_registry|test_agent_hook_registry_contract" --output-on-failure
# 预期：3/3 PASS
```

---

## 五、Batch 3 启动条件

P7（AgentRegistry）+ P3（Agent Hook）已 ship 骨架。完整实施需 Sprint 24+ 独立 change：

- 缺陷 3.1 完整 AgentWorker + spawn_agent DSL 节点 + YAML 配置
- 缺陷 4.2 Agent loop (ReactLoop/PlanExecuteLoop/ForkJoinLoop) 集成 hook 调用

预计估时：每 change 1-2 sprint。

---

**维护**：每 Sprint 收官同步（`scripts/sprint-closeout.sh` Step 8 加本表交叉检查）。
**关联**：master plan / defect-fix-roadmap-2026-08.md / defect-truth-table-2026-08.md §七.3