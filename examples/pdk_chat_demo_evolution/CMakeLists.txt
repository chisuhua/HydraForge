# examples/pdk_chat_demo_evolution/CMakeLists.txt
# L2 reference example demo (per `openspec/changes/pdk-chat-demo-evolution-reference-example/`)
# Standalone sub-project: consumes existing public APIs only, no include/ diff
# Test binaries labeled "l2-evolution" (per P0-7 fix): `ctest -LE l2-evolution` to exclude

# --- Main executable (skeleton; sources added per task) ---
add_executable(pdk_chat_demo_evolution
    main.cpp
    # Task 2: hermetic_home.cpp
    hermetic_home.cpp
    # Task 3: context_request.cpp
    context_request.cpp
    # Task 4: evolution_tracer.cpp
    evolution_tracer.cpp
    # Task 5: evolution_session.cpp + to_agent_config.cpp (L2 internal helper)
    evolution_session.cpp
)

target_include_directories(pdk_chat_demo_evolution PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(pdk_chat_demo_evolution PRIVATE
    agenticdsl_includes
    agenticdsl_core
    agenticdsl_common
    hydraforge_pdk
    pdk_chat_session
    Threads::Threads
)

# Make dlopen'd .so plugins resolve host binary symbols
target_link_options(pdk_chat_demo_evolution PRIVATE -Wl,--export-dynamic)

# --- Tests (10 binaries per P0-7 fix) ---
option(PDK_EVOLUTION_BUILD_TESTS "Build pdk_chat_demo_evolution tests" ON)
if(PDK_EVOLUTION_BUILD_TESTS AND AGENTICDSL_BUILD_TESTS)
    set(CATCH_INCLUDE_DIR "${PROJECT_SOURCE_DIR}/tests")
    enable_testing()
    add_subdirectory(tests)
endif()

# --- Install ---
install(TARGETS pdk_chat_demo_evolution RUNTIME DESTINATION bin)