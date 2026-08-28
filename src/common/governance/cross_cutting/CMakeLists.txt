# src/common/governance/cross_cutting/CMakeLists.txt
# 文件头注释
# 功能描述：Cross-Cutting Pattern PDK 模块构建配置 (ADR-0085 V1)
#          静态库 agenticdsl_cross_cutting, 含 6 个 .cpp 文件。
# 设计依据：openspec/changes/pdk-cross-cutting-patterns (ADR-0085)
# 作者：AgenticDSL Phase 3
# 最后修改日期：2026-08-28

add_library(agenticdsl_cross_cutting STATIC
  cross_cutting_orchestrator.cpp
  decorator_pattern.cpp
  hook_pattern.cpp
  composition_pattern.cpp
  bus_pattern.cpp
  cross_cutting_config.cpp
)

target_link_libraries(agenticdsl_cross_cutting PUBLIC
  agenticdsl_core
  agenticdsl_contract
  agenticdsl_policy
  agenticdsl_llm
)

target_include_directories(agenticdsl_cross_cutting PUBLIC
  ${PROJECT_SOURCE_DIR}/include
)
