# plugin-loader Specification

## Purpose
Phase 1 Sprint 5 PluginLoader + 智能体层收官(80% → 100%) — `dlopen` 加载第三方 `.so` 插件(动态 dlopen + `pdk_plugin_info` ABI 检查 + 注册回调),PDK v0.1.0 集成(`hydraforge-pdk` 独立仓库已创建),`examples/phase1_plugin_demo/main.cpp` 扩展 `--plugin <name>.so` CLI 模式 + `--mock` 模式;ADR-0019 §1.4 ✅ 已解决 + ADR-0020/0021/0022/0023 同步 ✅ Approved。
## Requirements
### Requirement: plugin-info-pod

`PluginInfo` POD 结构 MUST 提供插件元数据 (在 dlopen 后立即可读, 零代码执行), 含 ABI 版本字段用于兼容性检查.

#### Scenario: PluginInfo 字段定义

- **WHEN** 开发者构造 `PluginInfo`
- **THEN** MUST 含以下字段:
  - `abi_version` (uint32_t): ABI 兼容性版本号
  - `name` (char[64]): 插件名 (ASCII, max 63 字节 + null terminator)
  - `major_version` / `minor_version` / `patch_version` (uint32_t): 语义版本号
  - `description` (char[256]): 插件描述 (max 255 字节 + null terminator)
  - `capabilities` (char[512]): 能力标签 (逗号分隔, max 511 字节 + null terminator)
- **AND** MUST 为 POD 类型 (无构造/析构/虚函数), 保证 dlsym 后零代码执行读取
- **AND** MUST 提供 `inline constexpr uint32_t CURRENT_ABI_VERSION = 1` 常量

#### Scenario: PluginInfo 内存布局稳定

- **WHEN** 跨编译单元/共享库边界传递 PluginInfo (dlsym → 主程序读字段)
- **THEN** 内存布局 MUST 与 C 标准布局兼容 (无 name mangling, 固定字段偏移)
- **AND** MUST NOT 包含任何指针/引用/std::string (避免跨二进制 ABI 不兼容)
- **AND** 字段对齐 MUST 与 Linux x86_64 ABI 一致

### Requirement: plugin-loader-api

`PluginLoader` 类 MUST 提供动态加载 PDK 编译 `.so` 插件的 API, 含加载/卸载/列表操作.

#### Scenario: PluginLoader 构造与析构

- **WHEN** 构造 `PluginLoader loader`
- **THEN** MUST 初始化空的 `loaded_` 列表 (无副作用)
- **AND** 析构时 MUST 遍历 `loaded_` 并 dlclose 每个 handle (RAII 资源管理)

#### Scenario: load_so 单个 .so 加载

- **WHEN** 调用 `loader.load_so(path, registry, strict_version=true)`
- **THEN** MUST 使用 `dlopen(path, RTLD_NOW | RTLD_LOCAL)` 加载 .so
- **AND** MUST 用 `dlsym(handle, "pdk_plugin_info")` 读取 PluginInfo
- **AND** 若 `abi_version != CURRENT_ABI_VERSION` 且 `strict_version=true`: MUST 拒绝加载 (返回 false, dlclose)
- **AND** 若 `abi_version` 不匹配且 `strict_version=false`: MUST 警告但继续
- **AND** MUST 用 `dlsym(handle, "pdk_register_tools")` 读取注册函数
- **AND** MUST 调用 `register_tools(registry)` 注册工具
- **AND** MUST 追加到 `loaded_` 列表 (name, handle, info, path)

#### Scenario: load_all 扫描所有搜索路径

- **WHEN** 调用 `loader.load_all(registry)`
- **THEN** MUST 按优先级扫描搜索路径 (env > ./plugins > ~/.hydraforge/plugins > /usr/local/lib)
- **AND** 对每个路径下的 `*.so` 文件尝试 load_so
- **AND** 返回成功加载的插件数量

#### Scenario: list_loaded 列出已加载插件

- **WHEN** 调用 `loader.list_loaded()`
- **THEN** MUST 返回所有已加载插件的 PluginInfo 列表 (按加载顺序)
- **AND** 空列表表示无插件加载

#### Scenario: unload_plugin 卸载单个插件

- **WHEN** 调用 `loader.unload_plugin(name)`
- **THEN** MUST 在 `loaded_` 中按 name 查找
- **AND** MUST `dlclose(handle)`
- **AND** MUST 从 `loaded_` 移除
- **AND** 返回 true (找到并卸载) 或 false (未找到)

### Requirement: dlopen-implementation

`PluginLoader` MUST 使用 Linux dlopen API 实现 (Linux only), 含路径白名单与 ABI 检查.

