# ADR-0061-12: 浏览器端 WebLLM 集成

**日期**: 2026-07-16
**状态**: 🔍 Proposed (P2, v2 候选)
**父 ADR**: [../adr-0061-agent-evolution-and-solidification.md](../adr-0061-agent-evolution-and-solidification.md)

---

## 背景

WebLLM (MLC.AI 2024-2026, arXiv:2412.15803)：WebGPU + WebAssembly 双栈，Apache TVM/MLC-LLM 编译 WGSL/WASM；Llama-3.1-8B 解码保持 native **71-80%**。

Mozilla 3W Stack (2025-08)：浏览器内多运行时（Rust/Go/Python-Pyodide/JS）每语言独立 worker + WASM runtime + WebLLM 实例，全离线运行。

HydraForge 的 Wasm 阶段 4 可以扩展到浏览器端运行。

## 决策

### 决策 1 — 浏览器端 LLM 选项

| 选项 | 大小 | 性能 |
|------|------|------|
| WebLLM + WebGPU | ~3GB 模型文件 | 71-80% native |
| WebLLM + WASM (fallback) | 同上 | 较慢 |
| Cloud fallback | 0 | native (网络依赖) |

### 决策 2 — 集成架构

```
[浏览器]
   ├── HydraForge Agent (Wasm)      ← 由 ADR-0061-11 编译产物
   ├── WebLLM Runtime                ← LLM 推理
   ├── 3W Worker Pool                 ← 多运行时并行
   └── localStorage / IndexedDB      ← 持久化
```

### 决策 3 — 应用场景

- **离线 Agent**：断网时仍可执行
- **隐私敏感**：数据不出浏览器
- **边缘设备**：Chromebook / iPad / Chromebook
- **VS Code Web / GitHub Codespaces**：云端 IDE 内嵌 Agent

### 决策 4 — 与 Server-side 的差异

| 维度 | Server | Browser |
|------|--------|---------|
| LLM | Claude/GPT-4 | WebLLM (1-8B) |
| 工具调用 | MCP/A2A | WASI host functions |
| 持久化 | SQLite/Redis | IndexedDB |
| 多 Agent | 全功能 | 受限（memory/CPU） |

## 实施

- 依赖: [ADR-0056-wasm-runtime](./adr-0056-wasm-runtime.md), [ADR-0061-11-dsl-wasm](./adr-0061-11-dsl-wasm.md)
- 工作量: 4 weeks
- 优先级: P2

## 参考

- WebLLM: arXiv:2412.15803, github.com/mlc-ai/web-llm
- Mozilla 3W Stack: blog.mozilla.ai/3w-for-in-browser-ai-webllm-wasm-webworkers/
- WasmEdge: wasmedge.org/