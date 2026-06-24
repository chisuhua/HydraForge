# Tasks: PluginLoader + Phase 1 收官 (Sprint 5)

> **变更类型**: 真实实现 (4 sub-tasks per plan §Sprint 5)
> **关联 plan**: `.omo/plans/phase1-execution.md` §Sprint 5
> **关联 ADR**: docs/adr/adr-0022-plugin-loading.md (🔍 Proposed → ✅ Approved Sprint 5 ship)
> **关联 change**: `openspec/changes/2026-07-14-plugin-loader/`
> **创建日期**: 2026-06-16 (placeholder) → 2026-06-19 (filled)
> **修订说明**: W1D2.5 启动前置 → Sprint 5 启动时填充, 与 plan §Sprint 5 任务列表对齐

## Sprint 5 子任务 (4 commits, T5 = sync-pdk.sh 自动)

- [x] S5.T1 — feat(plugin): add PluginInfo POD + PluginLoader API (Sprint 5, ADR-0022) **(已 ship, 2026-06-24 STATUS NOTE A/B 补勾)**
- [x] S5.T2 — feat(plugin): implement PluginLoader with dlopen + ABI check (Sprint 5) **(已 ship, 2026-06-24 STATUS NOTE A/B 补勾)**
- [ ] S5.T3 — feat(plugin): extend phase1_plugin_demo with --load-plugin (Sprint 5) **(由 tech-debt-and-phase1-closure Step 4 完成)**
- [ ] S5.T4 — docs(adr+status+openspec): Phase 1 收官 + 5 ADR → ✅ Approved (Sprint 5) **(由 tech-debt-and-phase1-closure Step 5 完成)**
- [ ] S5.T5 — 自动: ship 后执行 `./scripts/sync-pdk.sh` (Dual-Repo 同步, 不单独 commit) **(由 tech-debt-and-phase1-closure Step 6 完成)**

## S5.T1: PluginInfo POD + PluginLoader API 头文件

- [x] **S5.T1.1** 新建 `include/agenticdsl/plugin/plugin_info.h` (~80 行)
  - `hydraforge` 命名空间
  - `PluginInfo` POD struct (abi_version + name[64] + 3 个版本字段 + description[256] + capabilities[512])
  - `inline constexpr uint32_t CURRENT_ABI_VERSION = 1`
  - 文件头注释: 功能描述 + ADR-0022 §1.2 + 设计依据 + 作者 + 最后修改日期

- [x] **S5.T1.2** 新建 `include/agenticdsl/plugin/plugin_loader.h` (~120 行)
  - `hydraforge` 命名空间
  - `PluginLoader` 类声明:
    - 构造 + 析构 (RAII, 自动 dlclose)
    - `load_all(ToolRegistry&) -> size_t` (扫描搜索路径)
    - `load_so(path, ToolRegistry&, strict_version=true) -> bool`
    - `list_loaded() -> vector<PluginInfo>`
    - `unload_plugin(name) -> bool`
  - 私有方法: `get_search_paths`, `check_compatibility`, `apply_path_whitelist`
  - 私有结构: `LoadedPlugin` (handle + info + path)

- [x] **S5.T1.3** 新建 `include/agenticdsl/plugin/CMakeLists.txt`
  - `add_library(agenticdsl_hdr_plugin INTERFACE)`
  - `target_include_directories` 指向 `include/`
  - `target_compile_features cxx_std_20`

- [x] **S5.T1.4** 根 `CMakeLists.txt` 修改 (单行 add_subdirectory 准备)
  - 添加 `add_subdirectory(include/agenticdsl/plugin)` (与 cognitive/contract/pdk 同级)

- [x] **S5.T1.5** 头文件独立编译验证
  - PluginInfo POD 字段正确
  - PluginLoader API 完整
  - 无 LSP error

**T1 验收**:
- [x] `plugin_info.h` + `plugin_loader.h` + `plugin/CMakeLists.txt` 存在 **(已 ship, 2026-06-24 STATUS NOTE A/B 补勾)**
- [x] 头文件独立编译通过 **(已 ship, 2026-06-24 STATUS NOTE A/B 补勾)**

## S5.T2: PluginLoader dlopen/dlsym 实现 + 测试

