# Design: PDK Skeleton (Sprint 4)

> **变更类型**: 真实实现 — 本 design 描述 PDK 头文件架构与 monorepo 治理策略
> **关联 proposal**: `openspec/changes/2026-07-07-pdk-skeleton/proposal.md`
> **关联 spec**: `openspec/changes/2026-07-07-pdk-skeleton/specs/pdk-skeleton/spec.md`
> **关联 ADR**: docs/adr/adr-0021-pdk-design.md (PDK 设计, 🔍 Proposed → 🟡 Partial Sprint 4 ship) + ADR-0022 (Plugin Loading) + ADR-0019 (IInteractionBus) + ADR-0020 (CognitiveWorker + DomainWorkerPool)
> **关联 plan**: `.omo/plans/phase1-execution.md` §Sprint 4

## 架构合规性检查

| 约束 | 状态 | 备注 |
|------|------|------|
| 2 空格缩进 | ✅ | 沿用现有 |
| 中文注释优先 | ✅ | 全部新增注释中文 |
| C++20 + CMake 3.20+ | ✅ | std::invoke_result_t (C++17) + cxx_std_20 |
| nlohmann_json | ✅ | ToolSpec.params + tool handler json 参数 |
| INTERFACE 库 | ✅ | PDK 是头文件库 (无 .cpp, 仅 .h), CMake INTERFACE 库 |
| Anti-pattern 避免 | ✅ | 不删失败测试, 提交前 ctest |
| monorepo `pdk/` 与 `src/` `include/` `tests/` 平级 | ✅ | K3 决策, 便于 Phase 2 拆分 |
| Runtime 零依赖 (P3 静态链接) | ✅ | PDK 仅依赖 Runtime 契约接口 |

## 关键设计决策

### 决策 1: DECLARE_TOOL 宏 — 5 行领域逻辑

**问题**: 当前 `engine->register_tool(name, callback)` 需要 ~20 行样板 (Schema/权限/日志/错误处理都需手写)。

**方案**: PDK 提供 `DECLARE_TOOL` 宏, 展开为 `ToolSpec` 元数据 + 错误处理包装的 handler 函数。

```cpp
// include/agenticdsl/pdk/tool_macros.h (Sprint 4 T1)
namespace hydraforge::pdk {

// 工具参数 Schema (简化版 MVP)
struct ToolParam {
  std::string name;
  std::string type;        // "string" | "int" | "json"
  bool required = false;
};

// 工具权限声明 (MVP 仅 metadata, Phase 2 集成 ADR-0004 权限校验)
struct ToolPermissions {
  std::vector<std::string> readonly_paths;
  std::vector<std::string> write_paths;
  bool network = false;
};

// 工具元数据 (Schema)
struct ToolSpec {
  std::string name;
  std::string description;
  std::vector<ToolParam> params;
  ToolPermissions permissions;
};

// DECLARE_TOOL 宏: 展开为 ToolSpec + handler 函数
//
// 用法:
//   DECLARE_TOOL(echo_tool, "回显工具") {
//       // 开发者只写领域逻辑, 5 行内完成
//       nlohmann::json args = /* injected */;
//       return args;
//   }
//
// 展开后:
//   - tool_spec_echo_tool (inline ToolSpec 实例)
//   - tool_handler_echo_tool (inline nlohmann::json function, try-catch 包装)
//
// MVP 实现: 宏展开为 inline 变量 + inline 函数, 不依赖 Runtime 注册表
// (Sprint 5 PluginLoader 通过 .so 反射获取 ToolSpec + handler)
#define DECLARE_TOOL(name, description, ...) \
    inline ToolSpec tool_spec_##name = { \
        #name, description, {}, {} \
    }; \
    inline nlohmann::json tool_handler_##name(const nlohmann::json& __pdk_args) { \
        try { \
            __VA_ARGS__ \
        } catch (const std::exception& __pdk_e) { \
            return nlohmann::json{{"error", __pdk_e.what()}}; \
        } \
    }

} // namespace hydraforge::pdk
```

