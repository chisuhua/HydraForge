# include/agenticdsl/plugin/CMakeLists.txt
# 占位头文件库 — Sprint 5 PluginLoader 将在此添加 PluginInfo POD + PluginLoader 声明
# 命名: agenticdsl_hdr_plugin (与 src/modules/plugin 实现的 agenticdsl_modules_plugin 区分)

add_library(agenticdsl_hdr_plugin INTERFACE)
target_include_directories(agenticdsl_hdr_plugin INTERFACE
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>
)
