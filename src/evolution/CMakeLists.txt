# src/evolution/CMakeLists.txt
# ADR-0086 v1.0+v1.1 attribution layer
# 静态库: agenticdsl_evolution
# 路径: src/evolution/ (新增模块目录, 与 src/modules/ src/common/ 平级)

add_library(agenticdsl_evolution STATIC
    attribution_record.cpp
    version_pair_diff.cpp
    transition_guard.cpp  # ADR-0088 H→D→M Transition Guard (Phase 6c MetaRSI-v1)
)

target_include_directories(agenticdsl_evolution
    PUBLIC
        ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(agenticdsl_evolution
    PUBLIC agenticdsl_core
)

# 内部依赖: 当前 implementation 不依赖其他模块, 但暴露公共 API 给 future consumers
target_compile_features(agenticdsl_evolution PUBLIC cxx_std_20)