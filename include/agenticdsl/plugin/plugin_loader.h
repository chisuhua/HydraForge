// include/agenticdsl/plugin/plugin_loader.h
// 文件头注释
// 功能描述：PluginLoader 类 — 动态加载 PDK 编译的 .so 插件 (per ADR-0022 §2-3)。
//          支持 Linux dlopen/dlsym, 含 ABI 版本检查 + 路径白名单 + 自动发现。
//          头文件仅前向声明 IToolRegistry (避免引入 Runtime 内部, 编译时验证 P3 静态链接)。
//          Phase 5 (OpenSpec `phase5-illmprovider-call-chain-v2` §6):
//            - 5 符号查找 (新增 pdk_create_llm_provider / pdk_plugin_init / pdk_plugin_fini)
//            - lifecycle 顺序保证 (init → register → 加载; 释放 shared_ptr → fini → dlclose)
//            - create_llm_provider() 抽象方法 — 通过 plugin 获取 shared_ptr<ILLMProvider>
//            - dependencies 循环检测 + 缺失依赖报错 (MVP, per proposal-v2 non-goals)
//          Phase 6a (OpenSpec `pdk-manifest-validation` §4):
//            - manifest-first 加载流程 (ManifestFinder → ManifestValidator → dlopen)
//            - require_manifest 参数 (默认 false, 保留向后兼容现有 12 PDK .so)
//            - IInteractionBus 事件发射 (plugin.manifest.invalid / plugin.manifest.missing)
//            - PIMPL-lite: Impl 结构体含 bus_ 指针 + manifest_ 缓存
// 设计依据：ADR-0022 §2-3 + ADR-0041 §1 PluginLoader lifecycle extension
//          + openspec/changes/phase5-illmprovider-call-chain-v2/specs/plugin-loader/spec.md
//          + openspec/changes/pdk-manifest-validation (Phase 6a §4)
// 作者：AgenticDSL Phase 1 Sprint 5 → Phase 5 B2 (C14 增量) → Phase 6a §4
// 最后修改日期：2026-08-10 (Phase 6a §4: manifest-first flow)

#pragma once

#include "agenticdsl/plugin/plugin_info.h"

#include <cstddef>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace agenticdsl { namespace pdk { struct Manifest; } }  // 前向声明 (避免引入 pdk 模块头文件)

namespace agenticdsl {
class IToolRegistry;  // contract 层抽象 (ADR-0019 §1.4)
class ILLMProvider;   // contract 层抽象 (Phase 5: pdk_create_llm_provider 返回类型)
class IInteractionBus;  // Phase 6a §5: 事件发射总线 (实际定义在 agenticdsl/contract/iinteraction_bus.h)
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
   * @param require_manifest false (默认): 无 manifest 警告但继续 (向后兼容);
   *              true: 无 manifest 则拒绝
   * @return 是否加载成功
   */
  bool load_so(const std::string& path,
               ::agenticdsl::IToolRegistry& registry,
               bool strict_version = true,
               bool require_manifest = false);

  /**
   * @brief 列出已加载的插件
   * @return 已加载插件的 PluginInfo 列表 (按加载顺序)
   */
  std::vector<PluginInfo> list_loaded() const;

  /**
   * @brief 设置 InteractionBus (Phase 6a §4: 用于 manifest 事件发射)
   * @param bus IInteractionBus 指针 (可为 nullptr)
   */
  void set_interaction_bus(::agenticdsl::IInteractionBus* bus);

  /**
   * @brief 清除 InteractionBus (Phase 6a §4)
   */
  void clear_interaction_bus();

  /**
   * @brief 卸载单个插件 (按 name 查找)
   *
   * Phase 5 lifecycle 顺序 (per REQ-PL-IPD-002 Scenario "lifecycle 顺序保证"):
   *   1. 释放该 plugin 持有的所有 shared_ptr<ILLMProvider> (若 loader 内部仍引用)
   *   2. 调用 pdk_plugin_fini() (若存在, 失败仅记 ERROR 不抛异常)
   *   3. 从 loaded_ 列表移除
   *   4. dlclose(handle)
   *
   * @param name 插件名 (PluginInfo::name)
   * @return true: 找到并卸载; false: 未找到
   */
  bool unload_plugin(const std::string& name);

