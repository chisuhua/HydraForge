# Proposal: PluginLoader + Phase 1 收官 (Sprint 5)

> **变更类型**: 真实实现 (新功能 + Phase 1 收官)
> **作者**: Sisyphus (Phase 1 Sprint 5 启动 + Phase 1 收官)
> **创建日期**: 2026-06-16 (placeholder) → 2026-06-19 (filled)
> **追溯范围**: `.omo/plans/phase1-execution.md` §Sprint 5
> **关联 ADR**: docs/adr/adr-0022-plugin-loading.md (Plugin Loading 设计) + ADR-0021 (PDK) + ADR-0020 (Thread Model) + ADR-0019 (IInteractionBus) + ADR-0023 (ToolResult)
> **前置**: Sprint 0/1a/1b/2/3/4 全部 ship (PDK v0.1.0 已发布, hydraforge-pdk 独立仓库已创建)
> **amends** (Phase 1 收官):
>   - ADR-0019 §1.4: ✅ 已解决 → ✅ Approved
>   - ADR-0020: 🟡 Partial → ✅ Approved
>   - ADR-0021: 🟡 Partial (Sprint 4) → ✅ Approved
>   - ADR-0022: 🔍 Proposed → ✅ Approved
>   - ADR-0023: 🟡 Partial → ✅ Approved

## Why

Phase 1 Sprint 0/1a/1b/2/3/4 全部 ship, ADR-0021 (PDK) 已落地并发布 v0.1.0, 但 **动态插件加载机制** (ADR-0022) 仍未实施。当前痛点:

- PDK 编译的 `.so` 插件没有运行时加载机制 (Sprint 4 PDK 只是工具注册脚手架)
- `examples/phase1_plugin_demo/main.cpp` 仅支持 `--mock` 模式 (无法加载真实 `.so`)
- `examples/phase1_model_router_plugin/main.cpp` 是独立可执行 (非 `.so`), 与 Sprint 5 的 PluginLoader 形成闭环
- Phase 1 智能体层 (80% → 100%) 的最后 20% 需要 PluginLoader + E2E demo 验证

**Sprint 5 范围内**:
- PluginLoader 实现 (dlopen/dlsym + ABI 版本检查 + 自动发现)
- 端到端 demo: 通过 PluginLoader 加载真实 `.so` 验证
- Phase 1 收官: 5 个候选 ADR 状态变更为 ✅ Approved (主 ADR 文件 + STATUS-GLOSSARY + relationships.md + SPECS-ALIGNMENT)
- `./scripts/sync-pdk.sh` Sprint 5 ship 后自动执行 (Dual-Repo Policy)

**不解决此问题** (Phase 2 范围):
- ❌ 完整 PluginLifecycle (on_unload + dlclose 资源回收)
- ❌ dlclose 后 C++ 静态析构保证 (MVP 警告 + on_unload 兜底)
- ❌ 跨平台 dlopen 抽象 (Linux dlopen + macOS dylib + Windows LoadLibrary) — Sprint 5 Linux only
- ❌ hot reload (运行时替换 .so)
- ❌ plugin marketplace / 远程下载

## What Changes

### 决策 1: PluginLoader 类 (per ADR-0022 §1-4)

```cpp
// include/agenticdsl/plugin/plugin_info.h (新建)
namespace hydraforge {

struct PluginInfo {
  uint32_t abi_version;       // ABI 兼容性 (CURRENT_ABI_VERSION = 1)
  char name[64];              // 插件名 (ASCII, max 63 字节)
  uint32_t major_version;
  uint32_t minor_version;
  uint32_t patch_version;
  char description[256];
  char capabilities[512];     // 逗号分隔能力标签
};

inline constexpr uint32_t CURRENT_ABI_VERSION = 1;

} // namespace hydraforge

// include/agenticdsl/plugin/plugin_loader.h (新建)
namespace hydraforge {

class PluginLoader {
 public:
  PluginLoader();
  ~PluginLoader();

  // 扫描所有发现路径, 加载可用插件
  // 返回成功加载的插件数量
  std::size_t load_all(class ToolRegistry& registry);

  // 加载指定路径的单个 .so
  // strict_version = true 时, abi_version 不匹配拒绝加载
  bool load_so(const std::string& path,
               class ToolRegistry& registry,
               bool strict_version = true);

  // 列出已加载的插件
  std::vector<PluginInfo> list_loaded() const;

  // 卸载单个插件 (Sprint 5 MVP 仅 dlclose, 无 on_unload 钩子)
  bool unload_plugin(const std::string& name);

 private:
  std::vector<std::string> get_search_paths() const;
  bool check_compatibility(const PluginInfo& info) const;
  bool apply_path_whitelist(const std::string& path) const;

  struct LoadedPlugin {
    void* handle;              // dlopen handle
    PluginInfo info;
    std::string path;
  };

  std::vector<LoadedPlugin> loaded_;
};

} // namespace hydraforge
```

