# src/common/governance/CMakeLists.txt
# 功能描述：Mutation Governance 模块构建配置 (ADR-0084 V1)
#          静态库 agenticdsl_governance, 含 mutation_governor.cpp。
#          注意: 无 mutation_topics.cpp — EventBuilder 无主题注册 API,
#          主题为 ADR-0068 附录 A 文档注册 (design D2)。
# 设计依据：openspec/changes/2026-08-26-adr-0084-mutation-governance-contract/design.md D2
# 作者：HydraForge Phase 6 / ADR-0084 V1 ship
# 最后修改日期：2026-08-26

add_library(agenticdsl_governance STATIC
  mutation_governor.cpp
  cross_cutting/decorator_pattern.cpp
  cross_cutting/hook_pattern.cpp
  cross_cutting/composition_pattern.cpp
  cross_cutting/bus_pattern.cpp
  cross_cutting/cross_cutting_orchestrator.cpp
)

target_include_directories(agenticdsl_governance PUBLIC
  ${PROJECT_SOURCE_DIR}/include
  ${PROJECT_SOURCE_DIR}/src
)

target_link_libraries(agenticdsl_governance PUBLIC
  agenticdsl_includes
  agenticdsl_common   # ToolResult (core/types/tool_result.cpp)
  agenticdsl_policy   # IApprovalHandler 配套值类型 (execution_policy.h)
)
