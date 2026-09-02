# Plugin 与 Agent 架构指南

> **目的**：快速理解 HydraForge 中 **Plugin（PDK 动态库）** 与 **Agent（智能体循环）** 的设计架构与构建方式。
> **范围**：覆盖 PDK Plugin 构建、Agent 双视角（PDK 静态 / Runtime 动态）、关键基础设施。
> **依据**：ADR-0021/0022/0041（Plugin）、ADR-0020/0060/0080/0081/0082（Agent）。

---

## 一、Plugin 定义

**Plugin = 用 PDK (Plugin Development Kit) 编译的 `.so` 动态库**，通过 `PluginLoader` 在运行时 `dlopen` 加载。

### 1.1 三层抽象

| 层 | 抽象 | 位置 |
|---|---|---|
| **元数据层** | `hydraforge::PluginInfo` POD | `include/agenticdsl/plugin/plugin_info.h` |
| **入口层** | `extern "C"` 符号 `pdk_register_tools` | 每个 plugin 的 `src/pdk_entry.cpp` |
| **运行时层** | `hydraforge::PluginLoader` | `include/agenticdsl/plugin/plugin_loader.h` |

### 1.2 Plugin 必须导出的 C 符号

```cpp
// 1. 插件元数据（ABI 版本自检）
extern "C" const hydraforge::PluginInfo pdk_plugin_info = {
    hydraforge::CURRENT_ABI_VERSION, // ABI 版本（当前 = 2）
    "tool.fs",                          // plugin 唯一 ID（≤64 字符）
    0, 1, 0,                            // semver
    "FS Tools - ...",                   // description
    "file_read,file_write,...",         // capabilities（逗号分隔，≤512）
    ""                                  // dependencies（plugin 名列表）
};

// 2. 工具注册入口（Phase 5+ 可选 pdk_plugin_init/fini/pdk_create_llm_provider）
extern "C" void pdk_register_tools(::agenticdsl::IToolRegistry& registry) {
    // 通过 registry.register_tool_function() 注册 N 个工具
}
```

### 1.3 Plugin Manifest（Phase 6a manifest-first 加载）

`pdk_manifest.json`（与 `.so` 同目录）：

```json
{
  "$schema": "https://schemas.hydraforge.io/pdk-manifest-v1.json",
  "id": "tool.fs",
  "name": "FS Tools",
  "abi_version": 2,
  "min_host_version": "0.3.0",
  "interface_versions": ["IAgentV1"],
  "implementation_forms": ["cpp"],
  "provided_tools": ["fs/read", "fs/write", "fs/list", "fs/exists"],
  "capabilities": ["file_read", "file_write"],
  "requires_isolation": false,
  "publisher": "hydraforge-team",
  "trust_level": "high"
}
```

### 1.4 ToolMetadata V2 安全模型（ADR-0004）

每个工具注册时声明元数据，决定 **审批策略 + Layer 权限**：

```cpp
::agenticdsl::ToolMetadata{
    .name = "fs/read",
    .description = "Read file content",
    .domain = "fs",
    .category = ::agenticdsl::ToolCategory::ReadOnly,    // 5 类：ReadOnly/WriteFile/Execute/Network/StateModify
    .min_layer = ::agenticdsl::LayerProfile::Workflow,   // 3 层：Cognitive/Thinking/Workflow
    .approval = ::agenticdsl::ApprovalPolicy{
        .requires_approval_in_plan = false,
        .requires_approval_in_agent = false,
        .requires_approval_in_yolo = false,
        .force_approval_always = false
    },
    .allowed_layers = {::agenticdsl::LayerProfile::Workflow}
}
```

---

## 二、构建 Plugin（两种风格）

### 2.1 方式 A：传统手写风格（最灵活，参考 `pdk/fs_tools/`）

**目录结构**：

```
pdk/my_plugin/
├── CMakeLists.txt
├── pdk_manifest.json      (可选, Phase 6a+)
└── src/
    └── pdk_entry.cpp
```

