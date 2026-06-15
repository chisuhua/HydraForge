# include/agenticdsl/pdk/CMakeLists.txt
# 占位头文件库 — Sprint 4 PDK 骨架将在此添加 DECLARE_TOOL / DEFINE_AGENT 宏
# 命名: agenticdsl_hdr_pdk (与 src/ 实现库区分, 避免与未来 agenticdsl_modules_pdk 冲突)

add_library(agenticdsl_hdr_pdk INTERFACE)
target_include_directories(agenticdsl_hdr_pdk INTERFACE
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>
)
