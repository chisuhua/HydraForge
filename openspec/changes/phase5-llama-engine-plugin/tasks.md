# Tasks: Phase 5 Llama Engine Plugin (C14)

> **STATUS: ACTIVE** 🟡
> **关联 proposal**: `proposal.md`
> **关联 spec**: `specs/llama-engine-plugin/spec.md`
> **关联 master plan**: `docs/superpowers/plans/2026-07-03-phase5-self-bootstrapping.md` §五
> **关联 Oracle session**: `ses_0ce717ac4ffejvLa2We0gzbuds`
> **前置依赖**: C12 ✅ archived + C13 🟡 (parallel)
> **后续依赖**: C15 (BatchingQueue plugin)
> **估时**: 2-3 天 (原 1-1.5 天，因 Oracle 审查新增 4 个架构工具注册 + D5 显式 load_plugin API 改造)
> **最后更新**: 2026-07-07 (Oracle 审查后修订 — P0 阻塞项修复)

---

## 1. pdk/llama_engine/ plugin 骨架

- [ ] 1.1 创建 `pdk/llama_engine/CMakeLists.txt` (~40 行)
  - 参考 `pdk/model_router/CMakeLists.txt` 结构
  - add_library(hydraforge_llama_engine SHARED ...) — 输出 .so
  - target_link_libraries(hydraforge_llama_engine PRIVATE hydraforge_pdk agenticdsl_core llama)
  - set_target_properties PROPERTIES PREFIX "lib" OUTPUT_NAME "hydraforge_llama_engine"
- [ ] 1.2 创建 `pdk/llama_engine/README.md` (~80 行)
  - Plugin 用途说明
  - 注册的 12 个工具列表（inference/engine/{init,generate,stream,status} × 4 + inference/model/{load,unload,list,switch} × 4 + **C13 架构工具** prefix_cache/kv_cache/decoding/cloud_engine 各 .configure × 4）
  - 与 C12 YIELD 集成说明
  - 构建/加载命令
  - ABI 版本策略

## 2. B2.1 engine plugin 实现

- [ ] 2.1 创建 `pdk/llama_engine/src/llama_engine.cpp` (~150 行)
  - `extern "C" void pdk_register_tools(IToolRegistry&)` 注册 4 个工具
  - inference/engine/init (从 llm_config.json 读取配置, 调用 LlamaAdapter 初始化)
  - inference/engine/generate (同步生成, 返回 text)
  - inference/engine/stream (与 C12 IGenerationStream 集成)
  - inference/engine/status (返回引擎状态 JSON)
- [ ] 2.2 复用 `pdk/model_router/cost_strategy/cost_router.cpp` 模式
  - `pdk_register_tools` + ToolMetadata 模式 (支持 `/` 路径)
  - ToolMetadata V2: 4 字段 (category, approval_policy, allowed_layers, layer 等)
- [ ] 2.3 验证 llama.cpp 调用不直接发生（通过现有 LlamaAdapter 间接调用）

## 3. B2.2 model plugin 实现

- [ ] 3.1 创建 `pdk/llama_engine/src/llama_model.cpp` (~100 行)
  - `extern "C" void pdk_register_tools(IToolRegistry&)` 注册 4 个工具
  - inference/model/load (调用 LlamaAdapter 加载模型路径)
  - inference/model/unload (释放模型)
  - inference/model/list (返回已加载模型列表)
  - inference/model/switch (切换活跃模型)
- [ ] 3.2 与 engine 工具职责边界：
  - engine 是"推理能力"（生成 token）
  - model 是"模型资源"（加载/管理 GGUF 文件）
  - 两者解耦：可独立升级

## 4. C13 架构工具注册（Oracle 审查补充 — P0 阻塞项修复）

> **背景**: C13 定义了 4 个架构层工具的 schema 契约，但 C14 原提案仅包含 8 个 engine/model 工具，
> **遗漏**了 `prefix_cache.configure` / `kv_cache.configure` / `decoding.configure` / `cloud_engine.configure` 的注册任务。
> 本 section 将 4 个架构工具纳入 C14 实施范围，确保 schema 定义的每个工具都有对应的 C++ 注册。

- [ ] 4.1 创建 `pdk/llama_engine/src/inference_arch.cpp` (~100 行)
  - `extern "C" void pdk_register_arch_tools(IToolRegistry&)` 注册 4 个架构工具
  - prefix_cache.configure — 委托 llama.cpp 内置 prefix cache（enabled + max_size）
  - kv_cache.configure — 委托 llama.cpp KV cache 策略（evict_policy: lru/lfu/fifo + max_size_gb）
  - decoding.configure — 委托 llama.cpp sampling API（temperature/top_p/top_k/repeat_penalty/sampler: 5 种字符串选项）
  - cloud_engine.configure — **PLACEHOLDER stub**（返回 "not yet implemented"，等 Phase 5 Stage 2+）
