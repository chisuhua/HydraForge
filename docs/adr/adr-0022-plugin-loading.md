# ADR-0022: 插件加载机制

## 状态

**✅ Approved (2026-06-24, Sprint 5 ship)** — PluginInfo POD + PluginLoader 类已落地 (commit `968937f`),含 Linux dlopen/dlsym 实现 + ABI 版本检查 + 路径白名单 + 5 个 ctest case 通过。OpenSpec change [`2026-07-14-plugin-loader`](../../openspec/changes/2026-07-14-plugin-loader/) 进行中 (Sprint 5 ship 后 archive)。Phase 2+ 后续: 跨平台 dlopen 抽象 (macOS/Windows) + 完整 PluginLifecycle 钩子 + hot reload + plugin marketplace。Phase 1 智能体层 100% 收官，变更依据: `openspec/changes/tech-debt-and-phase1-closure/`。

> **Sprint 5 增量 (2026-06-21, commit 968937f)**：PluginInfo POD (abi_version + name + 3 version + desc + caps, 848 字节) + PluginLoader (load_all / load_so / list_loaded / unload_plugin) + Linux dlopen 实现 + 5 ctest case (POD layout / ABI validate / lifecycle / whitelist / E2E) 落地。`include/agenticdsl/plugin/plugin_info.h` + `plugin_loader.h` + `src/modules/plugin/plugin_loader.cpp`。fix 跨命名空间 forward declaration (hydraforge vs agenticdsl) + MockToolRegistry 实现 IToolRegistry 9 虚函数。33/33 ctest pass 零回归。

---

## 生命周期扩展对齐 (2026-07-06 追加)

**与 ADR-0041 (PluginLoader 生命周期扩展) 的对齐说明**: 本 ADR 的 §决策 2 (符号约定) 当前仅定义 `pdk_register_tools` 和 `pdk_plugin_info` 两个符号。Phase 2+ 规划的 "完整 PluginLifecycle 钩子" (如 `pdk_plugin_init`/`pdk_plugin_fini`) 将由 [ADR-0041 (PluginLoader 生命周期扩展)](../adr-0041-pluginloader-lifecycle-extension.md) (P2, 待创建) 定义。GPU 初始化等有状态 Plugin 需求在当前阶段通过 `inference/engine/init` 工具手动触发, 不阻塞 PluginLoader core。

---

## 替代关系

无替代关系。本 ADR 填补 ADR-0021 (PDK) 与 ADR-0020 (Runtime) 之间的缺口，定义 **Runtime 如何加载 PDK 编译的 `.so` 插件**。

---

## 背景

### 缺口

| ADR | 定义了什么 | 未定义什么 |
|-----|-----------|-----------|
| ADR-0021 (PDK) | `DECLARE_TOOL` 宏、`DEFINE_AGENT` 模板、`SafeExec`、测试替身 | 编译产物如何被 Runtime 加载 |
| ADR-0020 (Runtime) | `ToolRegistry`、`CognitiveWorker`、`DomainWorkerPool` | 工具如何从 `.so` 进入 `ToolRegistry` |
| ADR-0019 | `IInteractionBus`、`InMemoryBus` | 不涉及 |

### 依赖方向

```
ADR-0021 (PDK)           →  编译时: 生成 .so
    ↓
本 ADR  (Loading)        →  运行时: 加载 .so → 注册工具
    ↓
ADR-0020 (Runtime)       →  执行时: ToolRegistry → NodeExecutor
```

### 现状

- 当前 Runtime **无 `dlopen`/`dlsym` 调用**
- 所有工具通过 `engine->register_tool(name, lambda)` 手动注册
- 工具注册是编译时决定的，无法动态扩展

### 参考文档

| ADR | 关系 |
|-----|------|
| ADR-0021 | PDK 定义 `.so` 的编译方式，本 ADR 定义加载方式 |
| ADR-0020 | `ToolRegistry` 是工具的目标注册地 |
| ADR-0004 | 安全策略在工具执行时生效，本 ADR 的路径白名单是加载时安全层 |

---

## 决策

