# ADR-0056: WebAssembly Agent 运行时

## 状态

✅ Approved (2026-07-16, 架构评审确认)

## 领域

Agent-as-Plugin 架构 / Wasm 运行时

## 关联

- [ADR-0055 — SKILL.md 执行与隔离模型](./adr-0055-skill-isolation.md) — 共享 capability 模型
- [ADR-0053 — AgentDescriptor 与 pdk_register_agent](./adr-0053-agent-descriptor-interface.md) — Agent 形态支持 Wasm
- [ADR-0052 — Agent Plugin Manifest](./adr-0052-agent-plugin-manifest.md) — manifest 中的 `implementation_forms: ["wasm"]`
- [ADR-0004 — ToolRegistry Security](../adr-0004-toolregistry-security.md) — 权限校验

## 背景

### 问题

Wasm 是 Agent 进化路径的最终阶段：
- **SKILL.md** → 原型阶段（解释执行）
- **.agent.md** → 生产阶段（编译执行）
- **C++** → 性能阶段（原生）
- **Wasm** → 可移植阶段（强隔离 + 跨平台）

但 HydraForge 当前没有 Wasm 运行时支持。需要定义：
1. 如何将 Agent 编译为 Wasm？（两条路径：DSL→Wasm 和 C++→Wasm）
2. OS 如何加载和执行 Wasm Agent？
3. Wasm 的 capability 模型如何设计？

### 目标

定义 Wasm Agent 的运行时模型，包括编译路径、加载执行、capability 限制。

## 决策

### 决策 1 — Wasm 来源：v1 只支持 C++ → Wasm

**两条路径，v1 先做一条**：

```
路径 1: C++ → Wasm（v1 支持）
  C++ PDK 代码 → wasi-sdk / Emscripten → .wasm
  性能接近原生，适合 AgenticDSL 计算图节点

路径 2: DSL → Wasm（Phase 2）
  .agent.md → DSL Compiler → .wasm (embedded interpreter)
  保留图结构，可在 Wasm 内解释执行
```

**理由**：
- DSL → Wasm 编译器需要设计 DSL bytecode 格式，投入大
- C++ → Wasm 工具链成熟（wasi-sdk / Emscripten）
- v1 可以先验证 Wasm Runtime 的正确性，Phase 2 再补 DSL 路径

### 决策 2 — 运行时选型：WAMR

| 候选 | 体积 | AOT 支持 | C++ 集成 | 边缘部署 | 选型 |
|------|:----:|:--------:|:--------:|:--------:|:----:|
| **WAMR** | ~100KB | ✅ | ✅ 好 | ✅ | ✅ **v1 推荐** |
| Wasmtime | ~5MB | ✅ | ✅ 中 | ❌ 较重 | — |
| Wasmer | ~3MB | ✅ | ❌ 中 | ❌ 较重 | — |
| v8 | ~30MB | ❌ | ❌ 差 | ❌ | — |

**WAMR 优势**：
- 体积最小（100KB vs Wasmtime 5MB），适合边缘部署
- C++ 原生 API，集成成本低
- 支持 AOT 编译（.wasm → .aot 加速 2-5x）
- Intel 赞助的开源项目

### 决策 3 — Capability 模型（与 SKILL 共享）

```cpp
// 与 ADR-0055 共享同一 capability 结构
struct AgentCapability {
    std::vector<std::string> allowed_tools;
    std::vector<std::string> allowed_topics;
    uint32_t max_steps;
    std::chrono::milliseconds timeout_ms;
    double budget_limit_usd;
    bool allow_llm;
    
    // Wasm 特有
    std::vector<std::string> allowed_host_functions;  // host function 白名单
};
```

**默认 host functions（所有 Wasm Agent 可用）**：

```cpp
// 基础（默认可用）
"host_call_tool"
"host_emit_event"
"host_consume_budget"
"host_log"

// 扩展（需要 capability 声明）
"host_read_file"     // manifest 中声明 allow_fs
"host_write_file"    // manifest 中声明 allow_fs
"host_http_request"  // manifest 中声明 allow_network
```

**与 SKILL 的关键区别**：
- Wasm 的 host function 是**静态编译时绑定**（wasm-import）
- SKILL 的 host function 是**运行时解释器动态注入**
- 但最终的有效 capability 计算逻辑相同（交集原则）

### 决策 4 — Wasm 模块注册与执行

