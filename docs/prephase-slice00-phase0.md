# Pre-Phase + Slice 00 + Phase 0 详细实施计划

**版本**: v1.2（基于 2026-05-30 Alex 跨文档对齐审查修正）
**关联**: `docs/implementation-roadmap.md` Phase Pre-Phase / Slice 00 / Phase 0
**工期**: 7-10 天
**前提**: 无（项目第一个实施阶段）

### 💡 文档对齐说明

**命名冲突提醒**: `docs/agenticdsl/implementation/` 系列文档（如 `phase-0-implementation.md`）中的"Phase 0"特指 **AgenticDSL 自举路径的阶段 0（云端 LLM 集成）**，对应本路线图的 **Phase 0 Track 0.1**。如果两个文档对阶段编号的理解不一致，以 `docs/implementation-roadmap.md` 为准。详见附录 A 的跨文档对照表。

---

## 修订说明

本计划基于初版审查意见修正，主要变更：

| # | 审查问题 | 修正 |
|---|---------|------|
| 🚨 1 | 原 `hydraforge` 命名空间与现有 `agenticdsl` 冲突 | 统一为 `agenticdsl`（目录改为 `include/agenticdsl/`）|
| 🚨 2 | 测试使用 `test_runner`（不存在） | 改为独立测试二进制名（`test_<模块>`） |
| 🚨 3 | 引用不存在的 `illm_provider.h` | 遵循 `phase-0-implementation.md`，deprecate `ILLMAdapter` |
| ⚠️ 4 | CMake 直接改 `agenticdsl_core` | 改为使用 `agenticdsl_includes` INTERFACE 库 |
| ⚠️ 5 | SSE 解析器命名 `sse_parser` | 统一为 `sse_stream`（与源文档一致） |
| ⚠️ 6 | 新建 `src/domains/` 层 | 取消，示例工具内联注册 |
| ⚠️ 7 | `ExecutionResult` 重定义 | 复用现有 `agenticdsl::types::ExecutionResult` |
| 💡 8 | 新目录缺少 CMakeLists.txt | 补充所有新目录的 CMake 配置 |
| 💡 9 | 编号体系 | 附录增加 roadmap 对照表 |
| 💡 10 | `syncAwait` 大小写 | 修正并确认头文件路径 |
| 💡 11 | Cloud LLM 可选编译 | 增加编译选项 |

---

## 概览

```
时间线 ──────────────────────────────────────────────────────────→

Pre-Phase (0.5天)
  [P0.0~P0.4] 核心接口头文件 + CMake 配置
    │
    ├── Slice 00 (1-2天, 可与 Track 0.1 并行)
    │   [S0.1~S0.6] 引入 Taskflow + async_simple，验证编译
    │
    ▼
Phase 0 — 三条并行 Track
    │
    ├── Track 0.1 (3-4天): 云端 LLM 集成
    │   [M1.1~M3.3] CloudLLMAdapter + MockLLMProvider
    │
    ├── Track 0.3 (2-3天, 与 0.1 并行): 最小契约层
    │   [M5.1~M6.3] IInteractionBus MVP + InMemoryBus
    │
    └── Track 0.2 (5-7天, 依赖 0.1): 三层调用链
        [M4.1~M4.9] ModelRegistry + SimpleCognitiveOrchestrator + 示例
```

---

## 一、Pre-Phase — 核心接口定义

### 目标

确保所有后续 Phase 有统一的编译目标和基类，Slice/Track 可直接继承。

### 原则

- 仅 `.h` 头文件，无 `.cpp` 实现
- 纯虚接口（`virtual ... = 0`），无默认行为
- **命名空间统一为 `agenticdsl`**（与现有代码一致）
- 最小依赖（仅标准库 + nlohmann_json + 现有类型）

### 任务列表

| # | 任务 | 文件 | 操作 | 验证标准 |
|---|------|------|:----:|---------|
| P0.0a | 创建公共头文件目录 | `include/agenticdsl/cognitive/` | 新建 | 目录存在 |
| P0.0b | 创建策略头文件目录 | `include/agenticdsl/policy/` | 新建 | 目录存在 |
| P0.0c | 创建类型头文件目录 | `include/agenticdsl/types/` | 新建 | 目录存在 |
| P0.1 | 认知层入口接口 | `include/agenticdsl/cognitive/icognitive_orchestrator.h` | 新建 | 编译通过 |
| P0.2 | 执行策略接口 | `include/agenticdsl/policy/iexecution_policy.h` | 新建 | 编译通过 |
| P0.3 | Session 前置声明 | `include/agenticdsl/types/session_fwd.h` | 新建 | 编译通过 |
| P0.4 | CMake include 配置 | `CMakeLists.txt`（根） | 修改 | include 路径生效 |

