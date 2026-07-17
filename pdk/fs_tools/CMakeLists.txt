# pdk/fs_tools/CMakeLists.txt
# FS Tools Plugin - 文件系统读/写/列表
# 关联: docs/examples/pdk_chat_demo/DESIGN.md §5.7

cmake_minimum_required(VERSION 3.20)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)


add_library(FSTools SHARED
    src/pdk_entry.cpp
)

target_include_directories(FSTools PRIVATE
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/src

  ${PROJECT_SOURCE_DIR}/external/nlohmann_json/single_include)

target_link_libraries(FSTools PRIVATE
    hydraforge_pdk

)

set_target_properties(FSTools PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/pdk/fs_tools"
)

install(TARGETS FSTools LIBRARY DESTINATION lib/pdk/fs_tools)