### 1. 符号约定

#### 1.1 规则 (2026-07-06 扩展)

每个 PDK 编译的 `.so` **必须导出 2 个核心符号**, **可选导出 1 个 ILLMProvider 工厂符号**:

```cpp
// 符号 1 (必须): 插件元数据 (POD 数据符号)
extern "C" const hydraforge::PluginInfo pdk_plugin_info;

// 符号 2 (必须): 工具注册函数 (无返回值, 接受 ToolRegistry 引用)
extern "C" void pdk_register_tools(::agenticdsl::IToolRegistry& registry);

// 符号 3 (可选, per ADR-0035 §1.2):
//            ILLMProvider 工厂函数, 仅提供 LLM 能力的 Plugin (如推理引擎) 导出
extern "C" std::shared_ptr<::agenticdsl::ILLMProvider> pdk_create_llm_provider();
```

**符号 3 设计要点** (ADR-0035 §1.2):
- 返回 `std::shared_ptr<ILLMProvider>` 而非 raw pointer — 跨 .so 边界内存安全, 与 C++ RAII 一致
- 若 plugin 不导出符号 3, PluginLoader 跳过此 lookup, DSLEngine 使用默认 ILLMProvider 实现
- PluginLoader 析构所有 shared_ptr 后再 dlclose (析构顺序保证 — 参考 Sprint 17 C7 destruction order bug)

**符号查找机制** (Linux): `dlsym(handle, "pdk_create_llm_provider")`, 若返回 NULL 表示 plugin 未提供此能力。

#### 1.2 PluginInfo 结构体

```cpp
// src/common/plugin/plugin_info.h
#ifndef AGENTICDSL_PLUGIN_PLUGIN_INFO_H
#define AGENTICDSL_PLUGIN_PLUGIN_INFO_H

#include <cstdint>

namespace hydraforge {

// 插件元数据 — POD 结构，只能包含值类型
// 目的: Runtime 在调用任何插件函数前即可读取

// === PluginInfo v1 (2026-06-21 Sprint 5 ship, sizeof = 848 bytes) ===
struct PluginInfoV1 {
    uint32_t abi_version;             // = 1
    char name[64];
    uint32_t major_version, minor_version, patch_version;
    char description[256];
    char capabilities[512];
};

// === PluginInfo v2 (2026-07-06 P1 fix per ADR-0041, sizeof = 1104 bytes) ===
//   新增 dependencies[256] 字段 — comma-separated plugin names 表达加载依赖
struct PluginInfoV2 {
    uint32_t abi_version;             // = 2 (CURRENT_ABI_VERSION)
    char name[64];
    uint32_t major_version, minor_version, patch_version;
    char description[256];
    char capabilities[512];
    char dependencies[256];            // NEW: comma-separated plugin names
};

// Type alias: 与既有代码兼容
using PluginInfo = PluginInfoV2;

// 当前 ABI 版本 (bumped to 2 per ADR-0041)
inline constexpr uint32_t CURRENT_ABI_VERSION = 2;

// V1 也被 PluginLoader 接受 (向后兼容老 .so)
static constexpr uint32_t SUPPORTED_ABI_VERSIONS[] = {1, CURRENT_ABI_VERSION};

} // namespace hydraforge

#endif
```

**选择理由**:
- `PluginInfo` 是 POD → `dlsym` 获取地址后直接读字段,**零代码执行**
- ABI 版本控制独立于功能版本,避免 ABI break 误判
- `capabilities` 标签支持未来按能力发现插件
- **v1/v2 dual ABI support**: PluginLoader 根据 `info.abi_version` dispatch 读 V1 或 V2, 老 .so 仍可加载
- **依赖声明** (v2 新): orchestration plugin 声明对 inference plugin 依赖 → PluginLoader topological sort
- 放弃选项 A (固定符号): 多 `.so` 场景符号冲突不可控
- 放弃纯选项 B (纯版本化符号): 无法在调用前获取元数据

#### 1.2.1 Dual ABI dispatch (2026-07-06 P0 fix per [ADR-0041 §1.5](./adr-0041-pluginloader-lifecycle-extension.md))

