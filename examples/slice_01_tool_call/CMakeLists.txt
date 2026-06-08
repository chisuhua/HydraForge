# examples/slice_01_tool_call/CMakeLists.txt
# 功能描述：Track 0.2 slice_01 端到端示例（--mock 模式）
# 设计依据：plan §7
# 作者：AgenticDSL Phase 0 / Track B
# 最后修改日期：2026-06-08

add_executable(slice_01_tool_call main.cpp)

target_link_libraries(slice_01_tool_call
    agenticdsl_includes
    agenticdsl_core
    agenticdsl_modules_cognitive
)