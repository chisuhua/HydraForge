# ADR-0041: PluginLoader 生命周期扩展 (pdk_plugin_init / fini 钩子)

## 状态

✅ Approved (2026-07-10 — OpenSpec change `phase5-illmprovider-call-chain-v2` (C16) ship, 含 C14 集成); **2026-07-06 renumber**: 兄弟 ADR-0036 → ADR-0045 (编排 plugin), ADR-0037 → ADR-0046 (通信协议), 避免与旧 ADR-0036-三层服务协议 / ADR-0037-因果序冲突

> **实施依据**: `phase5-illmprovider-call-chain-v2` (C16) 已 ship + archived (2026-07-09), 验证: PluginLoader V2 升级 — 5 符号查找 (`pdk_plugin_info` + `pdk_register_tools` + `pdk_create_llm_provider` + **`pdk_plugin_init`** + **`pdk_plugin_fini`**) + PluginInfo v2 ABI (`dependencies[256]` 字段) + dual ABI dispatch (向后兼容 v1) + 拓扑排序依赖加载 + lifecycle 钩子 pair-test。详见 `docs/superpowers/plans/2026-07-03-phase5-self-bootstrapping.md` §三 C16 行 + `openspec/changes/archive/2026-07-09-phase5-illmprovider-call-chain-v2/` + `tests/test_plugin_loader.cpp` (Sprint 17 C7 + Sprint 21 C16 累计)。

## 领域

基座 / PDK / Plugin Loading Lifecycle

## 关联

