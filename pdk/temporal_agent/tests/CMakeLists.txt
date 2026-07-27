# pdk/temporal_agent/tests/CMakeLists.txt
# 功能描述：Temporal Agent PDK 测试构建配置 (Task 1: ITemporalClient 接口测试)
#          Catch2 amalgamated 模式, 复用 tests/ 目录的 catch_amalgamated.{cpp,hpp}
# 设计依据：.rddf/plans/pkm-temporal-demo-scaffold.md Task 1 Step 1
# 作者：pkm-temporal-demo-scaffold Task 1
# 最后修改日期：2026-07-28

set(CATCH_DIR ${PROJECT_SOURCE_DIR}/tests)

add_executable(test_itemporal_client
    test_itemporal_client.cpp
    ${CATCH_DIR}/catch_amalgamated.cpp
    ${CATCH_DIR}/main_test_runner.cpp
)

target_include_directories(test_itemporal_client PRIVATE
    ${CATCH_DIR}
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/src
    ${PROJECT_SOURCE_DIR}/external/nlohmann_json/single_include
)

target_compile_features(test_itemporal_client PRIVATE cxx_std_20)
target_compile_definitions(test_itemporal_client PRIVATE CATCH_CONFIG_ENABLE_ALL_STRINGMAKERS=1)

# Task 1: 仅链接 hydraforge_pdk INTERFACE (header-only ITemporalClient)
target_link_libraries(test_itemporal_client PRIVATE
    hydraforge_pdk
)

add_test(NAME test_itemporal_client COMMAND test_itemporal_client)
