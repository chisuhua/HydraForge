# pdk-manifest-validation Tasks

## 1. Setup & Types

- [ ] 1.1 新增 `include/agenticdsl/pdk/manifest.h` — 定义 `Manifest`, `ToolSpec`, `Resources`, `ValidationError`, `ManifestValidationResult` POD 结构(零 nlohmann 依赖, 纯数据)
- [ ] 1.2 新增 `include/agenticdsl/pdk/manifest_validator.h` — 声明 `ManifestValidator` 类 + 静态 `validate(const std::string& json_content) -> ManifestValidationResult`
- [ ] 1.3 新增 `include/agenticdsl/pdk/manifest_finder.h` — 声明 `ManifestFinder` 类 + 静态 `find(const std::filesystem::path& so_path) -> std::optional<std::filesystem::path>`
- [ ] 1.4 新增 `src/modules/pdk/CMakeLists.txt` — 链接 nlohmann_json, 生成 `agenticdsl_modules_pdk` 静态库 target (与 `src/modules/plugin/CMakeLists.txt` 同模式)
- [ ] 1.5 根 `CMakeLists.txt` 添加 `add_subdirectory(src/modules/pdk)`

## 2. TDD: ManifestValidator (RED → GREEN)

- [ ] 2.1 写失败测试 `test_pdk_manifest_validator.cpp::valid_minimal_manifest` (合法 manifest → valid=true, errors 为空) — RED
- [ ] 2.2 实现 `ManifestValidator::validate()` 最小骨架(读 JSON + 校验 9 必填字段类型 + 严格类型检查) — GREEN (1 test pass)
- [ ] 2.3 写失败测试 `::missing_required_field_id` (缺 `id` 字段 → valid=false, errors 含 field="id" reason="required") — RED → GREEN
- [ ] 2.4 写失败测试 `::invalid_semver_version` (version="1.0" → invalid_semver error) — RED → GREEN
- [ ] 2.5 写失败测试 `::abi_version_out_of_range` (abi_version=3 → mismatch error expected="1|2") — RED → GREEN
- [ ] 2.6 写失败测试 `::wrong_type_string_for_uint32` (`"abi_version": "1"` string → wrong_type error) — RED → GREEN
- [ ] 2.7 写失败测试 `::wrong_type_null_for_string` (`"name": null` → wrong_type error) — RED → GREEN
- [ ] 2.8 写失败测试 `::invalid_implementation_forms_value` (`implementation_forms=["python"]` → invalid_enum error) — RED → GREEN
- [ ] 2.9 写失败测试 `::entry_tool_not_in_provided_tools` (entry_tool="foo" but provided_tools=["bar"] → cross-field error) — RED → GREEN
- [ ] 2.10 写失败测试 `::tools_missing_input_schema` (tools[0] 缺 input_schema → required error) — RED → GREEN
- [ ] 2.11 写失败测试 `::invalid_approval_policy_value` (approval_policy="random" → invalid_enum error) — RED → GREEN
- [ ] 2.12 跑 `ctest -R test_pdk_manifest_validator` 验证 11+ case 全部 PASS

## 3. TDD: ManifestFinder (路径查找 + symlink 安全)

- [ ] 3.1 写失败测试 `test_pdk_manifest_finder.cpp::find_manifest_same_dir` (`.so` 同目录有 manifest → 找到) — RED
- [ ] 3.2 实现 `ManifestFinder::find()` 最小骨架(同目录查找) — GREEN (1 test pass)
- [ ] 3.3 写失败测试 `::find_manifest_parent_dir` (向上 1 层有 manifest) — RED → GREEN (扩展为向上遍历)
- [ ] 3.4 写失败测试 `::find_manifest_not_found` (向上到根仍未找到 → nullopt) — RED → GREEN
- [ ] 3.5 写失败测试 `::find_manifest_closest_wins` (多层都有 → 浅层优先) — RED → GREEN
- [ ] 3.6 写失败测试 `::find_manifest_symlink_resolved` (`.so` 是 symlink → weakly_canonical 后 walk) — RED → GREEN
- [ ] 3.7 写失败测试 `::find_manifest_max_depth_16` (向上 16 层仍未找到 → nullopt) — RED → GREEN
- [ ] 3.8 写失败测试 `::find_manifest_permission_denied_skip` (路径上有权限拒绝目录 → 跳过, 继续) — RED → GREEN
- [ ] 3.9 跑 `ctest -R test_pdk_manifest_finder` 验证 8+ case 全部 PASS

## 4. PluginLoader Integration (manifest-first load)

- [ ] 4.1 改 `include/agenticdsl/plugin/plugin_loader.h` 加可选参数 `bool require_manifest = false` (默认向后兼容) + `void set_interaction_bus(IInteractionBus* bus)` setter + `void clear_interaction_bus()` 重置 (PIMPL 模式, 内部 `unique_ptr<Impl>` 持有 bus 弱引用)
- [ ] 4.2 改 `src/modules/plugin/plugin_loader.cpp` `load_so()` 函数 — 在 dlopen 之前插入 `ManifestFinder::find()` + `ManifestValidator::validate()` 流程
  - 校验失败时 `dlopen` 不执行, emit `plugin.manifest.invalid` 事件
  - 缺 manifest 时按 `require_manifest` 决定行为
  - PluginInfo.abi_version 与 manifest.abi_version 不一致 → emit warn (PluginInfo 优先)
