# Proposal: Phase 5 Stage 1 Step 0 — Lazy ModuleState (C10)

> **STATUS: ACTIVE** 🟡 (Oracle Q1 ✅ resolved — Option A, ModuleState 独立于 LayeredContext)
> **关联 Oracle 决议**: Q1 — ModuleState 作为 ExecutionSession 的 `map<string, json>` 独立成员
> **关联 master plan**: `docs/superpowers/plans/2026-07-03-phase5-self-bootstrapping.md` §十六.1
> **关联 IP-001**: `docs/proposals/implementation-roadmap/01-roadmap.md` §Step 0
> **关联 IP-002**: `docs/proposals/implementation-roadmap/02-code-mapping.md` §Step 0
> **关联 ADR**: ADR-0008 (LayeredContext) — **不修改**, 仅协同
> **前置依赖**: C9 ✅ archived (2026-07-03)
> **最后更新**: 2026-07-03

## Why

Phase 5 自举服务化需要让 Agent 通过 DSL 控制推理参数 (BOOT-001 阶段 1 目标)。
当前 ExecutionSession 没有 per-module 持久化状态, dsl_call 调用间无法累积模块级数据。

依据 IP-001 §Step 0 设计 + Oracle 决议 Q1:
- ModuleState 是 ExecutionSession 的 `std::map<std::string, nlohmann::json>` 独立成员
- **不**塞入 LayeredContext (会污染 L1-L5 语义纯性, ADR-0008 已 ship)
- Lazy init: 首次 dsl_call 访问时自动创建空 json 对象
- 无 schema / 无 imports / 无 fork_behavior (Session 3 Oracle 决议)

## What Changes

### 1. ExecutionSession 扩展

- `src/modules/scheduler/execution_session.h`: PIMPL-lite 内部新增:
  ```cpp
  struct ExecutionSession::Impl {
      // existing fields...
      std::map<std::string, nlohmann::json> module_states_;  // C10 new
  };
  ```
- 公开 API:
  - `nlohmann::json& ensure_module_state(const std::string& module_path)` — lazy init
  - `const nlohmann::json* get_module_state(const std::string& module_path) const`
  - `bool has_module_state(const std::string& module_path) const`
- `src/modules/scheduler/execution_session.cpp`: 实现 lazy init — 首次访问自动创建空 json 对象
- 析构时 PIMPL 模式天然释放 module_states_ (no extra code)

### 2. NodeExecutor 集成

- `src/modules/executor/node_executor.h/cpp`: dsl_call 执行时传入 `ExecutionSession&` 引用
  - 目标子图 sub-execution 时, 共享 session.module_states_ (按 module_path 隔离)
  - **不**改 Node/NodeType 签名

### 3. 序列化支持 (基础)

- `module_states_` 加入 ExecutionSession snapshot/dump (通过 IBudgetController 调用 save_state)

## What Does NOT Change

- **ADR-0008 LayeredContext** — 完全不动
- **Node/NodeType 签名** — 不加新字段
- **DSL 语法** — 无新节点类型, 行为完全透明

## Capabilities

### ADDED Requirements

- `lazy-modulestate-injection`: ExecutionSession MUST 在 PIMPL Impl struct 持有 `module_states_: std::map<std::string, nlohmann::json>` 成员
- `lazy-modulestate-init`: 首次调用 `ensure_module_state(path)` MUST 自动创建空 json 对象
- `lazy-modulestate-isolation`: 不同 module_path 的 state MUST 完全隔离
- `lazy-modulestate-cleanup`: ExecutionSession 析构时 PIMPL 自动释放 module_states_ (no leak)
- `lazy-modulestate-dsl-call`: NodeExecutor dsl_call 执行时 MUST 传入 ExecutionSession& 引用, 使子图可访问 module_states_

## Impact

**修改文件** (估):
- `src/modules/scheduler/execution_session.h` (+15 行)
- `src/modules/scheduler/execution_session.cpp` (+25 行)
- `src/modules/executor/node_executor.h` (+1 行: 方法签名微调)
- `src/modules/executor/node_executor.cpp` (+5 行: dsl_call 集成)
- `tests/test_module_state.cpp` (新, 5-7 test case)

**API 兼容性**: 零 breaking change (PIMPL-lite 模式, 公开 API 不变)

**估时**: 1-1.5 天

## Non-goals

- 不实现 LayeredContext ↔ ModuleState 桥接 (留 C11/C12 便利工具)
- 不实施模块 schema 校验 (Session 3 Oracle 决议: 无 schema)
- 不实施 fork_behavior 配置 (远期, Phase 5 Stage 2 C13)
- 不修改 ADR-0008 LayeredContext 5-层设计

## 关联 change

- **前置**: C9 `2026-07-03-phase4-5-impl-scope-audit` (audit, 0 DRIFT ✅, archived 2026-07-03)
- **后续**: C11 依赖 C10 (SessionVars 需 module_states_ 的命名空间约定)
- **后续**: C12 依赖 C10 (YIELD resume_context 持久化需要 module_states_ API)

## 验证标准

- [ ] ctest 61/61 + 新增 test_module_state 5-7 case 全绿
- [ ] 零 ADR 修改 (ADR-0008 状态保持 ✅ Approved)
- [ ] ExecutionSession PIMPL-lite 公开 API 零变化
- [ ] module_state 在 dsl_call 间可累积 (counter example)
- [ ] 析构时无内存泄漏 (ASan 验证)
- [ ] 不同 module_path 的 state 完全隔离 (isolation test)