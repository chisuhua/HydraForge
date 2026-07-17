# ADR-0061-05: wasi-sdk 集成 + C++ → Wasm CI

**日期**: 2026-07-16
**状态**: ✅ Approved (P1, 父 ADR-0061 拆分)
**父 ADR**: [../adr-0061-agent-evolution-and-solidification.md](../adr-0061-agent-evolution-and-solidification.md)

---

## 背景

`docs/architecture/agent-evolution-pipeline.md` 阶段 4 要求将 C++ 编译为 Wasm。wasi-sdk 是工业事实标准工具链。本 ADR 负责 wasi-sdk 集成与 CI 流水线。

## 决策

### 决策 1 — wasi-sdk 工具链

```bash
# 安装
wasi-sdk-installer.sh https://github.com/WebAssembly/wasi-sdk/releases/latest

# 编译
wasi-sdk-clang++ \
  -O3 -flto \
  --target=wasm32-wasi \
  -o wasm/agent.wasm \
  src/agent.cpp
```

### 决策 2 — CMake 集成

```cmake
# pdk/{agent}/CMakeLists.txt
if(AGENTICDSL_BUILD_WASM)
  add_executable(agent_wasm src/agent.cpp)
  set_target_properties(agent_wasm PROPERTIES
    SUFFIX ".wasm"
    COMPILE_FLAGS "-O3 -flto --target=wasm32-wasi"
  )
endif()
```

### 决策 3 — CI 流水线

```yaml
# .github/workflows/wasm-build.yml
name: Wasm Build
on: [push, pull_request]
jobs:
  build-wasm:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v4
      - name: Install wasi-sdk
        run: ./scripts/install-wasi-sdk.sh
      - name: Build all PDK plugins as Wasm
        run: cmake --build build --target wasm-all
      - name: Verify Wasm loads via WasmRuntime
        run: ctest -R wasm_runtime
```

### 决策 4 — 与 ADR-0056 集成

编译产物通过 `WasmRuntime` 加载（见 ADR-0056）。Wasm → .wasm 产物包含：

```cpp
// pdk/code_review/wasm/code_review.wasm
extern "C" int agent_run(const char* args_json, char* result, int max_len) {
    // 入口函数（与 .agent.md 入口对应）
    ...
}
```

## 实施

- 工具: wasi-sdk 18+ (Linux/macOS/Windows)
- 文件: `scripts/install-wasi-sdk.sh`, `cmake/wasm-toolchain.cmake`
- 工作量: 2 weeks
- 优先级: P1

## 参考

- wasi-sdk: https://github.com/WebAssembly/wasi-sdk
- WasmEdge: https://wasmedge.org/
- [ADR-0056-wasm-runtime](./adr-0056-wasm-runtime.md)