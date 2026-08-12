# pdk-manifest Specification

**Capability**: `pdk-manifest`
**关联 ADR**: ADR-0052 (Agent Plugin Manifest 规范)
**实现位置**: `include/agenticdsl/pdk/manifest.h` + `manifest_finder.h` + `manifest_validator.h`
**源 change**: `openspec/changes/archive/2026-08-10-pdk-manifest-validation/`

## Purpose

定义 `pdk_manifest.json` 文件格式与 `ManifestValidator` + `ManifestFinder` 校验/查找语义,使 OS 能在不加载 `.so` 的前提下发现 PDK Plugin 元数据、校验版本兼容性、读取工具 schema。这是 Phase 6 服务化 (MCP server 暴露 plugin 元数据) 的硬前置,亦支撑 AgentForge MVP 复用 PDK。

完整履行 ADR-0052 §决策 1-3 (9 必填 + 8 推荐 + 1 可选字段), 与 `PluginLoader` Sprint 5 contract lock 兼容。

## Requirements

### Requirement: pdk-manifest-format

`pdk_manifest.json` MUST 是符合 JSON Schema 2020-12 的机器可读文件, 放置在 Plugin 根目录(与 `CMakeLists.txt` 同级), 文件名固定为 `pdk_manifest.json`。

#### Scenario: 文件位置与命名

- **WHEN** Plugin 维护者创建 manifest
- **THEN** MUST 命名为 `pdk_manifest.json` (无大小写变体)
- **AND** MUST 放置在 Plugin 根目录 (与 `CMakeLists.txt` 同级)
- **AND** MUST 是有效 JSON 文本 (RFC 8259)

#### Scenario: 必填字段 (9 个, per ADR-0052 §决策 2)

- **WHEN** `ManifestValidator` 解析 manifest
- **THEN** 必填字段缺失 MUST 拒绝 (`reason="required"`):
  - `id` (string, reverse-DNS 风格, max 64 字节, kebab-case)
  - `name` (string, human-readable, max 128 字节)
  - `version` (semver 字符串, e.g. "0.1.0")
  - `abi_version` (uint32, MUST ∈ `{1, 2}` per `SUPPORTED_ABI_VERSIONS[]`, per ADR-0022 §3.2 dual-ABI)
  - `min_host_version` (semver 字符串)
  - `max_host_version` (semver 字符串, MUST >= `min_host_version`)
  - `implementation_forms` (string[], non-empty, 每个值 ∈ `{"skill", "dsl", "cpp", "wasm"}`)
  - `entry_tool` (string, MUST ∈ `provided_tools[]`)
  - `provided_tools` (string[], non-empty, kebab-case 命名 per ADR-0043)

#### Scenario: 推荐字段 (8 个, per ADR-0052 §决策 3, 缺省有默认值)

- **WHEN** manifest 缺推荐字段
- **THEN** MUST 使用默认值:
  - `interface_versions` (string[], 默认 `[]`)
  - `capabilities` (string[], 默认 `[]`)
  - `input_schema` (JSON Schema 2020-12 object, 默认 `{}`)
  - `output_schema` (JSON Schema 2020-12 object, 默认 `{}`)
  - `requires_isolation` (bool, 默认 `false`)
  - `resources.timeout_ms` (uint32, 默认 30000)
  - `resources.max_concurrent` (uint32, 默认 1)
  - `publisher` (string, 默认 `""`)
  - `trust_level` (string enum `{"high", "medium", "low", "untrusted"}`, 默认 `"untrusted"`)
  - `activation_events` (string[], 默认 `[]`)

#### Scenario: 可选字段 (1 个, per ADR-0052 §决策 7)

- **WHEN** manifest 包含 `signature` 字段
- **THEN** MUST 接受 (string, Phase 6a 仅记录不验签, 推迟至 Phase 7+)

### Requirement: pdk-manifest-validator

`ManifestValidator` MUST 是 `agenticdsl::pdk` 命名空间下的独立类, 提供静态 `validate()` 方法, 接受 JSON 字符串并返回 `ManifestValidationResult`。

#### Scenario: 有效 manifest 校验通过

- **WHEN** `ManifestValidator::validate(json_content)` 接收到符合全部 spec 的 manifest
- **THEN** MUST 返回 `valid=true` + `manifest=Manifest{...}` 填充的 `std::optional`
- **AND** `errors` MUST 为空数组

#### Scenario: 缺必填字段校验失败

- **WHEN** manifest 缺 `id` 字段
- **THEN** MUST 返回 `valid=false`
- **AND** `errors` MUST 包含一条 `ValidationError{field="id", reason="required"}`

#### Scenario: semver 格式错误

- **WHEN** `version` 字段不是合法 semver (e.g. "1.0", "v1.0.0", "abc")
- **THEN** MUST 返回 `valid=false`
- **AND** `errors` MUST 包含 `ValidationError{field="version", reason="invalid_semver", value=<actual>}`

#### Scenario: abi_version 不在 SUPPORTED_ABI_VERSIONS

- **WHEN** `abi_version` ∉ `{1, 2}` (e.g. `3` 或 `0`)
- **THEN** MUST 返回 `valid=false`
- **AND** `errors` MUST 包含 `ValidationError{field="abi_version", reason="mismatch", expected="1|2", actual=<value>}`

#### Scenario: 字段类型不匹配 (严格类型检查)

