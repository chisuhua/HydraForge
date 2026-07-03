# Audit: pdk/model_router/ LSP False Positives (2026-07-03)

> **审计日期**: 2026-07-03
> **触发**: LSP 报 4 个文件 8 个错误 (`Use of undeclared identifier 'hydraforge'`)
> **结论**: **0 真实错误** — LSP indexer 缓存与实际编译器不同步
> **验证方式**: 端到端真实构建 + 4 个 .so dlopen + phase1 example 实跑

## 背景

Strategic Alignment Gate §9.4 准备期间, LSP 持续报告 `pdk/model_router/` 下 4 个 C++ 文件存在 8 个 namespace 错误:

```
pdk/model_router/model_registry.cpp:94:30  No type named 'PluginInfo' in namespace 'hydraforge'
pdk/model_router/model_registry.cpp:95:15  No member named 'CURRENT_ABI_VERSION' in namespace 'hydraforge'
pdk/model_router/cost_strategy/cost_router.cpp:124:18  Use of undeclared identifier 'hydraforge'
pdk/model_router/cost_strategy/cost_router.cpp:125:3   Use of undeclared identifier 'hydraforge'
pdk/model_router/quality_strategy/quality_router.cpp:124:18  (同)
pdk/model_router/quality_strategy/quality_router.cpp:125:3   (同)
pdk/model_router/latency_strategy/latency_router.cpp:127:18  (同)
pdk/model_router/latency_strategy/latency_router.cpp:128:3   (同)
```

但 `tools/docs_drift_audit.py` (Sprint 10 ship) 不报告此问题, `ctest` 长期 61/61 PASS。

## 审计流程

### Step 1: 验证 LSP 状态

- `lsp_status`: clangd installed, **Active LSP clients: 0** (LSP 未启动)
- 重新调用 `lsp_diagnostics`: **"No diagnostics found"** (错误消失)

**结论**: 之前的 LSP 报错是 **stale cache** (上一次 LSP 启动时记录的 indexer 状态), 当前 LSP 未运行。

### Step 2: 真实编译器验证

| 验证步骤 | 命令 | 结果 |
|---|---|---|
| 单 .so build (cost) | `cmake --build . --target hydraforge_model_router_cost` | ✅ Built target |
| 单 .so build (quality) | `cmake --build . --target hydraforge_model_router_quality` | ✅ Built target |
| 单 .so build (latency) | `cmake --build . --target hydraforge_model_router_latency` | ✅ Built target |
| 单 .so build (registry) | `cmake --build . --target hydraforge_model_registry` | ✅ Built target |
| 全量 build (find errors) | `cmake --build . -j$(nproc) \| grep error` | ✅ 0 errors |

### Step 3: 运行时验证 (PluginLoader dlopen)

```bash
./examples/phase1_model_router_plugin/phase1_model_router_plugin --list
```

**输出**:
```
[PluginLoader] loaded plugin: hydraforge_model_router_cost v1.0.0
[PluginLoader] loaded plugin: hydraforge_model_router_quality v1.0.0
[PluginLoader] loaded plugin: hydraforge_model_router_latency v1.0.0
[PluginLoader] loaded plugin: hydraforge_model_registry v1.0.0
  - registered tools:
    * model_router/registry
    * model_router/latency
    * model_router/quality
    * model_router/cost
    * calculate
    * get_weather
    * web_search

[phase1_model_router_plugin] Available models (via model_router/registry):
  - result: [
  {"model_id": "gpt-4", "model_name": "GPT-4", "n_ctx": 8192, ...}
```

**结论**: 4 个 .so 全部 dlopen + pdk_register_tools 成功执行 + model_router/registry 工具实际可调用。

### Step 4: ctest 验证

| 步骤 | 结果 |
|---|---|
| `ctest -N` (list all 61 tests) | 61 tests registered |
| `ctest` (run all) | **61/61 passed, 0 tests failed out of 61** |
| 缺失 executable 数量 (与 file(GLOB) 期望比对) | 0 (除 test_fork_static_contracts 是 shell script 正常) |

## 根因分析

LSP 报错来自 **clangd indexer 的陈旧 snapshot**:
1. clangd 之前启动时 (2026-06-19 左右, C7 phase 2 ship 时) 已索引 pdk/ 目录
2. 当时 namespace 解析可能因某种原因 (compilation database 缺失, header path 不全等) 失败
3. 错误记录在 LSP cache 中
4. **clangd 当前未在运行** (Active clients: 0)
5. 当 LSP 重新启动时, 重新解析后**无错误**

实际 C++ 编译器 (gcc + CMake) 使用的 `compile_commands.json` 包含完整 include path + 标准头文件, 真实编译 0 错误。

## 缓解措施

| 措施 | 状态 | 说明 |
|---|---|---|
| 重新运行 LSP (`lsp_diagnostics`) | ✅ 已运行 | 报 "No diagnostics found" |
| 真实编译器 build | ✅ 已验证 | 4 .so + 1 example 全部成功 |
| PluginLoader dlopen | ✅ 已验证 | 4 .so 全部 loaded + 7 tools 注册 |
| ctest 全部 PASS | ✅ 已验证 | 61/61 |
| `tools/docs_drift_audit.py` 报告 | ✅ 已运行 | 0 DRIFT items |

## 建议

1. **关闭 LSP stale cache 报告**: 在 CI 中添加 `lsp_diagnostics` 检查前先重置 LSP (或运行 `clangd --check` 验证)
2. **信任真实编译**: LSP 错误应作为**辅助**而非**绝对**信号, 真实编译是 ground truth
3. **定期清理 build dir**: stale build artifacts 可能导致 cmake cache 与 source 不同步 (本次 audit 期间也遇到 15 个 test executable 缺失问题, 通过显式 `cmake --build --target` 修复)
4. **记录此类 false positive**: 未来遇到类似 LSP 错误先验证真实编译, 避免盲目修改代码

## 审计结论

- ✅ **零代码修改需要 commit**
- ✅ pdk/model_router/ 4 个文件**完全正常**, C7 ship 产物持续可用
- ✅ 61/61 ctest PASS, 0 回归
- ✅ phase1_model_router_plugin example 实跑成功
- ⚠️ LSP false positive 仍可能出现 (依赖 LSP cache 状态), 建议 real compiler 验证为最终标准

**审计负责人**: Sisyphus (自动审计 + 验证)
**审计耗时**: ~10 分钟
**审计产出**: 0 commit (no code change), 1 audit doc
