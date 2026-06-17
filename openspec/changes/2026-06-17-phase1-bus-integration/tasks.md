# Tasks: Phase 1 Sprint 1b — DSLEngine Bus 集成

> **关联**: [proposal.md](proposal.md) | [design.md](design.md) | [specs/](specs/)

## 任务依赖图

```
S1b.T1 (engine.h include 缩减 + bus 成员)
   ↓
S1b.T2 (engine.cpp set/get/subscribe 实现)
   ↓
S1b.T3 (NodeExecutor 集成 + 5 事件推送)
   ↓
S1b.T4 (5+ 端到端集成测试)
   ↓
S1b.T5 (文档同步 + 验证 + 提交)
```

## Tasks

- [x] **S1b.T1** — DSLEngine 头文件改造 (P2.1) ✅ 完成

   **文件**: `src/core/engine.h`

   **工作**:
   - ~~移除 `#include "common/tools/registry.h"` (P1.T4 遗留)~~ **保留**: 内联模板方法需要完整类型
   - ~~添加 `class ToolRegistry;` 前向声明~~ **保留完整类型**
   - 添加 `#include <memory>` (用于 `std::shared_ptr<IInteractionBus>`) ✅
   - 添加 `#include "agenticdsl/contract/iinteraction_bus.h"` ✅
   - 添加 3 个公开方法签名 ✅:
     - `void set_interaction_bus(std::shared_ptr<IInteractionBus> bus);`
     - `std::shared_ptr<IInteractionBus> get_interaction_bus() const;`
     - `size_t subscribe(const std::string& topic, std::function<void(const ToolResult&)> cb);`
   - 添加私有成员 `std::shared_ptr<IInteractionBus> bus_;` (默认 nullptr) ✅

   **粒度**: 0.5h

   **Acceptance**:
   - [ ] ~~`engine.h` 不直接 include `common/tools/registry.h`~~ **偏离合规**: 保留以避免内联模板需要完整类型的问题
   - [x] 3 个新方法签名存在 ✅
   - [x] 编译通过 (LSP 0 错误) ✅

- [ ] **S1b.T2** — DSLEngine bus 方法实现 (P2.2)

  **文件**: `src/core/engine.cpp`

  **工作**:
  - 实现 `set_interaction_bus` (move semantics)
  - 实现 `get_interaction_bus` (const accessor)
  - 实现 `subscribe` (透传 `bus_->subscribe`, nullptr 时返回 0)

  **粒度**: 1h

  **Acceptance**:
  - [ ] set/get 方法行为正确 (单元测试 1 验证)
  - [ ] subscribe nullptr 路径返回 0
  - [ ] subscribe 透传返回正确 token

- [ ] **S1b.T3** — NodeExecutor bus 集成 (P2.3-P2.4)

  **文件**: `src/modules/executor/node_executor.h` + `node_executor.cpp`

  **工作**:
  - 两个构造函数新增 `IInteractionBus* bus = nullptr` 参数
  - 添加私有成员 `IInteractionBus* bus_;`
  - `execute_dsl_node` 入口推送 `dsl.call.started`, 出口推送 `dsl.call.completed`
  - `execute_tool_call` 完成后推送 `tool.completed` (Sprint 1a 的 ToolResult envelope)
  - `execute_tool_call` 错误路径 (Retry/Abort) 推送 `execution.failed` 后 throw
  - Skip 错误码不推送事件, 不 throw (Sprint 1a 行为保留)

  **粒度**: 2.5h

  **Acceptance**:
  - [ ] 构造函数新增参数, 现有 7+ 调用点零修改 (默认 nullptr)
  - [ ] DSLNode execute 期间推送 2 事件 (started + completed)
  - [ ] ToolNode execute 完成后推送 tool.completed (含 P2-P4 字段)
  - [ ] Abort → execution.failed + throw
  - [ ] Retry → execution.failed + throw
  - [ ] Skip → 不推送 + 不 throw

- [ ] **S1b.T4** — 端到端集成测试 (5+ tests)

  **文件**: `tests/test_engine_bus_integration.cpp` (新建) + `tests/CMakeLists.txt` (自动 glob)

  **工作**:
  - 测试 1: DSLEngine 注入 custom bus + subscribe → emit 验证
  - 测试 2: DSLNode execute 期间 bus 收到 started + completed
  - 测试 3: ToolNode execute 完成后 bus 收到 envelope (error_code=Retry, 含 4 P2-P4 字段)
  - 测试 4: Abort 错误码触发 execution.failed + 抛异常
  - 测试 5: 默认 nullptr 路径 (零回归) — 现有 27 tests 仍 pass
  - 测试 6 (Bonus): 1000x 并发 subscribe + emit 无死锁

  **粒度**: 2h

  **Acceptance**:
  - [ ] 5+ 新测试全部通过
  - [ ] 全量 32+ 测试通过 (27 Sprint 1a + 5+ Sprint 1b)
  - [ ] ASan 干净
  - [ ] LSP 0 错误

- [ ] **S1b.T5** — 文档同步 + OpenSpec validate + 提交

  **文件**:
  - `docs/roadmap-status.md` (Sprint 1b 状态)
  - `docs/phase1-roadmap.md` (S1b.T1-S1b.T4 勾选)
  - `docs/adr/adr-0019-iinteraction-bus-mvp.md` (P2 状态更新)

  **工作**:
  - 更新 `roadmap-status.md` Sprint 1b 行: 0% → 100% ✅
  - 更新 `phase1-roadmap.md` S1b.T1-S1b.T4 状态: `[ ]` → `[x]`
  - 更新 ADR-0019 P2 状态: 🟡 Partial → ✅ Approved
  - 运行 `openspec validate --strict`
  - Single commit `feat(bus): integrate IInteractionBus with DSLEngine + NodeExecutor`

  **粒度**: 0.5h

  **Acceptance**:
  - [ ] `openspec validate 2026-06-17-phase1-bus-integration --strict` 通过
  - [ ] ADR-0019 P2 状态变更为 ✅ Approved
  - [ ] 提交信息符合 conventional commits
  - [ ] Sprint 1b 状态在 roadmap-status + phase1-roadmap 同步

## 总工作量

~6.5 小时 (1 天单人, 留 0.5 天 buffer)

## 验证清单

- [ ] 27 Phase 1a + 5+ Sprint 1b = 32+ 测试通过
- [ ] ASan 干净
- [ ] LSP 0 错误
- [ ] TSan 干净 (待 CI 验证)
- [ ] `openspec validate --strict` 0 error
- [ ] CI 6 job 全绿
- [ ] 提交信息符合 conventional commits

## 提交策略

**Single commit**: `feat(bus): integrate IInteractionBus with DSLEngine + NodeExecutor`
**包含**: S1b.T1-T5 全部代码 + 测试 + 文档

## 风险

- engine.h include 缩减可能引入循环依赖 → PIMPL-lite 技术 + 完整类型限定在 .cpp
- NodeExecutor 构造函数签名变化可能破坏 11 测试 → 默认参数 nullptr 保持兼容
- 事件推送开销 → InMemoryBus 已有 mutex + 锁外 callback 协议, 性能已在 Sprint 1a 28/28 tests 验证
- Subscribe token 跨实例失效 → 由 InMemoryBus 统一管理, DSLEngine 不缓存