**关键设计点**:
- **inline 变量** (C++17): 避免多重定义错误, 允许头文件多个 TU
- **try-catch 包装**: 异常隔离, 调用方收到 json 错误而非 std::exception
- **不依赖 Runtime 注册表**: Sprint 4 MVP 仅生成 ToolSpec + handler, 不调 `engine->register_tool()` (Phase 2 集成 Runtime)
- **ToolSpec / ToolParam / ToolPermissions**: 数据结构, 无业务逻辑

### 决策 2: DEFINE_AGENT 宏 — React 循环模板

**问题**: Agent 循环 (ReAct: 思考-行动-观察) ~1000 行手写, 每个领域插件重复实现。

**方案**: PDK 提供 `DEFINE_AGENT` 宏 + `AgentLoopType` enum, 展开为 `class XXXAgent` 含 run() 方法。

```cpp
// include/agenticdsl/pdk/agent_macros.h (Sprint 4 T2)
namespace hydraforge::pdk {

// Agent 循环类型
enum class AgentLoopType {
  React,        // 思考 → 行动 → 观察 → 重复 (MVP Sprint 4)
  PlanExecute,  // 规划 → 执行 → 验证 → 完成 (Phase 2)
  ForkJoin,     // 并行分支 → 合并结果 (Phase 2)
};

// DEFINE_AGENT 宏: 展开为 XXXAgent class
//
// 用法:
//   DEFINE_AGENT(coding_assistant, React) {
//       ON_INTENT("code_review", [](const nlohmann::json& args) { ... });
//       ON_INTENT("refactor", [](const nlohmann::json& args) { ... });
//   }
//
// MVP 实现: 展开为 class, run() 委托 SimpleCognitiveOrchestrator (per-agent DSLEngine)
#define DEFINE_AGENT(name, loop_type) \
    static_assert(loop_type == AgentLoopType::React, \
        "DEFINE_AGENT MVP only supports React loop. " \
        "PlanExecute/ForkJoin are Phase 2 (see ADR-0021 §3.2)."); \
    class name##Agent { \
    public: \
      name##Agent(std::unique_ptr<DSLEngine> engine, \
                  std::shared_ptr<IInteractionBus> bus) \
          : engine_(std::move(engine)), bus_(std::move(bus)) {} \
      ToolResult run(const std::string& prompt) { \
        // MVP: 单轮 ReAct (delegated to SimpleCognitiveOrchestrator) \
        SimpleCognitiveOrchestrator orch( \
            &engine_->get_tool_registry(), \
            engine_->get_llm_provider()); \
        ToolResult result; \
        orch.process(prompt, [&result](ToolResult r) { \
            result = std::move(r); \
        }); \
        return result; \
      } \
    private: \
      std::unique_ptr<DSLEngine> engine_; \
      std::shared_ptr<IInteractionBus> bus_; \
    };

} // namespace hydraforge::pdk
```

**关键设计点**:
- **static_assert**: MVP 仅 React, PlanExecute/ForkJoin 编译失败 + 明确错误信息
- **per-agent 隔离**: class 持有独立 DSLEngine + IInteractionBus (ADR-0020 §2.2.1)
- **PIMPL-lite**: 头文件前向声明 DSLEngine + IInteractionBus, 析构由 class 隐式处理
- **ON_INTENT**: Phase 2 实施, Sprint 4 MVP 仅为 run() 单轮 ReAct 占位

### 决策 3: SafeExec — 超时 + 异常捕获 (MVP)

**问题**: 工具执行可能 hang (无限循环) 或抛异常, 调用方需要超时控制 + 异常隔离。

**方案**: PDK 提供 `SafeExec` 类, MVP 用 `std::async` + `wait_for` 实现超时, try-catch 异常传播 (不包装)。