- [ ] 4.2 ToolMetadata 规范（与 engine/model 工具一致）
  - category: ToolCategory::Cognitive（架构层配置属认知操作）
  - approval_policy: ApprovalPolicy::Plan（配置变更需计划审批）
  - allowed_layers: LayerProfile::ReadOnly（架构工具仅读操作）
- [ ] 4.3 `decoding.configure` 采样器 clamp 逻辑内联（按 D1 决策，不创建独立 SamplerStrategy PDK 接口）
- [ ] 4.4 验证 C13 4 个 schema 文件引用的工具名与 C++ 注册名一致：
  - `lib/inference/prefix_cache.md` ↔ `prefix_cache.configure` ✅
  - `lib/inference/kv_cache.md` ↔ `kv_cache.configure` ✅
  - `lib/inference/decoding.md` ↔ `decoding.configure` ✅
  - `lib/inference/cloud_engine.md` ↔ `cloud_engine.configure` ✅ (PLACEHOLDER)

## 5. 已删除（按 Adversarial Review D1 决策 — SamplerStrategy 接口推迟）

## 6. pdk_plugin_info 元数据

- [ ] 5.1 创建 `pdk/llama_engine/src/pdk_plugin_info.cpp` (~20 行)
  - `extern "C" const hydraforge::PluginInfo pdk_plugin_info`
  - abi_version: CURRENT_ABI_VERSION (= 1)
  - name: "hydraforge_llama_engine"
  - version: 1.0.0
  - description: "Llama.cpp reference engine plugin — load/generate/stream + model management + sampling"
  - capabilities: {"engine", "model", "sampler"}

## 6. lib/inference schema 升级

- [ ] 6.1 升级 `lib/inference/engine.md` (从 PLACEHOLDER → 真实 schema)
  - 移除顶部 PLACEHOLDER 标记
  - YAML signature: `(model_path, n_ctx, n_gpu_layers) -> (status, engine_id)`
  - tool_call 节点引用 `inference/engine/init` 工具
- [ ] 6.2 升级 `lib/inference/model.md` (从 PLACEHOLDER → 真实 schema)
  - YAML signature: `(path, name) -> (status, model_id)`
  - tool_call 节点引用 `inference/model/load` 工具
- [ ] 6.3 保留与 B2.3/B2.4/B2.5 (C13) 的交叉引用（prefix_cache + kv_cache + decoding schema 已 ship）

## 7. DSLEngine 默认 plugin 注入

- [ ] 7.1 修改 `src/core/engine.cpp` DSLEngine 构造 (~30 行)
  - 尝试加载 `libhydraforge_llama_engine.so`
  - 加载失败 fallback 到内嵌 LlamaAdapter（不抛异常, 仅 WARN log）
