# examples/pdk_chat_demo_evolution/tests/CMakeLists.txt
# L2 reference example test binaries (10 binaries per P0-7 fix)
# All tests labeled "l2-evolution" (ctest -LE l2-evolution to exclude)

# --- Catch2 amalgamated (shared with root tests) ---
set(CATCH_INCLUDE_DIR "${PROJECT_SOURCE_DIR}/tests")

# Helper: register one L2 test binary
function(add_l2_test TEST_NAME)
    add_executable(${TEST_NAME}
        ${ARGN}
        ${CATCH_INCLUDE_DIR}/catch_amalgamated.cpp
        ${CATCH_INCLUDE_DIR}/main_test_runner.cpp
    )
    target_include_directories(${TEST_NAME} PRIVATE
        ${CATCH_INCLUDE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/..   # for #include "hermetic_home.h" / "context_request.h"
    )
    target_link_libraries(${TEST_NAME} PRIVATE
        agenticdsl_includes
        agenticdsl_core
    )
    add_test(NAME ${TEST_NAME} COMMAND ${TEST_NAME})
    set_tests_properties(${TEST_NAME} PROPERTIES
        LABELS "l2-evolution"
        TIMEOUT 60
    )
endfunction()

# --- Test 1: hermetic_home (Task 2) ---
add_l2_test(test_hermetic_home test_hermetic_home.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../hermetic_home.cpp)

# --- Test 2: context_request_validation (Task 3) ---
add_l2_test(test_context_request_validation test_context_request_validation.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../context_request.cpp)

# Tests 3-10 will be added in Tasks 6-8