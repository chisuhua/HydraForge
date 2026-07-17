# pdk/shell_tools/CMakeLists.txt
# Shell Tools Plugin - 执行 shell 命令
# 关联: docs/examples/pdk_chat_demo/DESIGN.md §5.7

cmake_minimum_required(VERSION 3.20)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)


add_library(ShellTools SHARED
    src/pdk_entry.cpp
)

target_include_directories(ShellTools PRIVATE
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/src

  ${PROJECT_SOURCE_DIR}/external/nlohmann_json/single_include)

target_link_libraries(ShellTools PRIVATE
    hydraforge_pdk

)

set_target_properties(ShellTools PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/pdk/shell_tools"
)

install(TARGETS ShellTools LIBRARY DESTINATION lib/pdk/shell_tools)