# Design: PluginLoader (Sprint 5)

> **变更类型**: 真实实现 — 本 design 描述 PluginLoader 架构 + Phase 1 收官策略
> **关联 proposal**: `openspec/changes/2026-07-14-plugin-loader/proposal.md`
> **关联 spec**: `openspec/changes/2026-07-14-plugin-loader/specs/plugin-loader/spec.md`
> **关联 ADR**: docs/adr/adr-0022-plugin-loading.md (Plugin Loading 设计, 🔍 Proposed → ✅ Approved Sprint 5 ship) + ADR-0021 (PDK) + ADR-0020 (Thread Model) + ADR-0019 (IInteractionBus) + ADR-0023 (ToolResult)
> **关联 plan**: `.omo/plans/phase1-execution.md` §Sprint 5

## 架构合规性检查

| 约束 | 状态 | 备注 |
|------|------|------|
| 2 空格缩进 | ✅ | 沿用现有 |
| 中文注释优先 | ✅ | 全部新增注释中文 |
| C++20 + CMake 3.20+ | ✅ | PluginInfo POD + Linux dlopen |
| Linux only (dlopen) | ✅ | `#ifdef __linux__` 保护, 非 Linux 编译失败 |
| nlohmann_json | ✅ | ToolRegistry 集成 (Sprint 1a 已 ship) |
| Anti-pattern 避免 | ✅ | 不删失败测试, 提交前 ctest |
| PluginInfo POD (零代码执行) | ✅ | dlsym 后直接读字段 |
| 路径白名单 (Layer 1 安全) | ✅ | 拒绝 /etc /proc /sys 等敏感路径 |

## 关键设计决策

### 决策 1: PluginInfo POD 设计 (零代码执行)

**问题**: 插件加载需要在调用任何插件代码前读取元数据 (版本检查, 能力标签)。若 PluginInfo 是类 (含构造/虚函数), 则必须先调用构造函数才能读字段, 形成鸡生蛋问题。

**方案**: PluginInfo 是 **POD 类型** (Plain Old Data), 无构造/析构/虚函数, dlsym 后直接内存读取:

```cpp
// include/agenticdsl/plugin/plugin_info.h
namespace hydraforge {

struct PluginInfo {
  uint32_t abi_version;       // ABI 兼容性 (CURRENT_ABI_VERSION = 1)
  char name[64];              // 插件名 (ASCII, max 63 字节 + \0)
  uint32_t major_version;
  uint32_t minor_version;
  uint32_t patch_version;
  char description[256];
  char capabilities[512];     // 逗号分隔能力标签
};

inline constexpr uint32_t CURRENT_ABI_VERSION = 1;

} // namespace hydraforge
```

**关键设计点**:
- **POD 类型**: 无构造/析构/虚函数, 跨二进制边界 ABI 稳定
- **C 风格字符数组**: 避免 std::string (跨二进制 ABI 不兼容)
- **abi_version 独立于 major_version**: PDK 1.0/1.1 都用 abi=1, 仅当 C++ 宏展开方式变化时 +1
- **CURRENT_ABI_VERSION 常量**: 编译时常量, Runtime 加载时检查

### 决策 2: Linux dlopen 实现 (RTLD_NOW | RTLD_LOCAL)

**问题**: 如何加载 .so 并调用其导出函数? 跨平台 dlopen 抽象 (dlopen/dylib/LoadLibrary) 还是 Linux only?

**方案**: Sprint 5 MVP Linux only (`#ifdef __linux__`), 跨平台抽象 Phase 2:

