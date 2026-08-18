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
 path_policy.cpp
 policy_factory.cpp
 approval_callbacks.cpp
approval_handler.cpp
  mode_switch_dialog.cpp
  layer_profile.cpp
  dangerous_patterns.cpp   # ADR-0073 D3: OWASP 命令注入黑名单
  backend_policy.cpp       # ADR-0075 D5: BackendPolicy 默认策略表 + override
)

target_include_directories(agenticdsl_policy PUBLIC
 ${PROJECT_SOURCE_DIR}/include
 ${PROJECT_SOURCE_DIR}/src
)

target_link_libraries(agenticdsl_policy PUBLIC
 agenticdsl_includes
)
