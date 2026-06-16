# examples/phase1_model_router_plugin/CMakeLists.txt
# 功能描述：Phase 1 Sprint 0 ModelRouter Plugin Stub 二进制 (K1 决策)
#          Plugin 层独立可执行文件, 非 .so 加载
#          展示 ModelRouterPolicy 决策逻辑 (基于 available_models() + cost 路由)
# 设计依据：phase1-execution.md §Sprint 0 (K1)
# 作者：AgenticDSL Phase 1 / Sprint 0
# 最后修改日期：2026-06-16

add_executable(phase1_model_router_plugin main.cpp)

target_link_libraries(phase1_model_router_plugin
    agenticdsl_includes
    agenticdsl_common
)
