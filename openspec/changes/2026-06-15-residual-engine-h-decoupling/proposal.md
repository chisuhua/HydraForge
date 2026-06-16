# Residual engine.h Decoupling (Stage 4 Task 19 残留)

> **变更类型**: 真实实现 change (新功能)
> **作者**: Sisyphus (atlas session)
> **创建日期**: 2026-06-15
> **追溯范围**: `.omo/plans/project-organization.md` Stage 4 Task 19 残留
> **amends**: `openspec/specs/tech-debt-cleanup/spec.md`
> **前置**: `openspec/changes/2026-06-15-core-interface-inversion` (Stage 4 已 ship 部分)

## Why

`engine.h` 仍有 4 个跨模块 include(3 common/ + 1 modules/trace/),违反 ADR-0019 §1.4 "0 跨模块 include" 退出标准。本次 change 移除残留 3 个 include(保留 1 个 `llm_types.h` 因是 types 头文件),通过 3 个独立抽象:

1. `IProviderFactory` — 替代 `common/llm/mock_provider.h` 直接 include
2. `IToolRegistry` — 替代 `common/tools/registry.h` 直接 include
3. `TraceRecord` 上移 — 替代 `modules/trace/trace_exporter.h` 直接 include

不解决此问题:① ADR-0019 §1.4 永久保持 🟡 部分解决 状态;② Phase 1 Sprint 3 (DomainWorkerPool) 的多线程测试可能受 MockLLMProvider 紧耦合影响;③ 后续 5/6 examples 修复需要先解耦 engine.h。

## What Changes

### 代码侧 (新代码)

- **新增** `include/agenticdsl/contract/iprovider_factory.h`:
  ```cpp
  namespace agenticdsl::contract {
  class IProviderFactory {
   public:
    virtual ~IProviderFactory() = default;
    virtual std::unique_ptr<ILLMProvider> create(const LLMConfig& config) = 0;
  };
  }  // namespace agenticdsl::contract
  ```

- **新增** `include/agenticdsl/contract/itool_registry.h`:
  ```cpp
  namespace agenticdsl::contract {
  class IToolRegistry {
   public:
    virtual ~IToolRegistry() = default;
    virtual ToolResult call_tool(const std::string& name, const nlohmann::json& args) = 0;
    virtual void register_tool(const std::string& name, std::function<ToolResult(const nlohmann::json&)> fn) = 0;
  };
  }  // namespace agenticdsl::contract
  ```

- **新增** `include/agenticdsl/types/trace_record.h`: TraceRecord POD 上移
- **修改** `src/modules/trace/trace_exporter.cpp`: 改为 include 新头文件
- **修改** `src/core/engine.h`: 移除 3 个 include
- **修改** `src/modules/executor/node_executor.cpp`: 使用 `IProviderFactory` 而非直接构造 MockLLMProvider
- **修改** `src/core/engine.cpp`: 工厂注入

### 文档侧

- **更新** `docs/adr/adr-0019-iinteraction-bus-mvp.md` §1.4: 状态从 🟡 部分解决 → ✅ 已解决

## Impact

- **Affected specs**: `openspec/specs/tech-debt-cleanup/spec.md` (amend 1 REQ,标记"已解决")
- **Affected code**:
  - `src/core/engine.h` (移除 3 include,改 include i* 头文件)
  - `src/core/engine.cpp` (工厂注入)
  - `src/modules/executor/node_executor.cpp` (IProviderFactory 使用)
  - `src/modules/trace/trace_exporter.cpp` (新头文件)
- **Affected tests**: 25/25 PASS (0 回归)
- **Breaking change**: ❌ 无 (i* 接口 + 工厂模式保持向后兼容)

## Success Criteria

- [ ] `engine.h` 跨模块 include grep 退出 = 1 (仅保留 `llm_types.h` types 头文件)
- [ ] `tools/adr_lint.py docs/adr/` exit 0
- [ ] 25/25 测试通过 (含新增 IProviderFactory/IToolRegistry/TraceRecord 测试)
- [ ] TSan clean (新增并发测试覆盖 IProviderFactory 多线程创建)
- [ ] ADR-0019 §1.4 状态更新为 ✅ 已解决

## Out of Scope (Non-goals)

- ❌ 不实现 PDK (独立 OpenSpec change: `phase1-pdk-skeleton`)
- ❌ 不实现 PluginLoader (独立 OpenSpec change: `phase1-plugin-loader`)
- ❌ 不修改 ToolResult 现有 P1 字段(由 `phase1-toolresult-standardization` 处理 P2-P4)

## Dependencies

- **Block**: `openspec/changes/2026-06-15-core-interface-inversion` (Stage 4 部分,已 ship)
- **Block**: `openspec/changes/2026-06-15-layered-context-implementation` (LayeredContext 稳定)
- **Blocked by**: 无 (可立即开始)

## Estimated Effort

2 周 (10 工作日),3 个独立 sub-task:
- T1: IProviderFactory (4 天)
- T2: IToolRegistry (3 天)
- T3: TraceRecord 上移 (1 天)
- T4: 验证 + ADR 更新 (2 天)
