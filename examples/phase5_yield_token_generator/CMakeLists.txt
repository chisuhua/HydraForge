# examples/phase5_yield_token_generator/CMakeLists.txt

add_executable(phase5_yield_token_generator main.cpp)

target_link_libraries(phase5_yield_token_generator
    agenticdsl_includes
    agenticdsl_core
)
