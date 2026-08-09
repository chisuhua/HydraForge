# plugin-loader Specification (Delta)

## ADDED Requirements

### Requirement: plugin-manifest-first-load

`PluginLoader::load_so()` MUST 在 `dlopen` 之前先尝试读取并校验 `pdk_manifest.json` (per ADR-0052 §决策 4),缺 manifest 时保持向后兼容(默认 warn-only,不阻塞现有 plugin)。

#### Scenario: 有 manifest 且校验通过

- **WHEN** `.so` 路径向上查找发现 `pdk_manifest.json`
- **AND** `ManifestValidator::validate()` 返回 `valid=true`
- **THEN** MUST 继续 `dlopen` + `dlsym(pdk_plugin_info)` 流程
- **AND** MUST 交叉验证 `PluginInfo.abi_version` 与 `manifest.abi_version` 一致
- **WHEN** 一致
- **THEN** MUST 加载成功(返回 `true`)
- **WHEN** 不一致
- **THEN** MUST emit warn 日志(以 `PluginInfo` 为准,per ADR-0052 §决策 4)且继续加载

#### Scenario: 有 manifest 但校验失败

- **WHEN** `ManifestValidator::validate()` 返回 `valid=false`
- **THEN** MUST 拒绝加载,返回 `false`
- **AND** MUST NOT 调用 `dlopen`
- **AND** MUST 通过 `IInteractionBus::emit()` 发送 `plugin.manifest.invalid` 事件
  - payload MUST 含 `path` (string) + `errors[]` (array of `{field, reason}`)
- **AND** MUST 打印错误日志,含具体错误原因

#### Scenario: 缺 manifest 默认向后兼容

- **WHEN** 从 `.so` 路径向上查找至根仍未发现 `pdk_manifest.json`
- **AND** 调用 `load_so(path, registry, strict_version=true)` 未传 `require_manifest` 参数(默认 `false`)
- **THEN** MUST 打印 warn 日志 "manifest not found for <path>, fallback to legacy load"
- **AND** MUST 继续 `dlopen` + `dlsym` 旧流程(行为与 Sprint 5 一致)
- **AND** MUST 加载成功(若 PluginInfo ABI 检查通过)

#### Scenario: 显式 require_manifest=true 强制要求

- **WHEN** 调用 `load_so(path, registry, strict_version=true, require_manifest=true)`
- **AND** 未找到 `pdk_manifest.json`
- **THEN** MUST 拒绝加载,返回 `false`
- **AND** MUST 打印错误日志 "manifest required but not found for <path>"
- **AND** MUST NOT 调用 `dlopen`

#### Scenario: load_all 路径同样应用

- **WHEN** 调用 `load_all(registry)` 扫描搜索路径下的所有 `.so`
- **THEN** 对每个 `.so` MUST 应用 manifest-first 流程(per `plugin-manifest-first-load` 上述 4 scenarios)

### Requirement: plugin-manifest-event-emission

Manifest 校验失败时 MUST 通过 `IInteractionBus` 发送结构化事件(per ADR-0068 事件契约),便于 TUI / EventHandler 渲染失败原因。

#### Scenario: invalid manifest 事件

- **WHEN** `PluginLoader::load_so` 因 manifest 校验失败拒绝加载
- **THEN** MUST emit `BusEvent{topic="plugin.manifest.invalid", ...}`
- **AND** payload `data` MUST 含:
  - `path` (string): `.so` 路径
  - `errors` (array of object): `[{field, reason, value?}]`
- **AND** payload `meta` MUST 含:
  - `timestamp` (int64, microseconds)
  - `trace_id` (string, optional)
  - `phase` (string, "manifest_validation")

#### Scenario: 缺 manifest warn 事件(可选)

- **WHEN** `PluginLoader::load_so` 在默认 `require_manifest=false` 模式下未找到 manifest
- **THEN** MAY emit `BusEvent{topic="plugin.manifest.missing", payload={path}}` 事件(便于 dashboard 跟踪未迁移 plugin)
- **AND** 该事件 MUST NOT 阻塞加载流程
