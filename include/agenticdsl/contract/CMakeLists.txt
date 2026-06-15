# include/agenticdsl/contract/CMakeLists.txt
# 占位头文件库 — P1 Residual engine.h Decoupling 将添加 IProviderFactory + IToolRegistry
# 命名: agenticdsl_hdr_contract (与 src/common/contract 实现的 agenticdsl_contract 区分)

add_library(agenticdsl_hdr_contract INTERFACE)
target_include_directories(agenticdsl_hdr_contract INTERFACE
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>
)