- [x] **S5.T2.1** 新建 `src/modules/plugin/plugin_loader.cpp` (~250 行)
  - Linux only (`#ifdef __linux__` 保护)
  - `dlopen(path, RTLD_NOW | RTLD_LOCAL)` 加载 .so
  - `dlsym(handle, "pdk_plugin_info")` 读取 PluginInfo
  - `dlsym(handle, "pdk_register_tools")` 读取注册函数
  - ABI 版本检查 (`check_compatibility`)
  - 路径白名单 (`apply_path_whitelist` Layer 1 安全)
  - 搜索路径优先级 (`get_search_paths`: env > ./plugins > ~/.hydraforge/plugins > /usr/local)
  - RAII: 析构时 dlclose 所有 handle

- [x] **S5.T2.2** 新建 `src/modules/plugin/CMakeLists.txt`
  - `add_library(agenticdsl_modules_plugin STATIC plugin_loader.cpp)`
  - `target_link_libraries` PUBLIC agenticdsl_includes agenticdsl_common agenticdsl_core
  - `target_include_directories` PRIVATE src/

- [x] **S5.T2.3** 根 `CMakeLists.txt` 修改 (单行 add_subdirectory)
  - 添加 `add_subdirectory(src/modules/plugin)`

- [x] **S5.T2.4** 新建 `tests/test_plugin_loader.cpp` (~250 行, 5 test cases)
  - 测试 1: PluginInfo POD 字段验证 + 内存布局稳定
  - 测试 2: load_so 单个 .so + ABI 检查 (compile fixture .so)
  - 测试 3: list_loaded + unload_plugin 生命周期
  - 测试 4: 路径白名单 (拒绝 /etc /proc /sys 等敏感路径)
  - 测试 5: E2E 加载手工编译 .so + register_tools 调用验证

- [x] **S5.T2.5** 测试基础设施
  - Catch2 (沿用现有)
  - 手工编译的 test fixture .so (含 PluginInfo + register_tools)
  - 临时目录管理 (tmp_path)

- [x] **S5.T2.6** 测试编译 + 运行
  - 5/5 test case pass
  - 37/37 ctest pass (32 baseline + 5 new)
  - 零回归

**T2 验收**:
- [x] `plugin_loader.cpp` 存在且编译通过
- [x] 5/5 test case pass
- [x] 37/37 ctest pass (32 baseline + 5 new)

## S5.T3: 端到端 demo 扩展

- [ ] **S5.T3.1** 修改 `examples/phase1_plugin_demo/main.cpp` (~50 行新增)
  - 命令行参数解析:
    - `--mock` (默认, Sprint 0 fallback)
    - `--load-plugin=<path>` (加载单个 .so)
    - `--plugin-path=<path>` (扫描路径加载所有)
  - 互斥: `--mock` 与 `--load-plugin`/`--plugin-path` 二选一
  - 真实模式: 使用 PluginLoader + ToolRegistry 调用插件工具

- [ ] **S5.T3.2** 修改 `examples/phase1_plugin_demo/CMakeLists.txt`
  - 添加 `target_link_libraries(phase1_plugin_demo PRIVATE agenticdsl_modules_plugin)`
  - 添加 `target_link_libraries(phase1_plugin_demo PRIVATE agenticdsl_modules_cognitive)`
  - 添加 `target_include_directories` 指向 include/agenticdsl/plugin

- [ ] **S5.T3.3** 根 `CMakeLists.txt` (如需要)
  - 确保 `agenticdsl_modules_plugin` 与 `agenticdsl_modules_cognitive` 链接顺序

- [ ] **S5.T3.4** 构建 + 运行验证
  - `cmake --build build` 编译通过
  - `./phase1_plugin_demo --mock` (Sprint 0 fallback) 通过
  - `./phase1_plugin_demo --load-plugin=./plugins/test_plugin.so` (新模式) 加载并验证
  - `./phase1_plugin_demo --plugin-path=./plugins/` (扫描模式) 加载所有

**T3 验收**:
- [ ] demo 支持 3 种模式 (mock + load-plugin + plugin-path)
- [ ] E2E 真实 .so 加载验证通过

## S5.T4: Phase 1 收官 (5 ADR 状态变更 + 文档同步)