PluginLoader 读 `pdk_plugin_info` POD 后,根据 `info.abi_version` 字段选择 reading path:

```cpp
// PluginLoader::read_pdk_plugin_info unified read
PluginInfoV2 PluginLoader::read_plugin_info(void* info_handle) {
  // 1. 读 abi_version (前 4 字节, 安全)
  uint32_t abi_version;
  std::memcpy(&abi_version, info_handle, sizeof(uint32_t));

  if (abi_version == 1) {
    // v1 老 .so
    PluginInfoV1 raw;
    std::memcpy(&raw, info_handle, sizeof(PluginInfoV1));
    return PluginInfoV2{
      .abi_version = 1, .name = ... // strncpy from raw.name
      ...
      .dependencies = ""  // v1 无依赖字段, 视为空
    };
  } else if (abi_version == CURRENT_ABI_VERSION) {
    // 当前版本,直接 memcpy 整个 struct
    PluginInfoV2 raw;
    std::memcpy(&raw, info_handle, sizeof(PluginInfoV2));
    return raw;
  } else {
    throw std::runtime_error("Unsupported PluginInfo abi_version: " + std::to_string(abi_version));
  }
}
```

**后向兼容性**:
- 新 PluginLoader (abi_version=2) 加载老 v1 .so → OK, `dependencies` = ""
- 老 PluginLoader (abi_version=1) 加载新 v2 .so → 拒绝 (PluginInfoV1 read truncated, 内存不匹配)

#### 1.3 PDK 展开示例

`DECLARE_TOOL(edit_file, ...)` 在 PDK (ADR-0021) 中展开为:

```cpp
// PDK 自动生成的代码 (编译到 edit_file_plugin.so 中)
#include <hydraforge/pdk.h>

// 自动生成 PluginInfo v2
extern "C" const hydraforge::PluginInfoV2 pdk_plugin_info = {
    .abi_version = hydraforge::CURRENT_ABI_VERSION,  // = 2
    .name = "edit_file",
    .major_version = 1,
    .minor_version = 0,
    .patch_version = 0,
    .description = "Edit files in workspace",
    .capabilities = "code,code::edit,code::file",
    .dependencies = "",  // 无依赖
};

// 自动生成注册函数
extern "C" void pdk_register_tools(hydraforge::ToolRegistry& registry) {
    registry.register_tool("code/edit_file", [](const auto& args) {
        // 领域逻辑...
        return nlohmann::json{{"ok", true}};
    });
}

// 可选 init 钩子 (新增 per ADR-0041 §1)
extern "C" bool pdk_plugin_init(const hydraforge::PluginInfoV2& self) {
    // 资源初始化
    return true;
}

// 可选 fini 钩子
extern "C" void pdk_plugin_fini() {
    // 资源清理
}
```

---

### 2. 插件发现路径

#### 2.1 搜索顺序 (优先级从高到低)

```
1. $HYDRAFORGE_PLUGIN_PATH  (环境变量, 可指定多个路径, : 分隔)
2. ./plugins/                (工作目录下的 plugins 文件夹)
3. ~/.hydraforge/plugins/   (用户安装目录)
4. /usr/local/lib/hydraforge/plugins/  (系统安装目录)
```

#### 2.2 自动发现

从每个路径扫描 `*.so` 文件，尝试加载。

```cpp
// src/common/plugin/plugin_loader.h
namespace hydraforge {

class PluginLoader {
public:
    PluginLoader();

    // 扫描所有发现路径，加载可用的插件
    // 返回成功加载的插件列表
    std::vector<std::string> load_all(ToolRegistry& registry);

    // 加载指定路径的单个 .so
    // flag: 是否严格版本检查 (默认 true)
    bool load_so(const std::string& path,
                 ToolRegistry& registry,
                 bool strict_version = true);

    // 列出已加载的插件
    std::vector<PluginInfo> list_loaded() const;

private:
    std::vector<std::string> get_search_paths() const;
    bool check_compatibility(const PluginInfo& info) const;
    bool apply_path_whitelist(const std::string& path) const;

    struct LoadedPlugin {
        void* handle;
        PluginInfo info;
    };

    std::vector<LoadedPlugin> loaded_;
};

} // namespace hydraforge
```

