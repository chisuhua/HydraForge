# pdk/CMakeLists.txt
# 功能描述：PDK monorepo 子目录构建配置 (Sprint 4 实施, ADR-0021 K3 决策)
#          hydraforge-pdk PDK 头文件库 — INTERFACE 库, 静态链接到插件 (P3)
#          Phase 2 拆分至独立 hydraforge-pdk 仓库时, 此 CMakeLists.txt 移至独立仓库,
#          根 CMakeLists.txt 改用 find_package(hydraforge_pdk REQUIRED)
# 设计依据：ADR-0021 §2.2 + §4 + openspec/changes/2026-07-07-pdk-skeleton
# 作者：AgenticDSL Phase 1 Sprint 4
# 最后修改日期：2026-06-19

# INTERFACE 库 (header-only), 命名: hydraforge_pdk
add_library(hydraforge_pdk INTERFACE)

# 头文件路径: pdk/include/ (含 hydraforge/pdk/... 路径别名 + agenticdsl/pdk/ 实际头)
target_include_directories(hydraforge_pdk INTERFACE
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:include>
)

# C++20 标准 (PDK 使用 std::invoke_result_t + 后续 Phase 2/3 扩展)
target_compile_features(hydraforge_pdk INTERFACE cxx_std_20)

# 链接依赖: PDK 仅依赖 Runtime 契约接口 (P3 静态链接)
# MVP 不强依赖 agenticdsl_contract (PDK 头文件通过 #include 访问)
# Phase 2 集成 IToolRegistry/IInteractionBus 时启用:
# target_link_libraries(hydraforge_pdk INTERFACE agenticdsl_contract)