#### Scenario: Linux dlopen 实现

- **WHEN** 编译时检测 `__linux__` 宏
- **THEN** MUST 使用 `<dlfcn.h>` 的 dlopen/dlsym/dlclose
- **AND** MUST 用 `RTLD_NOW | RTLD_LOCAL` flag (避免未解析符号 + 局部符号可见性)
- **AND** MUST 在非 Linux 平台 (`#ifdef __linux__` 失败) 编译失败并返回明确错误信息

#### Scenario: ABI 版本检查

- **WHEN** `PluginInfo.abi_version == CURRENT_ABI_VERSION (=1)`
- **THEN** 加载成功 (兼容性)
- **WHEN** `PluginInfo.abi_version != CURRENT_ABI_VERSION`
- **AND** `strict_version=true`
- **THEN** 拒绝加载, 返回 false, 打印明确错误信息 ("ABI version mismatch: X != 1")
- **AND** MUST NOT 调用 `register_tools` (拒绝时直接 dlclose)

#### Scenario: 路径白名单 (Layer 1 安全)

- **WHEN** 加载的 .so 路径在白名单外 (e.g. `/etc/passwd.so`, `/proc/...`, `/sys/...`)
- **THEN** MUST 拒绝加载, 返回 false
- **AND** 白名单规则 (per ADR-0022 §5.1):
  - `$HYDRAFORGE_PLUGIN_PATH` 环境变量指定的路径 (信任)
  - `./plugins/` (工作目录)
  - `~/.hydraforge/plugins/` (用户目录)
  - `/usr/local/lib/hydraforge/plugins/` (系统目录)
- **AND** 拒绝 `/etc/`, `/proc/`, `/sys/`, `/tmp/` 等敏感路径

### Requirement: end-to-end-demo

`examples/phase1_plugin_demo` MUST 支持通过 PluginLoader 加载真实 `.so` 插件, 与 `--mock` 模式共存.

#### Scenario: --load-plugin 加载单个 .so

- **WHEN** 运行 `./phase1_plugin_demo --load-plugin=./plugins/model_router.so`
- **THEN** MUST 使用 PluginLoader 加载指定 .so
- **AND** MUST 通过 ToolRegistry 调用插件工具验证
- **AND** 与 `--mock` 模式互斥 (二者只能选一)

#### Scenario: --plugin-path 加载路径下所有插件

- **WHEN** 运行 `./phase1_plugin_demo --plugin-path=./plugins/`
- **THEN** MUST 使用 PluginLoader::load_all() 扫描路径
- **AND** MUST 列出所有已加载插件
- **AND** 调用每个插件的代表性工具 (验证集成)

#### Scenario: 默认 --mock 模式 (fallback)

- **WHEN** 运行 `./phase1_plugin_demo` 无参数
- **THEN** MUST 使用 MockLLMProvider (Sprint 0 模式)
- **AND** 显示 "[phase1_plugin_demo] Sprint 0 Plugin Stub mode"

#### Scenario: E2E 真实 .so 加载验证

- **WHEN** 加载真实 PDK 编译的 `.so` (abi_version=1, 含 pdk_register_tools)
- **THEN** PluginLoader.load_so MUST 返回 true
- **AND** 插件工具 MUST 在 ToolRegistry 中可调用
- **AND** E2E demo 输出 MUST 含 "Plugin loaded: <name> v<major>.<minor>.<patch>"

### Requirement: phase1-finalize

Phase 1 收官 MUST 包含 5 个候选 ADR 状态变更, 从 🟡 Partial / 🔍 Proposed 变更为 ✅ Approved.

#### Scenario: 5 ADR 状态变更

- **WHEN** Sprint 5 ship 完成
- **THEN** 5 个 ADR 头部 `## 状态` 行 MUST 变更为 ✅ Approved:
  - `docs/adr/adr-0019-iinteraction-bus-mvp.md` §1.4: ✅ 已解决 → ✅ Approved
  - `docs/adr/adr-0020-thread-model-isolation.md`: 🟡 Partial → ✅ Approved
  - `docs/adr/adr-0021-pdk-design.md`: 🟡 Partial (Sprint 4) → ✅ Approved
  - `docs/adr/adr-0022-plugin-loading.md`: 🔍 Proposed → ✅ Approved
  - `docs/adr/adr-0023-tool-result-standard.md`: 🟡 Partial → ✅ Approved
- **AND** `docs/adr-management/STATUS-GLOSSARY.md` 词汇表 MUST 更新 (Approved 状态定义)
- **AND** `docs/adr-management/relationships.md` MUST 通过 `tools/adr_relationships.py` 自动重新生成
- **AND** `docs/roadmap-status.md`: Phase 1 80% → 100%