**选择理由**：
- 环境变量覆盖优先级：开发者可在 CI/CD 中指定测试插件路径
- B(固定目录) 不够灵活，A(仅环境变量) 增加用户心智负担

---

### 3. 生命周期钩子

#### 3.1 支持的钩子

```cpp
// PDK 生成的插件入口 (模板)
struct Plugin_v1 {
    // ── 必需 ──
    // 加载时调用，用于插件初始化（如分配资源、检查依赖）
    bool (*on_load)(PluginContext& ctx);

    // 注册工具到 ToolRegistry
    void (*register_tools)(ToolRegistry& registry);

    // ── 可选 (nullptr 表示不支持) ──
    // 卸载前调用，用于清理
    void (*on_unload)();
};
```

#### 3.2 加载流程 (2026-07-06 更新 — per [ADR-0041 §2](./adr-0041-pluginloader-lifecycle-extension.md))

**Note**: 本节最初描述的 `pdk_plugin_lifecycle` struct-based 钩子是早期理论设计。Sprint 5 实际落地采用 **individual dlsym'd symbols** 模式,本节改写以反映现实 + ADR-0041 扩展。

```
dlopen(".so", RTLD_NOW | RTLD_LOCAL)
    │
    ├── dlsym("pdk_plugin_info") → PluginInfoV2 (or V1 if old .so)
    │   ├── abi_version ∈ {1, CURRENT_ABI_VERSION}?   → 继续 (dual ABI support)
    │   └── abi_version 不匹配?                  → 拒绝加载
    │
    ├── build dependency graph from PluginInfo.dependencies
    │   └── cyclic dependency?   → 拒绝加载, throw runtime_error
    │
    ├── dlsym("pdk_plugin_init")         → optional init 钩子 (NEW per ADR-0041)
    │   ├── 存在 + 返回 true?        → 继续
    │   ├── 存在 + 返回 false?       → dlclose rollback, 拒绝加载
    │   └── 不存在?                  → 跳过,继续
    │
    ├── dlsym("pdk_register_tools")   → 必选
    │   └── 调用 register_tools(registry)
    │
    ├── dlsym("pdk_create_llm_provider") → optional (NEW P0 fix)
    │   ├── 存在?                    → plugin->llm_provider = factory_fn()
    │   └── 不存在?                  → 跳过
    │
    └── 记录到 loaded_ 列表
```

完整 5 个符号集见 [ADR-0041 §1](./adr-0041-pluginloader-lifecycle-extension.md)。

#### 3.3 卸载策略 (2026-07-06 更新 — per [ADR-0041 §2](./adr-0041-pluginloader-lifecycle-extension.md))

