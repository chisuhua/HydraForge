add_library(agenticdsl_modules_testing STATIC
    behavioral_regression.cpp
)
target_include_directories(agenticdsl_modules_testing PUBLIC
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src/core
)
target_link_libraries(agenticdsl_modules_testing PUBLIC agenticdsl agenticdsl_core)