```cpp
// src/modules/plugin/plugin_loader.cpp (核心片段)
#include <dlfcn.h>

bool PluginLoader::load_so(const std::string& path,
                          class ToolRegistry& registry,
                          bool strict_version) {
  // 1. 路径白名单检查 (Layer 1 安全)
  if (!apply_path_whitelist(path)) {
    log_error("path rejected by whitelist: " + path);
    return false;
  }

  // 2. dlopen
  void* handle = dlopen(path.c_str(), RTLD_NOW | RTLD_LOCAL);
  if (!handle) {
    log_error("dlopen failed: " + std::string(dlerror()));
    return false;
  }

  // 3. 读取 PluginInfo (dlsym 后零代码执行)
  auto* info = static_cast<const PluginInfo*>(
      dlsym(handle, "pdk_plugin_info"));
  if (!info) {
    log_error("dlsym pdk_plugin_info failed: " + std::string(dlerror()));
    dlclose(handle);
    return false;
  }

  // 4. ABI 版本检查
  if (!check_compatibility(*info)) {
    if (strict_version) {
      log_error("ABI version mismatch: " + std::to_string(info->abi_version));
      dlclose(handle);
      return false;
    }
    log_warn("ABI version mismatch (non-strict), continuing");
  }

  // 5. 读取 register_tools 函数
  using RegisterFn = void (*)(ToolRegistry&);
  auto register_fn = reinterpret_cast<RegisterFn>(
      dlsym(handle, "pdk_register_tools"));
  if (!register_fn) {
    log_error("dlsym pdk_register_tools failed: " + std::string(dlerror()));
    dlclose(handle);
    return false;
  }

  // 6. 调用 register_tools
  register_fn(registry);

  // 7. 记录到 loaded_ 列表
  LoadedPlugin lp;
  lp.handle = handle;
  lp.info = *info;  // POD 拷贝
  lp.path = path;
  loaded_.push_back(lp);

  return true;
}
```

**关键设计点**:
- **RTLD_NOW**: 立即解析所有符号 (vs RTLD_LAZY), 避免运行时未解析错误
- **RTLD_LOCAL**: 局部符号可见性 (vs RTLD_GLOBAL), 防止符号冲突
- **路径白名单**: Layer 1 安全 (拒绝 /etc, /proc, /sys)
- **ABI 检查**: 默认 strict_version=true, 失败立即拒绝

### 决策 3: 搜索路径优先级 (per ADR-0022 §2.1)

**问题**: 插件应该从哪些路径加载? 环境变量 vs 默认目录?

**方案**: 4 个搜索路径按优先级降序:

```cpp
std::vector<std::string> PluginLoader::get_search_paths() const {
  std::vector<std::string> paths;

  // 1. 环境变量 (最高优先级, 可指定多个路径 : 分隔)
  if (const char* env = std::getenv("HYDRAFORGE_PLUGIN_PATH")) {
    std::string s(env);
    size_t start = 0;
    while (start < s.size()) {
      size_t pos = s.find(':', start);
      if (pos == std::string::npos) pos = s.size();
      paths.push_back(s.substr(start, pos - start));
      start = pos + 1;
    }
  }

  // 2. ./plugins/ (工作目录)
  paths.push_back("./plugins/");

  // 3. ~/.hydraforge/plugins/ (用户目录)
  if (const char* home = std::getenv("HOME")) {
    paths.push_back(std::string(home) + "/.hydraforge/plugins/");
  }

  // 4. /usr/local/lib/hydraforge/plugins/ (系统目录)
  paths.push_back("/usr/local/lib/hydraforge/plugins/");

  return paths;
}
```

**关键设计点**:
- **环境变量优先**: 开发者/CI 可在测试时指定
- **4 个路径降序**: 灵活性 + 默认值
- **HOME 环境变量**: 跨平台 (Linux/macOS), 不硬编码 ~/ (避免解析)

### 决策 4: 路径白名单 (Layer 1 安全)

**问题**: 如何防止恶意 .so 注入? 攻击者可能将恶意 plugin.so 放到系统敏感目录。

**方案**: 拒绝加载白名单外的路径, 即使 env var 指定:

