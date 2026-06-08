# src/modules/cognitive/CMakeLists.txt
# 功能描述：B 轨道（Track 0.2）cognitive 模块构建配置
#          Phase 1 预留 SimpleCognitiveOrchestrator
# 设计依据：plan §5
# 作者：AgenticDSL Phase 0 / Track B
# 最后修改日期：2026-06-08

add_library(agenticdsl_modules_cognitive STATIC
    simple_orchestrator.cpp
)

target_link_libraries(agenticdsl_modules_cognitive PUBLIC
    agenticdsl_includes
    agenticdsl_common
    agenticdsl_core
)