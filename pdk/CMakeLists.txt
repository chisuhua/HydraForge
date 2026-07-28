# pdk/CMakeLists.txt
# 功能描述：PDK monorepo 子目录构建配置 (Sprint 4 实施, ADR-0021 K3 决策)
#          hydraforge-pdk PDK 头文件库 — INTERFACE 库, 静态链接到插件 (P3)
#          Phase 2 拆分至独立 hydraforge-pdk 仓库时, 此 CMakeLists.txt 移至独立仓库,
#          根 CMakeLists.txt 改用 find_package(hydraforge_pdk REQUIRED)
# 设计依据：ADR-0021 §2.2 + §4 + openspec/changes/2026-07-07-pdk-skeleton
# 作者：AgenticDSL Phase 1 Sprint 4
# 最后修改日期：2026-06-19

# INTERFACE 库 (header-only), 命名: hydraforge_pdk
add_library(hydraforge_pdk INTERFACE)

# 头文件路径: pdk/include/ (含 hydraforge/pdk/... 路径别名 + agenticdsl/pdk/ 实际头)
# 此外需要包括 src/ 以便可以找到 common/policy/execution_policy.h 等内部头
target_include_directories(hydraforge_pdk INTERFACE
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
  $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/src>
  $<INSTALL_INTERFACE:include>
)

# C++20 标准 (PDK 使用 std::invoke_result_t + 后续 Phase 2/3 扩展)
target_compile_features(hydraforge_pdk INTERFACE cxx_std_20)

# 链接依赖: PDK 仅依赖 Runtime 契约接口 (P3 静态链接)
# MVP 不强依赖 agenticdsl_contract (PDK 头文件通过 #include 访问)
# Phase 2 集成 IToolRegistry/IInteractionBus 时启用:
# target_link_libraries(hydraforge_pdk INTERFACE agenticdsl_contract)

# C7 Phase 1-2 MVP: 模型路由插件 (cost + quality)
add_subdirectory(model_router)

# C14 Phase 5 B2.1: Llama Engine Plugin (12 推理工具)
add_subdirectory(llama_engine)

# Phase 6 W1: G3 Knowledge Base Plugin (1 retrieval + LLM Q&A 工具)
add_subdirectory(g3_knowledge_base)

# Phase 6 W1: G1 Coding Assistant Plugin (2-step ReAct loop + G3 orchestration)
add_subdirectory(g1_coding_assistant)

# pdk_chat_demo v1 - Phase A 端到端 demo 的 6 个 Agent Plugin
# 关联: examples/pdk_chat_demo/DESIGN.md
add_subdirectory(loop_agent)        # Chat Loop Agent (DSL loader)
add_subdirectory(provider_agent)    # LLM Provider + 凭据管理
add_subdirectory(session_agent)     # 多轮会话 + JSONL 持久化
add_subdirectory(budget_agent)      # 预算查询 + 告警
add_subdirectory(fs_tools)          # 文件系统 read/write/list/exists
add_subdirectory(shell_tools)       # shell exec + 危险命令黑名单

# pkm-temporal-demo-scaffold: Temporal Agent PDK Plugin (ITemporalClient + MockTemporalClient + 5 工具)
# 关联: .rddf/plans/pkm-temporal-demo-scaffold.md
add_subdirectory(temporal_agent)     # Task 1: ITemporalClient 接口 + 测试