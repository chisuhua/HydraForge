# external/CMakeLists.txt
# 异步基础设施配置（Slice 00）
#
# Taskflow 与 async_simple 作为第三方依赖被 add_subdirectory 引入。
# 关闭其内部测试 / 示例以避免 gtest 等额外依赖与编译时间。

# Taskflow (header-only, v3.9.0) — 在主 CMakeLists.txt 中 add_subdirectory(external/taskflow) 之前已设置
# 这里仅作说明，不重复 add_subdirectory（避免重复配置）。

# async_simple: 禁用测试、demo、ASAN 以避免 gtest/standalone-asio 依赖及减少编译时间
# - ASYNC_SIMPLE_ENABLE_TESTS=OFF  : 跳过 GTest 测试
# - ASYNC_SIMPLE_BUILD_DEMO_EXAMPLE=OFF : 跳过 demo_example/（含 async_echo_server 等大文件，需 standalone-asio）
# - ASYNC_SIMPLE_ENABLE_ASAN=OFF   : Debug 模式下不注入 -fsanitize=address，避免污染主项目链接
set(ASYNC_SIMPLE_ENABLE_TESTS OFF CACHE BOOL "" FORCE)
set(ASYNC_SIMPLE_BUILD_DEMO_EXAMPLE OFF CACHE BOOL "" FORCE)
set(ASYNC_SIMPLE_ENABLE_ASAN OFF CACHE BOOL "" FORCE)
add_subdirectory(async_simple)
