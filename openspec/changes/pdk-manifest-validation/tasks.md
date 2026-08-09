# pdk-manifest-validation Tasks

## 1. Setup & Types

- [ ] 1.1 新增 `include/agenticdsl/pdk/manifest.h` — 定义 `Manifest`, `ToolSpec`, `Resources`, `ValidationError`, `ManifestValidationResult` POD 结构(零 nlohmann 依赖,纯数据)
- [ ] 1.2 新增 `include/agenticdsl/pdk/manifest_validator.h` — 声明 `ManifestValidator` 类 + 静态 `validate(const std::string& json_content) -> ManifestValidationResult`
- [ ] 1.3 新增 `src/common/pdk/CMakeLists.txt` — 链接 nlohmann_json,生成 `agenticdsl_common_pdk` 静态库 target

## 2. TDD: ManifestValidator (RED → GREEN)

- [ ] 2.1 写失败测试 `test_pdk_manifest.cpp::valid_minimal_manifest` (合法 manifest → valid=true,errors 为空) — RED
- [ ] 2.2 实现 `ManifestValidator::validate()` 最小骨架(读 JSON + 校验 6 必填字段类型) — GREEN (1 test pass)
- [ ] 2.3 写失败测试 `::missing_required_field` (缺 `id` 字段 → valid=false, errors 含 field="id") — RED → GREEN
- [ ] 2.4 写失败测试 `::invalid_semver` (version="1.0" → invalid_semver error) — RED → GREEN
- [ ] 2.5 写失败测试 `::abi_version_mismatch` (abi_version=2 → mismatch error expected=1) — RED → GREEN
- [ ] 2.6 写失败测试 `::tools_missing_input_schema` (tools[0] 缺 input_schema → required error) — RED → GREEN
- [ ] 2.7 写失败测试 `::invalid_approval_policy` (approval_policy="random" → invalid_enum error) — RED → GREEN
- [ ] 2.8 跑 `ctest -R test_pdk_manifest` 验证 6+ case 全部 PASS

## 3. TDD: ManifestFinder (路径查找)

- [ ] 3.1 写失败测试 `::find_manifest_same_dir` (`.so` 同目录有 manifest → 找到) — RED
- [ ] 3.2 实现 `ManifestFinder::find(const std::string& so_path) -> std::optional<std::filesystem::path>` (同目录查找) — GREEN
- [ ] 3.3 写失败测试 `::find_manifest_parent_dir` (向上 1 层有 manifest) — RED → GREEN (扩展为向上遍历)
- [ ] 3.4 写失败测试 `::find_manifest_not_found` (向上到根仍未找到 → nullopt) — RED → GREEN
- [ ] 3.5 写失败测试 `::find_manifest_closest_wins` (多层都有 → 浅层优先) — RED → GREEN
- [ ] 3.6 跑 `ctest -R test_pdk_manifest` 验证 5+ 新 case 全部 PASS

## 4. PluginLoader Integration (manifest-first load)

- [ ] 4.1 改 `src/common/plugin_loader/plugin_loader.h` 加可选参数 `bool require_manifest = false` (默认向后兼容)
- [ ] 4.2 改 `src/common/plugin_loader/plugin_loader.cpp` `load_so()` 函数 — 在 dlopen 之前插入 manifest 读 + 校验逻辑
  - 失败时 `dlopen` 不执行,emit `plugin.manifest.invalid` 事件
  - 缺 manifest 时按 `require_manifest` 决定行为
- [ ] 4.3 改 `load_all()` 同步应用 manifest-first 流程
- [ ] 4.4 写失败测试 `test_plugin_loader_manifest.cpp::load_with_valid_manifest` — RED
- [ ] 4.5 跑 `ctest -R test_plugin_loader_manifest` 验证 manifest-first 路径 PASS

## 5. Event Emission (per ADR-0068)

- [ ] 5.1 在 `PluginLoader` 注入 `IInteractionBus*` (默认 `nullptr`, 通过 setter 注入)
- [ ] 5.2 manifest 校验失败时 emit `plugin.manifest.invalid` (含 path + errors[] + trace_id)
- [ ] 5.3 写测试 `::emits_invalid_manifest_event` (使用 InMemoryBus mock) — RED → GREEN

## 6. Backward Compatibility 验证

- [ ] 6.1 跑全量 `ctest` 验证零回归(基线 140/143,3 pre-existing 不变)
- [ ] 6.2 验证 6 个现有 PDK plugin 缺 manifest 仍可加载(legacy path)
- [ ] 6.3 验证 `examples/phase1_plugin_demo --load-plugin` 流程未破坏

## 7. Architecture Compliance

- [ ] 7.1 验证 `manifest.h` 头文件未引入 `src/modules/*` 反向依赖
- [ ] 7.2 验证 `agenticdsl_modules_executor` + `agenticdsl_modules_scheduler` 模块级编译通过
- [ ] 7.3 跑 `tools/adr_lint.py` + `tools/docs_drift_audit.py` 验证 0 error
- [ ] 7.4 跑 `openspec validate pdk-manifest-validation --strict` 验证 EXIT 0

## 8. Commit & Sync

- [ ] 8.1 跑全量 `ctest` 确认 140/143 PASS(零新增回归)
- [ ] 8.2 跑 `make -j$(nproc)` 确认零编译错误
- [ ] 8.3 按 git-master 5+ commit 拆分(per atomic unit: types / validator / finder / loader / events / tests / docs)
- [ ] 8.4 更新 ADR-0052 状态行:保持 ✅ Approved + 追加 ship 实施依据段
- [ ] 8.5 更新 `docs/active-status.md` 追加本 change ship 记录
- [ ] 8.6 `openspec archive pdk-manifest-validation` 归档

## Estimated Time

- Setup & Types: 1 小时
- Validator TDD: 3-4 小时
- Finder TDD: 1.5 小时
- PluginLoader Integration: 2-3 小时
- Event Emission: 1 小时
- Backward Compat: 0.5 小时
- Architecture Compliance: 0.5 小时
- Commit & Sync: 1 小时
- **总计: 10-12 小时(1.5-2 天日历,Solo dev 模式)**