> **与 Phase 3 的边界**: Pre-Phase 仅声明纯虚接口（`virtual ... = 0`），不含任何实现。Phase 3 ADR-0031 P1 才首次提供具体 `AgentModePolicy` 类。

### 详细实现

#### P0.1: ICognitiveOrchestrator

```cpp
// include/agenticdsl/cognitive/icognitive_orchestrator.h
#pragma once

#include <string>
#include <functional>
#include "core/types/node.h"  // 复用现有 NodePath 等类型

namespace agenticdsl {

/// 认知智能体编排器接口（基座层定义，认知层实现）
/// 类比：kernel 定义 shell 接口
class ICognitiveOrchestrator {
public:
    virtual ~ICognitiveOrchestrator() = default;

    /// 处理用户消息（异步，在独立线程/协程中执行）
    /// @param session_id 用户会话 ID
    /// @param on_complete 完成回调（在认知层工作线程调用）
    virtual void process(const std::string& session_id,
                         std::function<void(ExecutionResult)> on_complete) = 0;

    /// 用户中断（SIGINT 语义）
    virtual void interrupt(const std::string& session_id) = 0;

    /// 模式切换通知
    virtual void on_mode_changed(const std::string& session_id,
                                  SessionMode new_mode) = 0;
};

} // namespace agenticdsl
```

> **注意**: `ExecutionResult` 复用现有 `agenticdsl::types::ExecutionResult`（定义在 `src/core/types/budget.h`），不重新定义。

#### P0.2: IExecutionPolicy

```cpp
// include/agenticdsl/policy/iexecution_policy.h
#pragma once

#include <string>
#include <vector>

namespace agenticdsl {

// 前置声明（完整定义在 Phase 3 ADR-0004 V2）
struct ToolMetadata;
struct ToolCallContext;

/// 执行策略接口（基座层定义，认知层实现三种模式）
/// 类比：sudoers 规则
class IExecutionPolicy {
public:
    virtual ~IExecutionPolicy() = default;

    /// 此工具调用是否需要用户审批？
    virtual bool requires_approval(const ToolMetadata& meta,
                                   const ToolCallContext& ctx) const = 0;

    /// Plan 完成后是否自动进入 Execute？
    virtual bool should_auto_execute() const = 0;

    /// 是否展示完整计划？
    virtual bool should_show_plan() const = 0;

    /// 当前模式名称
    virtual std::string mode_name() const = 0;

    /// 舰队模式最大并行度
    virtual size_t fleet_max_concurrency() const = 0;
};

} // namespace agenticdsl
```

#### P0.3: Session 前置声明

```cpp
// include/agenticdsl/types/session_fwd.h
#pragma once

#include <string>

namespace agenticdsl {

class UserSession;
class TaskSession;
class SubtaskSession;

enum class SessionLevel { User, Task, Subtask };

struct SessionContext;

} // namespace agenticdsl
```

#### P0.4: CMake 修改

```cmake
# CMakeLists.txt（根），修改 agenticdsl_includes 定义处（约第 37 行）：
target_include_directories(agenticdsl_includes INTERFACE
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src>
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include>          # ← 新增
    $<INSTALL_INTERFACE:include>
    ${NLOHMANN_JSON_INCLUDE_DIR}
    ${INJA_INCLUDE_DIR}
    ${YAML_CPP_INCLUDE_DIR}
    ${HTTPLIB_INCLUDE_DIR}
)
```

### 验证命令

```bash
mkdir -p build && cd build
cmake .. && make -j$(nproc) agenticdsl_core
# 预期：编译通过，无新增 .o 文件（仅头文件）
```

---

## 二、Slice 00 — 异步基础设施验证

### 目标

引入 Taskflow 和 async_simple 到项目中，验证编译通过和基本桥接能力。为 Phase 2（ADR-0030）铺路。

### 前提

- Pre-Phase P0.4（CMake 配置）完成
- 网络可用（git submodule 拉取）

### 任务列表

| # | 任务 | 文件/目录 | 操作 | 验证标准 |
|---|------|----------|:----:|---------|
| S0.1 | 引入 Taskflow | `external/taskflow/` | git submodule | `taskflow/taskflow.hpp` 存在 |
| S0.2 | 引入 async_simple | `external/async_simple/` | git submodule | 目录存在 |
| S0.3 | Taskflow CMake 配置 | `CMakeLists.txt`（根） | 修改 | header 可 include |
| S0.4 | async_simple CMake 配置 | `external/CMakeLists.txt` | 新建/修改 | 编译通过 |
| S0.5 | 编译选项优化 | `external/CMakeLists.txt` | 修改 | 禁用测试/demo |
| S0.6 | 桥接验证测试 | `tests/test_async_bridge.cpp` | 新建 | 3 个测试通过 |