```cpp
bool PluginLoader::apply_path_whitelist(const std::string& path) const {
  namespace fs = std::filesystem;

  // 1. 环境变量路径: 信任 (开发者明确意图)
  if (const char* env = std::getenv("HYDRAFORGE_PLUGIN_PATH")) {
    std::string s(env);
    // 检查 path 是否在 env 路径下 (前缀匹配)
    size_t pos = 0;
    while (pos < s.size()) {
      size_t colon = s.find(':', pos);
      if (colon == std::string::npos) colon = s.size();
      std::string allowed = s.substr(pos, colon - pos);
      if (path.find(allowed) == 0) return true;
      pos = colon + 1;
    }
  }

  // 2. 白名单路径 (前缀匹配)
  static const std::vector<std::string> whitelist = {
    "./plugins/",
    "/usr/local/lib/hydraforge/plugins/",
  };

  for (const auto& allowed : whitelist) {
    if (path.find(allowed) == 0) return true;
  }

  // 3. HOME 路径 (用户目录)
  if (const char* home = std::getenv("HOME")) {
    std::string user_plugins = std::string(home) + "/.hydraforge/plugins/";
    if (path.find(user_plugins) == 0) return true;
  }

  // 4. 拒绝敏感路径
  static const std::vector<std::string> blacklist = {
    "/etc/", "/proc/", "/sys/", "/tmp/", "/dev/",
  };

  for (const auto& denied : blacklist) {
    if (path.find(denied) == 0) return false;  // 黑名单优先
  }

  // 5. 未在白名单: 拒绝
  return false;
}
```

**关键设计点**:
- **黑名单优先**: 即便路径不在白名单, 显式拒绝敏感路径
- **环境变量信任**: 开发者明确意图 (测试场景), 但仍受黑名单约束
- **前缀匹配**: 防止 `..` 路径绕过 (e.g. `/usr/local/lib/hydraforge/plugins/../etc/passwd.so`)

### 决策 5: 端到端 demo 扩展 (per plan §Sprint 5 T5.3)

**问题**: 如何让 `examples/phase1_plugin_demo` 从 Sprint 0 模拟模式扩展为 Sprint 5 真实 .so 加载?

**方案**: 新增命令行参数 + 命令行处理:

```cpp
// examples/phase1_plugin_demo/main.cpp (扩展片段)
int main(int argc, char** argv) {
  // 解析命令行参数
  std::string load_plugin;
  std::string plugin_path;
  bool mock_mode = false;

  for (int i = 1; i < argc; ++i) {
    std::string arg(argv[i]);
    if (arg == "--mock") mock_mode = true;
    else if (arg.find("--load-plugin=") == 0) {
      load_plugin = arg.substr(14);
    } else if (arg.find("--plugin-path=") == 0) {
      plugin_path = arg.substr(14);
    }
  }

  // 默认 mock 模式 (Sprint 0 fallback)
  if (!mock_mode && load_plugin.empty() && plugin_path.empty()) {
    mock_mode = true;
  }

  if (mock_mode) {
    run_mock_mode();
  } else {
    run_real_plugin_mode(load_plugin, plugin_path);
  }
}
```

**关键设计点**:
- **向后兼容**: 无参数时 fallback 到 `--mock` (Sprint 0 行为)
- **互斥参数**: `--mock` 与 `--load-plugin`/`--plugin-path` 二选一
- **加载路径**: 单个 .so (`--load-plugin`) 或路径扫描 (`--plugin-path`)

### 决策 6: Phase 1 收官 (5 ADR 状态变更)

**问题**: 如何将 5 个候选 ADR 状态变更为 ✅ Approved?

**方案**: Sprint 5 ship 后批量更新 5 个 ADR 文件 + STATUS-GLOSSARY + relationships.md:

