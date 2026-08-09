# pdk-manifest-validation Proposal

## Why

当前 PDK Plugin 只有 C++ 导出的 `PluginInfo` POD 结构体(在 dlopen 后通过 dlsym 读取),缺乏加载前可读的机器可读 manifest。这意味着 OS **必须在加载 .so 之后**才能了解 Plugin,无法做轻量发现、版本校验、信任检查与工具 schema 自省。

这是 Phase 6 服务化重开的硬阻塞条件之一(ADR-0050 §Candidate B 启动条件 #1: PDK 生产化)。Wave 3-A 完整 ship 后 OpenSpec + AI orchestration 已验证成熟(2026-08-09 audit),可启动 Phase 6a。

**为什么现在**: Wave 3-A momentum 真实 + AgentForge 即将消费 PDK(ADR-0050 评估)+ 服务化前提要求 manifest 校验。

## What Changes

- 新增 `pdk_manifest.json` 文件格式 (per ADR-0052 §决策 1-3, 完整履行)
  - **必填字段 (9)**: `id`, `name`, `version`, `abi_version`, `min_host_version`, `max_host_version`, `implementation_forms[]`, `entry_tool`, `provided_tools[]`
  - **推荐字段 (8)**: `interface_versions[]`, `capabilities[]`, `input_schema`, `output_schema`, `requires_isolation`, `resources{}`, `publisher`, `trust_level`, `activation_events[]`
  - **可选字段 (1)**: `signature` (Phase 6a 仅记录不验签)
  - 位置: Plugin 根目录(与 `CMakeLists.txt` 同级)
- 新增 manifest 校验器 (`ManifestValidator` 类)
  - JSON Schema 校验 (必填字段类型/格式)
  - semver 校验 (`version`, `min_host_version`, `max_host_version`)
  - **双 ABI 支持**: 接受 `abi_version ∈ {1, 2}` (镜像 `SUPPORTED_ABI_VERSIONS[]`, per `plugin_info.h:77`)
  - 类型严格策略: 字段类型不匹配 MUST reject with `reason="wrong_type"` (避免 nlohmann::json 隐式转换)
  - tools[] 元素 schema 校验
- 新增 manifest 查找器 (`ManifestFinder` 类)
  - 从 `.so` 路径向上查找 (max 16 层 bound)
  - symlink 安全 (`std::filesystem::weakly_canonical()`)
  - 权限拒绝降级 (跳过 + 继续)
- `PluginLoader` 加载流程前移 (per ADR-0052 §决策 4)
  - 现有: `dlopen` → `dlsym(pdk_plugin_info)` → 验证
  - 新: 读 `pdk_manifest.json` → 校验 → `dlopen` → 交叉验证 PluginInfo 与 manifest 一致
  - 缺 manifest 时按 `require_manifest` 参数决定行为 (默认 `false` = warn 但继续)
  - PluginInfo 与 manifest 不一致时: `abi_version` 以 PluginInfo 为准 + warn, 其余字段 warn 即可
- IInteractionBus opt-in 注入 (per ADR-0031 §决策 5)
  - `set_interaction_bus(IInteractionBus*)` setter
  - 默认 `nullptr` 静默跳过 emit
  - 构造签名零变化
- 新增 11+ 单元测试 (ManifestValidator 全部路径)
- 新增 8+ 单元测试 (ManifestFinder 路径查找 + symlink 处理)
- 新增 5+ 集成测试 (PluginLoader 加载路径 + event emission)

**BREAKING**: 无 (向后兼容路径: 缺 manifest 时 warn 但不 fail load, 默认 `require_manifest=false`)

## Capabilities

### New Capabilities

- `pdk-manifest`: 定义 `pdk_manifest.json` 文件格式 (per ADR-0052 §决策 1-3 完整履行) + ManifestValidator 校验语义 (JSON Schema + semver + 双 ABI + 严格类型检查 + tools schema) + ManifestFinder 路径查找 (symlink 安全 + 权限降级 + max 16 层 bound)

### Modified Capabilities

- `plugin-loader`: 修改 `PluginLoader::load_so()` 流程 — 在 `dlopen` 之前先读 manifest 并校验。**delta** spec 必填, 描述新增的 manifest-first 流程 + 向后兼容语义 + IInteractionBus opt-in 注入 (per ADR-0031 §决策 5 ToolCoordinator 模式)。

## Impact

**Affected code**:
- `include/agenticdsl/pdk/manifest.h` (新增, public API)
- `include/agenticdsl/pdk/manifest_validator.h` (新增, public API)
- `include/agenticdsl/pdk/manifest_finder.h` (新增, public API)
- `src/modules/pdk/manifest.cpp` (新增, 纯数据结构定义, 零 nlohmann 依赖)
- `src/modules/pdk/manifest_validator.cpp` (新增, 实际校验逻辑)
- `src/modules/pdk/manifest_finder.cpp` (新增, 路径查找 + symlink 处理)
- `src/modules/pdk/CMakeLists.txt` (新增, 链接 nlohmann_json)
- `src/modules/plugin/plugin_loader.{h,cpp}` (修改 load_so 流程 + 加 setter 注入 IInteractionBus)
- `tests/test_pdk_manifest_validator.cpp` (新增)
- `tests/test_pdk_manifest_finder.cpp` (新增)
- `tests/test_plugin_loader_manifest.cpp` (新增, 扩展现有 plugin-loader 测试)

**Affected APIs**:
- `PluginLoader::load_so()`: 签名增加可选 `bool require_manifest = false` (默认向后兼容)
- 新增 `PluginLoader::set_interaction_bus(IInteractionBus*)` (setter 注入, opt-in per ADR-0031 §决策 5)
- 新增 `PluginLoader::clear_interaction_bus()` (重置)
- 新增 `ManifestValidator::validate(const std::string&) -> ManifestValidationResult` (static)
- 新增 `ManifestFinder::find(const std::filesystem::path& so_path) -> std::optional<std::filesystem::path>` (static)

**Affected systems**:
- 12 个现有 PDK plugin (后续迁移, 本 change 不阻塞; 缺 manifest 仅 warn)
- Phase 6b AgentForge 将首先按新 manifest 格式开发

**Dependencies**:
- `nlohmann::json` (已 vendored, `external/nlohmann_json/`)
- 不引入新第三方依赖

**Out of scope (Non-goals)**:
- ❌ 不实现 `.hfpkg` 包格式 (ADR-0052 §决策 1 提及, Phase 6a 不做)
- ❌ 不实现 Wasm 插件 manifest 嵌入 (ADR-0052 §决策 1 提及, Phase 6a 不做)
- ❌ 不实现 trust 签名验证 (per ADR-0052 §决策 7, Phase 6a 仅声明 `signature` 字段, 不做验签)
- ❌ 不迁移现有 12 个 PDK plugin (后续 follow-up change)
- ❌ 不实现跨 plugin 资源协调 (manifest 声明的资源仅记录, 不做全局调度)
- ❌ 不实现 `pdk_create_llm_provider` 符号交叉验证 (per ADR-0041, 推迟至 follow-up change, 因 LLM 类 plugin 验证需扩展 PluginInfo)

## Success Criteria

- [ ] `ManifestValidator` 单元测试 ≥11 case (valid / missing field / bad semver / abi mismatch / wrong type / null for string / invalid impl_form / entry_tool not in provided_tools / tools missing input_schema / invalid approval_policy)
- [ ] `ManifestFinder` 单元测试 ≥8 case (same dir / parent dir / not found / closest wins / symlink resolved / max depth 16 / permission denied / hidden dirs)
- [ ] `PluginLoader` 集成测试 ≥5 case (load_with_valid_manifest / load_with_invalid_manifest_rejected / load_without_manifest_warn_continue / load_with_require_manifest_true_missing / load_all_with_mixed_manifests)
- [ ] `IInteractionBus` 注入 + 失败事件 emit 单元测试 ≥3 case (emits_invalid_manifest_event / emits_missing_manifest_event / no_bus_no_emit_silent)
- [ ] 0 新 ctest 回归 (基线 140/143, 3 pre-existing 不变)
- [ ] ADR-0052 status 保持 ✅ Approved + 标注本 change 作为实施依据
- [ ] Master plan (`2026-07-15-phase6-agentforge-mvp.md`) §十 状态更新为 superseded-by-audit-2026-08-09
- [ ] 现有 12 个 PDK plugin 不被阻塞 (向后兼容, 缺 manifest 仅 warn)

## Estimated Time

**18-22 小时 (3 天日历, Solo dev 模式)**, 包括:
- Setup & Types: 1.5h
- ManifestValidator TDD: 4h
- ManifestFinder TDD: 2h
- PluginLoader Integration: 3h
- EventBus setter 注入: 1.5h
- 集成测试 + 调试: 3h
- 向后兼容验证: 1h
- Architecture compliance: 0.5h
- Commit + Master plan 同步: 1.5h
- Buffer (集成调试 + spec 修订): 2-4h
