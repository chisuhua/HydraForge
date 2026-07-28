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

# ============================================================================
# Task 2: MockTemporalClient 状态机测试
# ============================================================================
add_executable(test_mock_client
    test_mock_client.cpp
    ${CATCH_DIR}/catch_amalgamated.cpp
    ${CATCH_DIR}/main_test_runner.cpp
)

target_include_directories(test_mock_client PRIVATE
    ${CATCH_DIR}
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/..          # mock_client.h
    ${PROJECT_SOURCE_DIR}/external/nlohmann_json/single_include
)

target_compile_features(test_mock_client PRIVATE cxx_std_20)
target_compile_definitions(test_mock_client PRIVATE CATCH_CONFIG_ENABLE_ALL_STRINGMAKERS=1)

# Task 2: 链接 mock_client.cpp (直接编译, 非 .so)
target_link_libraries(test_mock_client PRIVATE
    hydraforge_pdk
)

# Task 2: mock_client.cpp 直接编译进测试二进制 (避免单独 .so 依赖)
target_sources(test_mock_client PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../mock_client.cpp
)

add_test(NAME test_mock_client COMMAND test_mock_client)

# ============================================================================
# Task 3: PDK 5 工具注册测试
# ============================================================================
add_executable(test_pdk_entry
    test_pdk_entry.cpp
    ${CATCH_DIR}/catch_amalgamated.cpp
    ${CATCH_DIR}/main_test_runner.cpp
)

target_include_directories(test_pdk_entry PRIVATE
    ${CATCH_DIR}
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/..          # mock_client.h + pdk_entry symbols
    ${PROJECT_SOURCE_DIR}/external/nlohmann_json/single_include
)

target_compile_features(test_pdk_entry PRIVATE cxx_std_20)
target_compile_definitions(test_pdk_entry PRIVATE CATCH_CONFIG_ENABLE_ALL_STRINGMAKERS=1)

# Task 3: 编译 pdk_entry.cpp + mock_client.cpp 进测试 (无需 .so 链接)
target_sources(test_pdk_entry PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../pdk_entry.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../mock_client.cpp
)

target_link_libraries(test_pdk_entry PRIVATE
    hydraforge_pdk
    agenticdsl_common           # IToolRegistry / ToolMetadata 实现
)

add_test(NAME test_pdk_entry COMMAND test_pdk_entry)
