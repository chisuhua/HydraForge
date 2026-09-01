# src/modules/cognitive/CMakeLists.txt
# 功能描述：B 轨道（Track 0.2）cognitive 模块构建配置
#          Phase 1 预留 SimpleCognitiveOrchestrator
#          Phase 1 Sprint 2 (2026-06-18): 加入 CognitiveWorker
#            (ADR-0020 §3.1 + openspec/changes/2026-06-23-cognitive-worker)
#          Phase 1 Sprint 3 (2026-06-19): 加入 DomainWorkerPool
#            (ADR-0020 §3.2 + openspec/changes/2026-06-30-domain-worker-pool)
# 设计依据：plan §5
# 作者：AgenticDSL Phase 0 / Track B + Phase 1 Sprint 2/3
# 最后修改日期：2026-06-19 [Sprint 3: DomainWorkerPool 实施]

add_library(agenticdsl_modules_cognitive STATIC
    simple_orchestrator.cpp
    cognitive_worker.cpp    # Sprint 2: 认知智能体工作线程
    domain_worker_pool.cpp  # Sprint 3: 领域智能体工作线程池
    agent_composition.cpp   # Sprint 22 P8: Agent 编排模式实现
    skill_compiler.cpp      # Sprint 24 T17: SkillCompiler V1 (ADR-0061-03)
    behavioral_equivalence_evaluator.cpp # evaluator-v2-composite T0: T14 行为等价
    composite_evaluator.cpp      # evaluator-v2-composite T1: 多评估器聚合
    gepa_loop.cpp                # Sprint 24 T19: GEPA 反思循环编排器 (ADR-0071)
    mcts_workflow_search.cpp     # Sprint 24 T20: AFlow MCTS 工作流搜索 (ADR-0061-08)
    cognitive_tools.cpp          # T5 cognitive-specialists-as-tools
)

target_link_libraries(agenticdsl_modules_cognitive PUBLIC
    agenticdsl_includes
    agenticdsl_common
    agenticdsl_core
    agenticdsl_modules_testing  # Sprint 24 T17: T14 行为回归自检 (hotelling_t2_test)
)