# llama-engine-plugin Specification

> **Purpose**: 追踪 Phase 5 首个 engine plugin ship（C14 change 产出）
> **STATUS: ACTIVE** 🟡
> **关联 design**: `openspec/changes/phase5-llama-engine-plugin/proposal.md`
> **关联 tasks**: `openspec/changes/phase5-llama-engine-plugin/tasks.md`
> **关联 master plan**: `docs/superpowers/plans/2026-07-03-phase5-self-bootstrapping.md` §五
> **最后更新**: 2026-07-05

## ADDED Requirements

### Requirement: llama-engine-plugin-builds

`pdk/llama_engine/CMakeLists.txt` MUST 成功编译产出 `libhydraforge_llama_engine.so`。

#### Scenario: plugin 编译产出

- **WHEN** 运行 `cmake --build build -j$(nproc)`
- **THEN** exit 0
- **AND** `build/debug/pdk/llama_engine/libhydraforge_llama_engine.so` 存在

#### Scenario: plugin 元数据可读

- **WHEN** 运行 `nm -D build/debug/pdk/llama_engine/libhydraforge_llama_engine.so | grep pdk_plugin_info`
- **THEN** 输出包含 `pdk_plugin_info` 符号

---

### Requirement: llama-engine-plugin-registers-tools

`pdk_register_tools` MUST 注册 ≥6 个工具到 `IToolRegistry`。（SamplerStrategy 工具已按 Adversarial Review D1 决策删除）

#### Scenario: 6 工具注册清单

- **WHEN** plugin 加载完成
- **THEN** `IToolRegistry` 包含以下工具：
  - `inference/engine/init` (ReadOnly, agent approval)
  - `inference/engine/generate` (Execute, agent approval)
  - `inference/engine/stream` (Execute, agent approval)
  - `inference/engine/status` (ReadOnly, no approval)
  - `inference/model/load` (StateModify, plan approval)
  - `inference/model/unload` (StateModify, plan approval)
  - `inference/model/list` (ReadOnly, no approval)
  - `inference/model/switch` (StateModify, plan approval)

#### Scenario: ToolMetadata V2 完整

- **WHEN** 检查任一注册工具的 ToolMetadata
- **THEN** 包含 5 字段：name, description, category, approval_policy, allowed_layers

---
### Requirement: lib-inference-engine-md-upgraded

`lib/inference/engine.md` MUST 从 PLACEHOLDER 升级为引用 `inference/engine/init` 工具的真实 schema。

#### Scenario: engine.md 真实 schema

- **WHEN** 读取 `lib/inference/engine.md`
- **THEN** 文件存在
- **AND** 顶部**不**包含 `⚠️ PLACEHOLDER` 标记
- **AND** YAML signature 包含 `(model_path, n_ctx, n_gpu_layers) -> (status, engine_id)`
- **AND** 包含 `## /init` tool_call 节点引用 `inference/engine/init` 工具

#### Scenario: 默认值合理

- **WHEN** 检查 engine.md 参数默认值
- **THEN** `n_ctx` 默认 `2048`
- **AND** `n_gpu_layers` 默认 `0`

---

### Requirement: lib-inference-model-md-upgraded

`lib/inference/model.md` MUST 从 PLACEHOLDER 升级为引用 `inference/model/load` 工具的真实 schema。

#### Scenario: model.md 真实 schema

- **WHEN** 读取 `lib/inference/model.md`
- **THEN** 文件存在
- **AND** 顶部**不**包含 `⚠️ PLACEHOLDER` 标记
- **AND** YAML signature 包含 `(path, name) -> (status, model_id)`
- **AND** 包含 `## /load` tool_call 节点引用 `inference/model/load` 工具

---

### Requirement: dslengine-explicit-load-plugin（D5 Option B）

`DSLEngine` MUST 暴露 `bool load_plugin(const std::string& name)` 公开 API，用于显式加载 PDK plugin。DSLEngine 构造**不**自动加载任何 plugin（按 D5 决策，删除原默认注入 + fallback 设计）。

#### Scenario: 构造不加载 plugin

