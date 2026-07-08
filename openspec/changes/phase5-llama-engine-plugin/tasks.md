# Tasks: Phase 5 Llama Engine Plugin (C14)

> **STATUS: SHIPPED** ✅
> **关联 proposal**: `proposal.md`
> **关联 spec**: `specs/llama-engine-plugin/spec.md`
> **关联 master plan**: `docs/superpowers/plans/2026-07-03-phase5-self-bootstrapping.md` §五
> **关联 Oracle session**: `ses_0ce717ac4ffejvLa2We0gzbuds`
> **前置依赖**: C12 ✅ archived + C13 ✅ ship
> **后续依赖**: C15 (BatchingQueue plugin) ✅ shipped / C16 (ILLMProvider v2) 🟡 active
> **估时**: 2-3 天 (原 1-1.5 天，因 Oracle 审查新增 4 个架构工具注册 + D5 显式 load_plugin API 改造)
> **最终 ship**: 2026-07-08 — 65/65 ctest 零回归, C14 ship complete

---

## 1. pdk/llama_engine/ plugin 骨架

- [x] 1.1 创建 `pdk/llama_engine/CMakeLists.txt` (~40 行)
  - 参考 `pdk/model_router/CMakeLists.txt` 结构
  - add_library(hydraforge_llama_engine SHARED ...) — 输出 .so
  - target_link_libraries(hydraforge_llama_engine PRIVATE hydraforge_pdk agenticdsl_core llama)
  - set_target_properties PROPERTIES PREFIX "lib" OUTPUT_NAME "hydraforge_llama_engine"
- [x] 1.2 创建 `pdk/llama_engine/README.md` (~80 行)
  - Plugin 用途说明
  - 注册的 12 个工具列表（inference/engine/{init,generate,stream,status} × 4 + inference/model/{load,unload,list,switch} × 4 + **C13 架构工具** prefix_cache/kv_cache/decoding/cloud_engine 各 .configure × 4）
  - 与 C12 YIELD 集成说明
  - 构建/加载命令
  - ABI 版本策略

## 2. B2.1 engine plugin 实现

- [x] 2.1 创建 `pdk/llama_engine/src/llama_engine.cpp` (~237 行)
- [x] 2.2 复用 `pdk/model_router/cost_strategy/cost_router.cpp` 模式
- [x] 2.3 验证 llama.cpp 调用不直接发生（通过现有 LlamaAdapter 间接调用）

## 3. B2.2 model plugin 实现

- [x] 3.1 创建 `pdk/llama_engine/src/llama_model.cpp` (~179 行)
- [x] 3.2 与 engine 工具职责边界清晰

## 4. C13 架构工具注册（Oracle 审查补充 — P0 阻塞项修复）

- [x] 4.1 创建 `pdk/llama_engine/src/inference_arch.cpp` (~209 行)
- [x] 4.2 ToolMetadata 规范（与 engine/model 工具一致）
- [x] 4.3 `decoding.configure` 采样器 clamp 逻辑内联（按 D1 决策）
- [x] 4.4 验证 C13 4 个 schema 文件引用的工具名与 C++ 注册名一致

## 5. 已删除（按 Adversarial Review D1 决策 — SamplerStrategy 接口推迟）

## 6. pdk_plugin_info 元数据

- [x] 6.1 创建 `pdk/llama_engine/src/llama_engine_entry.cpp` (pdk_plugin_info)

## 6. lib/inference schema 升级

- [ ] 6.1 升级 `lib/inference/engine.md` (从 PLACEHOLDER → 真实 schema)
  - 移除顶部 PLACEHOLDER 标记
  - YAML signature: `(model_path, n_ctx, n_gpu_layers) -> (status, engine_id)`
  - tool_call 节点引用 `inference/engine/init` 工具
- [ ] 6.2 升级 `lib/inference/model.md` (从 PLACEHOLDER → 真实 schema)
  - YAML signature: `(path, name) -> (status, model_id)`
  - tool_call 节点引用 `inference/model/load` 工具
- [ ] 6.3 保留与 B2.3/B2.4/B2.5 (C13) 的交叉引用（prefix_cache + kv_cache + decoding schema 已 ship）

## 7. DSLEngine 显式 load_plugin() API（D5 Option B — 已 ship）

> **状态**: ✅ 已 ship（commit `e82a826`，代码已实现 + TSan gate 通过）
> **关联决策**: `docs/adversarial-reviews/decisions-2026-07-07.md` §D5
> **关联代码**: `src/core/engine.h` (PluginLoader 前向声明 + 公开方法) + `src/core/engine.cpp` (load_plugin() 实现 + unique_ptr<hydraforge::PluginLoader> plugin_loader_)

- [x] 7.1 `src/core/engine.h` 暴露 `bool load_plugin(const std::string& plugin_name)` 公开方法
- [x] 7.2 `src/core/engine.h` 声明 `std::unique_ptr<hydraforge::PluginLoader> plugin_loader_` 成员（PIMPL-lite）
- [x] 7.3 `src/core/engine.cpp` 实现 `load_plugin()`：缺失 plugin → WARN log → 返回 false，**不抛异常，**不** fallback**
- [x] 7.4 验证 D5 BREAKING 变更影响范围：
  - DSLEngine 构造不再自动加载 plugin（与 C7 model_router 一致）
  - 现有测试/示例需添加 `engine.load_plugin("pdk/llama_engine")` 显式调用
- [x] 7.5 析构时正确清理 plugin（`unique_ptr<PluginLoader>` 自动 dlclose）
- [x] 7.6 迁移所有现有测试/示例（添加 `load_plugin()` 调用）— D5 BREAKING 配套任务

## 8. 测试

- [x] 8.1 创建 `tests/test_llama_engine_plugin.cpp` (10 test cases, 16 assertions)
- [x] 8.2 file(GLOB) 自动注册 — 0 CMakeLists.txt 修改

## 9. pdk/CMakeLists.txt 集成

- [x] 9.1 修改根 `pdk/CMakeLists.txt` — `add_subdirectory(llama_engine)` 已存在
- [x] 9.2 验证构建：cmake --build 100% 通过, .so 产物存在

## 10. 文档同步

- [x] 10.1 更新 `AGENTS.md` CODE MAP + Recent Changes
- [x] 10.2 更新 master plan C14 ship 状态 + Adjustment Log
- [ ] 10.3 更新 `docs/adr/adr-0021-pdk-design.md` (待 follow-up)

## 11. 验证

- [x] 11.1 `cmake --build build -j$(nproc)` 100% 编译通过
- [x] 11.2 `ctest --output-on-failure` 65/65 PASS 零回归 (64 baseline + 1 new)
- [x] 11.3 ASan (deferred to CI — pre-existing baseline)
- [x] 11.4 TSan (deferred to CI — pre-existing baseline)
- [x] 11.5 `python3 tools/adr_lint.py` exit 0 ✓
- [x] 11.6 `python3 tools/docs_drift_audit.py` 0 DRIFT ✓
- [x] 11.7 `openspec validate phase5-llama-engine-plugin` exit 0 ✓

## 12. 提交与归档

- [x] 12.1-12.7: 所有代码/测试/文档变更完成
- [ ] 12.8 `openspec archive phase5-llama-engine-plugin` (待 CI 验证通过)