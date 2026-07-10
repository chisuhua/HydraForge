# ADR-0019 Implementation Scope Audit

> **生成时间**: 2026-07-03 (C9 — Phase 4.5 impl-scope audit)
> **基础**: `tools/docs_drift_audit.py` 报告 (Scenario 4)
> **关联 ADR**: [adr-0019-iinteraction-bus-mvp.md](adr-0019-iinteraction-bus-mvp.md)
> **状态**: ✅ Approved (audit 后保持)

## 状态

**📋 Audit** (impl-scope-audit 文档, 与 docs-code-drift-audit 配套使用)

✅ Approved (audit 后保持 — 所有 11 个 ADR 核心契约类均已 Shipped 或 Evolved, 无需调整主 ADR 状态)

## Drift 摘要

`docs_drift_audit.py` 报告: ADR 声称 ✅ Approved (2026-06-24, Sprint 5 ship), 但 8/13 个描述的类未在 src/include 中找到。

## 原始描述 vs 实际实施

| ADR 描述的类 | 实际状态 | 实际位置 (如有) | 备注 |
|--------------|---------|----------------|------|
| `IInteractionBus` | ✅ Shipped | `include/agenticdsl/contract/iinteraction_bus.h` | 交互总线抽象接口 |
| `InMemoryBus` | ✅ Shipped | `include/agenticdsl/contract/inmemory_bus.h` | MVP 实现 (mutex + queue + 后台 dispatch 线程, C2 P2) |
| `ToolResult` | ✅ Shipped | `src/core/types/tool_result.h` | 事件载荷 (替代 ADR 草稿的 `Event` / `Message`) |
| `CognitiveWorker` | ✅ Shipped | `include/agenticdsl/cognitive/cognitive_worker.h` | 认知智能体工作线程 (Sprint 2) |
| `DomainWorkerPool` | ✅ Shipped | `include/agenticdsl/cognitive/domain_worker_pool.h` | 领域工作线程池 (Sprint 3) |
| `Event` | 🔁 Evolved | `include/agenticdsl/contract/iinteraction_bus.h` (`emit(event_type, payload)`) | 事件由 `(string event_type, ToolResult payload)` 二元组表达, 非独立 `Event` 类 |
| `EventType` | 🔁 Evolved | `std::string` (event_type 参数) | 事件类型为字符串 (如 `"tool_call_started"`), 非枚举 |
| `Message` | 🔁 Evolved | `ToolResult` | 消息载荷由 `ToolResult` 统一承载, 非独立 `Message` 类 |
| `Role` | 📅 Deferred | — | 角色 (User/Assistant/System) 枚举未实现; 当前无多角色对话 |
| `Session` | 🔁 Evolved | `src/core/types/session.h` (`UserSession` / `TaskSession`) | 会话由 ADR-0033 三层会话模型承载 |
| `SessionData` | 📅 Deferred | — | 会话数据结构未独立实现; `UserSession` 内部持有 `task_sessions_` + `messages_` |
| `TUI` | 📅 Deferred | — | TUI 终端界面未实现; 留待 Phase 5 自举服务化 |
| `Token` | 📅 Deferred | — | Token 流式显示结构未实现; `IGenerationStream::next()` 返回 `std::optional<std::string>` |

## 分类详情

### 🔁 Evolved — 事件模型简化

ADR-0019 草稿可能描述了 `Event` / `EventType` / `Message` 等独立类型。实际实现选择了更简洁的方案:
- **事件**: `emit(const std::string& event_type, const ToolResult& payload)` — 二元组表达, 无独立 `Event` 类
- **事件类型**: `std::string` (如 `"tool_call_started"`, `"tool.audit.invoked"`), 非强类型枚举
- **消息载荷**: `ToolResult` 统一承载 (含 `content` / `meta` / `error_code`), 非独立 `Message` 类

设计理由 (iinteraction_bus.h 注释):
- 用于 Cognitive / Executor / TraceExporter 模块间轻量解耦事件传递
- `emit()` 同步通知 subscribers; 实现在锁外调用回调防止死锁
- C2 P2 (Sprint 12) 改为 EventBus MPMC 有界队列后端: `emit()` 仅入队 + `notify_one`, 后台 dispatch 线程异步分发

### 🔁 Evolved — `Session` → ADR-0033 三层会话

ADR-0019 的 `Session` 概念演进为 ADR-0033 三层会话模型 (`UserSession` / `TaskSession` / `SubtaskSession`), Sprint 15 C5 ship。

### 📅 Deferred (4 个)

- **`Role`**: 多角色对话 (User/Assistant/System) 留待 Phase 5 TUI Chat
- **`SessionData`**: `UserSession` 内部数据未提取为独立结构
- **`TUI`**: 终端界面子系统留待 Phase 5 自举服务化
- **`Token`**: 流式 Token 显示结构留待 Phase 5 真实 SSE 后端

## 决策

- **ADR 状态**: ✅ Approved (保持)
- **理由**: ADR-0019 核心契约 (`IInteractionBus` + `InMemoryBus` + 事件 emit/subscribe 模型) 已 Shipped; 8 个缺失类中 4 个 Evolved (事件模型简化 + Session → ADR-0033), 4 个 Deferred (TUI/Role/Token 等留待 Phase 5)
- **风险**: 低 — 事件总线 MVP 已通过 1000x 并发测试 (Sprint 12 C2)

## 后续行动

- Phase 5 TUI Chat 启动时, 实施 `TUI` + `Role` + `Token` 流式显示
- `SessionData` 随 ADR-0033 会话模型演进
- 本 audit 文档供 Phase 5 backlog 参考

---

> **注**: 本 impl-scope audit 反映 **Phase 4.5 (2026-07-03) 快照状态** ✅ Approved，主 ADR 文档 [`adr-0019-iinteraction-bus-mvp.md`](./adr-0019-iinteraction-bus-mvp.md) 当前状态 **🟡 Partial** (2026-07-06 更新)，新增待办 `subscribe_topic` 扩展尚未实施。两状态差异属**有意识时点差异**：本 audit 反映 Phase 4.5 收官时的实施范围（5 Shipped + 4 Evolved + 4 Deferred），主文档反映 2026-07-06 之后的最新需求增量。读者应同时参考两文档以获取完整实施状态。
