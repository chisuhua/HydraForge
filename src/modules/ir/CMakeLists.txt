# src/modules/ir/CMakeLists.txt
# 功能描述：Trajectory IR 模块构建配置 (T15, ADR-0061-06 v1.1)
#          V1: backends (to_sft_data/to_otel_spans) + ConstantFoldingPass 占位
# 设计依据：openspec/changes/t15-trajectory-ir/tasks.md T2.7
#          (新增本文件注册新源, 不修改既有模块 CMakeLists 规则)
# 作者：HydraForge Sprint 24 T15 ship
# 最后修改日期：2026-08-27

add_library(agenticdsl_modules_ir STATIC
    trajectory_ir_backend.cpp
    trajectory_ir_pass.cpp
)

target_link_libraries(agenticdsl_modules_ir PUBLIC
    agenticdsl_includes
)
