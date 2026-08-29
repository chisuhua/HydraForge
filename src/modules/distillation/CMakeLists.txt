# src/modules/distillation/CMakeLists.txt
# 功能描述：蒸馏模块构建配置 (Phase 1: FileDistillationWriter V1)
# 设计依据：openspec/changes/capture-mode-and-distillation-writer-v1/spec.md
# 最后修改日期：2026-08-29

add_library(agenticdsl_distillation STATIC
    file_writer.cpp
)

target_link_libraries(agenticdsl_distillation PUBLIC
    agenticdsl_includes
    agenticdsl_contract
)