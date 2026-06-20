# include/agenticdsl/pdk/CMakeLists.txt
# 功能描述: PDK 头文件库 (Sprint 4 实施, ADR-0021)
#          DECLARE_TOOL / DEFINE_AGENT 宏 + SafeExec 封装
#          INTERFACE 库, 无 .cpp, 静态链接到插件 (P3)
# 设计依据: ADR-0021 §2.2 PDK 仓库结构 + §3.1 DECLARE_TOOL + openspec/changes/2026-07-07-pdk-skeleton
# 作者: AgenticDSL Phase 1 Sprint 4
# 最后修改日期: 2026-06-19

# INTERFACE 库 (header-only), 不产生二进制
# 命名: agenticdsl_hdr_pdk (与 src/ 实现库区分)
add_library(agenticdsl_hdr_pdk INTERFACE)

# 头文件路径: 指向 include/agenticdsl/ 的父目录 (即 include/)
# (PDK 头文件 include 路径为 <agenticdsl/pdk/...>, 与契约头一致)
target_include_directories(agenticdsl_hdr_pdk INTERFACE
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>
  $<INSTALL_INTERFACE:include>
)

# C++20 标准 (PDK 使用 std::invoke_result_t + 后续 Phase 2/3 扩展)
target_compile_features(agenticdsl_hdr_pdk INTERFACE cxx_std_20)

# 链接依赖: PDK 仅依赖 Runtime 契约接口 (P3 静态链接)
# (MVP 不链接 agenticdsl_contract, 但 Phase 2 集成 IToolRegistry 时需要)
# target_link_libraries(agenticdsl_hdr_pdk INTERFACE agenticdsl_contract)