```cpp
// include/agenticdsl/pdk/safe_exec.h (Sprint 4 T3)
namespace hydraforge::pdk {

// 沙箱执行封装 (MVP: 超时 + 异常, Phase 2/3: + fork/cgroups/seccomp)
class SafeExec {
 public:
  SafeExec() = default;

  // 链式配置: 超时
  SafeExec& with_timeout(std::chrono::milliseconds timeout) {
    timeout_ = timeout;
    return *this;
  }

  // 链式配置: Layer profile (MVP no-op, Phase 2/3 集成 ADR-0004 权限)
  SafeExec& with_layer_profile(int profile) {
    layer_profile_ = profile;
    return *this;
  }

  // 执行 fn, 返回 fn() 结果, 超时/异常按合约传播
  template <typename F>
  auto run(F&& fn) -> std::invoke_result_t<F> {
    // MVP: std::async + wait_for(timeout)
    auto future = std::async(std::launch::async, std::forward<F>(fn));
    auto status = future.wait_for(timeout_);

    if (status == std::future_status::timeout) {
      throw std::runtime_error(
          "SafeExec: tool execution timed out after " +
          std::to_string(timeout_.count()) + "ms");
    }

    // 异常传播: future.get() 抛原异常 (不包装)
    return future.get();
  }

 private:
  std::chrono::milliseconds timeout_{30000};  // 默认 30s
  int layer_profile_{0};                       // MVP no-op
};

} // namespace hydraforge::pdk
```

**关键设计点**:
- **std::async + wait_for**: C++11 异步任务, MVP 简化实现
- **超时检测**: wait_for 返回 std::future_status::timeout → 抛 runtime_error
- **异常传播**: future.get() 不包装原异常 (与 DECLARE_TOOL 不同, SafeExec 调用方期望异常)
- **链式配置**: with_timeout().with_layer_profile() fluent API
- **Phase 2/3 扩展点**: layer_profile_ 字段为 ADR-0004 权限校验预留

### 决策 4: monorepo `pdk/` 子目录 + INTERFACE 库

**问题**: PDK 必须独立仓库 (P1) 但当前在 monorepo 内开发, 需要 bridge 方案。

**方案**: monorepo `pdk/` 子目录 + CMake INTERFACE 库, 便于 Sprint 4 ship + Phase 2 拆分。

```cmake
# pdk/CMakeLists.txt (Sprint 4 T4a)
# PDK 头文件库 (INTERFACE 库, 无 .cpp, 仅 .h)
# 静态链接到插件, Runtime 零感知 (ADR-0021 P3)

# 创建 INTERFACE 库 (header-only)
add_library(hydraforge_pdk INTERFACE)

# 设置 C++20 标准
target_compile_features(hydraforge_pdk INTERFACE cxx_std_20)

# 头文件路径: pdk/include/hydraforge/pdk/...
target_include_directories(hydraforge_pdk INTERFACE
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:include>
)

# 链接依赖: PDK 仅依赖 Runtime 契约接口 (P3)
target_link_libraries(hydraforge_pdk INTERFACE
  agenticdsl_contract  # IInteractionBus + IToolRegistry 契约接口
)
```

```cmake
# 根 CMakeLists.txt (Sprint 4 T4a 修改, 单行 add_subdirectory)
# 在 add_subdirectory(src) 之前或之后 (顺序不重要, INTERFACE 库无副作用)
add_subdirectory(pdk)  # Sprint 4 新增 T4a
```

```bash
# pdk/ 子目录结构 (Sprint 4 T4a)
pdk/
├── CMakeLists.txt           # INTERFACE 库配置
└── include/
    └── hydraforge/
        └── pdk/
            ├── pdk.h                  # 统一入口
            ├── tool_macros.h          # DECLARE_TOOL
            ├── agent_macros.h         # DEFINE_AGENT
            └── safe_exec.h            # SafeExec
```

