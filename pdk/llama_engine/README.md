# Llama Engine Plugin (`pdk/llama_engine/`)

> **C14 Phase 5 B2.1** | **STATUS**: 🟡 编码骨架就绪 (12 工具 + PLACEHOLDER stubs)
> **关联 OpenSpec**: `openspec/changes/phase5-llama-engine-plugin/`
> **关联决策**: `docs/adversarial-reviews/decisions-2026-07-07.md` (D1/D3/D5)
> **前置依赖**: C12 ✅ (YIELD/STREAM) + C13 ✅ (4 schema shipped) + TSan gate

---

## 插件概述

**Llama Engine Plugin** 是 HydraForge 的首个 PDK 推理引擎 plugin，遵循 ADR-0034 C7 Model Router 已 ship 的 plugin 范式。

### 注册工具清单 (12 个)

| 命名空间 | 工具 | 类型 | 审批策略 |
|---------|------|------|---------|
| `inference/engine/` | `init` | ReadOnly | yolo only |
| `inference/engine/` | `generate` | Execute | agent + plan |
| `inference/engine/` | `stream` | Execute | agent + plan |
| `inference/engine/` | `status` | ReadOnly | yolo only |
| `inference/model/` | `load` | Execute | agent + plan |
| `inference/model/` | `unload` | Execute | agent only |
| `inference/model/` | `list` | ReadOnly | yolo only |
| `inference/model/` | `switch` | Execute | agent + plan |
| `prefix_cache` | `configure` | ReadOnly (Cognitive) | plan only |
| `kv_cache` | `configure` | ReadOnly (Cognitive) | plan only |
| `decoding` | `configure` | ReadOnly (Cognitive) | plan only |
| `cloud_engine` | `configure` | ReadOnly (Cognitive) | plan only (PLACEHOLDER) |

### 与 C12 YIELD/STREAM 集成

- `inference/engine/stream` 通过 `IGenerationStream` 接口与 C12 YIELD 节点集成
- 流式 token 推送通过 `run_stream_to_bus(bridge)` 委托给 IInteractionBus

### 与 C13 架构 schema 的关系

- C13 定义 `lib/inference/{prefix_cache,kv_cache,decoding,cloud_engine}.md` schema 契约
- C14 本 plugin 注册对应的 4 个 C++ 工具 (Oracle 审查 P0 阻塞项修复)

---

## 文件结构

```
pdk/llama_engine/
├── CMakeLists.txt                  # 编译配置 (产出 libhydraforge_llama_engine.so)
├── README.md                       # 本文件
└── src/
    ├── llama_engine_entry.cpp      # 入口 (pdk_register_tools + pdk_plugin_info)
    ├── llama_engine.cpp            # 4 engine 工具注册
    ├── llama_model.cpp             # 4 model 工具注册
    └── inference_arch.cpp          # 4 C13 架构工具注册
```

---

## 构建

```bash
# 构建前提: llama.cpp 可用 (通过 $LLAMA_CPP_INCLUDE_DIR / $LLAMA_CPP_LIB_DIR 指定)
cmake --preset debug -DLLAMA_CPP_INCLUDE_DIR=/path/to/llama.cpp -DLLAMA_CPP_LIB_DIR=/path/to/llama.cpp/build
cmake --build build/debug --target hydraforge_llama_engine -j$(nproc)

# 产物: build/debug/pdk/llama_engine/libhydraforge_llama_engine.so
```

## 加载

```cpp
// DSLEngine 构造后显式加载 (D5 Option B: 删除默认注入)
auto engine = DSLEngine::from_markdown(dsl_source);
engine.load_plugin("pdk/llama_engine");  // dlopen libhydraforge_llama_engine.so

// 或通过 PluginLoader 直接加载:
auto& loader = PluginLoader::instance();
loader.load("pdk/llama_engine");
```

## ABI 版本策略

| 版本 | 变更 | 日期 |
|------|------|------|
| 1.0.0 | 初始 ship: 12 工具注册 | 2026-07-07 |

ABI 版本号与 `hydraforge::CURRENT_ABI_VERSION` 对齐。

---

## D1/D3/D5 决策跟踪

| 决策 | 描述 | 本 plugin 状态 |
|:---:|---|:---:|
| **D1** | SamplerStrategy PDK 接口删除 | ✅ 已应用: `inference/decoding/configure` 采样器 clamp 逻辑内联 |
| **D3** | 工具命名统一 `inference.*` | ✅ 已应用: 8 个 engine/model 工具使用 `inference/engine/*` / `inference/model/*` |
| **D5** | 删除默认注入 + 显式 load_plugin | ✅ 已应用: 本 plugin 不依赖 DSLEngine 默认注入，通过 `load_plugin()` 显式加载 |

---

## 实施状态

- [x] 目录结构 + CMakeLists.txt
- [x] 入口文件 + 12 个工具骨架 (PLACEHOLDER stubs)
- [ ] llama.cpp API 调用填充 (需完整 C++/llama.cpp 开发环境)
- [ ] 12 个测试 (tests/test_llama_engine_plugin.cpp)
- [ ] CI 集成 (ctest + ASan + TSan)

## 下次更新

C14 编码 session 完成 llama.cpp API 填充后，删除 PLACEHOLDER stubs，升级为完整 ship 状态。