# include/agenticdsl/plugin/CMakeLists.txt
# 功能描述: PluginLoader 头文件库 (Sprint 5 实施, ADR-0022)
#          PluginInfo POD + PluginLoader 类声明
#          INTERFACE 库, 无 .cpp, 静态链接到 plugin_loader (S5.T2)
# 设计依据: ADR-0022 §1.2 PluginInfo + §2-3 PluginLoader API + openspec/changes/2026-07-14-plugin-loader
# 作者: AgenticDSL Phase 1 Sprint 5
# 最后修改日期: 2026-06-19

# INTERFACE 库 (header-only)
# 命名: agenticdsl_hdr_plugin (与 src/modules/plugin 实现的 agenticdsl_modules_plugin 区分)
add_library(agenticdsl_hdr_plugin INTERFACE)

# 头文件路径: 指向 include/agenticdsl/ 的父目录 (即 include/)
# (PluginLoader 头文件 include 路径为 <agenticdsl/plugin/...>, 与契约头一致)
target_include_directories(agenticdsl_hdr_plugin INTERFACE
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>
  $<INSTALL_INTERFACE:include>
)

# C++20 标准 (PluginLoader 使用 std::filesystem 等)
target_compile_features(agenticdsl_hdr_plugin INTERFACE cxx_std_20)

# 链接依赖: PluginLoader 需要 ToolRegistry 抽象接口 (P1 解耦)
# (实际链接到 agenticdsl_core, 由 src/modules/plugin/CMakeLists.txt 处理)