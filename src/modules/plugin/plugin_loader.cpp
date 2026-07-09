// src/modules/plugin/plugin_loader.cpp
// 文件头注释
// 功能描述：PluginLoader 类实现 — 动态加载 PDK 编译的 .so 插件 (per ADR-0022 §2-3)。
//          Linux only: dlopen/dlsym/dlclose 实现 (RTLD_NOW | RTLD_LOCAL)。
//          含 ABI 版本检查 + 路径白名单 (Layer 1 安全) + 自动搜索路径。
//          析构时 dlclose 所有 handle (RAII 资源管理)。
//          Phase 5 (OpenSpec `phase5-illmprovider-call-chain-v2` §6):
//            - 5 符号查找 + pdk_plugin_init 调用 + pdk_plugin_fini 缓存
//            - unload_plugin 按生命周期顺序释放 (shared_ptr → fini → erase → dlclose)
//            - create_llm_provider() 抽象方法实现
//            - load_all 循环依赖检测 + 缺失依赖报错 (MVP)
// 设计依据：ADR-0022 §1.3 加载流程 + §2.1 搜索路径 + §5.1 路径白名单
//          + ADR-0041 §1 PluginLoader lifecycle extension
//          + openspec/changes/phase5-illmprovider-call-chain-v2/specs/plugin-loader/spec.md
// 作者：AgenticDSL Phase 1 Sprint 5 → Phase 5 B2
// 最后修改日期：2026-07-09 (Phase 5 §6: 5 符号查找 + lifecycle + create_llm_provider)

#ifdef __linux__

#include "agenticdsl/plugin/plugin_loader.h"
#include "agenticdsl/contract/itool_registry.h"
#include "common/llm/llm_types.h"
#include "common/log/log.h"

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <dlfcn.h>
#include <filesystem>
#include <memory>
#include <set>
#include <sstream>
#include <string>
#include <vector>

