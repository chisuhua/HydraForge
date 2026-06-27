add_library(agenticdsl_modules_scheduler STATIC
    topo_scheduler.cpp
    execution_session.cpp # v3.1
    resource_manager.cpp # v3.1
    factory.cpp # C1 Day 8-9
)
target_include_directories(agenticdsl_modules_scheduler PUBLIC
    include
    ${PROJECT_SOURCE_DIR}/external/taskflow
)
target_link_libraries(agenticdsl_modules_scheduler PUBLIC
    agenticdsl_common
    agenticdsl_modules_context
    agenticdsl_modules_budget
    agenticdsl_modules_executor
    agenticdsl_modules_trace
    Taskflow  # C2 Day 1-2: 并行 DAG executor (ADR-0030 V2)
)