- [ADR-0022 (Plugin Loading)](./adr-0022-plugin-loading.md) — 当前 §1.1 定义 2 个必选 + 1 个可选 (pdk_create_llm_provider) 符号, 本 ADR 扩展 lifecycle 钩子
- [ADR-0021 (PDK Design) §2.1](file://./adr-0021-pdk-design.md) — Plugin Lifecycle ✅ MVP 标记, 但实施尚未覆盖所有 4 个钩子
- [ADR-0035 (Inference Engine Plugin Spec)](./adr-0035-inference-engine-plugin-spec.md) — 推理 Plugin 需要 GPU context 初始化 (有状态初始化)
- [ADR-0040 §5 (ABI Version Management)](./adr-0040-inference-plugin-build-strategy.md) — ABI 兼容性协议

---

## 背景

### 问题

ADR-0022 §1.1 当前导出 2 个必选 + 1 个可选符号:
- `pdk_plugin_info` (POD 元数据)
- `pdk_register_tools` (注册入口)
- `pdk_create_llm_provider` (可选,ILLMProvider 工厂)

但 PluginLoader **缺少 lifecycle 钩子**,导致以下场景无法标准处理:

1. **GPU/CUDA context 初始化**: 推理 Plugin 需在 `pdk_register_tools` 之前完成 CUDA context, `inference/engine/init` tool 调用时再做太晚(已注册工具但 engine 未 ready)
2. **线程池/资源预分配**: Plugin 拥有自己的 worker 线程,需 PluginLoader 卸载时 join
3. **依赖排序**: 多个 Plugin 加载顺序需声明 (eg. 推理 Plugin 必须先于编排 Plugin)
4. **健康检查**: `list_loaded` 报告 plugin 已 dlopen,但不一定 ready
5. **upgrade/shutdown 钩子**: 缺少 `on_unload` 回调,依赖 .so 卸载顺序

### 当前 workaround

- ADR-0035 §1.2 临时方案: 通过 `inference/engine/init` tool 在第一次调用时初始化 GPU
- 问题: 多线程并发首次调用导致 race condition; 依赖 Tree-sitter 风格的 lazy init 难以诊断

### 目标

定义 4 个 Plugin lifecycle 钩子 (init/start/stop/fini), 与 ADR-0021 §2.1 Plugin Lifecycle MVP 对齐。

---

## 决策

### 1. 4 个新增可选符号

在 ADR-0022 §1.1 既有 3 个符号基础上,增加 **2 个可选 lifecycle 符号**:

```cpp
// 符号 4 (可选): Plugin 初始化钩子 — PluginLoader::load_so 调用前可选调用
//   参数: const hydraforge::PluginInfo&
//   返回: bool (true=success, false=失败 → 拒绝加载此 plugin)
//   用途: 分配 GPU context, 预分配线程池, 加载 backend, 初始化全局 state
extern "C" bool pdk_plugin_init(const hydraforge::PluginInfo& self);

// 符号 5 (可选): Plugin 关闭钩子 — PluginLoader::unload_plugin 调用时同步触发
//   参数: 无 (Plugin 内部应保留 self 引用)
//   返回: void
//   用途: join 线程, 释放 GPU context, flush telemetry, emit 关闭事件
extern "C" void pdk_plugin_fini();
```

**完整的 5 个符号集** (P1 fix 同步修正 ADR-0022 §1.1):

| # | 必选/可选 | 符号 | 用途 |
|---|---------|-----|------|
| 1 | **必选** | `pdk_plugin_info` | POD 元数据 + ABI version |
| 2 | **必选** | `pdk_register_tools` | 工具注册入口 |
| 3 | **可选** | `pdk_create_llm_provider` | ILLMProvider 工厂 (LLM plugin) — **P0 fix ADDED 2026-07-06** |
| 4 | **可选** | `pdk_plugin_init` | Lifecycle 初始化钩子 (NEW) |
| 5 | **可选** | `pdk_plugin_fini` | Lifecycle 关闭钩子 (NEW) |

**符号查找** (dlsym): PluginLoader 用 `dlsym(handle, "pdk_plugin_init")` 探测, NULL 表示无此钩子。`pdk_plugin_init` 失败时 `load_so` 返回 false (与其他符号缺失不同, 这是 failure 而非 absence)。

#### 1.5 PluginInfo v2 ABI 协议 (P1 fix per Oracle review — BLOCKING)

**PluginInfo 结构扩展是 ABI breaking change**, 必须显式 bump ABI version,否则老/新组合损坏内存:

```cpp
// PluginInfo v1 (ADR-0022 §1.2 已 ship, sizeof = 848 bytes)
struct PluginInfoV1 {
  uint32_t abi_version;            // = 1
  char name[64];
  uint8_t major_version, minor_version, patch_version;
  char description[256];
  char capabilities[512];
};

// PluginInfo v2 (本 ADR 引入, sizeof = 848 + 256 = 1104 bytes)
struct PluginInfoV2 {
  uint32_t abi_version;            // = 2
  char name[64];
  uint8_t major_version, minor_version, patch_version;
  char description[256];
  char capabilities[512];
  char dependencies[256];           // NEW: comma-separated plugin names
};
```

**PluginLoader dual ABI dispatch** (实现细节):PluginLoader 维护 `PluginInfoV1` + `PluginInfoV2` 两个 POD 类型,根据 `abi_version` 字段 dispatch 读取。`info.abi_version==1` → 读 V1 POD;`info.abi_version==CURRENT_ABI_VERSION` (即 2) → 读 V2 POD。**严格校验** (per ADR-0022 §4 strict_version=true): `abi_version` 必须 ∈ {1, CURRENT_ABI_VERSION},否则拒绝加载。

**CURRENT_ABI_VERSION** (同步更新 ADR-0022):bump 至 2,与原 Sprint 5 ship 的 v1 后向兼容。

**向后兼容协议**:
- 新 PluginLoader (支持 v1+v2) 加载老 v1 .so → OK, `dependencies` 视为空
- 老 PluginLoader (仅 v1) 加载新 v2 .so → 拒绝 (read truncated V1 → 内存不匹配)
- 同版本加载 → OK

**同步更新位置**:
- [ADR-0022 §1.2](../adr-0022-plugin-loading.md) 增加 PluginInfoV2 定义
- [ADR-0022 §4](../adr-0022-plugin-loading.md) 增加 dual ABI dispatch 协议
- [ADR-0040 §5](../adr-0040-inference-plugin-build-strategy.md) 增加 "PluginInfo v2 → ABI v2" 协调说明

### 2. PluginLoader 加载/卸载时序

```cpp
// load_so (Sprint 5 既有, 扩展):
bool PluginLoader::load_so(const std::string& path,
                            IToolRegistry& registry,
                            bool strict_version = true) {
  // 1. dlopen (existing)
  void* handle = dlopen(path.c_str(), RTLD_NOW);
  if (!handle) return false;

  // 2. 读取 pdk_plugin_info + ABI check (existing)
  auto* info = dlsym(handle, "pdk_plugin_info");
  if (!check_compatibility(*info)) return false;

  // 3. P0 fix: 调用 pdk_plugin_init 钩子 (if present)
  auto init_fn = dlsym(handle, "pdk_plugin_init");
  if (init_fn) {
    if (!init_fn(*info)) {
      // init 失败 → unload (rollback)
      dlclose(handle);
      return false;
    }
    plugin->has_init = true;
  }

  // 4. 调用 pdk_register_tools (existing)
  auto reg_fn = dlsym(handle, "pdk_register_tools");
  reg_fn(registry);

  // 5. 可选: pdk_create_llm_provider (existing, P0 added)
  if (provider_fn = dlsym(handle, "pdk_create_llm_provider")) {
    plugin->llm_provider = provider_fn();
    plugin->has_llm_provider = true;
  }

  // 6. 存储 LoadedPlugin 记录 (extended)
  loaded_.push_back({handle, *info, path, has_init, has_llm_provider});
  return true;
}

// unload_plugin (Sprint 5 既有, 扩展):
bool PluginLoader::unload_plugin(const std::string& name) {
  auto it = find_loaded(name);
  if (it == end) return false;

  // 1. 释放所有外部引用 (caller responsibility)
  //    - DSLEngine 释放 shared_ptr<ILLMProvider>
  //    - bus unsubscribe token (编排 Plugin 内部)
  //    - 任何 framework 持有的 plugin 资源

  // 2. P0 fix: 调用 pdk_plugin_fini 钩子 (if has_init)
  if (it->has_init) {
    auto fini_fn = dlsym(it->handle, "pdk_plugin_fini");
    if (fini_fn) fini_fn();
  }

  // 3. dlclose (existing)
  dlclose(it->handle);

  loaded_.erase(it);
  return true;
}
```

### 3. init 钩子的具体场景

**推理 Plugin (AgenticLlama)**:

```cpp
extern "C" bool pdk_plugin_init(const hydraforge::PluginInfo& self) {
  return LlamaInferenceProvider::initialize_global_state();  // GGML backend init
}
extern "C" void pdk_plugin_fini() {
  LlamaInferenceProvider::shutdown_global_state();  // GGML backend free
}
```

**LLM Adapter Plugin (e.g. 自定义 OpenAI adapter)**:

```cpp
extern "C" bool pdk_plugin_init(const hydraforge::PluginInfo& self) {
  HttpClient::warm_pool(kDefaultPoolSize);  // HTTP connection pool 预热
  return true;
}
extern "C" void pdk_plugin_fini() {
  HttpClient::flush_pool();
}
```

**通用 utility plugin** (无 init/fini): 只导出符号 1+2, 不导出符号 4+5。PluginLoader 检测不到时不调用。

### 4. 加载顺序保证 (依赖声明)

**PluginInfo 扩展 (P1 fix)**:

```cpp
struct PluginInfo {
  uint32_t abi_version;
  char name[64];
  uint8_t major_version, minor_version, patch_version;
  char description[256];
  char capabilities[512];

  // P1 fix: 新增 dependencies 字段 (POD 字符数组, NULL-terminated)
  char dependencies[256];  // comma-separated list of plugin names
};
```

**示例**:

```cpp
// Orchestration Plugin 声明对推理 Plugin 的依赖
extern "C" const PluginInfo pdk_plugin_info = {
  CURRENT_ABI_VERSION, "orchestration_v1", 1, 0, 0,
  "Orchestration Plugin",
  "orchestration,router",
  "agenticllama_inference"  // depends on this plugin name (ADR-0022 §1.2 name[64])
};
```

**加载顺序算法** (P1 fix per Oracle review — BLOCKING):

PluginLoader **必须** 按 dependency-aware topological sort 加载 (强顺序,非仅警告)。此决策符合 [ADR-0035 §8 test #20](../adr-0035-inference-engine-plugin-spec.md) "Inference Plugin 必须先于 Orchestration Plugin 加载"。

```cpp
// PluginLoader::load_all (扩展):
void PluginLoader::load_all(IToolRegistry& registry) {
  // 1. 扫描 plugin 搜索路径,收集全部 .so 的 PluginInfo
  std::vector<PluginInfoV2> all_info;
  std::unordered_map<std::string, std::filesystem::path> name_to_path;
  for (auto& path : find_search_paths()) {
    for (auto& so : glob(path / "*.so")) {
      void* h = dlopen_for_inspection(so);  // 不调 init/registers, 只读 info
      auto* info = read_pdk_plugin_info(h);
      all_info.push_back(*info);
      name_to_path[info->name] = so;
      dlclose_for_inspection(h);  // 等扫描完再决定加载序
    }
  }

  // 2. Build dependency graph (from each PluginInfo.dependencies)
  Graph g = build_graph(all_info);  // vertices=name, edges=a→b "a depends on b"

  // 3. Topological sort with cycle detection
  auto order = topo_sort(g);
  if (has_cycle(g)) {
    throw std::runtime_error("PluginLoader: cyclic dependency detected: ...");
  }

  // 4. 按序 load_so (注册 tools + 调用 init + 创建 ILLMProvider)
  for (auto& name : order) {
    load_so(name_to_path[name], registry);  // 调完整 load 流程
  }
}
```

**依赖缺失处理**:
- PluginLoader::load_all 时, 若 `dependencies` 字段引用的 plugin name 不在 all_info 中 → **拒绝加载该 plugin**,emit `plugin.dep.missing.{name}` 事件 + 抛出 `std::runtime_error`
- 不再是"仅警告"

**Phase 1 范围**:
- ✅ Topological sort
- ✅ Cycle detection
- ✅ Missing-dependency rejection
- ❌ Dynamic dependency (Phase 2: `dependencies` 可在 init 钩子中追加 — 复杂场景)
- ❌ Version constraint (eg. "needs agenticllama_inference >= 2.0") — Phase 2

**PluginLoader::load_so 调用方责任**: 单 plugin 加载时,调用方需保证依赖已加载。否则 plugin.init() 运行时调用 `inference/generate` 但推理 plugin 未加载 → 工具未注册错误。

### 5. 与现有机制的兼容性

| 现有机制 | 与 lifecycle 钩子的关系 |
|---------|---------------------|
| `inference/engine/init` tool (ADR-0035) | **保留**。仍用于 per-runtime engine 初始化 (eg. 加载具体模型); lifecycle `init` 用于 plugin-level 全局 state |
| `inference/configure` tool (ADR-0038 L3a) | **保留**。配置运行时参数; 与 `init` 无关 |
| ABI version check (ADR-0022 §4) | 保留。lifecycle 钩子调用不影响 ABI 检查 |
| PluginLoader `unload_plugin` (Sprint 5 已有) | **扩展**: 在 dlclose 前调 `pdk_plugin_fini` |

**关系图**:

```
dlopen
  ↓
pdk_plugin_info → ABI check
  ↓ (P1 fix: NEW)
pdk_plugin_init   ← 全局 GPU init, 预热资源池
  ↓ (existing)
pdk_register_tools
  ↓ (optional existing, P1 fix unchanged)
pdk_create_llm_provider
  ↓
load_so 返回 true
  ↓
...runtime 长期...
  ↓
unload_plugin called
  ↓ (P1 fix: NEW)
pdk_plugin_fini  ← GPU free, flush telemetry
  ↓ (existing)
dlclose
```

### 6. 错误处理语义

| 钩子调用结果 | PluginLoader 行为 |
|------------|-----------------|
| `pdk_plugin_init` 返回 false | load_so 返回 false,不调用 register_tools,不存储 record,立即 dlclose rollback |
| `pdk_plugin_init` throws exception | PluginLoader catch → 视为 return false 同等处理 |
| `pdk_plugin_fini` 抛异常 | PluginLoader 记录 warning 但继续 dlclose (fini 异常不应阻塞 unload) |
| `pdk_plugin_fini` 缺失 | PluginLoader 不调用,正常 dlclose |
| `pdk_plugin_init` 存在但 init 失败 | 已注册的 tool 不会被清 (因为在 init 失败时 register_tools 还没调) |

### 7. 安全考虑

| 风险 | 缓解 |
|------|------|
| init 钩子耗时过长阻塞 PluginLoader | init 钩子应 <100ms (只做 GPU context 创建); PluginLoader 不设 timeout 但记录耗时 metric |
| fini 钩子 hang (死锁 / 资源未释放) | PluginLoader 设 5s hard timeout, 强制 dlclose (OS 回收资源) |
| init 副作用未在 fini 中清理 (资源泄漏) | Test 验证 init/fini 配对 (pair-test) |
| **多 Plugin init 顺序竞争** (eg. 两个 plugin 都 init CUDA) | **MVP 单 inference plugin 独占 GPU** (per [ADR-0044 §5 资源隔离](../adr-0044-inference-plugin-security-model.md)); 多 plugin 场景 defer Phase 2 容器隔离 |
| **多 `pdk_create_llm_provider` 共存选择** (eg. 推理 Plugin + remote Plugin 同时存在) | PluginLoader **返回首个** (按 dependency ordering) 创建的 shared_ptr<ILLMProvider>。Multi-provider 场景由编排 Plugin 通过 `IModelRouter` 选择 (per [ADR-0034](./plugin/adr-0034-model-router.md) + [ADR-0045 §3](../adr-0045-orchestration-plugin-spec.md)) |

---

## 替代方案

### Option A: 维持当前模型 (无 lifecycle 钩子, 通过工具调用初始化)

**被拒绝理由**: 已 ADR-0035 §1.2 临时方案, 存在 race condition 与 lazy init 调试困难。

### Option B: 仅一个 `pdk_plugin_init` (无 fini)

**被拒绝理由**: 无法在 unload 时 flush 资源, 导致 GPU context 泄漏 (CUDA driver 进程卸载时 eror)。

### Option C (采用): 4 阶段 lifecycle (init/start/stop/fini)

**拒绝按需缩减**: 当前 2 个钩子 (init/fini) 已覆盖需求。start/stop 是 runtime hooks (resolve pause/resume), 与本文 lifecycle 不同; 可作 Phase 2 扩展。

---

## 实施顺序

1. 扩展 `pdk_plugin_info` 结构 (增加 `dependencies[256]` 字段, ABI bump to v2)
2. 扩展 `PluginLoader::load_so` + `unload_plugin` (增加 init/fini 调用)
3. 更新 ADR-0022 §1.1 同步声明 5 个符号
4. 实现 pair-test (init 必须有配对 fini)
5. Inference Plugin (`agenticllama_inference`) 实施 init/fini 包装 llama_backend_init/free

---

## 测试策略

| # | 测试 | 覆盖 |
|---|------|------|
| 1 | `plugin_init_success` | 符号 4 存在 + 返回 true → PluginLoader 成功存储 |
| 2 | `plugin_init_failure` | 符号 4 返回 false → load_so 失败 + rollback dlclose |
| 3 | `plugin_init_exception` | 符号 4 throws → load_so 失败 (catch 异常语义) |
| 4 | `plugin_fini_called_on_unload` | unload_plugin → fini 调用顺序在 dlclose 前 |
| 5 | `plugin_fini_missing_safe` | 无符号 5 → unload 跳过,直接 dlclose |
| 6 | `plugin_dependencies_parsed` | dependencies 字段正确解析 |
| 7 | `plugin_init_fini_pair_test` | init → register_tools → fini → 清理配对 |
| 8 | `plugin_load_performance` | init 钩子耗时 <100ms (perf benchmark) |
| 9 | `plugin_fini_timeout` | fini hang 超 5s 强制 dlclose (no deadlock) |

---

*创建日期*: 2026-07-06
*关联*: ADR-0022 (§1.1 符号同步), ADR-0035 (推理 Plugin init 使用), ADR-0040 (§5 ABI 版本协调), ADR-0021 (§2.1 Plugin Lifecycle MVP 补完)
*依赖*: ADR-0022, ADR-0035, ADR-0040
*P1 fix 应用*: 同步修正 ADR-0022 §1.1 列出 5 符号, PluginInfo 加 dependencies 字段, 与 ADR-0040 §5 ABI 协调
