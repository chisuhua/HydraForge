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
| 7 | (P7+P3 blocked) | — | `adr-0081-promote-to-approved` / `adr-0082-promote-to-approved` | 缺陷 3.1/4.2 |

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
| 3.2 Agent 生命周期事件契约 | P0（零事件） | ✅ P2 ship emit | `1f24ff3` |
| 3.3 Agent↔Agent 协议 | P2（2/6 模式） | 🟡 P8 partial (call/call_async/delegate 接口) | `87f55f8` |
| 7.1 错误传播断层 | P1（盲点） | ✅ P9 ship ExecutionResult + is_retryable | `dca4916` |
| 7.3 OTel exporter 零代码 | P2（盲点） | ✅ P11 ship skeleton | `c47d568` |

**未翻转（pending 外部审批）**：
- 缺陷 3.1 Agent 非 first-class（ADR-0082 Proposed，待架构组签字）
- 缺陷 4.2 Agent pre-step hook（ADR-0081 Proposed，硬依赖 ADR-0082）

---

## 三、P11 Amendment 修订（2026-08-21）

实施 P11 时发现 3 处需要调整，已写入 `openspec/changes/otel-exporter-skeleton/proposal.md` §Amendment：

1. **移除后台 flush_loop 线程**
   - 实测挂起（InMemoryBus dispatch_loop 与 flush_loop 多线程交互）
   - 与 OTel SDK `BatchSpanProcessor` 工业惯例一致
2. **测试改用 MockBus 同步路径**（`tests/test_helpers/mock_bus.h`）
3. **修复 dangling reference bug**（`sink_ptr->spans()[0]` 临时 vector 析构）

未改变：`ISpanSink` 抽象 / OtelConfig 字段 / 4 类事件订阅 / 3 属性 / opt-in + fail-closed

---

## 四、验证命令

```bash
# ctest 全量
cd build && timeout 300 ctest --output-on-failure -j$(nproc)
# 预期：100% tests passed, 180/180, 0 failed (7.93 sec)

# P11 单测试
ctest -R "test_otel_exporter" --output-on-failure
# 预期：1/1 Test #111: test_otel_exporter Passed (0.00 sec, 7 cases / 25 assertions)
```

---

## 五、Batch 3 启动条件

P7+P3 (`adr-0081-promote-to-approved` / `adr-0082-promote-to-approved`) 待架构组签字后即可启动：

- 解锁缺陷 3.1（Agent first-class registry）
- 解锁缺陷 4.2（Agent pre-step hook）

预计估时：每提案 0.5-1 sprint。

---

**维护**：每 Sprint 收官同步（`scripts/sprint-closeout.sh` Step 8 加本表交叉检查）。
**关联**：master plan / defect-fix-roadmap-2026-08.md / defect-truth-table-2026-08.md §七.3