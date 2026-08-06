# pdk/provider_agent/CMakeLists.txt
# Provider Agent Plugin - 扩展 model_router，注册/解析 LLM provider configs
# 关联: docs/adr/adr-0021-pdk-design.md, docs/examples/pdk_chat_demo/DESIGN.md

cmake_minimum_required(VERSION 3.20)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)


add_library(ProviderAgent SHARED
    src/pdk_entry.cpp
    src/credential_store.cpp
    src/provider_resolve.cpp
    src/provider_refresh.cpp
    src/provider_register_dynamic.cpp
    src/provider_switch.cpp
)

target_include_directories(ProviderAgent PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/src

  ${PROJECT_SOURCE_DIR}/external/nlohmann_json/single_include)

target_link_libraries(ProviderAgent PRIVATE
    hydraforge_pdk

)

set_target_properties(ProviderAgent PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/pdk/provider_agent"
)

install(TARGETS ProviderAgent LIBRARY DESTINATION lib/pdk/provider_agent)