**关键设计点**:
- **INTERFACE 库**: 无 .cpp 编译, 仅传播 include_directories + compile_features
- **路径映射**: `pdk/include/hydraforge/pdk/...` ← 与 ADR-0021 §2.2 期望路径一致 (后续可软链接或直接使用)
- **不依赖 Runtime 内部**: target_link_libraries 仅 `agenticdsl_contract` (P3)
- **可独立拆分**: Phase 2 `git mv pdk/ ../hydraforge-pdk/` 后, HydraForge 根 CMakeLists.txt 移除 `add_subdirectory(pdk)` + 添加 `find_package(hydraforge_pdk REQUIRED)`

### 决策 5: 测试策略 — 5 cases 覆盖 MVP

**问题**: PDK 头文件 + 宏展开需测试验证, Sprint 4 5 test cases 对齐 plan §Sprint 4 T5。

**测试用例清单**:

| # | 测试名 | 验证目标 | 工具 |
|---|--------|---------|------|
| 1 | `DECLARE_TOOL 展开` | 宏展开生成 tool_spec + handler, name/description 正确 | Catch2 TEST_CASE + 静态检查 |
| 2 | `DEFINE_AGENT 模板实例化` | coding_assistantAgent 编译 + 构造 + run() 调用 | Catch2 TEST_CASE + DSLEngine mock |
| 3 | `SafeExec 超时处理` | with_timeout(10ms) + sleep_for(100ms) → 抛 runtime_error | Catch2 TEST_CASE |
| 4 | `SafeExec 异常捕获` | handler 抛 std::runtime_error → SafeExec::run 传播原异常 | Catch2 TEST_CASE |
| 5 | `PDK 头文件无 Runtime 内部依赖` | 编译时检查 #include 列表仅 `agenticdsl/contract/*.h` | 编译测试 + grep 静态验证 |

**测试基础设施**:
- Catch2 (沿用现有)
- 使用 `MockLLMProvider` + 简单 DSL 模板 (同 test_cognitive_worker.cpp)
- 测试 compile_commands.json 验证 PDK 头文件 include 路径

### 决策 6: PDK 与 Runtime 契约解耦 (P3 验证)

**问题**: PDK 静态链接到插件, Runtime 零感知 (ADR-0021 P3)。如何验证 PDK 头文件不引入 Runtime 内部依赖?

**方案**: 静态分析 + 编译验证 + nm 检查 (Sprint 5 E2E)。

```cpp
// pdk/tool_macros.h 头文件 include 列表 (验证 P3)
// ✅ 允许: Runtime 契约接口
#include "agenticdsl/contract/itool_registry.h"
#include "agenticdsl/contract/iinteraction_bus.h"

// ✅ 允许: 标准库 + nlohmann_json
#include <nlohmann/json.hpp>
#include <string>
#include <vector>
#include <chrono>

// ❌ 禁止 (编译期 -Winclude 警告 + 静态扫描验证):
// #include "core/engine.h"        // Runtime 内部
// #include "core/types/..."        // Runtime 内部
// #include "modules/..."           // Runtime 内部模块
// #include "common/..."            // Runtime 内部 common
```

**验证方法**:
1. **编译期**: `-Wall -Wextra -Werror=unused-parameter` + 静态分析
2. **静态扫描**: `grep -r "core/\|modules/\|common/" pdk/include/` MUST 返回空
3. **运行时** (Sprint 5 E2E): `nm plugin.so | grep hydraforge_runtime` MUST 返回空

## 实施路径 (S4.T1 → T4a → T5)

### T1: tool_macros.h + CMakeLists (新建, ~120 行)
- `include/agenticdsl/pdk/tool_macros.h`: DECLARE_TOOL 宏 + ToolSpec/ToolParam/ToolPermissions
- `include/agenticdsl/pdk/CMakeLists.txt`: agenticdsl_hdr_pdk INTERFACE 库
- 验收: 头文件独立编译, DECLARE_TOOL 示例编译通过

