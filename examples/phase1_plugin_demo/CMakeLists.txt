# examples/phase1_plugin_demo/CMakeLists.txt
# 功能描述：Phase 1 Sprint 0 端到端 demo (Plugin Stub 触发)
#          演示 MockLLMProvider + ModelRouterPolicy 集成
# 设计依据：phase1-execution.md §Sprint 0
# 作者：AgenticDSL Phase 1 / Sprint 0
# 最后修改日期：2026-06-16

add_executable(phase1_plugin_demo main.cpp)

target_link_libraries(phase1_plugin_demo
    agenticdsl_includes
    agenticdsl_common
)
