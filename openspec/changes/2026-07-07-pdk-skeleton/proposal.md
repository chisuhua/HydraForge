# Proposal: PDK Skeleton (Sprint 4)

> **变更类型**: 真实实现 (新功能 + 仓库治理)
> **作者**: Sisyphus (Phase 1 Sprint 4 启动)
> **创建日期**: 2026-06-16 (placeholder) → 2026-06-19 (filled)
> **追溯范围**: `.omo/plans/phase1-execution.md` §Sprint 4
> **关联 ADR**: docs/adr/adr-0021-pdk-design.md (PDK 设计) + ADR-0022 (插件加载机制) + ADR-0019 (IInteractionBus) + ADR-0020 (CognitiveWorker + DomainWorkerPool) + ADR-0004 (ToolRegistry 安全)
> **前置**: Sprint 1a (ToolResult P2-P4, archived 2026-06-16) + Sprint 1b (IInteractionBus, archived 2026-06-17) + P1 解耦 (T1+T2+T3+T4+T5, 2026-06-18) + Sprint 2 CognitiveWorker (2026-06-18) + Sprint 3 DomainWorkerPool (2026-06-19)
> **amends**:
>   - ADR-0021 状态: 🔍 Proposed → 🟡 Partial (Sprint 4 ship 后)
>   - ADR-0022 状态: 保持 🔍 Proposed (Sprint 5 ship 后变更)

## Why

Phase 1 Sprint 2/3 已 ship CognitiveWorker + DomainWorkerPool (ADR-0020 P1+P2),31/31 ctest 零回归,但 **Plugin Development Kit (PDK)** 仍未落地。当前痛点:

- 工具注册: 手动 `engine->register_tool(name, callback)` 需要 ~20 行样板代码 (Schema/权限/日志/错误处理都需手写)
- Agent 循环: `agent_basic` 示例手写 ReAct 循环 (~1000 行),每个领域插件重复实现意图理解/规划/执行/观察
- 沙箱执行: 无封装,开发者需手写 fork/cgroups/seccomp/chroot (高门槛)
- 测试: 依赖完整 Runtime 集成测试,插件无法独立测试,反馈周期长
- 文档: `app-dev-guide-cpp.md` 744 行繁琐但缺少可执行脚手架

PDK 提供**标准化开发工具包**,使:
- 工具注册从 ~20 行降到 ~5 行领域逻辑 (DECLARE_TOOL 宏)
- Agent 循环从 ~1000 行降到 ~100 行回调填充 (DEFINE_AGENT 模板)
- 沙箱执行从 ~600 行降到 ~10 行声明式 (SafeExec 封装)
- 插件可独立测试,无需 Runtime (MockSandbox / FakeStateStore / StubLLM)

**关键约束** (ADR-0021 P1-P6):
- **P1**: PDK **不是** Runtime 的一部分 — 独立仓库 `hydraforge-pdk`
- **P2**: PDK 是**可选依赖** — 高级开发者可手写,但强烈推荐
- **P3**: PDK **静态链接**到插件 — Runtime 零感知、零负担
- **P4**: PDK 只封装**通用开发模式** — 不包含任何领域逻辑
- **P5**: PDK 版本与 Runtime **解耦** — 独立升级
- **P6**: PDK 提供**测试替身** — 插件独立测试

**Sprint 4 范围内**: monorepo `pdk/` 子目录先 ship (K3 决策,与 ADR-0021 一致),`hydraforge-pdk` 外部仓库推送为 T4b 异步执行 (外部阻塞: GitHub 组织存在性)。

**不解决此问题**:
- ❌ 完整 PDK (Phase 2/3/4, plan §ADR-0021 §5): DEFINE_AGENT 仅 MVP,完整 ReAct 循环 + ForkJoin + PluginLifecycle + MockSandbox + 完整 SafeExec (fork/seccomp)
- ❌ 真实 `.so` 加载 (Sprint 5 PluginLoader 范围)
- ❌ `examples/agent_chat_pdk/` (Sprint 5 收官 demo 范围)

## What Changes

### 决策 1: PDK 头文件架构 (3 个核心头 + 1 个统一入口)