- **WHEN** DSLEngine 构造
- **THEN** **不**调用 PluginLoader
- **AND** `plugin_loader_` 成员**保持** nullptr
- **AND** IToolRegistry **不**自动注册 inference/engine/* 或 inference/model/* 工具

#### Scenario: 显式 load_plugin 成功

- **WHEN** 调用 `engine.load_plugin("pdk/llama_engine")`（plugin .so 可用）
- **THEN** PluginLoader dlopen 成功
- **AND** 12 工具（4 engine + 4 model + 4 架构）注册到 IToolRegistry
- **AND** 返回 `true`

#### Scenario: 显式 load_plugin 失败 — WARN log 不抛异常

- **WHEN** 调用 `engine.load_plugin("pdk/nonexistent")`（plugin .so 不存在）
- **THEN** PluginLoader dlopen 失败
- **AND** DSLEngine **不**抛异常
- **AND** 输出 WARN log: `"Failed to load plugin '<name>': <error>"`
- **AND** 返回 `false`
- **AND** **不** fallback 到任何内嵌实现

#### Scenario: BREAKING 迁移指引

- **WHEN** 现有测试/示例使用 DSLEngine
- **THEN** **必须** 添加 `engine.load_plugin("pdk/llama_engine")` 显式调用
- **AND** 删除任何对 LlamaAdapter fallback 的依赖

#### Scenario: 析构时正确清理

- **WHEN** DSLEngine 析构
- **THEN** PluginLoader 正确卸载 plugin（dlclose）
- **AND** 内存零泄漏（ASan 验证）

---

### Requirement: tests-test-llama-engine-plugin-shipped

`tests/test_llama_engine_plugin.cpp` MUST 包含 ≥12 test cases，覆盖 plugin dlopen / ABI 匹配 / 12 工具（4 engine + 4 model + 4 C13 架构）/ generate/model lifecycle / sampler clamp / D5 load_plugin 显式注入。

#### Scenario: 12 test cases 覆盖

- **WHEN** 检查 `tests/test_llama_engine_plugin.cpp`
- **THEN** 包含以下 TEST_CASE：
  1. `plugin_dlopen_succeeds` — dlopen + dlsym pdk_register_tools
  2. `pdk_plugin_info_abi_version_matches` — ABI 版本断言
  3. `inference_engine_init_tool_registered` — registry 查询
  4. `inference_engine_generate_returns_text` — 同步生成 mock
  5. `inference_engine_stream_integrates_with_yield` — C12 YIELD 集成
  6. `inference_model_load_unload_lifecycle` — load + unload 序列
  7. `inference_model_switch_active` — switch 工具调用
  8. `prefix_cache_configure_registers` — C13 架构工具注册
  9. `kv_cache_configure_registers` — C13 架构工具注册
  10. `decoding_configure_sampler_clamp` — D1 决策验证：sampler clamp 内联 + 5 种字符串合法值校验
  11. `cloud_engine_configure_placeholder_stub` — C13 cloud_engine PLACEHOLDER 验证
  12. `dslengine_explicit_load_plugin_returns_true` — D5 验证：显式 load_plugin 返回 true

#### Scenario: 全部 12 测试 PASS

- **WHEN** 运行 `ctest -R test_llama_engine_plugin`
- **THEN** exit 0
- **AND** 12/12 PASS

#### Scenario: 零回归

- **WHEN** 运行完整 `ctest`
- **THEN** 76/76 PASS（64 baseline + 12 new）

---

### Requirement: pdk-cmakelists-integration

根 `pdk/CMakeLists.txt` MUST 添加 `add_subdirectory(llama_engine)` 行。

#### Scenario: CMake 子目录集成

- **WHEN** 读取 `pdk/CMakeLists.txt`
- **THEN** 包含 `add_subdirectory(llama_engine)` 行
- **AND** 行位置在 `add_subdirectory(model_router)` 之后

#### Scenario: plugin 与 PDK 头文件依赖正确

- **WHEN** 编译 `libhydraforge_llama_engine.so`
- **THEN** 链接 `hydraforge_pdk` INTERFACE 库（PDK 头文件）
- **AND** 链接 `agenticdsl_core` 静态库（核心实现）
- **AND** 链接 `llama` 库（llama.cpp）

---

### Requirement: documentation-synced

3 处文档 MUST 同步本 change ship 状态：AGENTS.md / master plan / ADR-0021。

#### Scenario: AGENTS.md 同步

- **WHEN** 读取 `AGENTS.md` CODE MAP
- **THEN** 包含 `pdk/llama_engine/` 行（与 `pdk/model_router/` 并列）

#### Scenario: master plan 同步

- **WHEN** 读取 `docs/superpowers/plans/2026-07-03-phase5-self-bootstrapping.md` §五
- **THEN** `pdk/llama_engine/` 标记 ✅ shipped
- **AND** §十一.2 Adjustment Log: C14 ship 条目

#### Scenario: ADR-0021 同步

- **WHEN** 读取 `docs/adr/adr-0021-pdk-design.md`
- **THEN** 追加 engine plugin 范式说明
- **AND** 引用 `pdk/llama_engine/` 作为首个 engine plugin 实例