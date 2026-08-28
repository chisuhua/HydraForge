# src/modules/prompt/CMakeLists.txt
# 功能描述：T21 Prompt Evidence Gate 模块构建配置 (ADR-0074)
#          质量门控层：evidence_gate (Go/No-Go 阈值) + prompt_assembler (两阶段注入)
# 设计依据：openspec/changes/t21-prompt-evidence-gate/
# 作者：HydraForge Sprint 25 T21 ship
# 最后修改日期：2026-08-28

add_library(agenticdsl_modules_prompt STATIC
    evidence_gate.cpp
)

target_link_libraries(agenticdsl_modules_prompt PUBLIC
    agenticdsl_includes
    agenticdsl_common
)