- **WHEN** 字段类型声明为 `uint32` 但收到 string (e.g. `"abi_version": "1"`)
- **THEN** MUST 返回 `valid=false`
- **AND** `errors` MUST 包含 `ValidationError{field=<field>, reason="wrong_type", expected="uint32", actual="string"}`
- **AND** MUST NOT 依赖 nlohmann::json 隐式类型转换 (避免 silent 接受错误数据)

#### Scenario: 字段类型不匹配 (string 收到 null)

- **WHEN** string 字段收到 `null` (e.g. `"name": null`)
- **THEN** MUST 返回 `valid=false`
- **AND** `errors` MUST 包含 `ValidationError{field="name", reason="wrong_type", expected="string", actual="null"}`

#### Scenario: implementation_forms 非法值

- **WHEN** `implementation_forms` 包含不在 `{"skill", "dsl", "cpp", "wasm"}` 的值
- **THEN** MUST 返回 `valid=false`
- **AND** `errors` MUST 包含 `ValidationError{field="implementation_forms[<index>]", reason="invalid_enum", allowed=["skill","dsl","cpp","wasm"]}`

#### Scenario: entry_tool 不在 provided_tools

- **WHEN** `entry_tool="foo/run"` 但 `provided_tools=["bar/run"]` (cross-field 不一致)
- **THEN** MUST 返回 `valid=false`
- **AND** `errors` MUST 包含 `ValidationError{field="entry_tool", reason="not_in_provided_tools", value="foo/run"}`

#### Scenario: tools[] 元素 schema 非法

- **WHEN** manifest 含 `tools[]` (per ADR-0052 推荐字段), 某元素缺 `name` 或 `input_schema`
- **THEN** MUST 返回 `valid=false`
- **AND** `errors` MUST 包含 `ValidationError{field="tools[<index>].<missing_field>", reason="required"}`

#### Scenario: tools[].approval_policy 非法值

- **WHEN** `approval_policy` 字段值不在 4 个允许值内 (per DECLARE_TOOL `make_approval()` 顺序: "always"/"plan"/"agent"/"yolo")
- **THEN** MUST 返回 `valid=false`
- **AND** `errors` MUST 包含 `ValidationError{field="tools[<index>].approval_policy", reason="invalid_enum", allowed=["always","plan","agent","yolo"]}`

### Requirement: pdk-manifest-finder

`ManifestFinder` MUST 是 `agenticdsl::pdk` 命名空间下的独立类, 提供静态 `find()` 方法, 从 `.so` 路径向上查找 `pdk_manifest.json`, 支持 symlink 安全 + 权限降级。

#### Scenario: 同目录查找

- **WHEN** `.so` 路径为 `/path/to/plugin/build/libfoo.so` (weakly_canonical)
- **AND** `/path/to/plugin/build/pdk_manifest.json` 存在
- **THEN** MUST 返回 `std::filesystem::path("/path/to/plugin/build/pdk_manifest.json")`

#### Scenario: 向上 1 层查找

- **WHEN** `.so` 路径为 `/path/to/plugin/build/libfoo.so`
- **AND** `/path/to/plugin/build/pdk_manifest.json` 不存在
- **AND** `/path/to/plugin/pdk_manifest.json` 存在
- **THEN** MUST 返回上层 manifest 路径

#### Scenario: 找不到 manifest (空结果)

- **WHEN** 从 `.so` 路径向上查找至文件系统根仍未找到 `pdk_manifest.json`
- **THEN** MUST 返回 `std::nullopt` (非错误, 仅缺 manifest)
- **AND** 调用方按 `require_manifest` 参数决定后续行为

#### Scenario: 多个 manifest 冲突 (浅层优先)

- **WHEN** 查找路径上存在多个 `pdk_manifest.json`
- **THEN** MUST 使用最接近 `.so` 路径的那一个 (浅层优先)
- **AND** MUST 打印 debug 日志记录选择的路径

#### Scenario: symlink 路径处理 (安全性)

- **WHEN** `.so` 路径是 symlink (e.g. `/var/symlink/build/foo.so` → `/real/path/build/foo.so`)
- **THEN** MUST `std::filesystem::weakly_canonical()` 后再 walk
- **AND** MUST NOT 跟随查找过程中遇到的 symlink (避免循环引用)
- **AND** MUST 限制最大向上层数 16 (防御性 bound, 防止系统根遍历)

#### Scenario: 权限拒绝降级

- **WHEN** 查找过程中遇到权限拒绝 (e.g. `/root/.ssh/pdk_manifest.json`)
- **THEN** MUST 跳过该目录, 继续向上查找
- **AND** MUST NOT 抛异常
- **AND** MUST 打印 warn 日志记录跳过的目录

#### Scenario: hidden 目录处理

- **WHEN** 查找路径上存在 `.git/`, `.cache/` 等 hidden 目录
- **THEN** MUST NOT 跳过 hidden 目录 (manifest 可能故意放在 hidden 目录)
- **AND** MUST 跳过 `.git/` 内部查找 (避免 git 仓库污染, max 16 层 bound 隐式保护)

### Requirement: pdk-manifest-find-from-so-path (向后兼容)

`PluginLoader` MUST 使用 `ManifestFinder::find()` 替代直接 `std::filesystem::exists()`, 接受 symlink + 权限降级 + 隐藏目录语义。

#### Scenario: PluginLoader 集成点

- **WHEN** `PluginLoader::load_so(path, ...)` 被调用
- **THEN** MUST 先调 `ManifestFinder::find(path)` 定位 manifest
- **AND** 找到 → 走 manifest-first 流程 (per `plugin-loader` delta spec)
- **AND** 未找到 → 按 `require_manifest` 参数决定 (per `plugin-loader` delta spec)