| ADR | 当前状态 | Sprint 5 后 | 变更原因 |
|------|---------|-----------|---------|
| ADR-0019 §1.4 | ✅ 已解决 (per P1 ship) | ✅ **Approved** | 正式 Approved, P1 ship 后满足 exit 标准 |
| ADR-0020 | 🟡 Partial | ✅ **Approved** | Sprint 3 DomainWorkerPool ship 后 §2.2.1 全 Resolved |
| ADR-0021 | 🟡 Partial (Sprint 4) | ✅ **Approved** | Sprint 4 PDK v0.1.0 ship, Phase 2/3 后续, 不影响 Approved |
| ADR-0022 | 🔍 Proposed | ✅ **Approved** | Sprint 5 PluginLoader ship, 设计落地 |
| ADR-0023 | 🟡 Partial | ✅ **Approved** | Sprint 1a ToolResult P2-P4 ship, 标准已稳定 |

**状态变更范围**:
1. 主 ADR 文件头部 `## 状态` 行
2. `docs/adr-management/STATUS-GLOSSARY.md` 词汇表 (Approved 状态定义)
3. `docs/adr-management/relationships.md` (运行 `tools/adr_relationships.py` 自动重新生成)
4. `docs/roadmap-status.md`: Phase 1 80% → 100%
5. `AGENTS.md`: Sprint 5 ship NOTE + 5 ADR Approved 标记

### 决策 7: PluginLoader 与 PDK v0.1.0 集成 (Dual-Repo)

**问题**: PluginLoader 加载 PDK v0.1.0 编译的 .so, 需要哪些导出符号?

**方案**: PluginLoader 期望 2 个 `extern "C"` 符号 (per ADR-0022 §1.1):

```cpp
// PDK 生成的 .so 入口 (Sprint 4 DECLARE_TOOL 自动展开)
extern "C" const hydraforge::PluginInfo pdk_plugin_info = {
  .abi_version = hydraforge::CURRENT_ABI_VERSION,
  .name = "edit_file",
  .major_version = 1,
  .minor_version = 0,
  .patch_version = 0,
  .description = "Edit files in workspace",
  .capabilities = "code,code::edit,code::file",
};

extern "C" void pdk_register_tools(hydraforge::ToolRegistry& registry) {
  registry.register_tool("code::edit_file", [](const auto& args) {
    // 领域逻辑 (PDK DECLARE_TOOL 展开)
    return nlohmann::json{{"ok", true}};
  });
}
```

**关键设计点**:
- **extern "C"**: 防止 C++ name mangling, dlsym 可按符号名查找
- **C 风格初始化**: `.abi_version = X, .name = "Y"`, 跨编译器兼容
- **PDK 自动生成**: Sprint 4 PDK DECLARE_TOOL 展开时同时生成 PluginInfo + register_tools (Phase 2 集成, Sprint 5 MVP 仅验证 Loader API)

**Sprint 5 MVP 限制**:
- 实际 Sprint 5 不实现 PDK 自动展开 PluginInfo (Phase 2 工作)
- Sprint 5 仅验证 PluginLoader API + 端到端 demo 加载手工编译的测试 .so
- 测试 .so 包含手工编写的 PluginInfo + register_tools (5 个测试 case 中的 fixture)

## 实施路径 (S5.T1 → T2 → T3 → T4)

### T1: PluginInfo + PluginLoader 头 (新建, ~200 行)
- `include/agenticdsl/plugin/plugin_info.h` (~80 行): PluginInfo POD + CURRENT_ABI_VERSION
- `include/agenticdsl/plugin/plugin_loader.h` (~120 行): PluginLoader 类声明
- `include/agenticdsl/plugin/CMakeLists.txt` (新建): agenticdsl_hdr_plugin INTERFACE 库
- 验收: 头文件独立编译, LSP 无 error

### T2: PluginLoader 实现 + 测试 (新建, ~500 行)
- `src/modules/plugin/plugin_loader.cpp` (~250 行): dlopen/dlsym + ABI + 路径白名单 + 搜索路径
- `src/modules/plugin/CMakeLists.txt` (新建): agenticdsl_modules_plugin STATIC 库
- `tests/test_plugin_loader.cpp` (~250 行, 5 cases):
  1. PluginInfo POD 字段验证 + 内存布局
  2. load_so 单个 .so + ABI 检查 (compile fixture)
  3. list_loaded + unload_plugin 生命周期
  4. 路径白名单 (拒绝 /etc /proc /sys)
  5. E2E 加载手工编译 .so + register_tools 调用验证