- [ ] 7.2 验证向后兼容：
  - 现有 64 测试零回归（构造时不抛异常）
  - examples/* 正常运行
- [ ] 7.3 析构时正确卸载 plugin

## 8. 测试

- [ ] 8.1 创建 `tests/test_llama_engine_plugin.cpp` (~250 行, ≥7 test cases)
  - TEST_CASE: plugin dlopen 成功
  - TEST_CASE: pdk_plugin_info ABI version 匹配
  - TEST_CASE: inference/engine/init 工具注册成功
  - TEST_CASE: inference/engine/generate 同步生成返回结果
  - TEST_CASE: inference/engine/stream 与 C12 YIELD 集成
  - TEST_CASE: inference/model/load + unload 生命周期
  - TEST_CASE: inference/model/switch 切换活跃模型
- [ ] 8.2 CMakeLists.txt: 添加 test_llama_engine_plugin target, link hydraforge_llama_engine

## 9. pdk/CMakeLists.txt 集成

- [ ] 9.1 修改根 `pdk/CMakeLists.txt` (+5 行)
  - `add_subdirectory(llama_engine)` (在 model_router 之后)
- [ ] 9.2 验证构建：
  - `cmake --build build -j$(nproc)` 100% 通过
  - `cmake --build build/pdk -j$(nproc)` plugin 编译通过
  - `libhydraforge_llama_engine.so` 产物存在

## 10. 文档同步

- [ ] 10.1 更新 `AGENTS.md` CODE MAP (+3 行)
  - pdk/llama_engine/ 加入代码地图
- [ ] 10.2 更新 `docs/superpowers/plans/2026-07-03-phase5-self-bootstrapping.md` (~15 行)
  - §五: 标记 pdk/llama_engine/ ship
  - §十一.2 Adjustment Log: C14 ship 记录
- [ ] 10.3 更新 `docs/adr/adr-0021-pdk-design.md` (~10 行)
  - 追加 engine plugin 范式参考（pdk/llama_engine/ 是首个 engine plugin 实例）

## 11. 验证

- [ ] 11.1 `cmake --build build -j$(nproc)` 100% 编译通过
- [ ] 11.2 `cd build/debug && ctest --output-on-failure` 72/72 PASS (64 baseline + 8 new)
- [ ] 11.3 `cmake --build build/asan -j$(nproc)` 64+ tests PASS (新增 plugin 测试不引入新 leak)
- [ ] 11.4 `cmake --build build/tsan -j$(nproc)` 63+ tests PASS (新增 plugin 测试不引入新 race)
- [ ] 11.5 `python3 tools/adr_lint.py` exit 0
- [ ] 11.6 `python3 tools/docs_drift_audit.py` 0 DRIFT
- [ ] 11.7 `openspec validate phase5-llama-engine-plugin` exit 0

## 12. 提交与归档

- [ ] 12.1 Git 提交 1: `feat(pdk-llama-engine): add 4 engine + 4 model tools (load/generate/stream/status + load/unload/list/switch)`
- [ ] 12.2 Git 提交 2: `feat(pdk-llama-engine): register 4 C13 architecture tools (prefix_cache/kv_cache/decoding/cloud_engine)`
- [ ] 12.3 Git 提交 3: `feat(phase5-stdlib): upgrade engine.md + model.md from PLACEHOLDER to real schema`
- [ ] 12.4 Git 提交 4: `feat(engine): DSLEngine explicit load_plugin() API (D5 Option B)`
- [ ] 12.5 Git 提交 5: `test(pdk-llama-engine): add 12 test cases for plugin dlopen + tools + arch tools + sampler`
- [ ] 12.6 Git 提交 6: `docs(pdk-llama-engine): README + AGENTS + master plan + ADR-0021 update + D5 recorded`
- [ ] 12.7 Git 提交 7: `chore(openspec): mark C14 tasks complete (12 tools shipped) before archive`
- [ ] 12.8 `openspec archive phase5-llama-engine-plugin`

---

## 验证检查清单 (C14 ship gate)

- [ ] 1. pdk/llama_engine/ plugin 编译成功 (.so 产物存在)
- [ ] 2. 12 个工具正确注册 (inference/engine/{init,generate,stream,status} × 4 + inference/model/{load,unload,list,switch} × 4 + **C13 架构工具** prefix_cache.configure / kv_cache.configure / decoding.configure / cloud_engine.configure × 4)
- [ ] 3. lib/inference/engine.md + model.md 从 PLACEHOLDER 升级
- [ ] 4. DSLEngine 默认 plugin 注入成功（构造时 dlopen）+ fallback 测试通过
- [ ] 5. tests/test_llama_engine_plugin.cpp 12 cases 全部 PASS (7 engine/model + 4 架构工具 + 1 D5 注入)
- [ ] 6. ctest 76/76 PASS 零回归 (64 baseline + 12 新测试)
- [ ] 8. ASan ≥ 64/64 / TSan ≥ 64/64
- [ ] 9. pdk_plugin_info ABI version 匹配
- [ ] 10. docs_drift_audit 0 DRIFT / adr_lint exit 0
- [ ] 11. AGENTS.md / master plan / ADR-0021 三处文档同步
- [ ] 10. Git 5 commits pushed to origin/main
- [ ] 11. C14 archived via openspec archive

## 关联 change 状态

- ✅ C9 (ADR impl-scope audit) — archived 2026-07-03
- ✅ C10 (Lazy ModuleState) — archived 2026-07-03
- ✅ C11 (SessionRegistry) — archived 2026-07-04
- ✅ C12 (YIELD/STREAM) — archived 2026-07-04
- 🟡 **C13 (B2 Architecture Schemas)** — ACTIVE (本 change 并行)
- 🟡 **C14 (Llama Engine Plugin)** — ACTIVE (本 change)
- ⚪ C15 (BatchingQueue Plugin) — deferred, 等待第二个推理后端出现时启动 (按 Adversarial Review D2 决策)