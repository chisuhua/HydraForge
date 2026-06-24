# examples/phase1_plugin_demo/CMakeLists.txt
# 功能描述：Phase 1 端到端 demo (Plugin Stub 触发)
#          演示 MockLLMProvider + ModelRouterPolicy 集成
#          Sprint 5 S5.T3: 扩展支持 3 模式 CLI (--mock / --load-plugin / --plugin-path)
#          链接 PluginLoader + cognitive 模块用于真实 .so 加载场景
# 设计依据：phase1-execution.md §Sprint 0 + openspec/changes/2026-07-14-plugin-loader S5.T3
# 作者：AgenticDSL Phase 1 / Sprint 0 + Sprint 5 S5.T3
# 最后修改日期：2026-06-24

add_executable(phase1_plugin_demo main.cpp)

target_link_libraries(phase1_plugin_demo
    agenticdsl_includes
    agenticdsl_common
    agenticdsl_modules_plugin
    agenticdsl_modules_cognitive
)

target_include_directories(phase1_plugin_demo PRIVATE
    ${CMAKE_SOURCE_DIR}/include/agenticdsl/plugin
)
