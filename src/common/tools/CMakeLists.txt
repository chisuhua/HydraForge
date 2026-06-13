# src/common/tools/CMakeLists.txt
# 功能描述：工具安全装饰器构建配置。SecureToolRegistry 在
#          ToolRegistry 之外以装饰器方式提供安全层，本库独立编译。
# 设计依据：ADR-0004 (ToolRegistry 安全模型) §5
# 作者：docs-code-drift-audit-2026-06 change
# 最后修改日期：2026-06-13

add_library(agenticdsl_secure_tools STATIC
 secure_tool_registry.cpp
)

target_include_directories(agenticdsl_secure_tools PUBLIC
 ${PROJECT_SOURCE_DIR}/include
 ${PROJECT_SOURCE_DIR}/src
)

target_link_libraries(agenticdsl_secure_tools PUBLIC
 agenticdsl_includes
 agenticdsl_policy
 agenticdsl_common
)
