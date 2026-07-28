# examples/pkm_temporal_demo/tests/CMakeLists.txt
# 功能描述：PKM Temporal Demo 测试构建配置 (Task 4-5)
# 设计依据：.rddf/plans/pkm-temporal-demo-scaffold.md Task 4-5
# 作者：pkm-temporal-demo-scaffold Task 4
# 最后修改日期：2026-07-28

set(CATCH_DIR ${PROJECT_SOURCE_DIR}/tests)

# ============================================================================
# Task 4: CLI 参数解析测试
# ============================================================================
add_executable(test_demo_main
    test_main.cpp
    ${CATCH_DIR}/catch_amalgamated.cpp
    ${CATCH_DIR}/main_test_runner.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../demo_args.cpp
)

target_include_directories(test_demo_main PRIVATE
    ${CATCH_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/..           # demo_args.h
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/external/nlohmann_json/single_include
)

target_compile_features(test_demo_main PRIVATE cxx_std_20)
target_compile_definitions(test_demo_main PRIVATE CATCH_CONFIG_ENABLE_ALL_STRINGMAKERS=1)

add_test(NAME test_demo_main COMMAND test_demo_main)
