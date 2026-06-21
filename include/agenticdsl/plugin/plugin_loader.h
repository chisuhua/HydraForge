// include/agenticdsl/plugin/plugin_loader.h
// 文件头注释
// 功能描述：PluginLoader 类 — 动态加载 PDK 编译的 .so 插件 (per ADR-0022 §2-3)。
//          支持 Linux dlopen/dlsym, 含 ABI 版本检查 + 路径白名单 + 自动发现。
//          头文件仅前向声明 IToolRegistry (避免引入 Runtime 内部, 编译时验证 P3 静态链接)。
// 设计依据：ADR-0022 §2-3 + openspec/changes/2026-07-14-plugin-loader
// 作者：AgenticDSL Phase 1 Sprint 5
// 最后修改日期：2026-06-19

#pragma once

#include "agenticdsl/plugin/plugin_info.h"

#include <cstddef>
#include <string>
#include <vector>

namespace agenticdsl {
class IToolRegistry;  // contract 层抽象 (ADR-0019 §1.4)
}

namespace hydraforge {

/**
 * @brief PluginLoader 类 — 动态加载 .so 插件
 *
 * Sprint 5 MVP 行为 (per ADR-0022 §3):
 * - Linux only (dlopen/dlsym/dlclose, #ifdef __linux__ 保护)
 * - 加载每个 .so 后读取 pdk_plugin_info (POD) + 调用 pdk_register_tools
 * - ABI 版本检查: 不匹配默认拒绝 (strict_version=true)
 * - 路径白名单 (Layer 1 安全): 拒绝 /etc /proc /sys /tmp 等敏感路径
 * - 搜索路径优先级: env var > ./plugins/ > ~/.hydraforge/plugins/ > /usr/local
 * - RAII: 析构时 dlclose 所有已加载 handle
 *
 * 不在 Sprint 5 范围 (Phase 2+):
 * - 跨平台 dlopen 抽象 (macOS dylib / Windows LoadLibrary)
 * - 完整 PluginLifecycle (on_load/on_unload 钩子)
 * - hot reload / plugin marketplace
 */
class PluginLoader {
 public:
  PluginLoader();
  ~PluginLoader();

  // 禁止拷贝/移动 (含 dlopen handle + loaded_ 状态)
  PluginLoader(const PluginLoader&) = delete;
  PluginLoader& operator=(const PluginLoader&) = delete;
  PluginLoader(PluginLoader&&) = delete;
  PluginLoader& operator=(PluginLoader&&) = delete;

  /**
   * @brief 扫描所有发现路径, 加载可用插件
   * @param registry 工具注册表 (插件工具注册到这里)
   * @return 成功加载的插件数量
   */
  std::size_t load_all(::agenticdsl::IToolRegistry& registry);

  /**
   * @brief 加载指定路径的单个 .so
   * @param path .so 文件路径 (绝对或相对)
   * @param registry 工具注册表
   * @param strict_version true (默认): ABI 不匹配拒绝; false: 警告但继续
   * @return 是否加载成功
   */
  bool load_so(const std::string& path,
               ::agenticdsl::IToolRegistry& registry,
               bool strict_version = true);

  /**
   * @brief 列出已加载的插件
   * @return 已加载插件的 PluginInfo 列表 (按加载顺序)
   */
  std::vector<PluginInfo> list_loaded() const;

  /**
   * @brief 卸载单个插件 (按 name 查找)
   * @param name 插件名 (PluginInfo::name)
   * @return true: 找到并卸载; false: 未找到
   */
  bool unload_plugin(const std::string& name);

 private:
  /**
   * @brief 获取插件搜索路径 (按优先级降序)
   * @return 路径列表 (env var > ./plugins > ~/.hydraforge/plugins > /usr/local)
   */
  std::vector<std::string> get_search_paths() const;

  /**
   * @brief 检查 PluginInfo ABI 兼容性
   * @param info 插件元数据
   * @return true: 兼容 (info.abi_version == CURRENT_ABI_VERSION)
   */
  bool check_compatibility(const PluginInfo& info) const;

  /**
   * @brief 应用路径白名单 (Layer 1 安全)
   * @param path .so 文件路径
   * @return true: 路径在白名单; false: 拒绝 (含黑名单路径)
   */
  bool apply_path_whitelist(const std::string& path) const;

  /**
   * @brief 内部结构: 已加载插件记录
   */
  struct LoadedPlugin {
    void* handle;              // dlopen handle (non-null)
    PluginInfo info;           // POD 拷贝
    std::string path;          // 原始路径 (用于日志)
  };

  std::vector<LoadedPlugin> loaded_;
};

} // namespace hydraforge