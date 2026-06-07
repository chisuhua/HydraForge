cmake_minimum_required(VERSION 3.20)
project(AgenticDSL LANGUAGES CXX)

option(AGENTICDSL_BUILD_TESTS "Build unit tests" OFF)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(PkgConfig REQUIRED)
find_package(Threads REQUIRED)

# cpp-httplib (header-only)
set(HTTPLIB_INCLUDE_DIR "${CMAKE_SOURCE_DIR}/external/cpp-httplib")

# nlohmann_json (header-only)
find_path(NLOHMANN_JSON_INCLUDE_DIR nlohmann/json.hpp)
if(NOT NLOHMANN_JSON_INCLUDE_DIR)
    set(NLOHMANN_JSON_INCLUDE_DIR "${CMAKE_SOURCE_DIR}/external/nlohmann_json/include")
endif()

# inja (header-only)
find_path(INJA_INCLUDE_DIR inja/inja.hpp)
if(NOT INJA_INCLUDE_DIR)
    set(INJA_INCLUDE_DIR "${CMAKE_SOURCE_DIR}/external/inja/include")
endif()

set(YAML_CPP_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(YAML_CPP_BUILD_TOOLS OFF CACHE BOOL "" FORCE)
set(YAML_CPP_INSTALL OFF CACHE BOOL "" FORCE)
add_subdirectory(external/yaml-cpp yaml-cpp-build)
set(YAML_CPP_INCLUDE_DIR "${CMAKE_SOURCE_DIR}/external/yaml-cpp/include")

# INTERFACE library for common includes and dependencies
add_library(agenticdsl_includes INTERFACE)

target_include_directories(agenticdsl_includes INTERFACE
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include>
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src>
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src/modules>
    $<INSTALL_INTERFACE:include>
    ${NLOHMANN_JSON_INCLUDE_DIR}
    ${INJA_INCLUDE_DIR}
    ${YAML_CPP_INCLUDE_DIR}
    ${HTTPLIB_INCLUDE_DIR}
)

target_link_libraries(agenticdsl_includes INTERFACE
    yaml-cpp::yaml-cpp
)

add_library(agenticdsl_common STATIC
    src/common/llm/http_adapter.cpp
    src/common/llm/llama_adapter.cpp
    src/common/llm/llama_tool.cpp
    src/common/tools/registry.cpp
    src/common/utils/parser_utils.cpp
    src/common/utils/template_renderer.cpp
    src/common/utils/yaml_json.cpp
)
target_link_libraries(agenticdsl_common PUBLIC
    agenticdsl_includes
)

add_subdirectory(src/modules/parser)
add_subdirectory(src/modules/context)
add_subdirectory(src/modules/budget)
add_subdirectory(src/modules/scheduler)
add_subdirectory(src/modules/executor)
add_subdirectory(src/modules/trace)
add_subdirectory(src/modules/library)
add_subdirectory(src/modules/system)

add_library(agenticdsl_core STATIC
    src/core/engine.cpp
)

target_link_libraries(agenticdsl_core
    agenticdsl_includes
    agenticdsl_common
    agenticdsl_modules_parser
    agenticdsl_modules_context
    agenticdsl_modules_budget
    agenticdsl_modules_executor
    agenticdsl_modules_scheduler
    agenticdsl_modules_trace
    agenticdsl_modules_library
    agenticdsl_modules_system
    Threads::Threads
)

# Alias target for tests
add_library(agenticdsl ALIAS agenticdsl_core)

# --- Sample: agent_basic ---
add_executable(agent_basic
    examples/agent_basic/main.cpp
)

target_link_libraries(agent_basic
    agenticdsl_includes
    agenticdsl_core
)

# Create alias for tests

# --- Tests (Optional) ---
if(AGENTICDSL_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
