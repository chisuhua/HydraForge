# examples/pkm_temporal_demo/CMakeLists.txt
# 功能描述：PKM Temporal Demo 构建配置 (Task 4-6)
#          Task 4: CLI 入口 (main.cpp + demo_args.h)
#          Task 5: 4 场景 DSL 文件 (.agent.md)
#          Task 6: ctest 注册
# 设计依据：.rddf/plans/pkm-temporal-demo-scaffold.md Task 4-6
# 作者：pkm-temporal-demo-scaffold Task 4
# 最后修改日期：2026-07-28

cmake_minimum_required(VERSION 3.20)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ============================================================================
# Task 4: Demo 可执行文件
# ============================================================================
add_executable(pkm_temporal_demo main.cpp)

target_include_directories(pkm_temporal_demo PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}                 # demo_args.h
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/src
    ${PROJECT_SOURCE_DIR}/pdk/temporal_agent    # mock_client.h
    ${PROJECT_SOURCE_DIR}/external/nlohmann_json/single_include
)

target_link_libraries(pkm_temporal_demo PRIVATE
    agenticdsl_includes
    agenticdsl_core
    pdk_temporal_agent                          # mock_client.cpp + pdk_entry.cpp
)

# ============================================================================
# Task 4: 测试子目录
# ============================================================================
add_subdirectory(tests)