namespace hydraforge {

namespace {

void log_error(const std::string& msg) { LOG_ERROR("[PluginLoader] " << msg); }
void log_warn(const std::string& msg) { LOG_WARN("[PluginLoader] " << msg); }
void log_info(const std::string& msg) { LOG_INFO("[PluginLoader] " << msg); }

std::vector<std::string> split_dependencies(const char* raw) {
  std::vector<std::string> result;
  if (!raw || raw[0] == '\0') return result;

  std::string s(raw);
  std::string current;
  for (char c : s) {
    if (c == ',' || c == ' ' || c == '\t') {
      if (!current.empty()) {
        result.push_back(current);
        current.clear();
      }
    } else {
      current += c;
    }
  }
  if (!current.empty()) result.push_back(current);
  return result;
}

} // namespace

// === PluginLoader 实现 ===

PluginLoader::PluginLoader() = default;

PluginLoader::~PluginLoader() {
  // RAII: 析构时按 Phase 5 lifecycle 顺序清理所有已加载 handle
  //   1. 清空 provider_refs (释放 loader 持有的 weak_ptr, 不影响 caller 的 shared_ptr)
  //   2. 调用 pdk_plugin_fini (若存在, 失败仅记 ERROR 不抛异常)
  //   3. dlclose(handle)
  for (auto& lp : loaded_) {
    lp.provider_refs.clear();
    if (lp.fini_fn) {
      using PluginFiniFn = void (*)();
      auto fini_fn = reinterpret_cast<PluginFiniFn>(lp.fini_fn);
      try {
        fini_fn();
      } catch (...) {
        log_error("pdk_plugin_fini() threw exception during destructor for " +
                  std::string(lp.info.name) + " (ignored)");
      }
    }
    if (lp.handle) {
      dlclose(lp.handle);
      lp.handle = nullptr;
    }
  }
  loaded_.clear();
}

std::vector<std::string> PluginLoader::get_search_paths() const {
  std::vector<std::string> paths;

  // 1. 环境变量 (最高优先级, 可指定多个路径 : 分隔)
  if (const char* env = std::getenv("HYDRAFORGE_PLUGIN_PATH")) {
    std::string s(env);
    size_t start = 0;
    while (start < s.size()) {
      size_t pos = s.find(':', start);
      if (pos == std::string::npos) {
        pos = s.size();
      }
      if (pos > start) {
        paths.push_back(s.substr(start, pos - start));
      }
      start = pos + 1;
    }
  }

  // 2. ./plugins/ (工作目录)
  paths.push_back("./plugins/");

  // 3. ~/.hydraforge/plugins/ (用户目录)
  if (const char* home = std::getenv("HOME")) {
    std::string home_str(home);
    paths.push_back(home_str + "/.hydraforge/plugins/");
  }

  // 4. /usr/local/lib/hydraforge/plugins/ (系统目录)
  paths.push_back("/usr/local/lib/hydraforge/plugins/");

  return paths;
}

bool PluginLoader::check_compatibility(const PluginInfo& info) const {
  // Dual ABI dispatch (per ADR-0041 §1.5): 接受 V1 (老 .so) + V2 (新 .so)
  for (uint32_t v : SUPPORTED_ABI_VERSIONS) {
    if (info.abi_version == v) return true;
  }
  return false;
}

// Dual ABI unified read: 读 V1 或 V2 统一返回 PluginInfoV2
// 老 V1 .so 的 dependencies 默认为空字符串
static PluginInfoV2 read_plugin_info_unified(void* info_handle) {
  // 安全读取前 4 字节 (abi_version 字段)
  uint32_t abi_version = 0;
  std::memcpy(&abi_version, info_handle, sizeof(uint32_t));

  if (abi_version == 1) {
    PluginInfoV1 raw;
    std::memcpy(&raw, info_handle, sizeof(PluginInfoV1));
    PluginInfoV2 unified{};
    unified.abi_version = raw.abi_version;
    std::strncpy(unified.name, raw.name, sizeof(unified.name));
    unified.major_version = raw.major_version;
    unified.minor_version = raw.minor_version;
    unified.patch_version = raw.patch_version;
    std::strncpy(unified.description, raw.description, sizeof(unified.description));
    std::strncpy(unified.capabilities, raw.capabilities, sizeof(unified.capabilities));
    unified.dependencies[0] = '\0';  // V1 无此字段 → 空字符串
    return unified;
  }

  // 当前版本 (V2) — 直接 memcpy
  PluginInfoV2 raw;
  std::memcpy(&raw, info_handle, sizeof(PluginInfoV2));
  return raw;
}

bool PluginLoader::apply_path_whitelist(const std::string& path) const {
  namespace fs = std::filesystem;

  // 路径规范化 (解析 symlink + ..)
  std::error_code ec;
  fs::path canonical_path = fs::weakly_canonical(fs::path{path}, ec);
  if (ec) {
    log_warn("path canonicalization failed for: " + path);
    canonical_path = fs::path{path};
  }

  std::string canonical = canonical_path.string();

  // 1. 环境变量路径: 信任 (开发者明确意图), 但仍受黑名单约束
  if (const char* env = std::getenv("HYDRAFORGE_PLUGIN_PATH")) {
    std::string s(env);
    size_t pos = 0;
    while (pos < s.size()) {
      size_t colon = s.find(':', pos);
      if (colon == std::string::npos) {
        colon = s.size();
      }
      std::string allowed = s.substr(pos, colon - pos);
      if (!allowed.empty() && allowed.back() != '/') {
        allowed += '/';
      }
      if (canonical.find(allowed) == 0) {
        return true;  // 环境变量路径 + 黑名单后续检查
      }
      pos = colon + 1;
    }
  }

  // 2. 白名单路径 (前缀匹配)
  static const std::vector<std::string> whitelist = {
    "./plugins/",
    "/usr/local/lib/hydraforge/plugins/",
  };

  for (const auto& allowed : whitelist) {
    if (canonical.find(allowed) == 0) {
      return true;
    }
  }

  // 3. HOME 路径 (用户目录)
  if (const char* home = std::getenv("HOME")) {
    std::string user_plugins = std::string(home) + "/.hydraforge/plugins/";
    if (canonical.find(user_plugins) == 0) {
      return true;
    }
  }

  // 4. 黑名单 (敏感路径)
  static const std::vector<std::string> blacklist = {
    "/etc/", "/proc/", "/sys/", "/tmp/", "/dev/",
  };

  for (const auto& denied : blacklist) {
    if (canonical.find(denied) == 0) {
      log_warn("path in blacklist, rejected: " + canonical);
      return false;  // 黑名单优先
    }
  }

  // 5. 未在白名单: 拒绝
  log_warn("path not in whitelist, rejected: " + canonical);
  return false;
}

bool PluginLoader::load_so(const std::string& path,
                           ::agenticdsl::IToolRegistry& registry,
                           bool strict_version) {
  // 1. 路径白名单检查 (Layer 1 安全)
  if (!apply_path_whitelist(path)) {
    log_error("path rejected by whitelist: " + path);
    return false;
  }

  // 2. dlopen (RTLD_NOW: 立即解析所有符号, RTLD_LOCAL: 局部符号可见性)
  void* handle = dlopen(path.c_str(), RTLD_NOW | RTLD_LOCAL);
  if (!handle) {
    log_error("dlopen failed for " + path + ": " + dlerror());
    return false;
  }

  // 3. 读取 PluginInfo (dlsym 后零代码执行)
  auto* info_handle = dlsym(handle, "pdk_plugin_info");
  if (!info_handle) {
    log_error("dlsym pdk_plugin_info failed for " + path + ": " + dlerror());
    dlclose(handle);
    return false;
  }

  // 3.5 Dual ABI dispatch: 读 V1/V2 统一为 PluginInfoV2
  PluginInfoV2 info = read_plugin_info_unified(info_handle);

  // 4. ABI 版本检查
  if (!check_compatibility(info)) {
    std::ostringstream oss;
    oss << "ABI version mismatch: plugin=" << info.abi_version
        << " runtime=" << CURRENT_ABI_VERSION << " (path=" << path << ")";
    if (strict_version) {
      log_error(oss.str());
      dlclose(handle);
      return false;
    }
    log_warn(oss.str() + " (non-strict, continuing)");
  }

  // 5. 读取 register_tools 函数
  using RegisterFn = void (*)(::agenticdsl::IToolRegistry&);
  auto register_fn = reinterpret_cast<RegisterFn>(
      dlsym(handle, "pdk_register_tools"));
  if (!register_fn) {
    log_error("dlsym pdk_register_tools failed for " + path + ": " + dlerror());
    dlclose(handle);
    return false;
  }

  // 6. 调用 register_tools
  register_fn(registry);

  // 7. 新增 Phase 5 符号查找 (三者均为可选)
  using CreateLLMProviderFn = std::shared_ptr<::agenticdsl::ILLMProvider>(*)(const void*);
  auto* create_llm_provider_fn =
      reinterpret_cast<CreateLLMProviderFn>(dlsym(handle, "pdk_create_llm_provider"));

  using PluginInitFn = bool (*)();
  auto* init_fn = reinterpret_cast<PluginInitFn>(dlsym(handle, "pdk_plugin_init"));

  using PluginFiniFn = void (*)();
  auto* fini_fn = reinterpret_cast<PluginFiniFn>(dlsym(handle, "pdk_plugin_fini"));

  // 8. 调用 pdk_plugin_init (若存在)
  if (init_fn) {
    if (!init_fn()) {
      log_error("pdk_plugin_init failed for " + path + ": init returned false");
      dlclose(handle);
      return false;
    }
    log_info("pdk_plugin_init() called successfully for " + path);
  }

  // 9. 记录到 loaded_ 列表
  LoadedPlugin lp;
  lp.handle = handle;
  lp.info = info;  // POD 拷贝 (PluginInfoV2 = PluginInfo)
  lp.path = path;
lp.create_llm_provider_fn = reinterpret_cast<void*>(create_llm_provider_fn);  // 可选, 缓存指针
lp.fini_fn = reinterpret_cast<void*>(fini_fn);  // 可选, 缓存指针
  loaded_.push_back(lp);

  log_info("loaded plugin: " + std::string(info.name) +
           " v" + std::to_string(info.major_version) + "." +
           std::to_string(info.minor_version) + "." +
           std::to_string(info.patch_version));
  return true;
}

std::size_t PluginLoader::load_all(::agenticdsl::IToolRegistry& registry) {
  std::size_t total_loaded = 0;

  auto paths = get_search_paths();
  for (const auto& search_path : paths) {
    namespace fs = std::filesystem;
    std::error_code ec;
    fs::path search_dir{search_path};
    if (!fs::exists(search_dir, ec) || !fs::is_directory(search_dir, ec)) {
      continue;  // 路径不存在或非目录, 跳过
    }

    // 扫描目录下的 *.so 文件
    for (const auto& entry : fs::directory_iterator{search_dir, ec}) {
      if (ec) break;
      if (!entry.is_regular_file(ec)) continue;

      const auto& path = entry.path();
      if (path.extension() != ".so") continue;

      std::string path_str = path.string();
      if (load_so(path_str, registry, true)) {
        total_loaded++;
      }
    }
  }

  // Phase 5: 依赖验证 (post-load, MVP: 循环检测 + 缺失依赖报错)
  if (total_loaded > 0) {
    std::vector<std::pair<std::string, std::string>> plugin_deps;
    plugin_deps.reserve(loaded_.size());
    for (const auto& lp : loaded_) {
      plugin_deps.emplace_back(std::string(lp.info.name),
                               std::string(lp.info.dependencies));
    }

    auto [missing, circular] = validate_dependencies(plugin_deps);
    for (const auto& err : missing) {
      log_error("dependency validation: " + err);
    }
    for (const auto& err : circular) {
      log_error("dependency validation: " + err);
    }
  }

  return total_loaded;
}

std::vector<PluginInfo> PluginLoader::list_loaded() const {
  std::vector<PluginInfo> result;
  result.reserve(loaded_.size());
  for (const auto& lp : loaded_) {
    result.push_back(lp.info);  // POD 拷贝
  }
  return result;
}

bool PluginLoader::unload_plugin(const std::string& name) {
  for (auto it = loaded_.begin(); it != loaded_.end(); ++it) {
    if (std::string(it->info.name) == name) {
      // Phase 5 lifecycle (per REQ-PL-IPD-002 Scenario "lifecycle 顺序保证"):
      //   1. 释放 loader 持有的 shared_ptr<ILLMProvider> (weak_ptr refs 不阻塞)
      //   2. 调用 pdk_plugin_fini (失败仅记 ERROR, 不抛异常)
      //   3. 从 loaded_ 移除
      //   4. dlclose(handle)
      it->provider_refs.clear();

      if (it->fini_fn) {
        using PluginFiniFn = void (*)();
        auto fini_fn = reinterpret_cast<PluginFiniFn>(it->fini_fn);
        try {
          fini_fn();
          log_info("pdk_plugin_fini() called for " + name);
        } catch (...) {
          log_error("pdk_plugin_fini() threw exception for " + name + " (ignored)");
        }
      }

      void* handle = it->handle;
      loaded_.erase(it);

      if (handle) {
        dlclose(handle);
      }
      log_info("unloaded plugin: " + name);
      return true;
    }
  }
  log_warn("plugin not found for unload: " + name);
  return false;
}

std::shared_ptr<::agenticdsl::ILLMProvider>
PluginLoader::create_llm_provider(const std::string& plugin_name,
                                  const void* config) {
  for (auto& lp : loaded_) {
    if (std::string(lp.info.name) != plugin_name) continue;

    if (!lp.create_llm_provider_fn) {
      return nullptr;
    }
    using CreateLLMProviderFn =
        std::shared_ptr<::agenticdsl::ILLMProvider>(*)(const void*);
    auto fn = reinterpret_cast<CreateLLMProviderFn>(lp.create_llm_provider_fn);
    auto provider = fn(config);
    if (provider) {
      lp.provider_refs.push_back(provider);
    }
    return provider;
  }
  throw std::runtime_error("plugin " + plugin_name + " unloaded");
}

std::pair<std::vector<std::string>, std::vector<std::string>>
PluginLoader::validate_dependencies(
    const std::vector<std::pair<std::string, std::string>>& plugin_deps) {
  std::vector<std::string> missing_errors;
  std::vector<std::string> circular_errors;

  if (plugin_deps.empty()) {
    return {missing_errors, circular_errors};
  }

  // 构建 name → deps 的 map (用于后续查找)
  std::map<std::string, std::vector<std::string>> dep_map;
  for (const auto& [name, deps_str] : plugin_deps) {
    dep_map[name] = split_dependencies(deps_str.c_str());
  }

  // 1. 缺失依赖检测：每个 plugin 的每个依赖项必须在 loaded plugin 集合中
  std::set<std::string> known_names;
  for (const auto& [name, _] : plugin_deps) {
    known_names.insert(name);
  }

  for (const auto& [name, deps_str] : plugin_deps) {
    auto deps = split_dependencies(deps_str.c_str());
    for (const auto& dep : deps) {
      if (known_names.find(dep) == known_names.end()) {
        std::ostringstream oss;
        oss << "plugin '" << name << "' depends on '" << dep
            << "' which is not loaded";
        missing_errors.push_back(oss.str());
      }
    }
  }

  // 2. 循环依赖检测：A 依赖 B 且 B 依赖 A
  std::vector<std::string> plugin_names;
  for (const auto& [name, _] : plugin_deps) {
    plugin_names.push_back(name);
  }

  for (size_t i = 0; i < plugin_names.size(); ++i) {
    for (size_t j = i + 1; j < plugin_names.size(); ++j) {
      const auto& name_a = plugin_names[i];
      const auto& name_b = plugin_names[j];

      auto it_a = dep_map.find(name_a);
      auto it_b = dep_map.find(name_b);
      if (it_a == dep_map.end() || it_b == dep_map.end()) continue;

      bool a_depends_on_b = std::find(it_a->second.begin(), it_a->second.end(),
                                      name_b) != it_a->second.end();
      bool b_depends_on_a = std::find(it_b->second.begin(), it_b->second.end(),
                                      name_a) != it_b->second.end();

      if (a_depends_on_b && b_depends_on_a) {
        std::ostringstream oss;
        oss << "circular dependency detected: '" << name_a << "' ↔ '" << name_b << "'";
        circular_errors.push_back(oss.str());
      }
    }
  }

  return {missing_errors, circular_errors};
}

} // namespace hydraforge