**pdk_entry.cpp 模板**（精简自 `pdk/fs_tools/src/pdk_entry.cpp`）：

```cpp
#include <agenticdsl/contract/itool_registry.h>
#include <agenticdsl/plugin/plugin_info.h>
#include <nlohmann/json.hpp>
#include <unordered_map>
#include <string>

extern "C" const hydraforge::PluginInfo pdk_plugin_info = {
    hydraforge::CURRENT_ABI_VERSION, "my.plugin", 0, 1, 0,
    "My Plugin - ...", "cap_a,cap_b", ""
};

extern "C" void pdk_register_tools(::agenticdsl::IToolRegistry& registry) {
    registry.register_tool_function(
        "my/echo",
        ::agenticdsl::ToolMetadata{
            "my/echo", "Echo input", "my",
            ::agenticdsl::ToolCategory::ReadOnly,
            ::agenticdsl::LayerProfile::Workflow,
            ::agenticdsl::ApprovalPolicy{false,false,false,false},
            {::agenticdsl::LayerProfile::Workflow}
        },
        [](const std::unordered_map<std::string,std::string>& args) -> nlohmann::json {
            auto it = args.find("text");
            std::string s = (it != args.end()) ? it->second : "";
            return {{"echo", s}};
        }
    );
}
```

**CMakeLists.txt 模板**（参考 `pdk/fs_tools/CMakeLists.txt`）：

```cmake
cmake_minimum_required(VERSION 3.20)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_library(MyPlugin SHARED src/pdk_entry.cpp)

target_include_directories(MyPlugin PRIVATE
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/src
    ${PROJECT_SOURCE_DIR}/external/nlohmann_json/single_include)

target_link_libraries(MyPlugin PRIVATE hydraforge_pdk)

set_target_properties(MyPlugin PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/pdk/my_plugin")
install(TARGETS MyPlugin LIBRARY DESTINATION lib/pdk/my_plugin)
```

**注册到根构建**：在 `pdk/CMakeLists.txt` 末尾加：

```cmake
add_subdirectory(my_plugin)
```

### 2.2 方式 B：DECLARE_TOOL 宏脚手架（Sprint 4 起的官方推荐）

**统一入口**：

```cpp
#include <agenticdsl/pdk/pdk.h>    // 拉入 tool_macros + agent_macros + safe_exec
using namespace hydraforge::pdk;
```

**DECLARE_TOOL 宏**（C6 升级版，4 参数 + 元数据）：

```cpp
DECLARE_TOOL(echo,                       // 工具名 → 自动生成 tool_spec_echo + tool_handler_echo
             "Echo user input",          // 描述
             ReadOnly,                   // ToolCategory
             "agent",                    // ApprovalPolicy: always/yolo/plan/agent
    // ↓ body 必须含 return 语句（宏自动包 try-catch）
    auto text = __pdk_args.value("text", std::string{});
    return nlohmann::json{{"echo", text}};
)
```

**宏展开为**（展开后等价手写版）：

```cpp
inline ::hydraforge::pdk::ToolSpec tool_spec_echo = {
    "echo", "Echo user input", {}, {},
    ::agenticdsl::ToolMetadata{
        "echo", "Echo user input", "plugin",
        ::agenticdsl::ToolCategory::ReadOnly,
        ::agenticdsl::LayerProfile::Workflow,
        make_approval("agent")   // → ApprovalPolicy{true,true,false,false}
    }
};
inline nlohmann::json tool_handler_echo(const nlohmann::json& __pdk_args) {
    try {
        // user body
    } catch (const std::exception& __pdk_e) {
        return nlohmann::json{{"error", __pdk_e.what()}};
    }
}
```

**DECLARE_TOOL_V3**（D4 自动 JSON Schema 生成）：

```cpp
struct EchoArgs { std::string text; };
struct EchoOutput { std::string echo; int len; };

DECLARE_TOOL_V3(echo, "Echo", ReadOnly, "agent",
    EchoArgs, EchoOutput,                // 自动生成 JSON Schema 2020-12
    auto a = __pdk_args.get<EchoArgs>();
    return EchoOutput{a.text, (int)a.text.size()};
)
```

