# pdk/budget_agent/CMakeLists.txt
# Budget Agent Plugin - 预算查询、限额设置、告警
# 关联: docs/adr/adr-0033-session-hierarchy.md

cmake_minimum_required(VERSION 3.20)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)


add_library(BudgetAgent SHARED
    src/pdk_entry.cpp
    src/budget_store.cpp
    src/hooks.cpp
)

target_include_directories(BudgetAgent PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/src

  ${PROJECT_SOURCE_DIR}/external/nlohmann_json/single_include)

target_link_libraries(BudgetAgent PRIVATE
    hydraforge_pdk

)

set_target_properties(BudgetAgent PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/pdk/budget_agent"
)

install(TARGETS BudgetAgent LIBRARY DESTINATION lib/pdk/budget_agent)