```cpp
// include/agenticdsl/pdk/tool_macros.h (Sprint 4 T1)
// DECLARE_TOOL 宏 — 工具注册脚手架
namespace hydraforge::pdk {

// 工具元数据 (Schema + 权限)
struct ToolSpec {
  std::string name;
  std::string description;
  std::vector<ToolParam> params;     // 参数 Schema
  ToolPermissions permissions;        // 权限声明 (filesystem/network/etc)
};

// DECLARE_TOOL 宏: 展开为 ToolSpec + 注册函数 + 错误处理包装
// 用途: 开发者用 DECLARE_TOOL 声明工具, PDK 自动处理 Schema + 权限 + 日志
#define DECLARE_TOOL(name, description, ...) \
    /* 展开为: */ \
    inline ToolSpec tool_spec_##name = { \
        #name, description, /* params */ {}, /* perms */ {} \
    }; \
    inline nlohmann::json tool_handler_##name(const nlohmann::json& args) { \
        try { \
            __VA_ARGS__ \
        } catch (const std::exception& e) { \
            return nlohmann::json{{"error", e.what()}}; \
        } \
    }

} // namespace hydraforge::pdk
```

```cpp
// include/agenticdsl/pdk/agent_macros.h (Sprint 4 T2)
// DEFINE_AGENT 模板 — Agent 循环脚手架
namespace hydraforge::pdk {

// Agent 循环类型
enum class AgentLoopType {
  React,        // 思考 → 行动 → 观察 → 重复 (MVP)
  PlanExecute,  // 规划 → 执行 → 验证 (Phase 2)
};

// DEFINE_AGENT 宏: 展开为 Agent 类 + 循环逻辑 + 异常处理
// MVP: 仅 React 循环,PlanExecute/ForkJoin 留 TODO
#define DEFINE_AGENT(name, loop_type) \
    /* 展开为: */ \
    class name##Agent { \
    public: \
      name##Agent(/* injected deps: DSLEngine + IInteractionBus */) {} \
      ToolResult run(const std::string& prompt); \
    private: \
      /* loop_type 决定循环模板, MVP 仅 React */ \
    };

} // namespace hydraforge::pdk
```

```cpp
// include/agenticdsl/pdk/safe_exec.h (Sprint 4 T3)
// SafeExec 沙箱封装 — 超时 + 异常捕获 (MVP,无 fork/seccomp)
namespace hydraforge::pdk {

class SafeExec {
 public:
  SafeExec& with_timeout(std::chrono::milliseconds timeout);
  SafeExec& with_layer_profile(LayerProfile profile);

  template <typename F>
  auto run(F&& fn) -> std::invoke_result_t<F> {
    // MVP: std::async + wait_for(timeout) + exception propagation
    // Phase 2: + fork/cgroups/seccomp/chroot
  }
};

} // namespace hydraforge::pdk
```

```cpp
// include/agenticdsl/pdk/pdk.h (统一入口)
// 引用所有 PDK 子模块
#include <hydraforge/pdk/tool_macros.h>
#include <hydraforge/pdk/agent_macros.h>
#include <hydraforge/pdk/safe_exec.h>
```

### 决策 2: monorepo `pdk/` 子目录 (Sprint 4 T4a)

**问题**: PDK 必须**独立仓库** (P1) 但**当前在 monorepo 内**开发。

**方案**: Phase 1 Sprint 4 先在 monorepo 内 `pdk/` 子目录 ship (与 `src/` `include/` `tests/` 平级),便于 CI 集成测试。**Phase 2** 后拆分至独立 `hydraforge-pdk` 仓库 (S4.T4b 异步执行)。

```cmake
# pdk/CMakeLists.txt (Sprint 4 T4a)
add_library(hydraforge_pdk INTERFACE)
target_include_directories(hydraforge_pdk INTERFACE
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)
target_compile_features(hydraforge_pdk INTERFACE cxx_std_20)
```

```cmake
# 根 CMakeLists.txt (Sprint 4 T4a 修改)
add_subdirectory(pdk)  # 新增 S4.T4a
# ... 现有 add_subdirectory(src) add_subdirectory(tests) 不变
```

### 决策 3: PDK 与 Runtime 契约解耦 (ADR-0021 §4)