```cpp
// 卸载单个插件 (Sprint 5 既有, ADR-0041 扩展)
void unload_plugin(const std::string& name) {
    auto it = find_plugin(name);
    if (it == end) return false;

    // 1. 释放所有外部引用 (caller responsibility)
    //    - DSLEngine 释放 shared_ptr<ILLMProvider>
    //    - 编排 Plugin unsubscribe bus topics
    //    - ToolRegistry lambdas 必须先于 dlclose 释放 (per Sprint 17 C7 destruction order)

    // 2. ADR-0041 NEW: 调用 pdk_plugin_fini (if present + has_init)
    if (it->has_init) {
        auto fini = dlsym(it->handle, "pdk_plugin_fini");
        if (fini) fini();  // 5s hard timeout (per ADR-0041 §7)
    }

    // 3. dlclose (existing)
    dlclose(it->handle);

    // 4. 注意: dlclose 后不保证 C++ 静态析构运行
    //    解决方案: 进程退出时 _exit(0) 跳过析构
    //    运行时卸载场景需确保 fini 已清理所有资源

    loaded_.erase(it);
}
```
```

**选择理由**：
- 放弃选项 A（仅 `register_tools`）：无法处理插件初始化（如打开日志文件、连接服务）
- 放弃选项 B（完整 `on_health_check` 等）：MVP 过度设计
- `on_unload` 可选且警告 `dlclose` 风险：诚实面对 C++ 限制

---

### 4. 版本兼容性

#### 4.1 规则

| 条件 | 结果 |
|------|------|
| `PluginInfo.abi_version == CURRENT_ABI_VERSION` (= 2) | ✅ 加载 (v2 PDK dispatch) |
| `PluginInfo.abi_version == 1` (P1 fix per [ADR-0041 §1.5](./adr-0041-pluginloader-lifecycle-extension.md)) | ✅ 加载 (v1 backward compat) |
| `PluginInfo.abi_version ∉ {1, CURRENT_ABI_VERSION}` | ❌ 拒绝 (ABI break) |
| major/minor/patch 不匹配且 abi_version 相同 | ✅ 加载（运行时检查） |

#### 4.2 为什么使用 `abi_version` 而非 `major_version`？

```
abi_version = 1 (当前版本)
──────────────────────────────────────────────────────
PDK v1.0 → PluginInfo{abi=1, major=1, minor=0}  → ✅
PDK v1.1 → PluginInfo{abi=1, major=1, minor=1}  → ✅ (新增宏但不改 ABI)
PDK v2.0 → PluginInfo{abi=2, major=2, minor=0}  → ❌ (C++ 宏展开变化)
Runtime   → CURRENT_ABI_VERSION = 1

好处:
- major_version 可自由演进 (功能版本), 不影响兼容性检查
- abi_version 只在"C++ 宏展开方式变化"时递增
- 避免"改了个文档就递增 major"的伪语义问题
```

#### 4.3 abi_version 递增规则

| 场景 | abi_version |
|------|------------|
| 宏展开方式不变（如新增 DECLARE_TOOL 参数） | 不变 |
| `PluginInfo` 结构体字段变化 | +1 |
| `Plugin_v1` 结构体布局变化 | +1 |
| `ToolRegistry` 接口签名变化 | +1 |

**选择理由**：
- 拒绝"无版本检查"：C++ ABI 无保证，跳版本加载静默崩溃
- 拒绝"semver 范围"：`major_version` 是功能版本，不反映 ABI 兼容性
- `abi_version` 显式门控：清晰地定义了"什么算 ABI break"

---

### 5. 安全措施

#### 5.1 路径白名单 (Layer 1)

```cpp
bool PluginLoader::apply_path_whitelist(const std::string& so_path) const {
    // 只允许从以下路径加载:
    // 1. $HYDRAFORGE_PLUGIN_PATH 下
    // 2. ./plugins/ 下
    // 3. ~/.hydraforge/plugins/ 下
    // 4. /usr/local/lib/hydraforge/plugins/ 下
    for (const auto& search_path : get_search_paths()) {
        if (so_path.starts_with(search_path)) {
            return true;
        }
    }
    return false;
}
```

#### 5.2 符号隔离 (Layer 2)

```cpp
// dlopen 使用 RTLD_LOCAL: 插件的符号不暴露到全局命名空间
// 避免插件 A 的符号被插件 B 意外使用
void* handle = dlopen(so_path.c_str(), RTLD_NOW | RTLD_LOCAL);