#### Scenario: Phase 1 收官验收

- **WHEN** 所有 5 ADR 变更为 ✅ Approved
- **THEN** 32 + 5 = 37/37 ctest pass (32 baseline + 5 new test_plugin_loader)
- **AND** CI 6 jobs 全绿 (4 build matrix + docker-tsan + asan)
- **AND** `openspec validate 2026-07-14-plugin-loader` exit 0
- **AND** `./scripts/sync-pdk.sh` Sprint 5 ship 后自动执行 (Dual-Repo Policy)

#### Scenario: Phase 1 → Phase 2 移交

- **WHEN** Sprint 5 ship 完成 (Phase 1 收官)
- **THEN** Phase 1 智能体层进度: 80% → 100%
- **AND** Phase 2 异步+EventBus 准备开始 (W6-W7, 2026-07-16 ~ 2026-07-30)
- **AND** 后续工作流: Phase 2 ADR 评估 → ADR-0026 (async_simple integration) → Sprint 6+ 实施

### Requirement: plugin-loader-sprint5-contract-lock

`plugin-loader` Sprint 5 实施 MUST 锁定对外契约, Phase 2 后续 sprint 仅在以下范围内变更.

#### Scenario: 锁定对外 API

- **THEN** 头文件 `include/agenticdsl/plugin/plugin_info.h` MUST 导出 `PluginInfo` struct + `CURRENT_ABI_VERSION`
- **AND** 头文件 `include/agenticdsl/plugin/plugin_loader.h` MUST 导出 `PluginLoader` 类 + 4 个公开方法 (load_all/load_so/list_loaded/unload_plugin)
- **AND** PDK 编译的 `.so` MUST 导出 `pdk_plugin_info` + `pdk_register_tools` (per ADR-0022 §1.1)
- **AND** 头文件 MUST NOT 引入 Runtime 内部 (core/engine.h, modules/*)

#### Scenario: Phase 2 可扩展点

- **THEN** Phase 2 MAY 添加 PluginLifecycle 完整钩子 (on_load/on_unload) 替代 Sprint 5 MVP 的 `register_tools` 直接调用
- **AND** Phase 2 MAY 添加跨平台 dlopen 抽象 (Linux dlopen + macOS dylib + Windows LoadLibrary)
- **AND** Phase 2 MAY 添加 hot reload (运行时替换 .so) 与 plugin marketplace
- **AND** Phase 2 MUST NOT 修改 PluginInfo 字段布局 (否则 abi_version 必须 +1)

#### Scenario: Phase 2+ 不破坏 Sprint 5 API

- **THEN** Phase 2 PluginLoader MAY 添加新方法 (e.g. `reload_plugin`, `get_loaded_handle`)
- **AND** Phase 2 MUST NOT 修改 Sprint 5 的 4 个公开方法签名
- **AND** Phase 2 MUST NOT 修改 PluginInfo 字段 (除非 abi_version +1)

### Requirement: plugin-manifest-first-load

`PluginLoader::load_so()` MUST 在 `dlopen` 之前先尝试读取并校验 `pdk_manifest.json` (per ADR-0052 §决策 4), 缺 manifest 时保持向后兼容(默认 warn-only, 不阻塞现有 plugin)。

#### Scenario: 有 manifest 且校验通过

- **WHEN** `.so` 路径向上查找发现 `pdk_manifest.json`
- **AND** `ManifestValidator::validate()` 返回 `valid=true`
- **THEN** MUST 继续 `dlopen` + `dlsym(pdk_plugin_info)` 流程
- **AND** MUST 交叉验证 `PluginInfo.abi_version` 与 `manifest.abi_version` 一致
- **WHEN** 一致
- **THEN** MUST 加载成功(返回 `true`)
- **WHEN** 不一致
- **THEN** MUST emit warn 日志(以 `PluginInfo` 为准, per ADR-0052 §决策 4)且继续加载

#### Scenario: 有 manifest 但校验失败

- **WHEN** `ManifestValidator::validate()` 返回 `valid=false`
- **THEN** MUST 拒绝加载, 返回 `false`
- **AND** MUST NOT 调用 `dlopen`
- **AND** MUST 通过 `IInteractionBus::emit()` 发送 `plugin.manifest.invalid` 事件
  - payload MUST 含 `path` (string) + `errors[]` (array of `{field, reason, value?, expected?}`)
- **AND** MUST 打印错误日志, 含具体错误原因

#### Scenario: 缺 manifest 默认向后兼容

- **WHEN** 从 `.so` 路径向上查找至根仍未发现 `pdk_manifest.json`
- **AND** 调用 `load_so(path, registry, strict_version=true)` 未传 `require_manifest` 参数(默认 `false`)
- **THEN** MUST 打印 warn 日志 "manifest not found for <path>, fallback to legacy load"
- **AND** MUST 继续 `dlopen` + `dlsym` 旧流程(行为与 Sprint 5 一致)
- **AND** MUST 加载成功(若 PluginInfo ABI 检查通过)

#### Scenario: 显式 require_manifest=true 强制要求

- **WHEN** 调用 `load_so(path, registry, strict_version=true, require_manifest=true)`
- **AND** 未找到 `pdk_manifest.json`
- **THEN** MUST 拒绝加载, 返回 `false`
- **AND** MUST 打印错误日志 "manifest required but not found for <path>"
- **AND** MUST NOT 调用 `dlopen`

#### Scenario: strict_version × require_manifest 组合语义 (AND)

- **WHEN** `strict_version=true AND require_manifest=true`
- **THEN** 拒绝条件: 任何一项失败 (manifest 缺 + abi 不匹配) → 拒绝加载
- **AND** 拒绝时 MUST 打印明确的失败原因(2 项都列出)
- **WHEN** `strict_version=false AND require_manifest=false` (双关闭)
- **THEN** 两者都按 warn + continue 处理, 缺 manifest 仅 warn, abi 不匹配仅 warn

#### Scenario: load_all 路径同样应用

- **WHEN** 调用 `load_all(registry)` 扫描搜索路径下的所有 `.so`
- **THEN** 对每个 `.so` MUST 应用 manifest-first 流程(per `plugin-manifest-first-load` 上述 5 scenarios)

### Requirement: plugin-manifest-event-emission

Manifest 校验失败时 MUST 通过 `IInteractionBus` 发送结构化事件(per ADR-0068 事件契约), 便于 TUI / EventHandler 渲染失败原因。

#### Scenario: invalid manifest 事件 (构造)

- **WHEN** `PluginLoader::load_so` 因 manifest 校验失败拒绝加载
- **THEN** MUST emit `BusEvent{topic="plugin.manifest.invalid", ...}`
- **AND** payload `data` MUST 含:
  - `path` (string): `.so` 路径
  - `errors` (array of object): `[{field, reason, value?, expected?}]`
- **AND** payload `meta` MUST 含:
  - `timestamp` (int64, microseconds)
  - `trace_id` (string, optional)
  - `phase` (string, "manifest_validation")

#### Scenario: 缺 manifest warn 事件 (推荐)

- **WHEN** `PluginLoader::load_so` 在默认 `require_manifest=false` 模式下未找到 manifest
- **THEN** MUST emit `BusEvent{topic="plugin.manifest.missing", payload={path, fallback_loaded}}` 事件 (便于 dashboard 跟踪未迁移 plugin)
- **AND** 该事件 MUST NOT 阻塞加载流程
- **AND** 静默模式 (bus_ == nullptr) MUST NOT 触发 emit

### Requirement: plugin-loader-set-interaction-bus

`PluginLoader` MUST 提供 `set_interaction_bus()` setter 注入 IInteractionBus (per ADR-0031 §决策 5 ToolCoordinator opt-in 模式), 默认 `nullptr` 跳过所有 emit。

#### Scenario: 默认 nullptr 跳过 emit

- **WHEN** `PluginLoader` 构造后未调 `set_interaction_bus()`
- **THEN** 所有 `load_so` / `load_all` 内部 emit 调用 MUST 静默跳过 (no-op)
- **AND** MUST NOT 打印 error 日志(避免噪音)

#### Scenario: 注入 bus 后正常 emit

- **WHEN** 调用 `loader.set_interaction_bus(bus_ptr)` (bus_ptr != nullptr)
- **THEN** 后续 `load_so` / `load_all` 内部 emit MUST 走 `bus_ptr->emit()`
- **AND** emit 失败 MUST NOT 影响加载流程 (catch + warn 日志)

#### Scenario: bus 指针生命周期

- **WHEN** bus owner 释放 bus 指针
- **THEN** `PluginLoader` 持有 weak 引用, 下次 emit 时检测到 dangling 指针 MUST 静默跳过
- **AND** MUST 提供 `clear_interaction_bus()` 重置方法

#### Scenario: 构造签名零变化 (向后兼容)

- **WHEN** 现有调用方构造 `PluginLoader loader`
- **THEN** MUST 零修改 (Sprint 5 contract lock)
- **AND** `bus_` 字段 MUST 默认 `nullptr` (PIMPL pattern per Sprint 18-19)