- [ ] **S5.T4.1** 5 个 ADR 状态行更新
  - `docs/adr/adr-0019-iinteraction-bus-mvp.md` §1.4: ✅ 已解决 → ✅ Approved
  - `docs/adr/adr-0020-thread-model-isolation.md`: 🟡 Partial → ✅ Approved
  - `docs/adr/adr-0021-pdk-design.md`: 🟡 Partial (Sprint 4) → ✅ Approved
  - `docs/adr/adr-0022-plugin-loading.md`: 🔍 Proposed → ✅ Approved
  - `docs/adr/adr-0023-tool-result-standard.md`: 🟡 Partial → ✅ Approved

- [ ] **S5.T4.2** STATUS-GLOSSARY 词汇表更新
  - `docs/adr-management/STATUS-GLOSSARY.md` 添加 ✅ Approved 完整定义 + 历史
  - 5 个 ADR 状态变更日志追加 (类似 ADR-0019 §状态变更日志 模式)

- [ ] **S5.T4.3** relationships.md 自动重新生成
  - 运行 `tools/adr_relationships.py` 重新生成
  - 验证 5 个 Approved ADR 关联图

- [ ] **S5.T4.4** 文档同步
  - `docs/roadmap-status.md`: Phase 1 80% → 100% (Sprint 5 完成)
  - `docs/phase1-roadmap.md`: Sprint 5 详细任务表 (T5.1-T5.4) 状态 ✅
  - `AGENTS.md`: Sprint 5 ship NOTE + 5 ADR Approved 标记

- [ ] **S5.T4.5** OpenSpec archive
  - `openspec archive 2026-07-14-plugin-loader -y --skip-specs`
  - 删除 working tree 中的 active change 文件

- [ ] **S5.T4.6** Phase 1 收官验证
  - `openspec validate 2026-07-14-plugin-loader` exit 0
  - `tools/adr_lint.py docs/adr/` exit 0
  - 37/37 ctest pass (32 baseline + 5 new)
  - CI 6 jobs 全绿 (本地构建验证)

**T4 验收**:
- [ ] 5 ADR ✅ Approved (主文件 + STATUS-GLOSSARY + relationships.md)
- [ ] Phase 1 智能体层进度: 80% → 100%
- [ ] `openspec validate` exit 0
- [ ] OpenSpec change 准备 archive

## S5.T5: 自动 sync-pdk.sh (Sprint 5 ship 后)

- [ ] **S5.T5.1** Sprint 5 ship commit 后, 自动执行:
  ```bash
  ./scripts/sync-pdk.sh
  ```
- [ ] **S5.T5.2** 验证 standalone hydraforge-pdk repo 收到新 sync commit
- [ ] **S5.T5.3** standalone repo CI (如已配置) 通过

**T5 验收** (集成在 Sprint 5 ship):
- [ ] sync-pdk.sh 成功执行
- [ ] standalone repo 同步 (commit + push)
- [ ] (可选) tag v0.2.0 标记 Sprint 5 收官

## 提交策略 (4 commits, per plan §Sprint 5)

```
S5.T1 → feat(plugin): add PluginInfo POD + PluginLoader API (Sprint 5, ADR-0022)
S5.T2 → feat(plugin): implement PluginLoader with dlopen + ABI check (Sprint 5)
S5.T3 → feat(plugin): extend phase1_plugin_demo with --load-plugin (Sprint 5)
S5.T4 → docs(adr+status+openspec): Phase 1 收官 + 5 ADR → ✅ Approved (Sprint 5)
S5.T5 → 自动: ship 后 sync-pdk.sh (不单独 commit)
```

## 依赖与阻塞

- **Block by**: Sprint 0/1a/1b/2/3/4 全部 ship ✅ (v0.1.0 tag 已推送)
- **Block**: Phase 2 异步+EventBus (W6-W7, 2026-07-16 ~ 2026-07-30)
- **External**: 无 (Linux only, dlopen 编译时依赖)

<!-- STATUS NOTE (2026-06-24)
本 tasks.md 中 45 个未勾选项 [ ] 分类对账 (经 Oracle 审查 ses_108c2a3b0ffe012zA30ujXdHOP):