### 详细实现

#### S0.1-S0.2: 引入依赖

```bash
cd external
git submodule add https://github.com/taskflow/taskflow.git taskflow
cd taskflow && git checkout v4.0.0 && cd ..

git submodule add https://github.com/alibaba/async_simple.git async_simple
cd async_simple && git checkout v1.4 && cd ..
```

#### S0.3-S0.5: CMake 配置

```cmake
# CMakeLists.txt（根），修改 agenticdsl_includes 定义处：
target_include_directories(agenticdsl_includes INTERFACE
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src>
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include>
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/external/taskflow>  # ← 新增
    $<INSTALL_INTERFACE:include>
    ${NLOHMANN_JSON_INCLUDE_DIR}
    ${INJA_INCLUDE_DIR}
    ${YAML_CPP_INCLUDE_DIR}
    ${HTTPLIB_INCLUDE_DIR}
)

# external/CMakeLists.txt（新建）— 用于管理第三方库构建
set(ASYNC_SIMPLE_ENABLE_TESTS OFF CACHE BOOL "" FORCE)
set(ASYNC_SIMPLE_BUILD_DEMO OFF CACHE BOOL "" FORCE)
add_subdirectory(async_simple)
target_link_libraries(agenticdsl_core PUBLIC async_simple)
```

同时修改根 `CMakeLists.txt`，在 `add_subdirectory(external/yaml-cpp ...)` 后添加：

```cmake
add_subdirectory(external)
```

#### S0.6: 桥接验证测试

```cpp
// tests/test_async_bridge.cpp
#include "catch_amalgamated.hpp"
#include <taskflow/taskflow.hpp>
#include <async_simple/coro/Lazy.h>
#include <async_simple/coro/SyncAwait.h>

using namespace async_simple::coro;

TEST_CASE("Taskflow parallel execution", "[async][stage0]") {
    tf::Executor executor(4);
    tf::Taskflow taskflow;
    std::atomic<int> counter{0};

    for (int i = 0; i < 4; ++i) {
        taskflow.emplace([&counter]() { counter.fetch_add(1); });
    }

    executor.run(taskflow).wait();
    REQUIRE(counter.load() == 4);
}

TEST_CASE("async_simple basic coroutine", "[async][stage0]") {
    auto lazy = []() -> Lazy<int> { co_return 42; };
    auto result = syncAwait(lazy());
    REQUIRE(result == 42);
}

TEST_CASE("Taskflow + async_simple bridge", "[async][stage0]") {
    tf::Executor executor(2);

    auto bridge_test = [&executor]() -> Lazy<int> {
        std::atomic<int> sum{0};
        tf::Taskflow taskflow;
        taskflow.emplace([&sum]() { sum += 10; });
        taskflow.emplace([&sum]() { sum += 20; });
        taskflow.emplace([&sum]() { sum += 12; });

        executor.run(taskflow).wait();
        co_return sum.load();
    };

    auto result = syncAwait(bridge_test());
    REQUIRE(result == 42);
}
```

### 验证命令

```bash
cd build
cmake .. -DAGENTICDSL_BUILD_TESTS=ON
make -j$(nproc) test_async_bridge
./tests/test_async_bridge
# 预期：3 个测试全部通过
```

### 风险与缓解

| 风险 | 可能性 | 缓解 |
|------|:------:|------|
| async_simple C++20 兼容性问题 | 中 | 确认编译器 >= GCC 11 / Clang 14 |
| async_simple CMake 配置复杂 | 中 | 参考其 CI 配置，逐项排查 |
| Taskflow 符号冲突 | 低 | `tf::` namespace 隔离 |
| git submodule 网络问题 | 低 | 备选：下载 release tarball |

---

## 三、Phase 0 Track 0.1 — 云端 LLM 集成

### 目标

实现 CloudLLMAdapter（HTTP 后端），支持 DeepSeek/OpenAI 兼容 API。
遵循 `docs/agenticdsl/implementation/phase-0-implementation.md` 的架构路线：
标记 `ILLMAdapter` deprecated → 统一配置 → 创建云端适配器 → 路由器。

### 前提

- Pre-Phase 完成
- 现有 cpp-httplib 可用

### 任务列表

