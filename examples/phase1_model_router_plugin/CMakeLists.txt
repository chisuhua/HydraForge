# examples/phase1_model_router_plugin/CMakeLists.txt
# 功能描述：ModelRouter Plugin 加载演示 (C7 Phase 2 升级)
#          链接 3 个策略 .so + ModelRegistry .so + PluginLoader, 设 BUILD_RPATH
# 设计依据：openspec/changes/2026-06-26-adr-0034-model-router-plugin/
#          specs/model-router-plugin/spec.md — model-router-plugin-entry requirement
# 作者：C7 Phase 2

add_executable(phase1_model_router_plugin main.cpp)
target_compile_definitions(phase1_model_router_plugin PRIVATE
    AGENTICDSL_EXAMPLE_BINARY_DIR="${CMAKE_BINARY_DIR}"
)

target_link_libraries(phase1_model_router_plugin PRIVATE
    agenticdsl_includes
    agenticdsl_common
    agenticdsl_modules_plugin
    hydraforge_model_router_cost
    hydraforge_model_router_quality
    hydraforge_model_router_latency
    hydraforge_model_registry
)

# BUILD_RPATH 指向 build/debug/pdk/model_router/*/ 目录, 确保运行时能找到 .so
set_target_properties(phase1_model_router_plugin PROPERTIES
    BUILD_RPATH "$ORIGIN/../../pdk/model_router/cost_strategy;$ORIGIN/../../pdk/model_router/quality_strategy;$ORIGIN/../../pdk/model_router/latency_strategy;$ORIGIN/../../pdk/model_router"
)
