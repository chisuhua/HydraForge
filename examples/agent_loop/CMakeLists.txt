# examples/agent_loop/CMakeLists.txt
# 功能描述：agent_loop 示例 — 多轮 LLM 调用循环演示 (MockLLMProvider 模式)
# 设计依据：openspec/changes/examples-mockllm-migration/proposal.md §2
# 作者：AgenticDSL Sprint 19
# 最后修改日期：2026-06-30

add_executable(agent_loop agent_loop.cpp)

target_link_libraries(agent_loop
    agenticdsl_includes
    agenticdsl_core
    agenticdsl_modules_cognitive
)