| # | 任务 | 文件 | 操作 | 验证标准 |
|---|------|------|:----:|---------|
| M1.1 | 统一配置结构 | `src/common/llm/llm_config.h` | **新建** | 编译通过 |
| M1.2 | 标记 ILLMAdapter deprecated | `src/common/llm/llm_adapter.h` | 修改 | `[[deprecated]]` 编译警告 |
| M1.3 | 更新 llm_types.h | `src/common/llm/llm_types.h` | 修改 | 导入统一配置 |
| M1.4 | CloudLLMAdapter 头文件 | `src/common/llm/cloud_adapter.h` | 新建 | 编译通过 |
| M1.5 | CloudLLMAdapter 实现 | `src/common/llm/cloud_adapter.cpp` | 新建 | mock 测试通过 |
| M1.6 | SSE 流式解析器 | `src/common/llm/sse_stream.h` | **新建** | 解析示例数据 |
| M1.7 | SSE 流式解析器实现 | `src/common/llm/sse_stream.cpp` | **新建** | 流式解析测试通过 |
| M1.8 | MockLLMProvider 头文件 | `src/common/llm/mock_provider.h` | 新建 | 编译通过 |
| M1.9 | MockLLMProvider 实现 | `src/common/llm/mock_provider.cpp` | 新建 | 支持预设响应 |
| M2.1 | llm_config 多模型配置 | `llm_config.json` | 修改 | 支持多模型声明 |
| M3.1 | CloudLLM 单元测试 | `tests/test_cloud_llm.cpp` | 新建 | mock 模式通过 |
| M3.2 | SSE 解析测试 | `tests/test_sse_stream.cpp` | 新建 | 验证流式解析 |
| M3.3 | 集成测试（可选） | `tests/test_cloud_llm_live.cpp` | 新建 | `--live` 模式通过 |

### 关键代码骨架

```cpp
// src/common/llm/llm_config.h（新建，统一配置结构）
#pragma once
#include <string>
#include <vector>
#include <optional>

namespace agenticdsl {

/// 统一 LLM 配置结构（合并 LLMConfig + LLMParams）
struct LLMConfig {
    // === 连接配置 ===
    std::string provider = "openai";
    std::string api_url = "http://localhost:8080";
    std::string api_endpoint = "/v1/chat/completions";
    std::string api_key;

    // === 模型配置 ===
    std::string model = "gpt-3.5-turbo";
    int n_ctx = 2048;

    // === 采样参数 ===
    float temperature = 0.7f;
    float top_p = 0.95f;
    int max_tokens = 2048;  // Track 0.1 M1.3: 默认从 512 调整为 2048
    std::vector<std::string> stop_tokens;
};

} // namespace agenticdsl
```

```cpp
// src/common/llm/cloud_adapter.h
#pragma once
#include "llm_config.h"
#include "llm_adapter.h"  // ILLMAdapter（将标记为 deprecated）

namespace agenticdsl {

/// 云端 LLM 适配器（OpenAI/DeepSeek 兼容 API）
class CloudLLMAdapter : public ILLMAdapter {
public:
    explicit CloudLLMAdapter(LLMConfig config);

    LLMResult generate(const std::string& prompt,
                       const LLMConfig& params = {}) override;
    bool is_available() const override;
    std::string name() const override;

private:
    LLMConfig config_;
    std::string call_api(const std::string& body);
};

} // namespace agenticdsl
```

```cpp
// src/common/llm/mock_provider.h
#pragma once
#include "llm_adapter.h"
#include <queue>

namespace agenticdsl {

/// 离线开发和 CI 测试用的 Mock Provider
class MockLLMProvider : public ILLMAdapter {
public:
    void enqueue_response(const std::string& content);
    void set_fixed_response(const std::string& content);

    LLMResult generate(const std::string& prompt,
                       const LLMConfig& params = {}) override;
    bool is_available() const override { return true; }
    std::string name() const override { return "mock"; }

    const std::vector<std::string>& call_history() const { return history_; }

private:
    std::queue<std::string> response_queue_;
    std::string fixed_response_;
    std::vector<std::string> history_;
};

} // namespace agenticdsl
```

### 验证命令

```bash
cd build && cmake .. -DAGENTICDSL_BUILD_TESTS=ON && make -j$(nproc)
./tests/test_cloud_llm          # mock 模式测试
./tests/test_sse_stream         # SSE 流式解析测试
# 可选（需 API key）：
# DEEPSEEK_API_KEY=xxx ./tests/test_cloud_llm_live
```

### CMake 配置

在 `CMakeLists.txt`（根）的 `agenticdsl_common` 中添加新源文件：

