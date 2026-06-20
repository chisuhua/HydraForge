# ADR-0021: Plugin Development Kit (PDK) 设计

## 状态

**🟡 Partial (2026-06-19, Sprint 4 增量 ship)** — V0.1 MVP 版。DECLARE_TOOL 宏 + DEFINE_AGENT 模板 (React MVP) + SafeExec 封装 (超时+异常 MVP) 已实施 (5/5 ctest pass, 32/32 baseline 零回归)。monorepo `pdk/` 子目录先 ship (K3 决策, ADR-0021 §2.2 一致),`hydraforge-pdk` 独立仓库推送留 Sprint 4 ship 后异步 (T4b, 外部阻塞: GitHub 组织存在性)。

> **Sprint 4 增量 (2026-06-19, OpenSpec change `2026-07-07-pdk-skeleton`)**：PDK 头文件落地（`include/agenticdsl/pdk/{tool_macros,agent_macros,safe_exec,pdk}.h`）+ monorepo `pdk/` 子目录 + INTERFACE 库 (`hydraforge_pdk`)。5 个新测试 + 31 基线 = 32/32 ctest pass, P3 静态链接验证 (PDK 头文件仅依赖 Runtime 契约接口 `agenticdsl/contract/*.h`)。Phase 2/3 后续: PlanExecute/ForkJoin 完整循环 + FakeStateStore/StubLLM/MockSandbox 测试替身 + PluginLifecycle + 完整 SafeExec (fork/cgroups/seccomp) + `hydraforge-pdk` 独立仓库发布。Sprint 5 后续: PluginLoader 通过 PDK 编译的 `.so` 加载 (T4b 异步, Sprint 5 收官变 ✅ Approved)。

## 背景

### 问题

HydraForge 当前缺少领域智能体的标准化开发工具包：

| 维度 | 现状 | 问题 |
|------|------|------|
| **工具注册** | 手动 `engine->register_tool(name, callback)` | 无 Schema 声明，无权限元数据，无标准化错误处理 |
| **Agent 循环** | agent_basic 示例手写 ReAct 循环 | 每个领域插件重复实现意图理解/规划/执行/观察 |
| **沙箱执行** | 无封装 | 开发者需要手写 fork/cgroups/seccomp/chroot |
| **测试** | 依赖完整 Runtime 集成测试 | 插件无法独立测试，反馈周期长 |
| **文档** | `app-dev-guide-cpp.md` 744 行但无脚手架 | 指南繁琐但缺少可执行工具 |

### 现有文档中的插件概念

`docs/guides/app-dev-guide-cpp.md` 中已提及动态插件加载：
```
agentic register-tool --lib ./build/libmy_app.so
```

但**从未实现**。无 dlopen/dlsym 调用，无宏定义，无标准生命周期。

### 目标

为领域智能体开发者提供**标准化开发工具包 (PDK)**，使：

- 工具注册从 ~20 行样板降到 ~5 行领域逻辑
- Agent 循环从 ~1000 行降到 ~100 行回调填充
- 沙箱执行从 ~600 行降到 ~10 行声明式
- 插件可独立测试，无需完整 Runtime

---

## 决策

### 1. PDK 定位

```
┌─────────────────────────────────────────────────────────────────┐
│  HydraForge Runtime — 领域无关, 最小化, 稳定                       │
│  ├─ L0-L6 核心引擎                                               │
│  ├─ 通过契约接口暴露工具调用/状态/事件能力                          │
│  └─ 当前: ToolRegistry · DSLEngine · StateStore                  │
├─────────────────────────────────────────────────────────────────┤
│  Plugin Dev Kit (PDK) — 独立仓库, 可选依赖, 开发者工具包            │
│  ├─ 宏定义 · 模板库 · 测试替身 · CMake 生成器                     │
│  ├─ 静态链接到插件, 不增加 Runtime 负担                           │
│  └─ 独立版本演进, 向后兼容多个 Runtime 版本                        │
├─────────────────────────────────────────────────────────────────┤
│  Domain Plugin (领域插件) — 基于 PDK 开发                         │
│  ├─ 编程助手插件 (code::)                                         │
│  ├─ 浏览器插件 (browser::)                                        │
│  └─ 文件系统插件 (fs::)                                           │
└─────────────────────────────────────────────────────────────────┘
```

**关键原则**：

| 原则 | 内容 |
|------|------|
| **P1** | **PDK 不是 Runtime 的一部分** — 独立仓库 `hydraforge-pdk` |
| **P2** | **PDK 是可选依赖** — 高级开发者可手写，但强烈推荐 |
| **P3** | **PDK 静态链接到插件** — 编译时融入，Runtime 零感知、零负担 |
| **P4** | **PDK 只封装通用开发模式** — 不包含任何领域逻辑 |
| **P5** | **PDK 版本与 Runtime 解耦** — PDK 可独立升级 |
| **P6** | **PDK 提供测试替身** — 插件可独立测试，无需 Runtime |

