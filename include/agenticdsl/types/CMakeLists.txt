# include/agenticdsl/types/CMakeLists.txt
# 占位头文件库 — P1.T3 TraceRecord 上移将使用此目录
# 命名: agenticdsl_hdr_types (与 src/core/types 区分)

add_library(agenticdsl_hdr_types INTERFACE)
target_include_directories(agenticdsl_hdr_types INTERFACE
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>
)