```cmake
add_library(agenticdsl_common STATIC
    # ... 现有文件 ...
    src/common/llm/cloud_adapter.cpp      # 新增
    src/common/llm/sse_stream.cpp         # 新增
    src/common/llm/mock_provider.cpp      # 新增
)

# 可选编译选项（无 API key 环境可禁用 Cloud LLM）
option(AGENTICDSL_BUILD_CLOUD_LLM "Build cloud LLM adapter" ON)
if(AGENTICDSL_BUILD_CLOUD_LLM)
    # 上述源文件已包含
endif()
```

---

## 四、Phase 0 Track 0.3 — 最小契约层

### 目标

实现 IInteractionBus MVP（std::mutex + std::queue），为三层调用链提供基础消息传递。

### 前提

- Pre-Phase 完成

> **IInteractionBus 后端升级时间点**: 当前 Phase 0 用 `std::mutex` + `std::queue`。
> 当 Phase 2 ADR-0002 V2 的 `InMemoryEventBus`（MPMC 有界队列）就绪后，
> `InMemoryBus` 的内部实现将从 `std::mutex` + `std::queue` 切换为 `EventBus` 后端。
> **接口 `IInteractionBus` 不变**，所有上层代码（CognitiveWorker、NodeExecutor、TUI）无需修改。
>
> 具体切换时机：Phase 2 ADR-0002 V2 P1 完成后，修改 `InMemoryBus` 构造函数使其接受 `IEventBus&` 参数。

### 任务列表

> **C1 实际交付路径（2026-06-08）**：公共头文件已迁移到 `include/agenticdsl/contract/`，
> 实现文件保留在 `src/common/contract/`（CMakeLists.txt 同样保留）。
> `events.h` 中间类型抽象（M5.2）在实际实施中**简化跳过**——直接用 `ToolResult` 作为
> `emit` 载荷，避免不必要的类型层级。

| # | 任务 | 文件 | 操作 | 验证标准 | 实际状态 |
|---|------|------|:----:|---------|:--------:|
| M5.1 | Contract 库 CMake | `src/common/contract/CMakeLists.txt` | **新建** | 静态库 `agenticdsl_contract` | [x] |
| M5.2 | IInteractionBus 接口 | `include/agenticdsl/contract/iinteraction_bus.h` | **新建** | 编译通过 | [x] |
| M5.3 | 事件类型定义 | ~~`src/common/contract/event_types.h`~~ | — | 简化跳过 | — |
| M5.4 | InMemoryBus 头文件 | `include/agenticdsl/contract/inmemory_bus.h` | **新建** | 编译通过 | [x] |
| M5.5 | InMemoryBus 实现 | `src/common/contract/inmemory_bus.cpp` | **新建** | 单元测试通过 | [x] |
| M5.6 | ToolResult 标准化 | `src/core/types/tool_result.h` | **新建** | 编译通过 | [x] |
| M6.1 | InMemoryBus 单元测试 | `tests/test_interaction_bus.cpp` | **新建** | emit/subscribe 工作 | [x] |
| M6.2 | 多线程安全测试 | `tests/test_interaction_bus.cpp` | 扩展 | 并发 emit 无死锁 | [x] |
| M6.3 | ToolResult 测试 | `tests/test_tool_result.cpp` | **新建** | 序列化/反序列化 | [x] |

### 关键代码骨架

```cpp
// include/agenticdsl/contract/iinteraction_bus.h（C1 实际位置）
#pragma once
#include "core/types/tool_result.h"

#include <cstddef>
#include <functional>
#include <string>

namespace agenticdsl {

/// 交互总线接口（MVP: mutex + queue）
/// 三阶段演进：Phase 0(mutex) → Phase 1(Engine集成) → Phase 2(EventBus后端)
class IInteractionBus {
public:
    virtual ~IInteractionBus() = default;
    virtual void emit(const std::string& event_type,
                      const ToolResult& payload) = 0;
    virtual size_t subscribe(
        const std::string& event_type,
        std::function<void(const ToolResult&)> callback) = 0;
    virtual void unsubscribe(size_t token) = 0;
};

} // namespace agenticdsl
```

```cpp
// src/core/types/tool_result.h（C1 实际实现）
#pragma once
#include <nlohmann/json.hpp>
#include <string>

namespace agenticdsl {

/// 统一工具调用结果（C1 MVP 实际实现）
struct ToolResult {
    bool ok = false;
    nlohmann::json data;    // 成功时的输出（nlohmann::json 而非 std::string）
    nlohmann::json meta;    // 错误时含 error_code / error_message；成功时含 tool_name 等

    static ToolResult success(nlohmann::json d, nlohmann::json m = {});
    static ToolResult error(std::string code, std::string msg, nlohmann::json m = {});

    nlohmann::json to_json() const;
    static ToolResult from_json(const nlohmann::json& j);
};

} // namespace agenticdsl
```