### T2: agent_macros.h (新建, ~80 行)
- `include/agenticdsl/pdk/agent_macros.h`: DEFINE_AGENT 宏 + AgentLoopType enum
- 验收: DEFINE_AGENT(React) 编译 + 实例化 + run() 调用通过

### T3: safe_exec.h (新建, ~70 行)
- `include/agenticdsl/pdk/safe_exec.h`: SafeExec 类 + 链式配置 + run() 模板
- 验收: 超时 + 异常 + 正常路径 3 测试通过

### T4a: monorepo pdk/ 子目录 (新建, ~30 行)
- `pdk/CMakeLists.txt`: hydraforge_pdk INTERFACE 库
- `pdk/include/hydraforge/pdk/pdk.h`: 统一入口 (引用 3 子头)
- 根 `CMakeLists.txt`: `add_subdirectory(pdk)`
- 验收: ctest 链接 hydraforge_pdk 通过

### T5: test_pdk_macros.cpp (新建, ~250 行)
- `tests/test_pdk_macros.cpp`: 5 test cases
- 验收: 5/5 test case pass, 36/36 ctest pass (31 baseline + 5 new), 零回归

## 提交策略 (5 commits, per plan §Sprint 4)

```
S4.T1 → feat(pdk): add DECLARE_TOOL macro + ToolSpec metadata
S4.T2 → feat(pdk): add DEFINE_AGENT template (React loop MVP)
S4.T3 → feat(pdk): add SafeExec wrapper (timeout + exception MVP)
S4.T4a → build(pdk): create monorepo pdk/ subdir + INTERFACE library
S4.T5 → test(pdk): add 5 test cases for PDK macros (36/36 ctest)
```

(T4b 外部阻塞, Sprint 4 ship 后异步, 单独 commit)

## 风险与缓解

| 风险 | 严重度 | 缓解措施 |
|------|-------|---------|
| 宏展开冲突 (Token 拼接 / 命名空间污染) | 中 | 使用 `__pdk_` 前缀 (工具宏保留标识符, N2550 C++ 提案) |
| DEFINE_AGENT 模板实例化开销 | 低 | 编译时间 < 200ms (单 class), MVP 单轮 ReAct |
| SafeExec MVP 无 fork/cgroups | 低 | Phase 2/3 扩展, 当前 MVP 仅超时+异常 |
| Runtime 内部依赖潜入 PDK | 高 | grep 静态扫描 + 编译时 -Winclude 警告 + Sprint 5 nm 验证 |
| monorepo `pdk/` 拆分后路径变化 | 中 | 头文件路径 `pdk/include/hydraforge/pdk/...` 与 ADR-0021 §2.2 一致, Phase 2 拆分时 git mv 不改路径 |
| GitHub `hydraforge-pdk` 组织不存在 (T4b) | 中 | Sprint 4 ship 后异步执行, monorepo `pdk/` 维持当前状态 |

## 相关 ADR / 文档

- **ADR-0021 PDK 设计** (主): 状态变更 🔍 Proposed → 🟡 Partial (Sprint 4 ship)
- **ADR-0022 插件加载**: 保持 🔍 Proposed (Sprint 5 ship 后变更)
- **ADR-0019 IInteractionBus**: PDK 工具运行时使用 (Sprint 1b 已 ship)
- **ADR-0020 §2.2.1 CognitiveWorker/DomainWorkerPool**: PDK Agent 内部使用 per-agent DSLEngine 模式
- **ADR-0004 ToolRegistry 安全**: SafeExec::with_layer_profile 集成 (Phase 2/3 实施)
- **Sprint 3 DomainWorkerPool**: DomainWorkerPool handler 是 PDK 工具的运行时实例 (Phase 2 集成点)
- **Sprint 5 PluginLoader**: 加载 PDK 编译的 `.so` 插件, 通过 tool_spec_xxx + tool_handler_xxx 反射调用