**A. T1 验收补漏 (2 项, 标 [x])**: T1 (PluginInfo POD + PluginLoader API) 已 ship,author 当时仅勾选 sub-task 未勾选验收项,补漏:
  - L51: `plugin_info.h` + `plugin_loader.h` + `plugin/CMakeLists.txt` 存在 ✅
  - L52: 头文件独立编译通过 ✅

**B. 主标题补漏 (2 项 of 5, 标 [x])**:
  - L12: S5.T1 — feat(plugin): add PluginInfo POD + PluginLoader API ✅
  - L13: S5.T2 — feat(plugin): implement PluginLoader with dlopen + ABI check ✅
  - (L14 S5.T3 / L15 S5.T4 / L16 S5.T5 暂不勾选,等本 change Step 4/5/6 完成)

**C-F. 本 change tech-debt-and-phase1-closure 解决 (33 项)**:
  - C. T3.1-T3.4 + T3 验收 (6 项) → Step 4 (P1.A plugin demo 3 modes)
  - D. T4.1-T4.6 + T4 验收 (10 项) → Step 5 (P1.B 5 ADR Approved + 文档)
  - E. T5.1-T5.3 + T5 验收 (6 项) → Step 6 (P1.C sync-pdk.sh + standalone)
  - F. Sprint 5 收官验收 11 项 → Step 7 (P1.D archive) 跟随完成勾选
  - 本 change 全部 ship 后,本 change 内 Step 7 一次性 flip 全部 [x] 标注于 tasks.md (S5.T1-T5 主标题 + 全部 sub-task + 验收)

**G. Phase 2/3/4 后续范围 (5 项, 显式 defer, 不在 Sprint 5 scope)**:
  - L208: Phase 2: PluginLifecycle 完整钩子 (on_load/on_unload) + 跨平台 dlopen 抽象
  - L209: Phase 2: ADR-0026 async_simple integration (Sprint 6+)
  - L210: Phase 3: PDK SafeExec 完整版 (fork/cgroups/seccomp) + plugin sandboxing
  - L211: Phase 3: Plugin health check + hot reload
  - L212: Phase 4: Plugin marketplace + 持久化 (配置文件 + 数据库)
  - ⏳ Defer 到独立 OpenSpec change `2026-07-xx-phase2-plugin-lifecycle` 等(待 Phase 2 启动时创建)
  - **明确不在** tech-debt-and-phase1-closure scope,避免 ship-as-is 假阳性

本 change ship gate 验证清单(V10 强化):archive 前 MUST 确认
  - [ ] 5 个 Phase 2/3/4 项目有显式 defer STATUS NOTE(本节)
  - [ ] 33 个 C/D/E/F 项目本 change 完成后 [x]
  - [ ] 7 个 A/B 项目补漏 [x]
  - [ ] 0 个 phantom flip
-->

## Sprint 5 收官验收

- [ ] 4 commits (T1 → T2 → T3 → T4) 已 push 到 origin
- [ ] 37/37 ctest PASS (32 baseline + 5 new test_plugin_loader)
- [ ] `openspec validate 2026-07-14-plugin-loader` exit 0
- [ ] `tools/adr_lint.py docs/adr/` exit 0
- [ ] 5 ADR ✅ Approved (主文件 + STATUS-GLOSSARY + relationships.md)
- [ ] Phase 1 智能体层进度: 80% → 100%
- [ ] OpenSpec change `2026-07-14-plugin-loader` 已 archive
- [ ] `./scripts/sync-pdk.sh` Sprint 5 ship 后成功执行
- [ ] standalone `hydraforge-pdk` repo 收到新 sync commit
- [ ] 零回归 (Sprint 0/1a/1b/2/3/4 全部 32 测试不变)
- [ ] Phase 1 → Phase 2 移交 (W6-W7 启动 Phase 2 异步+EventBus)

## Phase 2/3 后续范围

- [ ] Phase 2: PluginLifecycle 完整钩子 (on_load/on_unload) + 跨平台 dlopen 抽象
- [ ] Phase 2: ADR-0026 async_simple integration (Sprint 6+)
- [ ] Phase 3: PDK SafeExec 完整版 (fork/cgroups/seccomp) + plugin sandboxing
- [ ] Phase 3: Plugin health check + hot reload
- [ ] Phase 4: Plugin marketplace + 持久化 (配置文件 + 数据库)