#else  // !__linux__

// 非 Linux 平台: PluginLoader 为 stub 实现, 所有方法返回失败或空结果
// 跨平台 dlopen 抽象见 ADR-0022 Phase 2
namespace hydraforge {

PluginLoader::PluginLoader() = default;
PluginLoader::~PluginLoader() = default;
std::vector<std::string> PluginLoader::get_search_paths() const { return {}; }
bool PluginLoader::check_compatibility(const PluginInfo& /*info*/) const { return false; }
bool PluginLoader::apply_path_whitelist(const std::string& /*path*/) const { return false; }
bool PluginLoader::load_so(const std::string& /*path*/,
                           ::agenticdsl::IToolRegistry& /*registry*/,
                           bool /*strict_version*/) { return false; }
std::size_t PluginLoader::load_all(::agenticdsl::IToolRegistry& /*registry*/) { return 0; }
std::vector<PluginInfo> PluginLoader::list_loaded() const { return {}; }
bool PluginLoader::unload_plugin(const std::string& /*name*/) { return false; }
std::shared_ptr<::agenticdsl::ILLMProvider>
PluginLoader::create_llm_provider(const std::string& /*plugin_name*/,
                                  const void* /*config*/) {
  return nullptr;
}
std::pair<std::vector<std::string>, std::vector<std::string>>
PluginLoader::validate_dependencies(
    const std::vector<std::pair<std::string, std::string>>& /*plugin_deps*/) {
  return {};
}

} // namespace hydraforge

#endif  // __linux__