### 2.3 SafeExec 沙箱执行（推荐嵌入 DECLARE_TOOL）

`hydraforge::pdk::SafeExec` —— 超时 + 协同取消 + 异常透传：

```cpp
DECLARE_TOOL(my_tool, "示例", ReadOnly, "agent",
    return SafeExec()
        .with_timeout(2s)
        .with_grace_period(50ms)         // 默认 50ms
        .run([&] {
            return __pdk_args["input"].get<std::string>();
        });
)
```

**行为**：
- 超时（`std::runtime_error`）→ caller 在 `≤ timeout + grace` 内立即返回（不再阻塞至 fn 完成）
- 协同取消：worker 可通过 `std::stop_token` 检查 `stop_requested()` 主动退出
- 异常：原类型与消息完整透传

### 2.4 Plugin 加载流程

```cpp
// 方式 1：DSLEngine::load_plugin()
DSLEngine engine;
engine.load_plugin("pdk/fs_tools/libFSTools.so");  // C14 D5 公开方法

// 方式 2：PluginLoader（推荐用于批量）
hydraforge::PluginLoader loader;
size_t n = loader.load_all(registry);  // 扫描所有搜索路径
for (auto& info : loader.list_loaded()) {
    std::cerr << "loaded: " << info.name << "\n";
}
```

**加载顺序保证**（Phase 5 lifecycle）：
1. `dlopen` 拿 handle
2. `pdk_plugin_init`（可选）
3. `pdk_register_tools(registry)` 注入工具
4. `unload_plugin()`：释放 `shared_ptr<ILLMProvider>` → `pdk_plugin_fini` → `dlclose`

---

## 三、Agent 定义（双重视角）

**Agent 在项目中有两套互补的设计**：

| 视角 | 抽象 | 何时用 |
|---|---|---|
| **PDK Agent**（编译期） | `DEFINE_AGENT` 宏 + 3 种 Loop 类 | 业务侧 agent，开发期已知 |
| **Runtime Agent**（运行期） | `IAgent` / `IAgentRegistry` / `IAgentComposition` | Plugin 形式动态注册、subprocess 形态 |

### 3.1 PDK Agent 视角（ADR-0021 §3.2）

**`DEFINE_AGENT` 宏**（`include/agenticdsl/pdk/agent_macros.h`）：

```cpp
#include <agenticdsl/pdk/pdk.h>

DEFINE_AGENT(code_reviewer, AgentLoopType::React);      // 展开为 class code_reviewerAgent
DEFINE_AGENT(planner,       AgentLoopType::PlanExecute); // 规划 → 执行 → 验证
DEFINE_AGENT(parallel_ana,  AgentLoopType::ForkJoin);    // 并行分支 → 合并
```

**宏展开为**（核心结构）：

```cpp
class code_reviewerAgent {
public:
    using Loop = typename LoopDispatcher<AgentLoopType::React>::Type;  // = ReactLoop
    code_reviewerAgent(std::unique_ptr<agenticdsl::DSLEngine> engine,
                       std::shared_ptr<agenticdsl::IInteractionBus> bus)
        : loop_(std::move(engine), std::move(bus)) {}
    LoopResult run(const std::string& prompt) {
        return AgentRunner<AgentLoopType::React>::run(loop_, prompt);
    }
private:
    Loop loop_;
};
```

### 3.2 三种 Agent Loop 选择（ADR-0021 §3.2）

| Loop | 行为 | 适用场景 | 状态 |
|---|---|---|---|
| **`React`** | 思考 → 行动 → 观察 | 单 agent 工具调用、ReAct 模式 | ✅ Sprint 4 ship |
| **`PlanExecute`** | 规划 → 执行 → 验证 | 多步骤任务、规划验证 | ✅ Sprint 20 ship |
| **`ForkJoin`** | 并行分支 → 合并 | 多 worker 并行聚合 | ✅ Sprint 20 ship |