PDK 头文件**仅依赖** Runtime 契约接口,**不依赖** Runtime 内部实现:

```cpp
// pdk/tool_macros.h include 列表 (验证 P3 静态链接)
#include "agenticdsl/contract/itool_registry.h"  // 契约 (允许)
#include "agenticdsl/contract/iinteraction_bus.h" // 契约 (允许)
// 禁止:
// #include "core/engine.h"        // ❌ 内部实现
// #include "modules/..."         // ❌ 内部模块
```

**验证方法**: `nm plugin.so | grep hydraforge_runtime` 必须返回空 (Sprint 5 E2E 验证)。

### 决策 4: 测试策略 (Sprint 4 T5)

**5 个 test cases** (对齐 plan §Sprint 4 T5):

| # | 测试名 | 验证目标 |
|---|--------|---------|
| 1 | DECLARE_TOOL 展开 | 宏展开为 ToolSpec + handler, name/description 正确 |
| 2 | DEFINE_AGENT 模板实例化 | class 编译 + 构造 + run() 不崩溃 |
| 3 | SafeExec 超时处理 | 10ms timeout, 100ms 函数 → 抛 std::runtime_error |
| 4 | SafeExec 异常捕获 | handler 抛 std::exception → SafeExec 传播 |
| 5 | PDK 头文件无 Runtime 内部依赖 | 编译时 -Winclude 检查 |

**Phase 1 Sprint 4 范围**:
- DECLARE_TOOL + DEFINE_AGENT + SafeExec MVP (仅超时+异常,无 fork/seccomp)
- monorepo `pdk/` 子目录 (T4a)
- 5/5 test cases pass

**Phase 2 范围** (后续 sprint):
- DEFINE_AGENT 完整 ReAct 循环
- PlanExecute / ForkJoin 循环模板
- FakeStateStore / StubLLM / MockSandbox 测试替身
- PluginLifecycle 类
- 完整 SafeExec (fork/cgroups/seccomp)
- `hydraforge-pdk` 外部仓库推送 (T4b 异步)

### 代码侧 (新代码)

- `include/agenticdsl/pdk/tool_macros.h` (新建, ~100 行) — DECLARE_TOOL 宏 + ToolSpec 结构体
- `include/agenticdsl/pdk/agent_macros.h` (新建, ~80 行) — DEFINE_AGENT 宏 + AgentLoopType enum
- `include/agenticdsl/pdk/safe_exec.h` (新建, ~60 行) — SafeExec 模板类 (超时+异常)
- `include/agenticdsl/pdk/pdk.h` (新建, ~10 行) — 统一入口
- `include/agenticdsl/pdk/CMakeLists.txt` (新建, INTERFACE 库)
- `pdk/CMakeLists.txt` (新建, monorepo 子目录)
- `pdk/include/hydraforge/pdk/...` (软链接/symlink 到 `include/agenticdsl/pdk/...`, 或直接使用现有路径)
- 根 `CMakeLists.txt` 修改 (add_subdirectory(pdk))
- `tests/test_pdk_macros.cpp` (新建, 5 case, ~200 行)
- `tests/CMakeLists.txt` (无需改, GLOB 自动注册)

### 文档侧

- 更新 `docs/adr/adr-0021-pdk-design.md` 状态: 🔍 Proposed → 🟡 Partial (Sprint 4 ship 后)
- 更新 `docs/roadmap-status.md` line 44: Sprint 4 状态 (0% → 完成)
- 更新 `docs/phase1-roadmap.md` §Sprint 4 详细任务
- 更新 `AGENTS.md` NOTES: Sprint 4 ship 注释
- 不更新 `docs/adr/adr-0022-plugin-loading.md` 状态 (保持 🔍 Proposed, Sprint 5 ship 后再变更)
- 不更新 `examples/agent_chat_pdk/` (Sprint 5 范围)

## Impact

- **Affected specs**: 新增 PDK 契约 spec
- **Affected ADRs**:
  - `adr-0021-pdk-design.md` 状态: 🔍 Proposed → 🟡 Partial
  - `adr-0022-plugin-loading.md` 状态: 不变 (Sprint 5)
