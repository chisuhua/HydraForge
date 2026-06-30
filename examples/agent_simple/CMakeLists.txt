# examples/agent_simple/CMakeLists.txt
# 功能描述：agent_simple 示例 — 单轮 ReAct 演示 (MockLLMProvider 模式)
# 设计依据：openspec/changes/examples-mockllm-migration/proposal.md §1
# 作者：AgenticDSL Sprint 19
# 最后修改日期：2026-06-30

add_executable(agent_simple simple.cpp)

target_link_libraries(agent_simple
    agenticdsl_includes
    agenticdsl_core
    agenticdsl_modules_cognitive
)