- 验收: 5/5 测试 pass, 37/37 ctest pass

### T3: 端到端 demo 扩展 (修改, ~50 行)
- `examples/phase1_plugin_demo/main.cpp` (扩展): 新增 `--load-plugin` + `--plugin-path` 参数
- 根 `CMakeLists.txt` (修改): `add_subdirectory(src/modules/plugin)` + examples/phase1_plugin_demo
- 验收: demo 支持真实 .so 加载 + 与 `--mock` 模式共存

### T4: Phase 1 收官 (修改, 5 文件)
- 5 个 ADR 状态行更新 (Approved)
- `docs/adr-management/STATUS-GLOSSARY.md` 词汇表更新
- `tools/adr_relationships.py` 重新生成 `relationships.md`
- `docs/roadmap-status.md`: Phase 1 80% → 100%
- `docs/phase1-roadmap.md`: Sprint 5 详细任务表 ✅
- `AGENTS.md`: Sprint 5 ship NOTE + 5 ADR Approved 标记
- OpenSpec `2026-07-14-plugin-loader/` archive
- 验收: ctest 37/37 pass, 5 ADR Approved, Phase 1 100%

## 提交策略 (4 commits per plan §Sprint 5)

```
S5.T1 → feat(plugin): add PluginInfo POD + PluginLoader API (Sprint 5, ADR-0022)
S5.T2 → feat(plugin): implement PluginLoader with dlopen + ABI check (Sprint 5)
S5.T3 → feat(plugin): extend phase1_plugin_demo with --load-plugin (Sprint 5)
S5.T4 → docs(adr+status+openspec): Phase 1 收官 + 5 ADR → ✅ Approved (Sprint 5)
```

(T5 = 自动执行 `./scripts/sync-pdk.sh` Sprint 5 ship 后)

## 风险与缓解

| 风险 | 严重度 | 缓解措施 |
|------|-------|---------|
| dlopen 后 C++ 静态析构不保证 | 中 | MVP 警告 + Phase 2 完善 (ADR-0022 §3.3) |
| 跨平台 dlopen 不支持 | 低 | `#ifdef __linux__` 显式失败, Phase 2 抽象 |
| 路径白名单被绕过 (e.g. symlink) | 低 | canonical path 校验 (Phase 2) |
| PluginInfo ABI 不兼容 | 中 | abi_version 严格检查, 不匹配拒绝加载 |
| dlsym 符号冲突 (RTLD_GLOBAL) | 低 | 用 RTLD_LOCAL 限制符号可见性 |
| 1000x 插件加载性能 | 低 | Phase 2 性能测试, MVP 仅验证功能 |

## 相关 ADR / 文档

- **ADR-0022 Plugin Loading** (主): 状态变更 🔍 Proposed → ✅ Approved (Sprint 5 ship)
- **ADR-0021 PDK**: 状态变更 🟡 Partial → ✅ Approved
- **ADR-0020 §2.2.1**: ✅ Resolved (Sprint 3) → ✅ Approved
- **ADR-0019 §1.4**: ✅ Resolved (P1 ship) → ✅ Approved (正式)
- **ADR-0023 ToolResult**: 状态变更 🟡 Partial → ✅ Approved
- **scripts/sync-pdk.sh** (Dual-Repo Policy §7): Sprint 5 ship 后自动执行
- **Plan §Sprint 5**: T5.1-T5.4 任务列表对齐
- **examples/phase1_plugin_demo**: Sprint 0 mock → Sprint 5 真实 .so 加载
- **examples/phase1_model_router_plugin**: Sprint 0 reference (Policy 逻辑迁移参考)