### CMake 配置

```cmake
# src/common/contract/CMakeLists.txt（新建）
add_library(agenticdsl_contract STATIC
    inmemory_bus.cpp
)
target_link_libraries(agenticdsl_contract PUBLIC agenticdsl_includes)

# CMakeLists.txt（根）添加：
add_subdirectory(src/common/contract)
target_link_libraries(agenticdsl_core PUBLIC agenticdsl_contract)
```

### 验证命令

```bash
cd build && cmake .. -DAGENTICDSL_BUILD_TESTS=ON && make -j$(nproc)
./tests/test_interaction_bus
./tests/test_tool_result
```

---

## 五、Phase 0 Track 0.2 — 三层调用链

### 目标

实现 ModelRegistry + SimpleCognitiveOrchestrator + 端到端示例，验证基座→认知→领域三层链路。

### 前提

- Track 0.1 完成（CloudLLMAdapter / MockLLMProvider 可用）
- Pre-Phase 完成（ICognitiveOrchestrator 接口可继承）

### 任务列表

| # | 任务 | 文件 | 操作 | 验证标准 |
|---|------|------|:----:|---------|
| M4.1 | IModelRouter 接口 | `src/common/llm/imodel_router.h` | **新建** | 编译通过 | 移交 Phase 1 |
| M4.2 | ModelRegistry 头文件 | `src/common/llm/model_registry.h` | **新建** | 注册/查询工作 | 移交 Phase 1 |
| M4.3 | ModelRegistry 实现 | `src/common/llm/model_registry.cpp` | **新建** | 单元测试通过 | 移交 Phase 1 |
| M4.4 | DefaultModelRouter 头文件 | `src/common/llm/default_router.h` | **新建** | 按任务类型路由 | 移交 Phase 1 |
| M4.5 | DefaultModelRouter 实现 | `src/common/llm/default_router.cpp` | **新建** | 测试通过 | 移交 Phase 1 |
| M4.6 | SimpleCognitiveOrchestrator 头文件 | `include/agenticdsl/cognitive/simple_orchestrator.h` | **新建** | 继承接口 | [x] |
| M4.7 | SimpleCognitiveOrchestrator 实现 | `src/modules/cognitive/simple_orchestrator.cpp` | **新建** | 单轮调用链通过 | [x] |
| M4.8 | 端到端示例 main.cpp | `examples/slice_01_tool_call/main.cpp` | **新建** | 可运行 | [x] |
| M4.9 | 端到端示例 CMake | `examples/slice_01_tool_call/CMakeLists.txt` | **新建** | 构建配置 | [x] |

> **变更说明**: 与初版计划相比，移除了 `src/domains/` 目录和 `workflow.agent.md`。示例工具在 `main.cpp` 中通过 `engine.register_tool()` 内联注册，避免新增架构层。DSL 文件的加载依赖 parser 模块，MVP 阶段用 C++ 直接注册更高效。

### 关键验证场景

```
输入: 用户消息 "读取 README.md 的内容"
预期流转:
  1. 基座收到消息 → 调用 ICognitiveOrchestrator.process()
  2. SimpleCognitiveOrchestrator 调用 MockLLM → 意图识别
  3. MockLLM 返回 tool_call: code::read_file(path="README.md")
  4. ToolRegistry.call("code::read_file", {path:"README.md"})
  5. 工具执行 → 返回文件内容
  6. on_complete({success:true, output:"# HydraForge..."})

错误路径验证:
  7. LLM 超时 → orchestrator 返回 ExecutionResult{success:false, error:"LLM timeout"}
  8. Tool 不存在 → ToolRegistry 抛异常 → 返回 ExecutionResult{success:false, error:"tool not found"}
```

### CMake 配置

> **C1 实际实现**：cognitive 模块使用 `src/modules/cognitive/` 路径（与现有 8 个
> 模块同级），库目标名为 `agenticdsl_modules_cognitive`（与现有 `agenticdsl_modules_*`
> 命名一致），`include/agenticdsl/cognitive/` 为公共头搜索路径（通过根
> `agenticdsl_includes` INTERFACE 库自动包含）。

```cmake
# src/modules/cognitive/CMakeLists.txt（实际位置）
add_library(agenticdsl_modules_cognitive STATIC
    simple_orchestrator.cpp
)
target_link_libraries(agenticdsl_modules_cognitive PUBLIC
    agenticdsl_includes
    agenticdsl_common
    agenticdsl_core
)

# CMakeLists.txt（根）添加：
add_subdirectory(src/modules/cognitive)
target_link_libraries(agenticdsl_core PUBLIC agenticdsl_modules_cognitive)
```