`AgentRunner<T>::run()` 统一处理入口适配：
- `React`：`run(prompt, ctx)` —— `prompt` 作用户输入
- `PlanExecute`：`run(goal, ctx)` —— `prompt` 作目标
- `ForkJoin`：`run(branches, ctx)` —— `prompt` 按逗号分割为 vector

### 3.3 Runtime Agent 视角（ADR-0080/0082 Approved）

**`IAgent` 接口**（`include/agenticdsl/contract/iagent_registry.h`）：

```cpp
class IAgent {
public:
    virtual ~IAgent() = default;
    virtual const std::string& name() const = 0;   // 类型标识 (e.g. "react-loop-v1")
    virtual const std::string& id()   const = 0;   // 实例 ID (per-engine 唯一)
};

struct AgentConfig {
    std::string instance_id;   // 空 → create() 自动生成
};

using AgentFactory = std::function<std::unique_ptr<IAgent>(const AgentConfig&)>;
```

**`IAgentRegistry` 注册表**（per-engine 注册粒度）：

```cpp
class IAgentRegistry {
public:
    virtual bool register_agent(const std::string& string_id, AgentFactory factory) = 0;
    virtual std::unique_ptr<IAgent> create(const std::string& string_id, const AgentConfig&) = 0;
    virtual bool   unregister(const std::string& string_id) = 0;
    virtual std::vector<std::string> list_registered() const = 0;
    virtual bool   is_registered(const std::string& string_id) const = 0;
    virtual size_t size() const = 0;
};

// 工厂
std::unique_ptr<IAgentRegistry> make_in_memory_agent_registry();
```

**`IAgentComposition` 编排模式**（ADR-0060 决策 4）：

```cpp
class IAgentComposition {
public:
    virtual AgentResult<std::string> call(const std::string& agent_id,
                                          const std::string& args,
                                          std::chrono::milliseconds timeout = 30s) = 0;
    virtual std::future<AgentResult<std::string>> call_async(
        const std::string& agent_id, const std::string& args,
        std::function<void(AgentResult<std::string>)> callback = nullptr,
        std::chrono::milliseconds timeout = 30s) = 0;
    virtual TaskHandle delegate(const std::string& agent_id,
                                const std::string& task,
                                const std::string& priority = "normal") = 0;
    virtual StreamHandle stream(const std::string& agent_id, const std::string& args);  // Phase 2
};
```

**`IAgentHookRegistry` Hook 系统**（ADR-0081 Approved）：

```cpp
// Pre-step: Continue / Deny / ModifyContext（fail-closed：deny 不可被后续覆盖）
// Post-step: modify_result + modified_output
class IAgentHookRegistry {
public:
    virtual void register_pre_hook(const std::string& agent_glob, AgentPreHook hook,
                                   int priority, HookErrorPolicy policy) = 0;
    virtual void register_post_hook(const std::string& agent_glob, AgentPostHook hook,
                                    int priority, HookErrorPolicy policy) = 0;
    virtual AgentPreHookResult apply_pre_hooks(const IAgent&, const std::string& step_input,
                                               std::vector<std::string>& warnings) const = 0;
    virtual AgentPostHookResult apply_post_hooks(const IAgent&, const std::string& step_output,
                                                 std::vector<std::string>& warnings) const = 0;
};
```

---

## 四、构建 Agent（三种路径）

### 4.1 路径 1：PDK `DEFINE_AGENT` 宏（推荐，开发期）

完整示例（来自 `pdk/README.md` AgentForge MVP 衔接）：

```cpp
#include <agenticdsl/pdk/pdk.h>
#include <core/engine.h>
using namespace hydraforge::pdk;

DEFINE_AGENT(code_reviewer, AgentLoopType::React);
DECLARE_TOOL(lint_file, "Linter", ReadOnly, "agent",
  return SafeExec().with_timeout(5s).run([&] {
    return lint(__pdk_args["file"].get<std::string>());
  });
)

int main() {
    auto engine = agenticdsl::DSLEngine::from_markdown("workflow.agent.md");
    code_reviewerAgent agent(std::move(engine), std::make_shared<InMemoryBus>());
    auto result = agent.run("review src/main.cpp");
    return result.final_context.working.empty() ? 0 : 1;
}
```

