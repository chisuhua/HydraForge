# examples/pdk_chat_demo/CMakeLists.txt
# PDK Chat Demo - Agent-as-Plugin 架构端到端验证
# 关联: docs/examples/pdk_chat_demo/DESIGN.md
#      docs/adr/adr-0052 ~ adr-0065

# --- Source files (shared between demo and tests) ---
set(SOURCES
    chat_session.cpp
    event_handler.cpp
)

set(HEADERS
    chat_session.h
    event_handler.h
)

# --- OBJECT library (shared between demo and tests) ---
add_library(pdk_chat_demo_obj OBJECT ${SOURCES} ${HEADERS})

target_include_directories(pdk_chat_demo_obj PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
)
target_link_libraries(pdk_chat_demo_obj PUBLIC
    agenticdsl_includes
    hydraforge_pdk
    Threads::Threads
)

# --- Main executable ---
add_executable(pdk_chat_demo main.cpp)

target_include_directories(pdk_chat_demo PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
)
target_link_libraries(pdk_chat_demo PRIVATE
    pdk_chat_demo_obj
    agenticdsl_includes
    agenticdsl_core
    hydraforge_pdk
    Threads::Threads
)

# --- Configure config.json with build directory paths ---
set(SKILL_CODE_REVIEW_PATH "${PROJECT_SOURCE_DIR}/skills/code-review")

configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/config.json
    ${CMAKE_CURRENT_BINARY_DIR}/config.json
    @ONLY
)

# --- Tests ---
option(PDK_CHAT_BUILD_TESTS "Build pdk_chat_demo tests" ON)
if(PDK_CHAT_BUILD_TESTS)
    # Catch2 amalgamated (same as root tests)
    set(CATCH_INCLUDE_DIR "${PROJECT_SOURCE_DIR}/tests")
    enable_testing()
    add_subdirectory(tests)
endif()

# --- Install ---
install(TARGETS pdk_chat_demo RUNTIME DESTINATION bin)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/config.json DESTINATION share/pdk_chat_demo)