### 关键代码骨架

> **C1 实际实现路径（2026-06-08）**：cognitive 模块位于 `src/modules/cognitive/`，
> 头文件位于 `include/agenticdsl/cognitive/`。`SimpleCognitiveOrchestrator` 不再继承
> `ICognitiveOrchestrator`（Pre-Phase 接口）——其回调载荷为 `ToolResult` 而非
> `ExecutionResult`，定位为 MVP/B 轨道层。`interrupt` / `on_mode_changed` 属于
> Pre-Phase 接口的 Phase 1+ 高层方法，当前未实现。

```cpp
// include/agenticdsl/cognitive/simple_orchestrator.h
#pragma once
#include "common/tools/registry.h"
#include "common/llm/llm_types.h"
#include "core/types/tool_result.h"

namespace agenticdsl {

class SimpleCognitiveOrchestrator {
public:
    // Phase 1 预留：构造时允许 nullptr（MVP 允许依赖缺失）
    explicit SimpleCognitiveOrchestrator(
        ToolRegistry* registry = nullptr,
        ILLMProvider* llm = nullptr);

    void process(const std::string& session_id,
                 std::function<void(ToolResult)> on_complete);

private:
    ToolRegistry* registry_;
    ILLMProvider* llm_;
    ToolResult react_once(const std::string& user_prompt);  // 单轮 ReAct（MVP）
};

} // namespace agenticdsl
```

```cpp
// examples/slice_01_tool_call/main.cpp
#include "core/engine.h"
#include "common/llm/mock_provider.h"
#include "cognitive/simple_orchestrator.h"
#include <iostream>
#include <filesystem>

namespace fs = std::filesystem;

int main(int argc, char* argv[]) {
    bool use_mock = (argc > 1 && std::string(argv[1]) == "--mock");

    // 1. 创建引擎
    auto engine = agenticdsl::DSLEngine::from_file("workflow.agent.md");

    // 2. 注册工具
    engine->register_tool("code::read_file",
        [](const std::string& path) -> std::string {
            std::ifstream file(path);
            if (!file) return "ERROR: file not found";
            return std::string((std::istreambuf_iterator<char>(file)),
                                std::istreambuf_iterator<char>());
        });

    // 3. 设置 LLM（mock 或真实）
    std::unique_ptr<agenticdsl::ILLMAdapter> llm;
    if (use_mock) {
        auto mock = std::make_unique<agenticdsl::MockLLMProvider>();
        mock->set_fixed_response(
            R"({"tool": "code::read_file", "args": {"path": "README.md"}})");
        llm = std::move(mock);
    } else {
        agenticdsl::LLMConfig cfg;
        cfg.provider = "deepseek";
        cfg.api_key = std::getenv("DEEPSEEK_API_KEY");
        llm = std::make_unique<agenticdsl::CloudLLMAdapter>(cfg);
    }

    // 4. 执行
    agenticdsl::SimpleCognitiveOrchestrator orchestrator(*llm, engine->get_tool_registry());
    orchestrator.process("session_1", [](agenticdsl::ExecutionResult result) {
        if (result.success) {
            std::cout << "[Output] " << result.message << std::endl;
        } else {
            std::cerr << "[Error] " << result.message << std::endl;
        }
    });

    return 0;
}
```

### 验证命令

```bash
cd build && cmake .. -DAGENTICDSL_BUILD_TESTS=ON && make -j$(nproc)

# 单元测试
./tests/test_model_registry
./tests/test_default_router

# 端到端示例（mock 模式）
./examples/slice_01_tool_call/slice_01_tool_call --mock
# 预期输出：三层调用链完整执行，打印 README.md 内容
```

---

## 六、完成标准总览

| 阶段 | 完成标准 | 工期 |
|------|---------|------|
| Pre-Phase | 3 个头文件编译通过 + CMake include 生效 | 0.5 天 |
| Slice 00 | `[async][stage0]` 3 个测试通过 | 1-2 天 |
| Track 0.1 | `[cloud_llm]` + `[sse_stream]` 测试通过 | 3-4 天 |
| Track 0.3 | `[interaction_bus]` + `[tool_result]` 测试通过 | 2-3 天 |
| Track 0.2 | `examples/slice_01_tool_call --mock` 输出正确 | 5-7 天 |
| **Phase 0 总计** | 全部通过 | **7-10 天** |

### 通用完成标准（每个 Track）