### 4.2 路径 2：Runtime Agent（动态注册）

实现自定义 `IAgent` + 通过 `IAgentRegistry` 注册：

```cpp
#include <agenticdsl/contract/iagent_registry.h>
#include <memory>

class MyAgent : public ::agenticdsl::IAgent {
public:
    explicit MyAgent(std::string id) : id_(std::move(id)) {}
    const std::string& name() const override {
        static const std::string n = "my-agent-v1";
        return n;
    }
    const std::string& id() const override { return id_; }
private:
    std::string id_;
};

int main() {
    auto registry = ::agenticdsl::make_in_memory_agent_registry();

    // 注册 factory
    registry->register_agent("my-agent-v1", [](const ::agenticdsl::AgentConfig& cfg) {
        return std::make_unique<MyAgent>(cfg.instance_id);
    });

    // 创建实例
    auto agent = registry->create("my-agent-v1", ::agenticdsl::AgentConfig{"inst-001"});

    // 通过 IAgentComposition 编排
    auto composition = ::agenticdsl::make_agent_composition(registry);
    auto result = composition->call("my-agent-v1", "do something", 10s);
    return result.ok ? 0 : 1;
}
```

### 4.3 路径 3：作为 Plugin 加载（含 LLM 推理）

参考 `pdk/loop_agent/src/pdk_entry.cpp` —— 注册 `loop/run` 工具，加载 `.agent.md` DSL 文件 + 父引擎 LLM provider：

```cpp
extern "C" void pdk_register_tools(::agenticdsl::IToolRegistry& registry) {
    registry.register_tool_function(
        "loop/run",
        ::agenticdsl::ToolMetadata{ /* ... */ },
        [](const auto& args) -> nlohmann::json {
            std::string loop_type = str_arg(args, "loop_type", "react");
            std::string prompt    = str_arg(args, "prompt");

            // 加载 .agent.md 文件
            auto agent_content = load_agent_file(loop_type);
            auto child = ::agenticdsl::DSLEngine::from_markdown(agent_content, *tls_parent_provider);

            // 注入 LLM tool bridge（让 DSL 的 llm_call 节点复用父 provider）
            child->register_llm_tool("llama-default",
                std::make_unique<ProviderLLMTool>(*tls_parent_provider, cancellation_token));

            ::agenticdsl::LayeredContext ctx;
            ctx.working["user_input"] = prompt;
            return child->run(ctx).final_context;
        }
    );
}
```

---

## 五、关键基础设施速查

| 组件 | 头文件 | 用途 |
|---|---|---|
| **PDK 统一入口** | `include/agenticdsl/pdk/pdk.h` | `#include` 拉入全部 PDK |
| **`DECLARE_TOOL`** | `pdk/tool_macros.h` | 工具注册脚手架（5 行领域逻辑） |
| **`DEFINE_AGENT`** | `pdk/agent_macros.h` | Agent 循环脚手架（3 种 Loop） |
| **`SafeExec`** | `pdk/safe_exec.h` | 超时 + 协同取消（替代 `std::async`） |
| **`PluginLoader`** | `agenticdsl/plugin/plugin_loader.h` | `dlopen` + ABI 检查 + 路径白名单 |
| **`PluginInfo`** | `agenticdsl/plugin/plugin_info.h` | POD 元数据（abi_version/name/capabilities） |
| **`IToolRegistry`** | `agenticdsl/contract/itool_registry.h` | 工具注册表抽象 |
| **`IAgentRegistry`** | `agenticdsl/contract/iagent_registry.h` | Agent 第一类注册（ADR-0082） |
| **`IAgentComposition`** | `agenticdsl/contract/iagent_composition.h` | call / call_async / delegate / stream |
| **`IAgentHookRegistry`** | `agenticdsl/contract/iagent_hook_registry.h` | pre/post-step hook（ADR-0081） |
| **`LayeredContext`** | `agenticdsl/types/layered_context.h` | 5 层结构化上下文（L1-L5） |
| **`IInteractionBus`** | `agenticdsl/contract/iinteraction_bus.h` | 事件总线 |
| **`DSLEngine::from_markdown`** | `core/engine.h` | 加载 `.agent.md` 工作流 |