### 2. PDK 核心组件

#### 2.1 组件全景

| 组件 | 功能 | MVP |
|------|------|-----|
| **Plugin Lifecycle** | `init()` / `load()` / `unload()` / `health_check()` | ✅ |
| **Tool Registration** | `DECLARE_TOOL()` / `REGISTER_SCHEMA()` / `SET_PERMISSIONS()` | ✅ |
| **Agent Loop Template** | `REACT_LOOP_TEMPLATE` / `PLAN_EXECUTE_TEMPLATE` | ✅ |
| **SafeExec** | 沙箱执行封装（MVP 跳过 seccomp） | ✅ |
| **Testing Mocks** | `MockSandbox` / `FakeStateStore` / `StubLLM` | ✅ |
| **State Wrapper** | `StateReader()` / `StateWriter()` / `NamespaceGuard()` | ✅ |
| **Logging & Tracing** | `StructuredLog()` / `SpanTracer()` | 🔜 Phase 2 |
| **Build Generator** | `cmake_init()` / `project_template()` | 🔜 Phase 2 |
| **Metrics** | `MetricsCollector()` | 🔜 Phase 3 |
| **Cost Tracker** | `CostTracker()` / `BudgetGuard()` | 🔜 Phase 3 |

#### 2.2 PDK 仓库结构

```
hydraforge-pdk/                  # 独立仓库
├── CMakeLists.txt               # INTERFACE 库
├── include/hydraforge/pdk.h     # 统一入口
├── include/hydraforge/pdk/
│   ├── lifecycle.h              # PluginLifecycle
│   ├── tool_macros.h            # DECLARE_TOOL 宏
│   ├── tool_schema.h            # Schema 生成器
│   ├── agent_templates.h        # Agent Loop 模板
│   ├── safe_exec.h              # 沙箱执行封装
│   ├── state_access.h           # StateReader/Writer
│   ├── test_mocks.h             # MockSandbox / FakeStateStore / StubLLM
│   └── logging.h               # 结构化日志
├── examples/
│   ├── minimal/                  # 最小 PDK 使用示例
│   └── chat_agent/              # 基于 PDK 的聊天智能体
├── tests/
│   └── test_mocks.cpp           # 测试替身测试
└── README.md
```

#### 2.3 与现有示例对比

```
examples/agent_basic/            ← 现有: 纯手工, 无脚手架
    main.cpp               64 行 — tool_registry -> run() -> traces
    workflow.agent.md       — DSL 文件

examples/agent_chat/             ← ADR-0019: IInteractionBus + TUI
    main.cpp               — 连接基座
    chat_client.h/cpp       — 会话管理
    tui.h/cpp               — 界面渲染

examples/agent_chat_pdk/         ← PDK 最终形态: 基于脚手架
    main.cpp               — 10 行: import + run
    tools/my_tools.cpp      — 5 个 DECLARE_TOOL
    agent/my_agent.cpp      — 1 个 DEFINE_AGENT
```

### 3. 核心 API 设计

#### 3.1 DECLARE_TOOL 宏

当前方式 (20 行样板) 与 PDK 方式 (5 行领域逻辑) 的对比：

```cpp
// ——— 当前方式: 手动 (20行) ———
engine->register_tool("edit_file", [](const auto& args) {
    auto it = args.find("path");
    if (it == args.end())
        return nlohmann::json{{"error", "missing path"}};
    auto path = it->second;
    // ... 手动错误处理
    // ... 手动权限检查
    // ... 手动日志
    return nlohmann::json{{"success", true}};
});

// ——— PDK 方式: 宏 (5行领域逻辑) ———
// pdk/tool_macros.h
namespace hydraforge::pdk {

// PDK 工具注册宏
// 自动处理: Schema 生成 · 参数校验 · 权限检查 · 日志记录 · 错误捕获
#define DECLARE_TOOL(name, description, ...) \
    /* 展开为注册 + Schema + 权限声明 */

} // namespace hydraforge::pdk
```

**MVP 实现**：宏展开为 `ToolSpec` 结构体 + 注册函数，不依赖 Runtime 内部类型：