```cpp
// include/agenticdsl/runtime/wasm_runtime.h
namespace hydraforge {

class WasmRuntime {
public:
    WasmRuntime();
    ~WasmRuntime();
    
    /// 加载 .wasm 模块
    /// @param wasm_path  .wasm 文件路径
    /// @param caps       capability 限制
    /// @return           模块句柄
    WasmModuleHandle load(
        const std::string& wasm_path, 
        const AgentCapability& caps
    );
    
    /// 调用入口函数
    /// @param mod    模块句柄
    /// @param func   函数名（如 "agent_run"）
    /// @param args   JSON 参数
    /// @return       JSON 结果
    nlohmann::json invoke(
        WasmModuleHandle mod,
        const std::string& func,
        const nlohmann::json& args
    );
    
    /// 卸载模块
    void unload(WasmModuleHandle mod);

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace hydraforge
```

**加载流程**：

```
WasmRuntime::load("wasm/code_review.wasm", caps):
  1. 读取 .wasm 文件
  2. 校验 Wasm 版本和 magic header
  3. WAMR 实例化模块
  4. 绑定 host function（按 caps.allowed_host_functions）
  5. 注入 capability token（模块内只读）
  6. 返回 module handle

WasmRuntime::invoke(mod, "agent_run", args):
  1. 查找函数
  2. 序列化 args → JSON string
  3. 调用 wasm 函数
  4. 反序列化 result → JSON
  5. 返回结果
```

### 决策 5 — Wasm 与 PDK 的关系

```
Wasm Agent 也是 PDK Plugin：

pdk/code_review_agent/
├── CMakeLists.txt                  # 编译为 .so（C++ 入口）或 .wasm
├── pdk_manifest.json               # manifest
│   └── implementation_forms: ["wasm"]
├── wasm/
│   └── code_review.wasm            # Wasm 二进制
├── src/
│   └── pdk_entry.cpp               # 加载 wasm/code_review.wasm → 注册
└── config/
    └── default.json
```

**Wasm Plugin 的 C++ 入口**（薄胶水层）：

```cpp
// pdk_entry.cpp
extern "C" void pdk_register_tools(hydraforge::IToolRegistry& registry) {
    registry.register_tool_function(
        "code_review/run",
        ToolMetadata{...},
        [](const auto& args) -> nlohmann::json {
            static WasmRuntime runtime;
            static auto mod = runtime.load("wasm/code_review.wasm", caps);
            return runtime.invoke(mod, "agent_run", args);
        }
    );
}
```

## 替代方案

### 方案 A：只用 Wasm，不用进程隔离

**否决理由**：
- Wasm 只能隔离 Agent 的执行代码，不能隔离 Agent 的 host function 调用
- 进程隔离是**安全隔离**，Wasm 是**内存隔离**，两者互补
- SKILL 阶段需要进程隔离（ADR-0055），Wasm 阶段可以复用但不需要双重隔离

### 方案 B：只支持 DSL → Wasm，不支持 C++ → Wasm

**否决理由**：
- DSL → Wasm 编译器需要大量投入（DSL bytecode 格式、解释器引擎）
- C++ → Wasm 工具链成熟，v1 可快速验证
- 性能关键 Agent 需要 C++ → Wasm 路径

### 方案 C：运行时选 Wasmtime

**否决理由**：
- 5MB 体积对于边缘部署过大
- WAMR 在嵌入式设备上有更广泛的支持

## 不变量

- Wasm Agent 的 capability 不可超过其 `AgentDescriptor` 声明的范围
- Wasm 模块之间相互隔离（WAMR 实例化是隔离的）
- host function 调用失败不导致 OS 崩溃（异常捕获）
- Wasm 模块超时由 OS 的 `WasmRuntime` 管理（WAMR 支持 interrupt）

## 权衡

| 决策 | 选择 | 理由 |
|------|------|------|
| Wasm 来源 v1 | 仅 C++ → Wasm | 工具链成熟 |
| 运行时 | WAMR | 最小体积，最好 C++ 集成 |
| Capability 模型 | 与 SKILL 共享 | 统一安全模型 |
| Host function | 静态绑定 | Wasm import 机制 |
| Plugin 关系 | C++ 胶水层 | 保持 PDK 一致性 |

## 后续行动

- 集成 WAMR 到第三方依赖（`external/wasm-micro-runtime/`）
- 实现 `WasmRuntime` 类（load/invoke/unload）
- 添加 `tests/test_wasm_runtime.cpp`
- 添加 wasi-sdk 构建示例（`examples/agent_wasm/`）
- Phase 2: DSL → Wasm 编译器（`.agent.md` → `.wasm`）

## 参考

- [ADR-0055 — SKILL.md 执行与隔离模型](./adr-0055-skill-isolation.md)
- [ADR-0053 — AgentDescriptor 与 pdk_register_agent](./adr-0053-agent-descriptor-interface.md)
- [ADR-0052 — Agent Plugin Manifest](./adr-0052-agent-plugin-manifest.md)
- WAMR 项目: `github.com/bytecodealliance/wasm-micro-runtime`
- wasi-sdk: `github.com/WebAssembly/wasi-sdk`
- Emscripten: `emscripten.org`