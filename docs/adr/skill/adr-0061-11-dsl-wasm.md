# ADR-0061-11: DSL → Wasm Bytecode 编译器

**日期**: 2026-07-16
**状态**: 🔍 Proposed (P2, v2 候选)
**父 ADR**: [../adr-0061-agent-evolution-and-solidification.md](../adr-0061-agent-evolution-and-solidification.md)

---

## 背景

阶段 4 的 Wasm 路径目前只覆盖 C++ → Wasm（ADR-0061-05）。另一条路径是 DSL → Wasm bytecode（FLUX Runtime 风格），保留 .agent.md 图结构，在 Wasm 内解释执行。

## 决策

### 决策 1 — DSL Bytecode 设计

```
.opcode   type    operands              semantics
LOAD      const   str "system_prompt"  push const string
LOAD      var     str "user_input"      push var value
CALL      tool    str "fs/read"         call host function
BRANCH    cond    label                conditional jump
FORK      n       label×n              spawn n subtasks
JOIN      n                             join n subtasks
END       exit_code                    exit
```

### 决策 2 — 编译器实现

```cpp
class DSLToWasmCompiler {
    // 1. ParsedGraph → DSL Bytecode IR
    BytecodeModule compile_to_bytecode(const ParsedGraph& graph);
    
    // 2. Bytecode → Wasm
    std::vector<uint8_t> compile_bytecode_to_wasm(const BytecodeModule& m);
};
```

### 决策 3 — Wasm 嵌入解释器

```cpp
// WasmRuntime 加载 DSL bytecode
WasmModule mod = runtime.load(dsl_bytecode_path);
mod.instantiate();  // 内置 bytecode interpreter
mod.invoke("run", args_json);
```

### 决策 4 — 与 C++ → Wasm 路径对比

| 维度 | C++ → Wasm | DSL → Wasm |
|------|-----------|-----------|
| 性能 | 高（编译型） | 中（解释型） |
| 启动时间 | 慢（编译 + 链接） | 快（无编译） |
| 二进制大小 | 大（含 LLM stub 等） | 小（仅解释器） |
| 适用场景 | 性能敏感 | 快速分发/热更新 |

## 实施

- 依赖: [ADR-0061-06-trajectory-ir](./adr-0061-06-trajectory-ir.md), [ADR-0056-wasm-runtime](./adr-0056-wasm-runtime.md)
- 工作量: 4 weeks
- 优先级: P2

## 参考

- FLUX Runtime: github.com/SuperInstance/flux-runtime
- [ADR-0056-wasm-runtime](./adr-0056-wasm-runtime.md)