### 决策 2: 符号约定 (per ADR-0022 §1.1)

每个 PDK 编译的 `.so` 必须导出两个符号:
```cpp
extern "C" const hydraforge::PluginInfo pdk_plugin_info;
extern "C" void pdk_register_tools(hydraforge::ToolRegistry& registry);
```

### 决策 3: 加载流程 (per ADR-0022 §3.2)

```
dlopen(".so", RTLD_NOW | RTLD_LOCAL)
    │
    ├── dlsym("pdk_plugin_info") → PluginInfo
    │   ├── abi_version == CURRENT_ABI_VERSION?  → 继续
    │   └── abi_version != CURRENT_ABI_VERSION?  → 拒绝加载
    │
    ├── dlsym("pdk_register_tools") → function
    │
    ├── 调用 register_tools(registry)
    │
    └── 记录到 loaded_ 列表
```

### 决策 4: 搜索路径 (per ADR-0022 §2.1)

```
1. $HYDRAFORGE_PLUGIN_PATH  (环境变量, 可指定多个路径, : 分隔)
2. ./plugins/                (工作目录下的 plugins 文件夹)
3. ~/.hydraforge/plugins/   (用户安装目录)
4. /usr/local/lib/hydraforge/plugins/  (系统安装目录)
```

### 决策 5: Phase 1 收官 (per plan §Sprint 5 T5.4)

**5 个候选 ADR 状态变更**:
| ADR | 当前 | Sprint 5 后 |
|------|------|-----------|
| ADR-0019 §1.4 | ✅ 已解决 (per P1 ship) | ✅ **Approved** (正式 Approved) |
| ADR-0020 | 🟡 Partial | ✅ **Approved** |
| ADR-0021 | 🟡 Partial (Sprint 4) | ✅ **Approved** |
| ADR-0022 | 🔍 Proposed | ✅ **Approved** |
| ADR-0023 | 🟡 Partial | ✅ **Approved** |

**状态变更范围**:
- 主 ADR 文件头部 `## 状态` 行
- `STATUS-GLOSSARY.md` 词汇表
- `relationships.md` 自动生成 (tools/adr_relationships.py)
- `SPECS-ALIGNMENT` (如存在)

### 决策 6: 端到端 demo (per plan §Sprint 5 T5.3)

扩展 `examples/phase1_plugin_demo/main.cpp` 支持真实 `.so` 加载:

```bash
# Sprint 5 后 demo 用法
./phase1_plugin_demo                           # 默认: --mock 模式
./phase1_plugin_demo --load-plugin=./plugins/model_router.so
./phase1_plugin_demo --plugin-path=/usr/local/lib/hydraforge/plugins/
```

**实现**:
1. 检测命令行参数 `--load-plugin` 或 `--plugin-path`
2. 若指定, 用 PluginLoader 加载 + 调用 `register_tools`
3. 通过 ToolRegistry 调用插件工具验证
4. 与 `--mock` 模式共存 (fallback)

### 代码侧 (新代码)

- `include/agenticdsl/plugin/plugin_info.h` (新建, ~80 行) — PluginInfo POD + CURRENT_ABI_VERSION
- `include/agenticdsl/plugin/plugin_loader.h` (新建, ~120 行) — PluginLoader 类声明
- `src/modules/plugin/plugin_loader.cpp` (新建, ~250 行) — dlopen/dlsym 实现
- `src/modules/plugin/CMakeLists.txt` (新建) — agenticdsl_modules_plugin 库
- `tests/test_plugin_loader.cpp` (新建, 5 cases) — dlopen + ABI 检查 + 路径扫描 + 卸载
- `examples/phase1_plugin_demo/main.cpp` (修改) — 扩展 `--load-plugin` + `--plugin-path`
- 根 `CMakeLists.txt` (修改) — `add_subdirectory(src/modules/plugin)` + examples/phase1_plugin_demo
- ADR 状态更新 (5 个文件, S5.T4 阶段)

### 文档侧

- `docs/adr/adr-0022-plugin-loading.md` 状态: 🔍 Proposed → ✅ Approved
- `docs/adr/adr-0019-iinteraction-bus-mvp.md` §1.4 状态: ✅ 已解决 → ✅ Approved
- `docs/adr/adr-0020-thread-model-isolation.md` 状态: 🟡 Partial → ✅ Approved
- `docs/adr/adr-0021-pdk-design.md` 状态: 🟡 Partial (Sprint 4) → ✅ Approved
- `docs/adr/adr-0023-tool-result-standard.md` 状态: 🟡 Partial → ✅ Approved
- `docs/adr-management/STATUS-GLOSSARY.md` 词汇表更新
- `docs/adr-management/relationships.md` 自动重新生成
- `docs/roadmap-status.md`: Phase 1 80% → 100% (Sprint 5 完成)
- `docs/phase1-roadmap.md`: Sprint 5 详细任务表 (T5.1-T5.4) 状态 ✅
- `AGENTS.md`: Sprint 5 ship NOTE + 5 ADR Approved 标记
- OpenSpec change `2026-07-14-plugin-loader/` archive (commit 后)