```cpp
// 使用 PDK 的领域插件代码
#include <hydraforge/pdk.h>

using namespace hydraforge::pdk;

// DECLARE_TOOL 展开为:
// 1. ToolSchema (name, description, params, permissions)
// 2. 注册函数 (pdk_register_tools_xxx)
// 3. 错误处理包装
DECLARE_TOOL(edit_file, "编辑文件", LayerProfile::Workflow) {
    PARAM(path, string, required);
    PARAM(content, string, optional);

    auto path = GET_PARAM(path);
    auto content = GET_PARAM(content);

    // 开发者只写领域逻辑
    std::ofstream file(*path);
    file << *content;

    RETURN_SUCCESS("Edited {}", *path);
}
```

#### 3.2 Agent Loop 模板

```cpp
// pdk/agent_templates.h
namespace hydraforge::pdk {

// 预定义的 Agent 循环模式
enum class AgentLoopType {
    React,        // 思考 → 行动 → 观察 → 重复
    PlanExecute,  // 规划 → 执行 → 验证 → 完成
    ForkJoin,     // 并行分支 → 合并结果
    Sequential    // 顺序执行
};

// 定义 Agent（自动生成循环调度 + 状态管理 + 错误恢复）
#define DEFINE_AGENT(name, loop_type) \
    /* 展开为 Agent 类 + 循环逻辑 + 异常处理 */

} // namespace hydraforge::pdk
```

**现有 agent_basic 示例的 ReAct 循环**（`examples/agent_basic/main.cpp` + 隐含在 DSL 中）将被 PDK 模板替代为：

```cpp
// PDK 方式: 100 行
DEFINE_AGENT(coding_assistant, REACT_LOOP_TEMPLATE) {

    ON_INTENT("code_review", [](const Intent& intent) {
        auto file = intent.get_param("file");
        return analyze_code(file);  // 只写领域逻辑
    });

    ON_INTENT("refactor", [](const Intent& intent) {
        auto file = intent.get_param("file");
        auto target = intent.get_param("target");
        return generate_refactor_plan(file, target);
    });

    // PDK 自动处理: 意图解析 → 规划 → 执行 → 观察 → 循环
}
```

#### 3.3 SafeExec 沙箱封装

```cpp
// pdk/safe_exec.h
namespace hydraforge::pdk {

// 声明式沙箱执行器
// MVP: 使用权限校验 + 超时，跳过 fork/seccomp
// Phase 2: 添加子进程隔离
class SafeExec {
public:
    SafeExec& with_cgroups(const CgroupConfig& cfg);
    SafeExec& with_seccomp(const std::vector<std::string>& allowed_syscalls);
    SafeExec& with_timeout(std::chrono::milliseconds timeout);
    SafeExec& with_readonly_paths(const std::vector<std::string>& paths);

    // 执行领域逻辑
    template<typename F>
    auto run(F&& fn) -> std::invoke_result_t<F>;

private:
    std::chrono::milliseconds timeout_{30000};
    // MVP 先跳过进程隔离，使用 ADR-0004 权限校验
};

} // namespace hydraforge::pdk
```

**MVP 实现**：`SafeExec` MVP 版本只做超时控制和异常捕获，不做进程隔离：

```cpp
template<typename F>
auto SafeExec::run(F&& fn) -> std::invoke_result_t<F> {
    auto future = std::async(std::launch::async, std::forward<F>(fn));
    auto status = future.wait_for(timeout_);
    if (status == std::future_status::timeout) {
        throw std::runtime_error("Tool execution timed out after " +
            std::to_string(timeout_.count()) + "ms");
    }
    return future.get();
}
```

#### 3.4 测试替身

```cpp
// pdk/test_mocks.h
namespace hydraforge::pdk::test {

// Mock 沙箱 — 不执行，直接返回预设结果
class MockSandbox {
public:
    MOCK_METHOD(SandboxResult, execute, (const std::string& tool, const nlohmann::json& args));
};

// Fake StateStore — 内存 KV，线程安全
class FakeStateStore {
    std::unordered_map<std::string, nlohmann::json> data_;
public:
    void write(const std::string& key, const nlohmann::json& value) { data_[key] = value; }
    nlohmann::json read(const std::string& key) { return data_.count(key) ? data_[key] : nlohmann::json{}; }
};

// Stub LLM — 返回预设响应，不调用模型
class StubLLM {
    std::string response_;
public:
    explicit StubLLM(std::string response) : response_(std::move(response)) {}
    std::string generate(const std::string&) { return response_; }
};

} // namespace hydraforge::pdk::test
```

### 4. PDK 与 Runtime 的依赖关系

