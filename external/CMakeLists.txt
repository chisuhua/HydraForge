# external/CMakeLists.txt
# 第三方依赖配置
#
# Taskflow 与 async_simple 作为第三方依赖被 add_subdirectory 引入。
# 关闭其内部测试 / 示例以避免 gtest 等额外依赖与编译时间。

# cxxopts v3.2.1 - header-only, vendored under external/cxxopts/
# 版本记录: jarro2783/cxxopts@v3.2.1 (MIT License)
add_library(hydraforge_cxxopts INTERFACE)
target_include_directories(hydraforge_cxxopts INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/cxxopts)

# Taskflow (header-only, v3.9.0) — 在主 CMakeLists.txt 中 add_subdirectory(external/taskflow) 之前已设置
# 这里仅作说明，不重复 add_subdirectory（避免重复配置）。

# async_simple 已 deprecated (ADR-0030 V2 决策: std::jthread 替代)
# 通过 root CMake 独立 add_subdirectory(external/async_simple) 条件引入 (当前已注释)
# set(ASYNC_SIMPLE_ENABLE_TESTS OFF CACHE BOOL "" FORCE)
# set(ASYNC_SIMPLE_BUILD_DEMO_EXAMPLE OFF CACHE BOOL "" FORCE)
# set(ASYNC_SIMPLE_ENABLE_ASAN OFF CACHE BOOL "" FORCE)
# add_subdirectory(async_simple)
