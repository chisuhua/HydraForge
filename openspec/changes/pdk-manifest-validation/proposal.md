# pdk-manifest-validation Proposal

## Why

当前 PDK Plugin 只有 C++ 导出的 `PluginInfo` POD 结构体(在 dlopen 后通过 dlsym 读取),缺乏加载前可读的机器可读 manifest。这意味着 OS **必须在加载 .so 之后**才能了解 Plugin,无法做轻量发现、版本校验、信任检查与工具 schema 自省。

这是 Phase 6 服务化重开的硬阻塞条件之一(ADR-0050 §Candidate B 启动条件 #1: PDK 生产化)。Wave 3-A 完整 ship 后 OpenSpec + AI orchestration 已验证成熟(2026-08-09 audit),可启动 Phase 6a。

**为什么现在**: Wave 3-A momentum 真实 + AgentForge 即将消费 PDK(ADR-0050 评估)+ 服务化前提要求 manifest 校验。

## What Changes

- 新增 `pdk_manifest.json` 文件格式(per ADR-0052 §决策 1-2)
  - 必填字段: `id`, `name`, `version`, `abi_version`, `min_host_version`, `max_host_version`, `tools[]`, `resources{}`
  - 位置: Plugin 根目录(与 `CMakeLists.txt` 同级)
- 新增 manifest 校验器(`ManifestValidator` 类)
  - JSON Schema 校验(必填字段类型/格式)
  - semver 校验(`version` 字段)
  - abi_version 硬匹配校验
  - 工具 schema 校验(tools[] 每项必须有 name + input_schema)
- `PluginLoader` 加载流程前移(per ADR-0052 §决策 4)
  - 现有: `dlopen` → `dlsym(pdk_plugin_info)` → 验证
  - 新: 读 `pdk_manifest.json` → 校验 → `dlopen` → 交叉验证 PluginInfo 与 manifest 一致
- 新增 5+ 单元测试(manifest 校验全部路径)
- 新增 1 集成测试(PluginLoader 加载路径含 manifest)

**BREAKING**: 现有 PDK plugin(`pdk/llama_engine/`, `pdk/model_router/`, `pdk/loop_agent/`, `pdk/fs_tools/`, `pdk/shell_tools/`, `pdk/provider_agent/`)必须在 Phase 6a 期间补充 `pdk_manifest.json`。本 change **不**强制要求现有 plugin 立即迁移(向后兼容路径: 缺 manifest 时 warn 但不 fail load,strict_mode 关闭)。

## Capabilities

### New Capabilities

- `pdk-manifest`: 定义 `pdk_manifest.json` 文件格式 + ManifestValidator 校验语义(JSON Schema + semver + abi + tools schema)

### Modified Capabilities

- `plugin-loader`: 修改 `PluginLoader::load_so()` 流程 — 在 `dlopen` 之前先读 manifest 并校验。**delta** spec 必填,描述新增的 manifest-first 流程与向后兼容语义。

## Impact

**Affected code**:
- `include/agenticdsl/pdk/manifest.h` (新增)
- `include/agenticdsl/pdk/manifest_validator.h` (新增)
- `src/common/pdk/manifest.{h,cpp}` (新增,放 src 避免 include 头重)
- `src/common/plugin_loader/plugin_loader.{h,cpp}` (修改 load_so)
- `tests/test_pdk_manifest.cpp` (新增)
- `tests/test_plugin_loader_manifest.cpp` (新增,扩展现有 plugin-loader 测试)

**Affected APIs**:
- `PluginLoader::load_so()`: 签名不变,但内部多一步 manifest 读 + 校验
- 新增 `ManifestValidator::validate(json_str) -> Result<Manifest, Error>`
- 新增 `PluginLoader::load_so_strict(path, registry, strict_version=true, require_manifest=false)` (可选 manifest 强制)

**Affected systems**:
- 6 个现有 PDK plugin(后续迁移,本 change 不阻塞)
- Phase 6b AgentForge 将首先按新 manifest 格式开发

**Dependencies**:
- `nlohmann::json` (已 vendored,`external/nlohmann_json/`)
- 不引入新第三方依赖

**Out of scope (Non-goals)**:
- ❌ 不实现 `.hfpkg` 包格式(ADR-0052 §决策 1 提及,Phase 6a 不做)
- ❌ 不实现 Wasm 插件 manifest 嵌入(ADR-0052 §决策 1 提及,Phase 6a 不做)
- ❌ 不实现 trust 签名验证(ADR-0052 §目标 4,Phase 6a 仅声明 `signature` 字段,不做验签)
- ❌ 不迁移现有 6 个 PDK plugin(后续 follow-up change)
- ❌ 不实现跨 plugin 资源协调(manifest 声明的资源仅记录,不做全局调度)

## Success Criteria

- [ ] `ManifestValidator` 单元测试 ≥5 case (valid manifest / missing field / bad semver / abi mismatch / tools schema invalid)
- [ ] `PluginLoader` 集成测试 ≥1 case (manifest-first load 路径)
- [ ] 0 新 ctest 回归(基线 140/143,3 pre-existing 不变)
- [ ] ADR-0052 status 保持 ✅ Approved + 标注本 change 作为实施依据
- [ ] 现有 plugin 不被阻塞(向后兼容,缺 manifest 仅 warn)

## Estimated Time

2-3 天(per Phase 6a step 1 估时),Solo dev 模式,使用 OpenSpec + AI orchestration 1-2 天/change 节奏。
