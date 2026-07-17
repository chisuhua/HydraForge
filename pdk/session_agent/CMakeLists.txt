# pdk/session_agent/CMakeLists.txt
# Session Agent Plugin - 多轮会话状态管理（持久化 + 分支）
# 关联: docs/adr/adr-0033-session-hierarchy.md

cmake_minimum_required(VERSION 3.20)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)


add_library(SessionAgent SHARED
    src/pdk_entry.cpp
    src/session_store.cpp
)

target_include_directories(SessionAgent PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/src

  ${PROJECT_SOURCE_DIR}/external/nlohmann_json/single_include)

target_link_libraries(SessionAgent PRIVATE
    hydraforge_pdk

)

set_target_properties(SessionAgent PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/pdk/session_agent"
)

install(TARGETS SessionAgent LIBRARY DESTINATION lib/pdk/session_agent)