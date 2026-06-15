# include/agenticdsl/cognitive/CMakeLists.txt
# 占位头文件库 — Sprint 2/3 CognitiveWorker + DomainWorkerPool 将使用此目录
# 命名: agenticdsl_hdr_cognitive (与 src/modules/cognitive 实现的 agenticdsl_modules_cognitive 区分)

add_library(agenticdsl_hdr_cognitive INTERFACE)
target_include_directories(agenticdsl_hdr_cognitive INTERFACE
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>
)
