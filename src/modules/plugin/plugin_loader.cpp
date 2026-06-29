// src/modules/plugin/plugin_loader.cpp
// 文件头注释
// 功能描述：PluginLoader 类实现 — 动态加载 PDK 编译的 .so 插件 (per ADR-0022 §2-3)。
//          Linux only: dlopen/dlsym/dlclose 实现 (RTLD_NOW | RTLD_LOCAL)。
//          含 ABI 版本检查 + 路径白名单 (Layer 1 安全) + 自动搜索路径。
//          析构时 dlclose 所有 handle (RAII 资源管理)。
// 设计依据：ADR-0022 §1.3 加载流程 + §2.1 搜索路径 + §5.1 路径白名单
//          + openspec/changes/2026-07-14-plugin-loader
// 作者：AgenticDSL Phase 1 Sprint 5
// 最后修改日期：2026-06-19

#ifdef __linux__

#include "agenticdsl/plugin/plugin_loader.h"
#include "agenticdsl/contract/itool_registry.h"
#include "common/log/log.h"

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <dlfcn.h>
#include <filesystem>
#include <sstream>
#include <string>
#include <vector>

namespace hydraforge {

namespace {

void log_error(const std::string& msg) { LOG_ERROR("[PluginLoader] " << msg); }
void log_warn(const std::string& msg) { LOG_WARN("[PluginLoader] " << msg); }
void log_info(const std::string& msg) { LOG_INFO("[PluginLoader] " << msg); }

} // namespace

// === PluginLoader 实现 ===

PluginLoader::PluginLoader() = default;

PluginLoader::~PluginLoader() {
  // RAII: 析构时 dlclose 所有已加载 handle
  for (auto& lp : loaded_) {
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
  return info.abi_version == CURRENT_ABI_VERSION;
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
  auto* info = static_cast<const PluginInfo*>(
      dlsym(handle, "pdk_plugin_info"));
  if (!info) {
    log_error("dlsym pdk_plugin_info failed for " + path + ": " + dlerror());
    dlclose(handle);
    return false;
  }

  // 4. ABI 版本检查
  if (!check_compatibility(*info)) {
    std::ostringstream oss;
    oss << "ABI version mismatch: plugin=" << info->abi_version
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

  // 7. 记录到 loaded_ 列表
  LoadedPlugin lp;
  lp.handle = handle;
  lp.info = *info;  // POD 拷贝
  lp.path = path;
  loaded_.push_back(lp);

  log_info("loaded plugin: " + std::string(info->name) +
           " v" + std::to_string(info->major_version) + "." +
           std::to_string(info->minor_version) + "." +
           std::to_string(info->patch_version));
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
      if (it->handle) {
        dlclose(it->handle);
      }
      loaded_.erase(it);
      log_info("unloaded plugin: " + name);
      return true;
    }
  }
  log_warn("plugin not found for unload: " + name);
  return false;
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

} // namespace hydraforge

#endif  // __linux__