# plugin-loader Specification (Delta)

## ADDED Requirements

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