  /**
   * @brief 通过 plugin 创建 ILLMProvider 实例 (Phase 5 新增)
   *
   * 调用 plugin 导出的 `pdk_create_llm_provider(const void* config)` 符号,
   * 返回 `shared_ptr<ILLMProvider>` (RAII 自动管理, 解决 Sprint 17 C7 destruction order bug)。
   *
   * 语义 (per REQ-PL-IPD-001 Scenario "pdk_create_llm_provider 调用语义"):
   *   - plugin 已加载且实现该符号 → 调用并返回 shared_ptr
   *   - plugin 已加载但未实现该符号 → 返回 nullptr (plugin 仅提供工具, 不提供 LLM)
   *   - plugin 已卸载 (handle 已 dlclose) → throw std::runtime_error
   *
   * @param plugin_name 已加载 plugin 的 name (PluginInfo::name)
   * @param config 透传给 plugin 的配置指针 (cross-ABI 安全, 实际类型由 plugin 自行解释)
   * @return shared_ptr<ILLMProvider> 或 nullptr
   */
  virtual std::shared_ptr<::agenticdsl::ILLMProvider>
      create_llm_provider(const std::string& plugin_name,
                          const void* config);

  /**
   * @brief 验证插件依赖关系 (MVP: 循环检测 + 缺失依赖报错)
   *
   * Phase 5 新增 (per REQ-PL-IPD-003 Scenario "拓扑加载依赖"):
   *   - 检查每个 plugin 的依赖项是否都存在于 loaded 列表中
   *   - 检测循环依赖 (A 依赖 B 且 B 依赖 A)
   *   - 不做完整 Kahn 拓扑排序 (per proposal-v2 non-goals)
   *
   * @param plugin_deps 每个元素: {plugin_name, comma-separated dependencies string}
   * @return {missing_errors, circular_errors} 各包含人可读的错误消息
   */
  static std::pair<std::vector<std::string>, std::vector<std::string>>
      validate_dependencies(const std::vector<std::pair<std::string, std::string>>& plugin_deps);

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
   *
   * Phase 5 扩展: 新增 3 个符号缓存字段 (create_llm_provider_fn / fini_fn) +
   * 持有的 shared_ptr<ILLMProvider> 列表 (用于 unload 时顺序释放)。
   * init_fn 不缓存 (调用即结束, 不需后续调用)。
   */
  struct LoadedPlugin {
    void* handle = nullptr;              // dlopen handle (non-null)
    PluginInfo info{};                   // POD 拷贝
    std::string path;                    // 原始路径 (用于日志)

    // Phase 5 新增符号缓存 (可选, nullptr = plugin 未实现)
    void* create_llm_provider_fn = nullptr;  // pdk_create_llm_provider 函数指针
    void* fini_fn = nullptr;                 // pdk_plugin_fini 函数指针

    // Phase 5 新增: loader 内部持有的 provider 实例 (用于 unload 时先释放)
    // 注: caller 通过 create_llm_provider() 拿到的 shared_ptr 由 caller 自行 RAII 管理,
    //     loader 不持有 caller 的引用 (per REQ-PL-IPD-001 Scenario "plugin 卸载时释放 shared_ptr")
    std::vector<std::weak_ptr<::agenticdsl::ILLMProvider>> provider_refs;
  };

 private:
  /**
   * @brief PIMPL-lite Impl: 持有 IInteractionBus 指针 + manifest 缓存
   * Phase 6a §4 (per Sprint 18-19 PIMPL pattern)
   * 注: Impl 在 .cpp 中定义, 使用完整类型 (避免 header 引入 pdk 模块)
   */
  struct Impl;
  std::unique_ptr<Impl> impl_;

  std::vector<LoadedPlugin> loaded_;
};

} // namespace hydraforge