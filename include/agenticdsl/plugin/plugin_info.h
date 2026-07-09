// include/agenticdsl/plugin/plugin_info.h
// 文件头注释
// 功能描述：PluginInfo POD 结构 — 插件元数据 (per ADR-0022 §1.2 + ADR-0041 §1.5)。
//          dlsym 后零代码执行读取, 跨二进制 ABI 稳定。
//          含 abi_version 字段 + 当前 ABI 常量 (CURRENT_ABI_VERSION = 2)。
//          支持 dual ABI dispatch: PluginInfoV1 (老) + PluginInfoV2 (新) 都接受。
// 设计依据：ADR-0022 §1.2 PluginInfo 结构体 + ADR-0041 §1.5 dual ABI + openspec/changes/2026-07-14-plugin-loader
// 作者：AgenticDSL Phase 1 Sprint 5 → Sprint 21 (Phase 5 C14)
// 最后修改日期：2026-07-08 (ABI bump 1→2, 新增 dependencies 字段 per ADR-0041)

#pragma once

#include <cstdint>

namespace hydraforge {

/**
 * @brief 插件元数据 V1 (Sprint 5 MVP, ABI 1)
 *
 * 保留 V1 类型用于 backward compat — 老 .so 仍可被新 PluginLoader 加载
 * (PluginLoader 读 V1 后转换为 V2, dependencies 默认为空)。
 *
 * sizeof = 4 + 64 + 4*3 + 256 + 512 = 848 字节
 */
struct PluginInfoV1 {
  uint32_t abi_version;             // = 1
  char name[64];
  uint32_t major_version;
  uint32_t minor_version;
  uint32_t patch_version;
  char description[256];
  char capabilities[512];
};

/**
 * @brief 插件元数据 V2 (Sprint 21 Phase 5 C14, ABI 2)
 *
 * 新增 dependencies[256] 字段 — comma-separated plugin names 表达加载依赖。
 * orchestration plugin 声明对 inference plugin 依赖 → PluginLoader 拓扑排序。
 *
 * sizeof = 4 + 64 + 4*3 + 256 + 512 + 256 = 1104 字节
 */
struct PluginInfoV2 {
  uint32_t abi_version;             // = 2 (CURRENT_ABI_VERSION)
  char name[64];
  uint32_t major_version;
  uint32_t minor_version;
  uint32_t patch_version;
  char description[256];
  char capabilities[512];
  char dependencies[256];           // NEW: comma-separated plugin names (空字符串 = 无依赖)
};

/**
 * @brief 插件元数据 V2 (向后兼容别名 — Type alias: 与既有代码兼容)
 *
 * 新代码应直接使用 PluginInfoV2 (类型自描述)。
 * 老代码 `PluginInfo` 仍可用 (typedef 至 V2, 行为不变)。
 */
using PluginInfo = PluginInfoV2;

/**
 * @brief 当前 ABI 版本常量 (Runtime 期望 .so 的 abi_version 匹配此值)
 *
 * Sprint 21: 2 (per ADR-0041 §1.5 — 新增 dependencies 字段)
 *
 * 递增规则 (per ADR-0022 §4.3):
 * - 宏展开方式不变 (e.g. DECLARE_TOOL 新增参数): 不变
 * - PluginInfo 结构体字段变化: +1
 * - PluginLoader 接口签名变化: +1
 * - ToolRegistry 接口签名变化: +1
 *
 * 支持的 ABI 版本 (PluginLoader dual dispatch):
 * - {1, 2} — 老 V1 .so + 新 V2 .so 都可加载
 */
inline constexpr uint32_t CURRENT_ABI_VERSION = 2;
inline constexpr uint32_t SUPPORTED_ABI_VERSIONS[] = {1, CURRENT_ABI_VERSION};

} // namespace hydraforge