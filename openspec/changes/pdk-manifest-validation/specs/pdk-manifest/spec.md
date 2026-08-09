# pdk-manifest Specification Deltas

**Capability**: `pdk-manifest` (新增)
**关联 ADR**: ADR-0052 (Agent Plugin Manifest 规范)
**关联审计**: 2026-08-09 Phase 6 re-evaluation post Wave 3-A

## Purpose

定义 `pdk_manifest.json` 文件格式与 `ManifestValidator` 校验语义,使 OS 能在不加载 `.so` 的前提下发现 PDK Plugin 元数据、校验版本兼容性、读取工具 schema。这是 Phase 6 服务化(MCP server 暴露 plugin 元数据)的硬前置,亦支撑 AgentForge MVP 复用 PDK。

## ADDED Requirements

### Requirement: pdk-manifest-format

`pdk_manifest.json` MUST 是符合 JSON Schema 2020-12 的机器可读文件,放置在 Plugin 根目录(与 `CMakeLists.txt` 同级),文件名固定为 `pdk_manifest.json`。

#### Scenario: 文件位置与命名

- **WHEN** Plugin 维护者创建 manifest
- **THEN** MUST 命名为 `pdk_manifest.json`(无大小写变体)
- **AND** MUST 放置在 Plugin 根目录(与 `CMakeLists.txt` 同级)
- **AND** MUST 包含 `$schema` 字段指向 `https://schemas.hydraforge.io/pdk-manifest-v1.json`

#### Scenario: 必填字段

- **WHEN** `ManifestValidator` 解析 manifest
- **THEN** 必填字段缺失 MUST 拒绝:
  - `id` (kebab-case string, max 64 字节)
  - `name` (human-readable string, max 128 字节)
  - `version` (semver 字符串, e.g. "0.1.0")
  - `abi_version` (uint32, MUST 等于 `CURRENT_ABI_VERSION=1`)
  - `min_host_version` (semver 字符串)
  - `max_host_version` (semver 字符串, MUST >= min_host_version)
  - `tools` (array, MAY 为空数组但 MUST 存在)

#### Scenario: 可选字段

- **WHEN** manifest 包含可选字段
- **THEN** MUST 接受以下可选字段(缺省时使用默认值):
  - `resources.timeout_ms` (uint32, 默认 30000)
  - `resources.max_concurrency` (uint32, 默认 1)
  - `signature` (string, Phase 6a 仅记录不验签)

### Requirement: pdk-manifest-validator

`ManifestValidator` MUST 是 `agenticdsl::pdk` 命名空间下的独立类,提供静态 `validate()` 方法,接受 JSON 字符串并返回 `ManifestValidationResult`。

#### Scenario: 有效 manifest 校验通过

- **WHEN** `ManifestValidator::validate(json_content)` 接收到符合 schema 的 manifest
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

#### Scenario: abi_version 不匹配

- **WHEN** `abi_version` 不等于 `CURRENT_ABI_VERSION=1`
- **THEN** MUST 返回 `valid=false`
- **AND** `errors` MUST 包含 `ValidationError{field="abi_version", reason="mismatch", expected=1, actual=<value>}`

#### Scenario: tools[] 元素 schema 非法

- **WHEN** `tools[]` 中某元素缺 `name` 或 `input_schema` 字段
- **THEN** MUST 返回 `valid=false`
- **AND** `errors` MUST 包含 `ValidationError{field="tools[<index>].<missing_field>", reason="required"}`

### Requirement: pdk-manifest-tools-schema

`tools[]` 数组中每个 tool 元素 MUST 携带完整 input_schema(JSON Schema 2020-12),用于 Phase 6c 服务化(MCP 暴露)与 AgentForge 工具发现。

#### Scenario: tool 必填字段

- **WHEN** `tools[]` 元素被校验
- **THEN** 必填字段缺失 MUST 拒绝:
  - `name` (kebab-case string, MUST 符合 ADR-0043 §决策 1 命名规范)
  - `description` (human-readable string)
  - `input_schema` (JSON Schema 2020-12 object)

#### Scenario: tool 可选字段

- **WHEN** manifest 包含 tool 可选字段
- **THEN** MUST 接受:
  - `output_schema` (JSON Schema 2020-12 object, 默认空)
  - `approval_policy` (string enum: "always" | "plan" | "agent" | "yolo", 默认 "plan")

#### Scenario: approval_policy 非法值

- **WHEN** `approval_policy` 字段值不在 4 个允许值内
- **THEN** MUST 返回 `valid=false`
- **AND** `errors` MUST 包含 `ValidationError{field="tools[<index>].approval_policy", reason="invalid_enum", allowed=["always","plan","agent","yolo"]}`

### Requirement: pdk-manifest-find-from-so-path

`PluginLoader` MUST 实现从 `.so` 路径向上查找 manifest 文件的算法,支持 `build/` 与源码根目录分离的常见布局。

#### Scenario: 同目录查找

- **WHEN** `.so` 路径为 `/path/to/plugin/build/libfoo.so`
- **AND** `/path/to/plugin/build/pdk_manifest.json` 存在
- **THEN** MUST 使用该 manifest

#### Scenario: 向上 1 层查找

- **WHEN** `.so` 路径为 `/path/to/plugin/build/libfoo.so`
- **AND** `/path/to/plugin/build/pdk_manifest.json` 不存在
- **AND** `/path/to/plugin/pdk_manifest.json` 存在
- **THEN** MUST 使用上层 manifest

#### Scenario: 找不到 manifest

- **WHEN** 从 `.so` 路径向上查找至文件系统根仍未找到 `pdk_manifest.json`
- **THEN** MUST 返回 `std::nullopt`(非错误,仅缺 manifest)
- **AND** `PluginLoader::load_so` 行为 MUST 由调用方的 `require_manifest` 参数决定(默认 `false` = warn 但继续)

#### Scenario: 多个 manifest 冲突

- **WHEN** 查找路径上存在多个 `pdk_manifest.json`
- **THEN** MUST 使用最接近 `.so` 路径的那一个(浅层优先)
- **AND** MUST 打印 debug 日志记录选择的路径