## Impact

- **Affected specs**: 新增 plugin-loader 契约 spec
- **Affected ADRs**: 5 个 ADR 状态变更 (Approved)
- **Affected code**:
  - `include/agenticdsl/plugin/` (新建 2 头)
  - `src/modules/plugin/` (新建 1 .cpp + 1 CMakeLists)
  - `examples/phase1_plugin_demo/main.cpp` (扩展)
  - 根 `CMakeLists.txt` (add_subdirectory + examples)
- **Affected tests**: 现有 32 测试零回归 + 新增 5 测试 = 37/37 ctest pass
- **Breaking change**: 无 (纯新增功能, dlopen 是 Linux-only 扩展)

## Success Criteria

- [ ] PluginInfo POD + CURRENT_ABI_VERSION 常量定义
- [ ] PluginLoader 类 API 完整 (load_all + load_so + list_loaded + unload_plugin)
- [ ] dlopen/dlsym 实现 (Linux only, `#ifdef __linux__`)
- [ ] ABI 版本检查 (`abi_version != CURRENT_ABI_VERSION` 拒绝加载)
- [ ] 搜索路径优先级 (env var > ./plugins/ > ~/.hydraforge/plugins/ > /usr/local)
- [ ] 路径白名单 (Layer 1 安全, 拒绝 /etc, /proc, /sys)
- [ ] 5/5 test_plugin_loader 测试通过 (dlopen + ABI + 路径 + 卸载 + E2E)
- [ ] 端到端 demo 通过 PluginLoader 加载真实 `.so` 验证
- [ ] 37/37 ctest pass (32 baseline + 5 new)
- [ ] 5 个候选 ADR 状态变更 ✅ Approved (主文件 + STATUS-GLOSSARY + relationships.md)
- [ ] `openspec validate 2026-07-14-plugin-loader` exit 0
- [ ] `tools/adr_lint.py docs/adr/` exit 0
- [ ] CI 6 jobs 全绿 (4 build matrix + docker-tsan + asan, 待 CI 集成)
- [ ] Phase 1 智能体层进度: 80% → 100%
- [ ] `./scripts/sync-pdk.sh` Sprint 5 ship 后执行 (Dual-Repo sync)
- [ ] 4 commits per plan §Sprint 5 (T1 → T2 → T3 → T4)

## Out of Scope (Non-goals)

- ❌ 完整 PluginLifecycle (on_load/on_unload 钩子, Sprint 5 MVP 仅 register_tools)
- ❌ dlclose 后 C++ 静态析构保证 (MVP 警告, Phase 2 完善)
- ❌ 跨平台 dlopen 抽象 (Linux dlopen + macOS dylib + Windows LoadLibrary) — Sprint 5 Linux only
- ❌ hot reload (运行时替换 .so)
- ❌ plugin marketplace / 远程下载
- ❌ plugin sandboxing (Phase 3 PDK 完整 SafeExec fork/cgroups/seccomp)
- ❌ plugin health check (Phase 3)
- ❌ plugin 持久化 (Phase 4 配置文件 + 数据库)
- ❌ 修改 PDK 头文件 API (Dual-Repo 已稳定 v0.1.0)
- ❌ 修改 CognitiveWorker / DomainWorkerPool (Sprint 2/3 已 ship)
- ❌ 修改 examples/phase1_model_router_plugin (Sprint 0 已 ship, 仅作为 reference)

## Dependencies

- **Block**: Sprint 0/1a/1b/2/3/4 全部 ship ✅ (v0.1.0 tag 已创建并推送)
- **Block**: PDK v0.1.0 (✅ shipped at commit f8da1d7, tag v0.1.0)
- **Block**: scripts/sync-pdk.sh (✅ shipped at commit d7612cc)
- **Block by**: Phase 2 异步+EventBus (2026-07-16 ~ 2026-07-30, W6-W7)
- **Related**:
  - ADR-0021 PDK (Dual-Repo Policy §7)
  - ADR-0020 §2.2.1 ✅ Resolved (Sprint 3)
  - ADR-0019 §1.4 ✅ Resolved (P1 ship)
  - ADR-0023 ToolResult P1-P4 ✅ shipped (Sprint 1a)

## Estimated Effort

~1.4 天 (单人, 反映 5 测试 + 5 ADR 状态变更):
- T1 PluginLoader 头 (PluginInfo POD + PluginLoader API): 0.3d
- T2 dlopen/dlsym 实现 (含 ABI 检查 + 路径白名单 + 搜索路径): 0.5d
- T3 端到端 demo 扩展 (--load-plugin + --plugin-path): 0.3d
- T4 Phase 1 收官 (5 ADR 状态变更 + ctest 60/60 + CI 验证 + 文档同步): 0.3d
- Sprint 5 ship 后自动执行 sync-pdk.sh (Dual-Repo 同步): 0d (集成)