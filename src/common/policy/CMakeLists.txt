# src/common/policy/CMakeLists.txt
# 功能描述：执行策略模块构建配置。静态库 agenticdsl_policy包含
# ADR-0031 中定义的三种执行模式（Plan / Agent / YOLO）。
# 设计依据：ADR-0031 (IExecutionPolicy) Phase1实施
# 作者：AgenticDSL Phase3 / Track A
# 最后修改日期：2026-06-10

add_library(agenticdsl_policy STATIC
 plan_mode_policy.cpp
 agent_mode_policy.cpp
 yolo_mode_policy.cpp
)

target_include_directories(agenticdsl_policy PUBLIC
 ${PROJECT_SOURCE_DIR}/include
 ${PROJECT_SOURCE_DIR}/src
)

target_link_libraries(agenticdsl_policy PUBLIC
 agenticdsl_includes
)
