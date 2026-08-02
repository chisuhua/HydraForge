# tests/CMakeLists.txt
# pdk_chat_demo tests

# test_chat_session
add_executable(test_chat_session
    test_chat_session.cpp
    ${CATCH_INCLUDE_DIR}/catch_amalgamated.cpp
    ${CATCH_INCLUDE_DIR}/main_test_runner.cpp
)
target_link_libraries(test_chat_session PRIVATE
    pdk_chat_demo_obj
    agenticdsl_includes
    agenticdsl_core
    hydraforge_pdk
    Threads::Threads
)
target_include_directories(test_chat_session PRIVATE
    ${CATCH_INCLUDE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/..   # for #include "chat_session.h"
)
target_compile_definitions(test_chat_session PRIVATE
    CATCH_CONFIG_ENABLE_ALL_STRINGMAKERS=1
)
add_test(
    NAME test_chat_session
    COMMAND test_chat_session
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}  # config.json is here (../ relative to tests/)
)

# test_e2e_mock
add_executable(test_e2e_mock
    test_e2e_mock.cpp
    ${CATCH_INCLUDE_DIR}/catch_amalgamated.cpp
    ${CATCH_INCLUDE_DIR}/main_test_runner.cpp
)
target_link_libraries(test_e2e_mock PRIVATE
    pdk_chat_demo_obj
    agenticdsl_includes
    agenticdsl_core
    hydraforge_pdk
    Threads::Threads
)
target_include_directories(test_e2e_mock PRIVATE
    ${CATCH_INCLUDE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/..
)
target_compile_definitions(test_e2e_mock PRIVATE
    CATCH_CONFIG_ENABLE_ALL_STRINGMAKERS=1
    LOOP_AGENT_SO_PATH="${PROJECT_BINARY_DIR}/pdk/loop_agent/libLoopAgent.so"
)
# dlopen 的 LoopAgent.so 需解析 agenticdsl_core 符号
target_link_options(test_e2e_mock PRIVATE -Wl,--export-dynamic)
add_dependencies(test_e2e_mock LoopAgent)
add_test(
    NAME test_e2e_mock
    COMMAND test_e2e_mock
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
)
set_tests_properties(test_e2e_mock PROPERTIES
    ENVIRONMENT "HYDRAFORGE_PLUGIN_PATH=${PROJECT_BINARY_DIR}/pdk:${CMAKE_BINARY_DIR}/pdk"
)

# test_dsl_validation — T2: DSL schema 校验 (≥5 scenarios)
add_executable(test_dsl_validation
    test_dsl_validation.cpp
    ${CATCH_INCLUDE_DIR}/catch_amalgamated.cpp
    ${CATCH_INCLUDE_DIR}/main_test_runner.cpp
)
target_link_libraries(test_dsl_validation PRIVATE
    pdk_chat_demo_obj
    agenticdsl_includes
    agenticdsl_core
    hydraforge_pdk
    Threads::Threads
)
target_include_directories(test_dsl_validation PRIVATE
    ${CATCH_INCLUDE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/src
    ${PROJECT_SOURCE_DIR}/external/nlohmann_json/single_include
)
target_compile_definitions(test_dsl_validation PRIVATE
    CATCH_CONFIG_ENABLE_ALL_STRINGMAKERS=1
)
add_test(
    NAME test_dsl_validation
    COMMAND test_dsl_validation
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
)

# test_loop_agent_plugin — loop-agent-dsl-execution 集成测试
# 验证: dlopen LoopAgent.so → set_parent_provider → loop/run 真实执行
add_executable(test_loop_agent_plugin
    test_loop_agent_plugin.cpp
    ${CATCH_INCLUDE_DIR}/catch_amalgamated.cpp
    ${CATCH_INCLUDE_DIR}/main_test_runner.cpp
)
target_link_libraries(test_loop_agent_plugin PRIVATE
    pdk_chat_demo_obj
    agenticdsl_includes
    agenticdsl_core
    hydraforge_pdk
    Threads::Threads
)
target_include_directories(test_loop_agent_plugin PRIVATE
    ${CATCH_INCLUDE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/src
    ${PROJECT_SOURCE_DIR}/external/nlohmann_json/single_include
)
target_compile_definitions(test_loop_agent_plugin PRIVATE
    CATCH_CONFIG_ENABLE_ALL_STRINGMAKERS=1
    LOOP_AGENT_SO_PATH="${PROJECT_BINARY_DIR}/pdk/loop_agent/libLoopAgent.so"
)
# dlopen 的 LoopAgent.so 需解析 agenticdsl_core 符号
target_link_options(test_loop_agent_plugin PRIVATE -Wl,--export-dynamic)
add_test(
    NAME test_loop_agent_plugin
    COMMAND test_loop_agent_plugin
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/../../..  # build/tests → workspace root
)
set_tests_properties(test_loop_agent_plugin PROPERTIES
    ENVIRONMENT "HYDRAFORGE_PLUGIN_PATH=${PROJECT_BINARY_DIR}/pdk:${CMAKE_BINARY_DIR}/pdk"
)

# test_session_persistence - T1: session save/restore, corrupted JSON, stale cleanup
add_executable(test_session_persistence
    test_session_persistence.cpp
    ${CATCH_INCLUDE_DIR}/catch_amalgamated.cpp
    ${CATCH_INCLUDE_DIR}/main_test_runner.cpp
)
target_link_libraries(test_session_persistence PRIVATE
    pdk_chat_demo_obj
    agenticdsl_includes
    agenticdsl_core
    hydraforge_pdk
    Threads::Threads
)
target_include_directories(test_session_persistence PRIVATE
    ${CATCH_INCLUDE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/src
    ${PROJECT_SOURCE_DIR}/external/nlohmann_json/single_include
)
target_compile_definitions(test_session_persistence PRIVATE
    CATCH_CONFIG_ENABLE_ALL_STRINGMAKERS=1
)
add_test(
    NAME test_session_persistence
    COMMAND test_session_persistence
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
)

# test_budget_alert - T1: budget exceeded emit, at-limit, thread-safe callback
add_executable(test_budget_alert
    test_budget_alert.cpp
    ${CATCH_INCLUDE_DIR}/catch_amalgamated.cpp
    ${CATCH_INCLUDE_DIR}/main_test_runner.cpp
)
target_link_libraries(test_budget_alert PRIVATE
    pdk_chat_demo_obj
    agenticdsl_includes
    agenticdsl_core
    hydraforge_pdk
    Threads::Threads
)
target_include_directories(test_budget_alert PRIVATE
    ${CATCH_INCLUDE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/src
    ${PROJECT_SOURCE_DIR}/external/nlohmann_json/single_include
)
target_compile_definitions(test_budget_alert PRIVATE
    CATCH_CONFIG_ENABLE_ALL_STRINGMAKERS=1
)
add_test(
    NAME test_budget_alert
    COMMAND test_budget_alert
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
)

# test_e2e_real_llm — 真实 LLM 集成测试 (minimax)
# 需要 MINIMAX_API_KEY 环境变量；未设置时自动跳过
add_executable(test_e2e_real_llm
    test_e2e_real_llm.cpp
    ${CATCH_INCLUDE_DIR}/catch_amalgamated.cpp
    ${CATCH_INCLUDE_DIR}/main_test_runner.cpp
)
target_link_libraries(test_e2e_real_llm PRIVATE
    pdk_chat_demo_obj
    agenticdsl_includes
    agenticdsl_core
    hydraforge_pdk
    Threads::Threads
)
target_include_directories(test_e2e_real_llm PRIVATE
    ${CATCH_INCLUDE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/src
    ${PROJECT_SOURCE_DIR}/external/nlohmann_json/single_include
)
target_compile_definitions(test_e2e_real_llm PRIVATE
    CATCH_CONFIG_ENABLE_ALL_STRINGMAKERS=1
)
add_test(
    NAME test_e2e_real_llm
    COMMAND test_e2e_real_llm
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
)