- **Affected code**:
  - `include/agenticdsl/pdk/` (新建 4 头 + 1 CMakeLists)
  - `pdk/` (新建 monorepo 子目录 + CMakeLists)
  - 根 `CMakeLists.txt` (add_subdirectory(pdk))
  - `tests/test_pdk_macros.cpp` (新建)
- **Affected tests**: 现有 31 测试零回归 + 新增 5 测试 = 36/36 ctest pass
- **Breaking change**: 无 (纯新增 PDK 头文件,不修改 Runtime)
- **Runtime 影响**: 零 (PDK 静态链接到插件, Runtime 零感知, ADR-0021 P3)

## Success Criteria

- [ ] `pdk/tool_macros.h` / `agent_macros.h` / `safe_exec.h` / `pdk.h` 编译通过
- [ ] `DECLARE_TOOL` 宏展开正确 (5 行领域逻辑示例编译 + 运行)
- [ ] `DEFINE_AGENT` 模板实例化通过 (class 构造 + run() 调用)
- [ ] `SafeExec::run()` 超时 + 异常行为正确 (3 测试覆盖)
- [ ] `pdk/` monorepo 子目录 CMakeLists 配置正确 (INTERFACE 库)
- [ ] 根 CMakeLists.txt `add_subdirectory(pdk)` 通过
- [ ] 5/5 test_pdk_macros 测试通过
- [ ] 36/36 ctest pass (31 baseline + 5 new)
- [ ] PDK 头文件无 Runtime 内部依赖 (验证 #include 列表仅契约接口)
- [ ] `tools/adr_lint.py docs/adr/` exit 0 (ADR-0021 状态更新)
- [ ] `openspec validate 2026-07-07-pdk-skeleton` exit 0
- [ ] ADR-0021 状态: 🔍 Proposed → 🟡 Partial (Sprint 4 ship)
- [ ] 6 commits per plan §Sprint 4 (T1 → T2 → T3 → T4a → T4b → T5)

## Out of Scope (Non-goals)

- ❌ 不实现完整 ReAct 循环 (Phase 2 范围)
- ❌ 不实现 PlanExecute / ForkJoin 循环模板 (Phase 2 范围)
- ❌ 不实现 FakeStateStore / StubLLM / MockSandbox (Phase 2 范围)
- ❌ 不实现 PluginLifecycle 类 (Phase 3 范围)
- ❌ 不实现完整 SafeExec (fork/cgroups/seccomp, Phase 3 范围)
- ❌ 不实现 CMake 生成器 (Phase 4 范围)
- ❌ 不实现真实 `.so` 加载 (Sprint 5 PluginLoader 范围)
- ❌ 不修改 Runtime 任何代码 (P3 静态链接, Runtime 零感知)
- ❌ 不修改 CognitiveWorker / DomainWorkerPool (Sprint 2/3 已 ship)
- ❌ 不修改 examples/* (Sprint 5 收官范围)

## Dependencies

- **Block**: Sprint 0/1a/1b/P1/CognitiveWorker/DomainWorkerPool (✅ 全部 ship)
- **Block by**: Sprint 5 PluginLoader (2026-07-14 ~ 2026-07-16, W5)
- **Related**:
  - `2026-07-14-plugin-loader/` (Sprint 5, PluginLoader 加载 PDK 编译的 `.so`)
  - `2026-06-30-domain-worker-pool` (Sprint 3, DomainWorkerPool handler 是 PDK 工具的运行时实例)

## Estimated Effort

~3.3 天 (单人, 反映 5 测试 + T4b 外部阻塞):
- T1 DECLARE_TOOL 宏 (PIMPL-lite 模式, no Runtime 依赖): 0.5d
- T2 DEFINE_AGENT 模板 (MVP React 循环, Phase 2 完整化): 1d
- T3 SafeExec 封装 (MVP 仅超时+异常, 无 fork/seccomp): 0.5d
- T4a monorepo `pdk/` 子目录 + 根 CMakeLists add_subdirectory: 1d
- T4b 推到独立 `hydraforge-pdk` GitHub 仓库: 0.3d (含外部阻塞等待)
- T5 5 test cases: 0.5d (含 PDK 头文件无 Runtime 依赖验证)