// 只允许通过 dlsym 获取 pdk_* 前缀的符号
// 拒绝直接调用插件内部函数
auto* life = reinterpret_cast<Plugin_v1*>(
    dlsym(handle, "pdk_plugin_lifecycle")
);
```

#### 5.3 MVP 安全范围

| Layer | MVP | Phase 2 |
|-------|-----|---------|
| 路径白名单 | ✅ | ✅ |
| `RTLD_LOCAL` 符号隔离 | ✅ | ✅ |
| 数字签名验证 | ❌ | 🔜 |
| seccomp 沙箱 | ❌ | 🔜 |

**选择理由**：
- `.so` 有完整进程权限（读内存、写文件、发网络请求），必须控制加载源
- 放弃"无安全"：不符合 ADR-0004 的纵深防御原则
- 数字签名和沙箱留给 Phase 2：MVP 阶段通过路径白名单 + 符号隔离足够应对常见风险

---

## 替代方案

### 方案 A: 编译时静态注册 (当前方式)

- **优点**：简单，零运行时开销
- **缺点**：无法动态扩展，每次添加工具需重新编译 Runtime
- **结论**：被否决。与 ADR-0021 的"独立仓库"理念冲突

### 方案 B: 基于配置文件的注册 (YAML/JSON 声明工具路径)

- **优点**：声明式，无二进制符号依赖
- **缺点**：仍需要注册函数，加载时仍需 dlopen
- **结论**：可叠加使用（作为路径发现的补充），但不替代

---

## 权衡

| 决策 | 选择 | 理由 |
|------|------|------|
| 符号数量 | 2 个（元数据 + 注册） | 元数据优先，拒绝前零风险执行 |
| 路径发现 | 环境变量 + 3 个固定目录 | 开发/用户/系统三方兼顾 |
| 生命周期 | on_load + register_tools + 可选 on_unload | 够用且诚实面对 C++ 限制 |
| 版本门控 | `abi_version` 显式对比 | 避免 semver 伪语义 |
| 安全 | 路径白名单 + RTLD_LOCAL | MVP 够用，不过度设计 |

---

## 实施计划

| Phase | 任务 | 产出 |
|-------|------|------|
| **Phase 1** | `PluginInfo` 结构体定义<br>`PluginLoader` 类 (load_all, load_so)<br>路径发现逻辑<br>版本兼容性检查 | 核心加载器 |
| **Phase 2** | PDK 展开 `pdk_plugin_info` + `pdk_register_tools`<br>调整 `DECLARE_TOOL` 宏输出 | PDK 集成 |
| **Phase 3** | `Plugin_v1` 生命周期支持<br>`PluginContext` 定义<br>卸载功能 | 完整生命周期 |
| **Phase 4** | 数字签名验证<br>`PluginManager` (依赖管理) | 生产就绪 |

---

## 验证标准

| 标准 | 验证方法 |
|------|---------|
| `.so` 加载 | 编译 PDK 示例为 `.so`，Runtime `load_so()` 注册成功 |
| 版本门控 | `abi_version` 不匹配时拒绝加载，记录日志 |
| 路径白名单 | 白名单外路径加载被拒绝 |
| 符号隔离 | `nm -D plugin.so` 只有 `pdk_*` 和 `PluginInfo` 符号暴露 |
| 卸载安全 | `dlclose` 后进程可继续运行，无段错误 |

---

## 参考

- [ADR-0021: Plugin Development Kit (PDK) 设计](./adr-0021-pdk-design.md)
- [ADR-0020: 多智能体线程模型与隔离策略](./adr-0020-thread-model-isolation.md)
- [ADR-0004: ToolRegistry 安全模型](./adr-0004-toolregistry-security.md)
- [relationships.md](./relationships.md) — ADR 联合分析

---

## 附录 A: 与 ADR-0021 的协作

本 ADR 与 ADR-0021 (PDK) 联合构成完整的插件体系：

```
PDK (命令行/构建时)         加载器 (运行时)
─────────────────          ────────────────
DECLARE_TOOL(foo)          PluginLoader::load_all()
    │                              │
    ├── 生成 PluginInfo             ├── 扫描 .so
    ├── 生成 pdk_register_tools     ├── dlsym 元数据
    └── 编译为 foo.so               ├── 版本检查
                                    ├── dlsym 生命周期
                                    ├── on_load()
                                    └── register_tools()
                                            │
                                            ▼
                                      ToolRegistry 就绪
```

## 附录 B: 文件变更清单

| 操作 | 文件路径 |
|------|---------|
| **新建** | `src/common/plugin/CMakeLists.txt` |
| **新建** | `src/common/plugin/plugin_info.h` |
| **新建** | `src/common/plugin/plugin_loader.h` |
| **新建** | `src/common/plugin/plugin_loader.cpp` |
