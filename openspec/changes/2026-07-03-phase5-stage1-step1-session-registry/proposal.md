# Proposal: Phase 5 Stage 1 Step 1 — Session Registry + Session Vars (C11)

> **STATUS: PLACEHOLDER** ⚠️
> **关联 Oracle 决议**: Q2 — Option A (SessionRegistry 是 Session 之外的注册表)
> **关联 master plan**: `docs/superpowers/plans/2026-07-03-phase5-self-bootstrapping.md` §十六.2
> **关联 IP-001**: `docs/proposals/implementation-roadmap/01-roadmap.md` §Step 1
> **关联 ADR**: ADR-0033 (Session Hierarchy) — **不修改**, SessionRegistry 与 UserSession 并列
> **前置依赖**: C10 ✅ archived (ModuleState 基础设施)
> **最后更新**: 2026-07-03

## Why

Phase 5 自举需要支持多用户/多会话隔离。当前 ADR-0033 (C5 ship) 已定义 UserSession/TaskSession/SubtaskSession 三层,
但缺少**多 UserSession 的注册表**。DSLEngine::run() 当前调用方需要自行管理 UserSession 生命周期。

依据 IP-001 §Step 1 设计 + Oracle 决议 Q2:
- SessionRegistry 是 DSLEngine 持有, 与 tool_registry_ 并列
- 与 ToolRegistry 模式对称 (string→Tool mapping vs string→UserSession mapping)
- 不重复 ADR-0033 的 UserSession::task_sessions_ 职责 (后者管单 user 多 task, 前者管多 user)
- SessionVars 放在 ExecutionSession (per-run) 而非 UserSession (per-conversation)

## What Changes

### 1. 新建 SessionRegistry

- `src/modules/scheduler/session_registry.h/cpp` 新建
  ```cpp
  class SessionRegistry {
  public:
      std::string create_session(const SessionConfig& config);
      void destroy_session(const std::string& id);
      UserSession& get_session(const std::string& id);
      std::vector<std::string> list_sessions() const;
  private:
      std::unordered_map<std::string, std::unique_ptr<UserSession>> sessions_;
      std::mutex mutex_;
  };
  ```
- 线程安全 (与 ToolRegistry 一致)

### 2. DSLEngine 集成

- `src/core/engine.h`: 加 `session_registry_` PIMPL-lite 成员 (与 tool_registry_ 模式一致)
- `src/core/engine.cpp`: 构造时初始化 SessionRegistry
- 公开 `get_session_registry()` 访问器

### 3. ExecutionSession 扩展

- `src/modules/scheduler/execution_session.h`:
  - 加 `session_id_: std::string` 字段
  - 加 `session_vars_: nlohmann::json` 字段 (per-run vars)
- 与 C10 的 module_states_ 正交 (不同语义: session_vars = run-time config, module_states = module persistence)

### 4. 注册 4 个 Session 工具

- `src/common/tools/registry.cpp`: 注册
  - `session.create` — 创建 UserSession
  - `session.destroy` — 销毁 UserSession (含清理所有 TaskSession + module_states)
  - `session.set_var` — 设置 session var (per-run)
  - `session.get_var` — 读取 session var

### 5. 析构安全

- DSLEngine 析构时, SessionRegistry 析构 → 所有 UserSession 析构 → 所有 TaskSession 析构 → 所有 ExecutionSession.module_states_ 释放
- **ship gate 必加**断言测试: 销毁 Session 后 module_states 完全释放 (Oracle Q2 风险 mitigation)

## What Does NOT Change

- **ADR-0033 Session Hierarchy** — 完全不动
- **UserSession/TaskSession/SubtaskSession 三层** — 不动
- **ExecutionSession.module_states_** (C10 ship) — 不动
- **Node/NodeType 签名** — 不动

## Capabilities

### ADDED Requirements

- `session-registry-creation`: SessionRegistry MUST 持有 unordered_map<string, unique_ptr<UserSession>> + mutex
- `session-registry-thread-safe`: create/destroy/get MUST 线程安全
- `session-vars-isolation`: ExecutionSession.session_vars_ MUST per-run 隔离 (不跨 run 共享)
- `session-lifecycle-cleanup`: destroy_session MUST 清理所有 TaskSession + module_states (无泄漏)
- `session-tools-exposed`: MUST 注册 session.create/destroy/set_var/get_var 4 个工具

## Impact

**修改文件** (估):
- `src/modules/scheduler/session_registry.h` (新, +50 行)
- `src/modules/scheduler/session_registry.cpp` (新, +120 行)
- `src/core/engine.h` (+5 行)
- `src/core/engine.cpp` (+10 行)
- `src/modules/scheduler/execution_session.h` (+2 字段)
- `src/common/tools/registry.cpp` (+30 行 4 工具)
- `tests/test_session_registry.cpp` (新, 6-8 test case)

**API 兼容性**: 零 breaking change (PIMPL-lite, 新增成员不破坏公开 API)

**估时**: 2-3 天 (Oracle 决议后保持不变)

## Non-goals

- 不实现 Session persistence to disk (留 Sprint 25+)
- 不实现 Session migration (远期)
- 不实现 cross-Session 通信 (远期, 不在 Phase 5 范围)
- 不修改 ADR-0033

## 关联 change

- **前置**: C9 `2026-07-03-2026-07-03-phase4-5-impl-scope-audit` (audit ✅, archived 2026-07-03) + C10 ✅ (ModuleState 基础设施)
- **后续**: C12 (YIELD 可选地引用 SessionID, 但不强依赖)

## 验证标准

- [ ] ctest 61/61 + 新增 test_session_registry 6-8 case 全绿
- [ ] 零 ADR 修改 (ADR-0033 状态保持 ✅ Approved)
- [ ] ASan 验证 Session 销毁后 0 leak
- [ ] 4 工具通过 DSL tool_call 可调用
- [ ] SessionRegistry 多线程并发 create/destroy 无 data race (TSan 验证)
