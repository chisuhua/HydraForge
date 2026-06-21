// include/agenticdsl/plugin/plugin_info.h
// 文件头注释
// 功能描述：PluginInfo POD 结构 — 插件元数据 (per ADR-0022 §1.2)。
//          dlsym 后零代码执行读取, 跨二进制 ABI 稳定。
//          含 abi_version 字段 + 当前 ABI 常量 (CURRENT_ABI_VERSION = 1)。
// 设计依据：ADR-0022 §1.2 PluginInfo 结构体 + openspec/changes/2026-07-14-plugin-loader
// 作者：AgenticDSL Phase 1 Sprint 5
// 最后修改日期：2026-06-19

#pragma once

#include <cstdint>

namespace hydraforge {

/**
 * @brief 插件元数据 (POD 结构, 仅含值类型)
 *
 * 目的: Runtime 在调用任何插件函数前即可读取元数据
 * (无需执行插件代码, 避免鸡生蛋问题)。
 *
 * 约束:
 * - 必须是 POD 类型 (无构造/析构/虚函数)
 * - 字段顺序固定 (跨编译单元 ABI 兼容)
 * - 字符数组使用 C 风格 (避免 std::string 跨二进制 ABI 问题)
 */
struct PluginInfo {
  // 接口版本 (定义编译时 ABI 兼容性)
  // 当前版本: 1
  uint32_t abi_version;

  // 插件名称 (仅 ASCII, 最大 63 字节 + null terminator)
  char name[64];

  // 语义版本号
  uint32_t major_version;
  uint32_t minor_version;
  uint32_t patch_version;

  // 插件描述 (最大 255 字节 + null terminator)
  char description[256];

  // 提供的能力标签集合 (逗号分隔, 最大 511 字节 + null terminator)
  // 例如: "code,code::edit,code::lsp"
  char capabilities[512];
};

/**
 * @brief 当前 ABI 版本常量 (Runtime 期望 .so 的 abi_version 匹配此值)
 *
 * Sprint 5 MVP: 1
 *
 * 递增规则 (per ADR-0022 §4.3):
 * - 宏展开方式不变 (e.g. DECLARE_TOOL 新增参数): 不变
 * - PluginInfo 结构体字段变化: +1
 * - PluginLoader 接口签名变化: +1
 * - ToolRegistry 接口签名变化: +1
 */
inline constexpr uint32_t CURRENT_ABI_VERSION = 1;

} // namespace hydraforge