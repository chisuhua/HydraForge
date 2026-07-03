# Proposal: Phase 5 Stage 1 Step 0 — Lazy ModuleState (C10)

> **STATUS: PLACEHOLDER** ⚠️
> **关联 Oracle 决议**: Q1 — Option A (ModuleState 独立于 LayeredContext)
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
- ModuleState 是 ExecutionSession 的 `std::map<string, json>` 独立成员
- **不**塞入 LayeredContext (会污染 L1-L5 语义纯性, ADR-0008 已 ship)
- Lazy init: 首次 dsl_call 访问时自动创建空 json 对象
- 无 schema / 无 imports / 无 fork_behavior (Session 3 Oracle 决议)

## What Changes

### 1. ExecutionSession 扩展

- `src/modules/scheduler/execution_session.h`: 新增 PIMPL-lite 内部成员
  ```cpp
  class ExecutionSession {
  private:
      std::map<std::string, nlohmann::json> module_states_;  // 新增
  public:
      nlohmann::json& ensure_module_state(const std::string& module_path);
      const nlohmann::json* get_module_state(const std::string& module_path) const;
  };
  ```
- `src/modules/scheduler/execution_session.cpp`: 实现 `ensure_module_state()` lazy init
- 析构时自动清理所有 module_states (PIMPL 模式自然)

### 2. NodeExecutor 集成

- `src/modules/executor/node_executor.cpp`: dsl_call 执行时
  - 传入 `ExecutionSession& session` 引用
  - 目标子图 sub-execution 时, 共享 session.module_states_ (按 module_path 隔离)
  - **不** 改 Node/NodeType 签名

### 3. 序列化支持 (可选, 视估时)

- 复用现有 `flatten_layers` / `from_context` 机制
- `module_states_` 加入 ExecutionSession snapshot/dump (若 IBudgetController 调用 save_state)

## What Does NOT Change

- **ADR-0008 LayeredContext** — 完全不动
- **Node/NodeType 签名** — 不加新字段
- **DSL 语法** — 无新节点类型, 行为完全透明

## Capabilities

### ADDED Requirements

- `lazy-modulestate-injection`: ExecutionSession MUST 持有 `module_states_` map<string, json> 成员
- `lazy-modulestate-init`: 首次访问 module_path 时 MUST 自动创建空 json 对象
- `lazy-modulestate-isolation`: 不同 module_path 的 state MUST 完全隔离
- `lazy-modulestate-cleanup`: ExecutionSession 析构时 MUST 释放所有 module_states

## Impact

**修改文件** (估):
- `src/modules/scheduler/execution_session.h` (+10 行)
- `src/modules/scheduler/execution_session.cpp` (+20 行)
- `src/modules/executor/node_executor.cpp` (+5 行 dsl_call 集成)
- `tests/test_module_state.cpp` (新, 5-8 test case)

**API 兼容性**: 零 breaking change (PIMPL-lite 模式, 公开 API 不变)

**估时**: 1-1.5 天 (Oracle 决议后从 1-2 天略降)

## Non-goals

- 不实现 LayeredContext ↔ ModuleState 桥接 (留 C11/C12 便利工具)
- 不实施模块 schema 校验 (Session 3 Oracle 决议: 无 schema)
- 不实施 fork_behavior 配置 (远期, Phase 5 Stage 2 C13)
- 不修改 ADR-0008 LayeredContext 5-层设计

## 关联 change

- **前置**: C9 `2026-07-03-phase4-5-impl-scope-audit` (audit, 0 DRIFT ✅)
- **后续**: C11 (依赖 C10 提供的 module_states_ 基础设施)
- **后续**: C12 (依赖 C10, YIELD 需要 module_state 持久化)

## 验证标准

- [ ] ctest 61/61 + 新增 test_module_state 5-8 case 全绿
- [ ] 零 ADR 修改 (ADR-0008 状态保持 ✅ Approved)
- [ ] ExecutionSession PIMPL-lite 公开 API 零变化
- [ ] module_state 在 dsl_call 间可累积 (counter example)
- [ ] 析构时无内存泄漏 (ASan 验证)
