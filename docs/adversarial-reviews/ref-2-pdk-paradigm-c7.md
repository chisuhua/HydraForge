# 参考报告 2: C7 Model Router PDK Plugin 范式审查

> **来源**: explore agent (bg_54cd898e, 2m43s)
> **关联 session**: `ses_0cb0ff434ffeQORz3s79pF0I3q`
> **审查范围**: ADR-0034 Model Router (C7 Phase 1+2 完整 ship) + PDK 骨架 (Sprint 4) + PluginLoader (Sprint 5)
> **日期**: 2026-07-06

---

## 1. ADR-0034 实施回顾

### 1.1 4 个工具如何拆分到 .so?

**答案: 1 个 .so 注册 1 个 tool, 通过 `pdk_register_tools` 入口。**

| .so 文件名 | 工具名 | 类 |
|-----------|--------|-----|
| `libhydraforge_model_router_cost.so` | `model_router/cost` | `CostModelRouterPolicy` |
| `libhydraforge_model_router_quality.so` | `model_router/quality` | `QualityModelRouterPolicy` |
| `libhydraforge_model_router_latency.so` | `model_router/latency` | `LatencyModelRouterPolicy` |
| `libhydraforge_model_registry.so` | `model_router/registry` | 纯 lambda |

**关键设计点**:
- 工具名采用分层命名约定: `model_router/cost` → `/` 前缀避免 naming collision
- 每个 .so 导出 `extern "C" void pdk_register_tools(IToolRegistry&)` + `extern "C" const PluginInfo pdk_plugin_info`
- 每个 .so 精确注册 1 个 tool

### 1.2 pdk_plugin_info 格式 (POD C 风格)

```cpp
extern "C" const hydraforge::PluginInfo pdk_plugin_info = {
  hydraforge::CURRENT_ABI_VERSION,                    // abi_version = 1
  "hydraforge_model_router_cost",                     // name[64]
  1, 0, 0,                                            // major, minor, patch
  "CostModelRouter - lowest per_token_cost routing",  // description[256]
  "model_routing,cost"                                // capabilities[512]
};
```

**ABI 约束**: `PluginInfo` 是 POD (char[] 而非 std::string), 确保跨二进制 ABI 兼容。

### 1.3 CMakeLists 结构 (3 级嵌套)

```
pdk/CMakeLists.txt                          → add_subdirectory(model_router)
  pdk/model_router/CMakeLists.txt           → cost + quality + latency + registry
    cost_strategy/CMakeLists.txt             → SHARED library target
    quality_strategy/CMakeLists.txt          → SHARED library target
    latency_strategy/CMakeLists.txt          → SHARED library target
```

每个策略 CMake 结构一致:
```cmake
add_library(hydraforge_model_router_cost SHARED cost_router.cpp cost_router.h)
target_link_libraries(hydraforge_model_router_cost PRIVATE hydraforge_pdk)
target_compile_features(hydraforge_model_router_cost PRIVATE cxx_std_20)
```

---

## 2. C7 实施中的 5 个坑

### 坑 1: PluginLoader 销毁顺序 segfault

`examples/phase1_model_router_plugin/main.cpp` 注释明确:
> "声明顺序很重要 — PluginLoader 必须在 registry 之后声明, 这样销毁时 registry 先析构, 然后 PluginLoader 才 dlclose。颠倒顺序会导致 segfault。"

**教训**: 局部变量析构顺序是逆序。`ToolRegistry` 必须先于 `PluginLoader` 析构。B2 必须保持同一约束。

### 坑 2: DECLARE_TOOL 宏不支持含 `/` 的工具名

`model_registry.cpp` 注释:
> "使用 pdk_register_tools + ToolMetadata 模式, 而非 DECLARE_TOOL 宏 (宏的 `##name` 拼接不支持含 `/` 的字符串标识)。"

**教训**: B2 如果使用分层工具名 (如 `inference/engine`), 必须手动注册, 不能用 DECLARE_TOOL 宏。

### 坑 3: PluginLoader 路径白名单拒绝相对路径

`apply_path_whitelist()` 拒绝 `./plugins/` 前缀以外的相对路径。示例通过 CMake 宏注入绝对路径:
```cpp
#ifndef AGENTICDSL_EXAMPLE_BINARY_DIR
#define AGENTICDSL_EXAMPLE_BINARY_DIR "."
#endif
```

**教训**: B2 测试同样需注入路径。

### 坑 4: IToolRegistry 传递 `map<string,string>`, 非 JSON

所有 4 个 .cpp 手动实现 map→json 转换 (~15 行/处, 4 处重复)。应提取到共享头文件。

### 坑 5: Latency 策略复用 `budget_remaining` 字段作为 `max_latency`

`RoutingContext::budget_remaining` (本是 `optional<double>` 表示美元预算) 被 latency 策略误用作 `max_latency` (毫秒整数)。

**教训**: C7 最大设计妥协。B2 应为 `max_latency_ms` 增加独立字段。

---

## 3. B2 应避免的 5 个 cargo-culting 反模式

| # | 反模式 | C7 做法 | B2 正确做法 |
|---|--------|---------|------------|
| 1 | 每个 .so 注册 1 个 tool | 4 .so × 1 tool | Engine plugin 需注册 2-3 个 tool, 放 1 个 .so 更合理 |
| 2 | lambda 持有 long-lived state | `shared_ptr<CostModelRouterPolicy>` (无状态) | `struct EnginePluginState { map<string, ModelHandle> models; mutex mtx; }` |
| 3 | 复用 `RoutingContext` 的 `optional<double>` 字段 | `budget_remaining` 被 latency 误用 | B2 定义独立的 `InferenceRequest` 参数结构体 |
| 4 | 3 个策略 × 3 份 CMakeLists | 28 行/份, 代码膨胀 | 用 `function()` 或 `foreach` 批量生成 |
| 5 | 所有 Plugin 用 `model_router/` 前缀 | `model_router/cost` + `model_router/quality` | 保留 `inference/` 前缀, 但从领域动作命名 |

---

## 4. B2 vs C7 时间/复杂度估算 Diff

| 指标 | C7 (模型路由) | B2 (引擎/模型/batching) |
|------|:-----------:|:----------------------:|
| 实际 commit 数 | 11 (实现 6 + 文档 4 + 归档 1) | **预估 14-20** (+27%-82%) |
| 人天 | 2.5 天 | **预估 3.5-5 天** |
| PDK 接口新增 | +1 头文件 (`model_router.h`) | 预估 +2-3 头文件 |
| 新增 TEST_CASE | 15 个 | 预估 11 个 (3 plugin × 3-4 场景) |
| 新增依赖链接 | 仅 `hydraforge_pdk` | **需 `PRIVATE llama`** 或类似依赖 |

### B2 增加的原因
1. `IGenerationStream` 跨 .so `unique_ptr` 工厂 (C7 无)
2. Batching plugin 需要 `std::mutex` + 并发队列 (C7 零并发)
3. Engine plugin 需要 ModelHandle 生命周期管理 (C7 stateless)
4. 测试需要 mock model 或真实模型 (C7 仅 mock 数据)

### B2 减少的原因
1. CMake 可直接复制 C7 模板 (~节省 0.5 天)
2. `pdk_register_tools + pdk_plugin_info` 入口零改动 (~节省 0.3 天)
3. PluginLoader 已经就绪 (~节省 0.3 天)