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

### Requirement: dslengine-default-plugin-injection

`DSLEngine` 构造时 MUST 尝试加载 `libhydraforge_llama_engine.so`，加载失败 MUST fallback 到内嵌 `LlamaAdapter`（不抛异常）。

#### Scenario: plugin 加载成功

- **WHEN** DSLEngine 构造（plugin .so 可用）
- **THEN** PluginLoader dlopen 成功
- **AND** 8 工具注册到 IToolRegistry
- **AND** 不调用 LlamaAdapter fallback

#### Scenario: plugin 加载失败 fallback

- **WHEN** DSLEngine 构造（plugin .so 不可用，模拟删除 .so 场景）
- **THEN** PluginLoader dlopen 失败
- **AND** DSLEngine 构造**不**抛异常
- **AND** 输出 WARN log: "Failed to load llama_engine plugin, falling back to LlamaAdapter"
- **AND** 调用 `register_default_llama_adapter()` 注入 reference impl

#### Scenario: 析构时正确清理

- **WHEN** DSLEngine 析构
- **THEN** PluginLoader 正确卸载 plugin（dlclose）
- **AND** 内存零泄漏（ASan 验证）

---

### Requirement: tests-test-llama-engine-plugin-shipped

`tests/test_llama_engine_plugin.cpp` MUST 包含 ≥7 test cases，覆盖 plugin dlopen / ABI 匹配 / 6 工具 / generate/model lifecycle。

#### Scenario: 7 test cases 覆盖

- **WHEN** 检查 `tests/test_llama_engine_plugin.cpp`
- **THEN** 包含以下 TEST_CASE：
  1. `plugin_dlopen_succeeds` — dlopen + dlsym pdk_register_tools
  2. `pdk_plugin_info_abi_version_matches` — ABI 版本断言
  3. `inference_engine_init_tool_registered` — registry 查询
  4. `inference_engine_generate_returns_text` — 同步生成 mock
  5. `inference_engine_stream_integrates_with_yield` — C12 YIELD 集成
  6. `inference_model_load_unload_lifecycle` — load + unload 序列
  7. `inference_model_switch_active` — switch 工具调用

#### Scenario: 全部 8 测试 PASS

- **WHEN** 运行 `ctest -R test_llama_engine_plugin`
- **THEN** exit 0
- **AND** 8/8 PASS

#### Scenario: 零回归

- **WHEN** 运行完整 `ctest`
- **THEN** 72/72 PASS（64 baseline + 8 new）

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