| 标准 | 说明 |
|------|------|
| ✅ 编译通过 | `make -j$(nproc)` 无错误 |
| ✅ 单元测试全绿 | 各 `test_<模块>` 二进制全部 pass |
| ✅ 可运行示例 | `examples/slice_01_tool_call` 可执行并输出正确 |
| ✅ LSP 诊断清洁 | 新增文件无 error / warning |
| ✅ 错误路径覆盖 | 至少覆盖 2 种异常场景（超时、无效输入等） |
| ✅ 无 MVP 残留 | 无 `TODO(mvp)` 标记残留（仅 SimpleCognitiveOrchestrator 允许） |
| ✅ 新目录 CMake 存在 | 每个新 `src/` 子目录有对应的 `CMakeLists.txt` |

---

## 七、后续衔接

Phase 0 完成后，解锁以下工作：

- **Phase 1**（智能体层增强）：DSLEngine 集成 bus、NodeExecutor token push
- **Phase 2**（异步架构）：复用 Slice 00 成果，实现 AsyncRuntime + EventBus
- **Phase 3**（安全与策略）：实现三种 Policy + ToolCoordinator 中间件

### SimpleCognitiveOrchestrator 替换计划

```
Phase 0:  SimpleCognitiveOrchestrator（硬编码 ReAct）  ← MVP
Phase 4:  CognitiveOrchestrator（IPER 闭环）           ← 正式实现
Phase 4.5: 删除 SimpleCognitiveOrchestrator             ← 清理
```

---

## 附录 A：与 Roadmap 编号对照

| 本计划编号 | Roadmap 编号 | 说明 |
|-----------|-------------|------|
| P0.0-P0.4 | Pre-Phase | 新增 P0.0 目录创建 |
| S0.1-S0.6 | Slice 00 | 完全对应 |
| M1.1-M1.9 | Track 0.1 Step 0.1.1-0.1.2 | M1.2 改用 deprecated |
| M2.1 | Track 0.1 Step 0.1.3 | llm_config.json |
| M3.1-M3.3 | Track 0.1 验证 | 对应测试 |
| M4.1-M4.9 | Track 0.2 | 移除了 workflow.agent.md |
| M5.1-M5.6 | Track 0.3 Step 0.3.1 | Contract 库 |
| M6.1-M6.3 | Track 0.3 Step 0.3.2 | ToolResult |

---

## 附录 B：目录结构变更预览

```
HydraForge/
├── include/                          ← 新增
│   └── agenticdsl/
│       ├── cognitive/
│       │   └── icognitive_orchestrator.h    (P0.1)
│       ├── policy/
│       │   └── iexecution_policy.h          (P0.2)
│       └── types/
│           └── session_fwd.h                (P0.3)
├── external/
│   ├── taskflow/                     ← 新增 (S0.1)
│   └── async_simple/                 ← 新增 (S0.2)
├── src/
│   ├── common/
│   │   ├── llm/
│   │   │   ├── llm_config.h               (M1.1, 新建)
│   │   │   ├── cloud_adapter.h/cpp        (M1.4-M1.5, 新建)
│   │   │   ├── sse_stream.h/cpp           (M1.6-M1.7, 新建)
│   │   │   ├── mock_provider.h/cpp        (M1.8-M1.9, 新建)
│   │   │   ├── imodel_router.h            (M4.1, 新建)
│   │   │   ├── model_registry.h/cpp       (M4.2-M4.3, 新建)
│   │   │   └── default_router.h/cpp       (M4.4-M4.5, 新建)
│   │   └── contract/                 ← 新增目录
│   │       ├── CMakeLists.txt              (M5.1, 新建)
│   │       ├── iinteraction_bus.h          (M5.2, 新建)
│   │       ├── event_types.h               (M5.3, 新建)
│   │       └── inmemory_bus.h/cpp          (M5.4-M5.5, 新建)
│   ├── cognitive/                    ← 新增目录
│   │   ├── CMakeLists.txt                  (M4.6, 新建)
│   │   ├── simple_orchestrator.h           (M4.6, 新建)
│   │   └── simple_orchestrator.cpp         (M4.7, 新建)
│   └── core/types/
│       └── tool_result.h                   (M5.6, 新建)
├── examples/
│   └── slice_01_tool_call/           ← 新增
│       ├── CMakeLists.txt                  (M4.9, 新建)
│       └── main.cpp                        (M4.8, 新建)
└── tests/
    ├── test_async_bridge.cpp              (S0.6, 新建)
    ├── test_cloud_llm.cpp                 (M3.1, 新建)
    ├── test_sse_stream.cpp                (M3.2, 新建)
    ├── test_interaction_bus.cpp           (M6.1-M6.2, 新建)
    ├── test_tool_result.cpp               (M6.3, 新建)
    ├── test_model_registry.cpp            (M4.x, 新建)
    └── test_default_router.cpp            (M4.x, 新建)
```
