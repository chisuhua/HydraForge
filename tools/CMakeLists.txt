# ADR-0074 C3 — C++ 工具可执行目标 (python 脚本无需注册)
add_executable(measure_prompt_baseline measure_prompt_baseline.cpp)
target_link_libraries(measure_prompt_baseline PRIVATE
    agenticdsl_common
    yaml-cpp::yaml-cpp
)
