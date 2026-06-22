# 启用测试支持
enable_testing()

# Catch2 单头文件路径
set(CATCH_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR})

# 定义创建单个 Catch2 测试的函数
function(add_catch_test TEST_NAME)
    # 参数：测试名称，其余为该测试的源文件（不含 catch_amalgamated.cpp）
    set(SOURCES ${ARGN})

    # 自动包含 catch_amalgamated.cpp
    list(APPEND SOURCES ${CATCH_INCLUDE_DIR}/catch_amalgamated.cpp main_test_runner.cpp)

    # 创建可执行文件
    add_executable(${TEST_NAME} ${SOURCES})

    # 包含目录
    target_include_directories(${TEST_NAME} PRIVATE
        ${CATCH_INCLUDE_DIR}
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/external/nlohmann_json/single_include
        ${PROJECT_SOURCE_DIR}/external/inja/include
        ${PROJECT_SOURCE_DIR}/external/yaml-cpp/include
        ${PROJECT_SOURCE_DIR}/external/taskflow
        ${PROJECT_SOURCE_DIR}/external/async_simple
    )

    target_link_libraries(${TEST_NAME} PRIVATE agenticdsl agenticdsl_modules_parser yaml-cpp::yaml-cpp)
    # Plugin 模块 (Sprint 5): dlopen/dlsym 实现的 PluginLoader
    target_link_libraries(${TEST_NAME} PRIVATE agenticdsl_modules_plugin)
    target_compile_definitions(${TEST_NAME} PRIVATE CATCH_CONFIG_ENABLE_ALL_STRINGMAKERS=1)

    # 注册为 CTest 测试
    add_test(NAME ${TEST_NAME} COMMAND ${TEST_NAME})

    # test_cloud_llm_live 需真实 API key，标记为 live 标签，CI 默认不跑
    if(TEST_NAME STREQUAL "test_cloud_llm_live")
        set_tests_properties(${TEST_NAME} PROPERTIES LABELS "live")
    endif()

    # test_async_bridge 额外链接 Taskflow 与 async_simple（Slice 00 桥接验证）
    # 这两个库是 INTERFACE 库，使用 PR/PRIVATE 传播即可。
    # 实际 target 名（已验证 CMakeLists 中无 namespace 别名）：
    #   - Taskflow           （来自 external/taskflow/CMakeLists.txt: project(Taskflow ...)）
    #   - async_simple_header_only （来自 external/async_simple/CMakeLists.txt: add_library(async_simple_header_only INTERFACE)）
    if(TEST_NAME STREQUAL "test_async_bridge")
        target_link_libraries(${TEST_NAME} PRIVATE Taskflow async_simple_header_only)
    endif()
endfunction()

# ================================
# 方案一：为每个 test_xxx.cpp 创建独立测试（推荐）
# ================================

file(GLOB SINGLE_TEST_SOURCES "test_*.cpp")
# 列出所有独立的测试源文件（每个文件一个测试）
#set(SINGLE_TEST_SOURCES
#    test_basic.cpp
#    test_engine.cpp
#    test_parser.cpp
#    test_scheduler.cpp
#)

# 为每个源文件创建一个独立的测试
foreach(TEST_SRC ${SINGLE_TEST_SOURCES})
    # 提取测试名，如 test_reg.cpp -> test_reg
    get_filename_component(TEST_NAME ${TEST_SRC} NAME_WE)
    add_catch_test(${TEST_NAME} ${TEST_SRC})
endforeach()

# ─────────────────────────────────────────────────────────────
# Sprint 7 Day 3 quick win (Oracle Top 2 - 推迟自 Day 2 spot-check):
# 静态 grep 锁定 fork 状态契约, 防止 Day 1 fork dedup 回归。
# 详见: tests/test_fork_static_contracts.sh
# 关联 Oracle: ses_111741590ffeF6HMxhgYq7YKrr
# 关联 spec: openspec/changes/sprint-7-tech-debt-followup/specs/sprint-7-tech-debt-followup/spec.md
# ─────────────────────────────────────────────────────────────
add_test(NAME test_fork_static_contracts
         COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/test_fork_static_contracts.sh
         WORKING_DIRECTORY ${PROJECT_SOURCE_DIR})

