# ADR-0022: 插件加载机制

## 状态

**🔍 Proposed** (2026-05-25)

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

#### 1.1 规则

每个 PDK 编译的 `.so` 必须导出两个符号：

```cpp
// 符号 1: 插件元数据 (POD 数据符号)
extern "C" const hydraforge::PluginInfo pdk_plugin_info;

// 符号 2: 工具注册函数 (无返回值, 接受 ToolRegistry 引用)
extern "C" void pdk_register_tools(hydraforge::ToolRegistry& registry);
```

#### 1.2 PluginInfo 结构体

```cpp
// src/common/plugin/plugin_info.h
#ifndef AGENTICDSL_PLUGIN_PLUGIN_INFO_H
#define AGENTICDSL_PLUGIN_PLUGIN_INFO_H

#include <cstdint>

namespace hydraforge {

// 插件元数据 — POD 结构，只能包含值类型
// 目的: Runtime 在调用任何插件函数前即可读取
struct PluginInfo {
    // 接口版本 (定义编译时 ABI 兼容性)
    // 当前版本: 1
    uint32_t abi_version;

    // 插件名称 (仅 ASCII, 最大 63 字节)
    char name[64];

    // 版本号
    uint32_t major_version;
    uint32_t minor_version;
    uint32_t patch_version;

    // 插件描述 (最大 255 字节)
    char description[256];

    // 提供的能力标签集合 (逗号分隔, 最大 511 字节)
    // 例如: "code,code::edit,code::lsp"
    char capabilities[512];
};

// 当前 ABI 版本
inline constexpr uint32_t CURRENT_ABI_VERSION = 1;

} // namespace hydraforge

#endif
```

**选择理由**：
- `PluginInfo` 是 POD → `dlsym` 获取地址后直接读字段，**零代码执行**
- ABI 版本控制独立于功能版本，避免 ABI break 误判
- `capabilities` 标签支持未来按能力发现插件
- 放弃选项 A（固定符号）：多 `.so` 场景符号冲突不可控
- 放弃纯选项 B（纯版本化符号）：无法在调用前获取元数据

#### 1.3 PDK 展开示例

`DECLARE_TOOL(edit_file, ...)` 在 PDK (ADR-0021) 中展开为：

```cpp
// PDK 自动生成的代码 (编译到 edit_file_plugin.so 中)
#include <hydraforge/pdk.h>

// 自动生成 PluginInfo
extern "C" const hydraforge::PluginInfo pdk_plugin_info = {
    .abi_version = hydraforge::CURRENT_ABI_VERSION,
    .name = "edit_file",
    .major_version = 1,
    .minor_version = 0,
    .patch_version = 0,
    .description = "Edit files in workspace",
    .capabilities = "code,code::edit,code::file",
};

// 自动生成注册函数
extern "C" void pdk_register_tools(hydraforge::ToolRegistry& registry) {
    registry.register_tool("code::edit_file", [](const auto& args) {
        // 领域逻辑...
        return nlohmann::json{{"ok", true}};
    });
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

#### 3.2 加载流程

```
dlopen(".so", RTLD_NOW | RTLD_LOCAL)
    │
    ├── dlsym("pdk_plugin_info") → PluginInfo
    │   ├── abi_version == CURRENT_ABI_VERSION?  → 继续
    │   └── abi_version != CURRENT_ABI_VERSION?  → 拒绝加载
    │
    ├── dlsym("pdk_plugin_lifecycle") → Plugin_v1
    │
    ├── 调用 on_load(ctx)
    │   ├── 返回 true? → 继续
    │   └── 返回 false? → dlclose, 记录错误
    │
    ├── 调用 register_tools(registry)
    │
    └── 记录到 loaded_ 列表
```

#### 3.3 卸载策略

```cpp
// 卸载单个插件
void unload_plugin(const std::string& name) {
    auto it = find_plugin(name);

    if (it->lifecycle.on_unload) {
        it->lifecycle.on_unload();   // 清理
    }

    dlclose(it->handle);             // 卸载 .so

    // 注意: dlclose 后不保证 C++ 静态析构运行
    // 解决方案: 进程退出时 _exit(0) 跳过析构
    // 运行时卸载场景需确保 on_unload 已清理所有资源
}
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
| `PluginInfo.abi_version == CURRENT_ABI_VERSION` | ✅ 加载 |
| `PluginInfo.abi_version != CURRENT_ABI_VERSION` | ❌ 拒绝（ABI break） |
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
