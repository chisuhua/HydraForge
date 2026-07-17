# tests/CMakeLists.txt
# pdk_chat_demo tests

# test_chat_session
add_executable(test_chat_session
    test_chat_session.cpp
    ${CATCH_INCLUDE_DIR}/catch_amalgamated.cpp
    ${CATCH_INCLUDE_DIR}/main_test_runner.cpp
)
target_link_libraries(test_chat_session PRIVATE
    pdk_chat_demo_obj
    agenticdsl_includes
    hydraforge_pdk
    Threads::Threads
)
target_include_directories(test_chat_session PRIVATE
    ${CATCH_INCLUDE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/..   # for #include "chat_session.h"
)
target_compile_definitions(test_chat_session PRIVATE
    CATCH_CONFIG_ENABLE_ALL_STRINGMAKERS=1
)
add_test(
    NAME test_chat_session
    COMMAND test_chat_session
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}  # config.json is here (../ relative to tests/)
)

# test_e2e_mock
add_executable(test_e2e_mock
    test_e2e_mock.cpp
    ${CATCH_INCLUDE_DIR}/catch_amalgamated.cpp
    ${CATCH_INCLUDE_DIR}/main_test_runner.cpp
)
target_link_libraries(test_e2e_mock PRIVATE
    pdk_chat_demo_obj
    agenticdsl_includes
    hydraforge_pdk
    Threads::Threads
)
target_include_directories(test_e2e_mock PRIVATE
    ${CATCH_INCLUDE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/..
)
target_compile_definitions(test_e2e_mock PRIVATE
    CATCH_CONFIG_ENABLE_ALL_STRINGMAKERS=1
)
add_test(
    NAME test_e2e_mock
    COMMAND test_e2e_mock
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
)