```
PDK (静态库/头文件库)
    │
    ├── 依赖: Runtime 契约接口
    │   ├── IInteractionBus (ADR-0019)
    │   ├── ToolSchema 结构体
    │   ├── Event/Token 类型
    │   └── C++20 标准库
    │
    └── 不依赖: Runtime 内部实现
        ├── DSLEngine 实现
        ├── TopoScheduler
        ├── NodeExecutor
        └── 其他 internal 符号

Runtime
    │
    └── 不依赖 PDK
        ├── 运行时零 PDK 头文件
        ├── 运行时零 PDK 符号
        └── 通过契约接口与插件交互
```

### 5. 实施计划

| Phase | 任务 | 产出 |
|-------|------|------|
| **Phase 1** | `pdk.h` 统一入口<br>`DECLARE_TOOL` 宏<br>`ToolSchema` 结构体<br>`SafeExec` 基础版（超时+异常） | 工具注册脚手架 |
| **Phase 2** | `DEFINE_AGENT` 宏<br>ReAct 循环模板<br>`FakeStateStore` / `StubLLM` | Agent 循环脚手架 |
| **Phase 3** | `PluginLifecycle`<br>`SafeExec` 完整版（fork/seccomp）<br>`MockSandbox` | 完整 PDK |
| **Phase 4** | CMake 生成器<br>插件 .so 动态加载<br>独立仓库发布 | 生产就绪 |

### 6. 验证标准

| 标准 | 验证方法 |
|------|---------|
| `DECLARE_TOOL` 编译 | PDK 示例编译通过，无 Runtime 依赖 |
| PDK 独立测试 | `FakeStateStore` + `StubLLM` 测试工具逻辑，不启动 Runtime |
| 插件 .so 加载 | Runtime 加载 PDK 编译的插件，工具可调用 |
| 无 Runtime 膨胀 | `nm plugin.so | grep hydraforge_runtime` 返回空 |

---

## 替代方案

### 方案 A: 将 PDK 放入 Runtime

**否决理由**：
- 违反 Runtime 的"领域无关"原则
- 增加编译时间
- 版本演进耦合
- 插件开发者被迫理解整个 Runtime

### 方案 B: 不提供 PDK，开发者手写全部

**否决理由**：
- 当前 developer-guide 已是 744 行复杂文档
- 每个领域插件重复实现相同的错误处理/日志/权限/测试模式
- 插件质量参差不齐

### 方案 C: 使用外部脚本语言 (Python/lua) 生成代码

**否决理由**：
- 引入额外依赖
- 生成代码难以调试
- 与 C++ 20 Modules 理念不一致

---

## 权衡

| 决策 | 选择 | 理由 |
|------|------|------|
| **仓库** | 独立于 Runtime | 解耦版本演进，可选依赖 |
| **链接方式** | 静态链接到插件 | 零运行时开销 |
| **宏风格** | C++ 宏（非代码生成） | 编译时可见，可调试 |
| **沙箱 MVP** | 仅超时+异常 | 跳过进程级隔离复杂性 |
| **测试替身** | 头文件 mock | 无额外依赖 |

---

## 参考

- [ADR-0019: IInteractionBus 接口与 TUI Chat MVP](./adr-0019-iinteraction-bus-mvp.md)
- [ADR-0020: 多智能体线程模型与隔离策略](./adr-0020-thread-model-isolation.md)
- [ADR-0004: ToolRegistry 安全模型](./adr-0004-toolregistry-security.md)

---

## 附录 A: 文件变更清单

| 仓库 | 操作 | 路径 |
|------|------|------|
| `hydraforge-pdk` | 新建 | `CMakeLists.txt` |
| `hydraforge-pdk` | 新建 | `include/hydraforge/pdk.h` |
| `hydraforge-pdk` | 新建 | `include/hydraforge/pdk/tool_macros.h` |
| `hydraforge-pdk` | 新建 | `include/hydraforge/pdk/agent_templates.h` |
| `hydraforge-pdk` | 新建 | `include/hydraforge/pdk/safe_exec.h` |
| `hydraforge-pdk` | 新建 | `include/hydraforge/pdk/state_access.h` |
| `hydraforge-pdk` | 新建 | `include/hydraforge/pdk/test_mocks.h` |
| `hydraforge-pdk` | 新建 | `examples/minimal/` |
| `HydraForge` | 修改 | `docs/guides/app-dev-guide-cpp.md` — 增加 PDK 章节 |

## 附录 B: 与现有 ADR 的协作

| ADR | 与 PDK 的关系 |
|-----|-------------|
| ADR-0019 | PDK 的 `DECLARE_TOOL` 运行时使用 `IInteractionBus` 推送事件 |
| ADR-0020 | PDK 的 `DEFINE_AGENT` 模板内部使用 `CognitiveWorker` 模式 |
| ADR-0004 | PDK 的 `SafeExec` 调用 ADR-0004 的权限校验 |