---

## 六、构建命令

```bash
# 构建所有 PDK 插件
cmake --preset debug -B build/debug
cmake --build build/debug -j$(nproc)

# 构建单个 plugin target
cmake --build build/debug --target MyPlugin -j$(nproc)

# 测试
cmake --preset tests -B build/tests
cmake --build build/tests -j$(nproc)
cd build/tests && ctest --output-on-failure

# 加载路径优先级（PluginLoader）
# env var > ./plugins/ > ~/.hydraforge/plugins/ > /usr/local
```

**当前 ABI 版本 = 2**（per ADR-0041）。PluginLoader 支持 V1 + V2 双调度（向后兼容）。

---

## 七、Plugin vs Agent 对照表

| 维度 | Plugin | Agent |
|---|---|---|
| **形态** | 独立 `.so` 动态库 | C++ 类（编译期）或 `IAgent` 实例（运行期） |
| **构建方式** | `add_library(SHARED) + link hydraforge_pdk` | `DEFINE_AGENT` 宏 OR `class : IAgent` |
| **入口符号** | `pdk_plugin_info` + `pdk_register_tools` | `name()` + `id()` |
| **加载方式** | `DSLEngine::load_plugin()` / `PluginLoader::load_so()` | `IAgentRegistry::create(string_id)` |
| **元数据** | `PluginInfo` + `pdk_manifest.json` | `IAgent::name()` + `AgentConfig` |
| **隔离** | 默认进程内；subprocess 形态 Phase 2 | per-engine registry 隔离 |
| **核心头** | `include/agenticdsl/pdk/pdk.h` | `include/agenticdsl/contract/iagent_registry.h` |

**两个关键设计选择**：

1. **PDK 静态链接**（P3）：`hydraforge_pdk` 是 INTERFACE 库，编译时链接，避免运行时 symbol 冲突。
2. **Agent first-class**（ADR-0080/0082）：Agent 不再隐藏在 Tool 后面，独立 registry + composition + hook 三层正交。

---

## 八、参考 ADR / Specs

- **Plugin**：[ADR-0021 PDK 骨架](../../adr/adr-0021-pdk-skeleton.md)、[ADR-0022 PluginLoader](../../adr/adr-0022-plugin-loader.md)、[ADR-0041 Plugin ABI v2](../../adr/adr-0041-plugin-abi-v2.md)
- **Agent**：[ADR-0020 CognitiveWorker](../../adr/adr-0020-cognitive-worker.md)、[ADR-0060 Agent Composition Patterns](../../adr/adr-0060-agent-composition-patterns.md)、[ADR-0080 Agent v1.1](../../adr/adr-0080-agent-v1-1.md)、[ADR-0081 Agent Hook](../../adr/adr-0081-agent-hook.md)、[ADR-0082 Agent First-Class Registry](../../adr/adr-0082-agent-first-class-registry.md)
- **OpenSpec Changes**：`openspec/changes/2026-07-07-pdk-skeleton/`、`openspec/changes/pdk-plan-execute-fork-join/`、`openspec/changes/pdk-manifest-validation/`
- **实例参考**：
  - Plugin（传统手写）：`pdk/fs_tools/src/pdk_entry.cpp`、`pdk/shell_tools/`
  - Plugin（含 LLM 推理）：`pdk/llama_engine/`、`pdk/model_router/`、`pdk/loop_agent/`
  - PDK 集合示例：`pdk/README.md` AgentForge MVP 衔接段