- [ ] 4.3 改 `load_all()` 同步应用 manifest-first 流程
- [ ] 4.4 写失败测试 `test_plugin_loader_manifest.cpp::load_with_valid_manifest` (manifest-first 路径 PASS) — RED
- [ ] 4.5 写失败测试 `::load_with_invalid_manifest_rejected` (manifest 校验失败 → 返回 false, 不调 dlopen) — RED → GREEN
- [ ] 4.6 写失败测试 `::load_without_manifest_warn_continue` (缺 manifest + require_manifest=false → warn, 继续 dlopen) — RED → GREEN
- [ ] 4.7 写失败测试 `::load_with_require_manifest_true_missing` (require_manifest=true + 缺 manifest → 拒绝) — RED → GREEN
- [ ] 4.8 写失败测试 `::load_all_with_mixed_manifests` (load_all 扫描: 部分有 manifest 部分无 → 应用不同分支) — RED → GREEN
- [ ] 4.9 跑 `ctest -R test_plugin_loader_manifest` 验证 5+ new case 全部 PASS

## 5. EventBus Integration (per ADR-0031 §决策 5 + ADR-0068)

- [ ] 5.1 注入 `IInteractionBus*` 通过 `set_interaction_bus()` setter (默认 nullptr, 静默跳过 emit) — 已在 §4.1 实施
- [ ] 5.2 manifest 校验失败时 emit `plugin.manifest.invalid` (含 path + errors[] + trace_id)
- [ ] 5.3 缺 manifest 时 emit `plugin.manifest.missing` (含 path + fallback_loaded)
- [ ] 5.4 写失败测试 `::emits_invalid_manifest_event` (使用 InMemoryBus mock) — RED → GREEN
- [ ] 5.5 写失败测试 `::emits_missing_manifest_event` (warn event payload 验证) — RED → GREEN
- [ ] 5.6 写失败测试 `::no_bus_no_emit_silent` (set_interaction_bus 未调 → 静默跳过, 无 crash) — RED → GREEN
- [ ] 5.7 跑 `ctest -R test_plugin_loader_manifest -R emits` 验证 3+ event case 全部 PASS

## 6. Backward Compatibility 验证

- [ ] 6.1 跑全量 `ctest` 验证零回归(基线 140/143, 3 pre-existing 不变)
- [ ] 6.2 验证 12 个现有 PDK plugin 缺 manifest 仍可加载(legacy path)
- [ ] 6.3 验证 `examples/phase1_plugin_demo --load-plugin` 流程未破坏
- [ ] 6.4 验证 PluginLoader 构造签名零变化(grep 调用方 + 编译验证)

## 7. Architecture Compliance

- [ ] 7.1 验证 `src/modules/pdk/manifest*.cpp` + `src/modules/plugin/plugin_loader.cpp` 不引入 `src/modules/*` 反向依赖
- [ ] 7.2 验证 `agenticdsl_modules_pdk` + `agenticdsl_modules_plugin` 模块级编译通过(增量加入 manifest 后零回归)
- [ ] 7.3 跑 `tools/adr_lint.py` + `tools/docs_drift_audit.py` 验证 0 error
- [ ] 7.4 跑 `openspec validate pdk-manifest-validation --strict` 验证 EXIT 0
- [ ] 7.5 跑 `make -j$(nproc)` 验证全工程零编译错误

## 8. Commit & Sync

- [ ] 8.1 跑全量 `ctest` 确认 140/143 PASS(零新增回归)
- [ ] 8.2 按 git-master atomic 5+ commit 拆分(per unit: types / validator / finder / loader / events / tests / docs)
- [ ] 8.3 更新 ADR-0052 状态行:保持 ✅ Approved + 追加 ship 实施依据段(指向本 change)
- [ ] 8.4 更新 `docs/active-status.md` 追加本 change ship 记录
- [ ] 8.5 更新 `docs/superpowers/plans/2026-07-15-phase6-agentforge-mvp.md`:
  - §十 状态行: `active` → `superseded-by-audit-2026-08-09` + 引用本 change
  - §十一 追加 Wave 3-A 前置完成注记(per audit §4.3)
  - §七 决策日志追加 2026-08-09 行: 审计 supersession 决议
- [ ] 8.6 跑 `tools/check_roadmap_drift.py` 验证 0 CRITICAL(master plan 与 audit 一致)
- [ ] 8.7 `openspec archive pdk-manifest-validation` 归档

## Estimated Time

**总计 18-22 小时 (3 天日历, Solo dev 模式)**:
- Setup & Types: 1.5h
- ManifestValidator TDD: 4h
- ManifestFinder TDD: 2h
- PluginLoader Integration: 3h
- EventBus setter 注入: 1.5h
- 集成测试 + 调试: 3h
- 向后兼容验证: 1h
- Architecture Compliance: 0.5h
- Commit + Master plan 同步: 1.5h
- Buffer (集成调试 + spec 修订): 2-4h

参考 Wave 3-A 节奏(单 change 1-2 天/实施, 跨模块 wiring 已成熟), 估时上限 3 天日历是保守值, 实际可能 2-2.5 天 ship。
