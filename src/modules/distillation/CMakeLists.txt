# src/modules/distillation/CMakeLists.txt
# 功能描述：蒸馏模块构建配置 (Phase 1: FileDistillationWriter V1; Phase 2: TrajectoryIR bridge)
# 设计依据：openspec/changes/capture-mode-and-distillation-writer-v1/spec.md
# 最后修改日期：2026-08-30

add_library(agenticdsl_distillation STATIC
    file_writer.cpp
    trajectory_bridge.cpp   # Phase 2: from_trajectory_ir (复用到 prompt module 的 hash_prompt)
)

target_link_libraries(agenticdsl_distillation PUBLIC
    agenticdsl_includes
    agenticdsl_contract
    agenticdsl_modules_prompt   # T21 hash_prompt() — payload redact 复用（trajectory_bridge.cpp 依赖）
)