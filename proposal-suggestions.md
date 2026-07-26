[
  {
    "name": "pdk-chat-demo-v1-recap",
    "priority": "P0",
    "source": "roadmap.md Phase 6a §任务 T1+T2",
    "status": "created",
    "phase": "phase-6a",
    "category": "demo-chat-v1",
    "effort": "8h (4h+4h)",
    "description": "## 架构依据\n- roadmap.md Phase 6a: pdk_chat_demo v1 收尾 (Session 持久化 + Budget 告警)\n- roadmap.md Phase 6a: Schema 校验基础版 (拒绝错误格式)\n\n## 范围\n- Session 持久化: save_to_disk / load_from_disk / 过期清理\n- Budget 告警: IInteractionBus event → TUI 显示\n- Schema 校验: SchemaValidator → 拒绝非法 DSL\n\n## 关键场景\n- GIVEN mock mode, WHEN chat session starts, THEN session persisted to disk\n- GIVEN budget exceeded, WHEN LLM call attempted, THEN alert shown in TUI\n- GIVEN invalid .agent.md, WHEN parsed, THEN rejected with line-level error\n\n## 验收标准\n- ctest -R pdk_chat 全绿\n- ./pdk_chat_demo --mock Session 持久化 + Budget 告警正常工作"
  },
  {
    "name": "pkm-temporal-demo-scaffold",
    "priority": "P0",
    "source": "roadmap.md Phase 6a 任务 T3+T4+T5",
    "status": "created",
    "phase": "phase-6a",
    "category": "demo-temporal-1a",
    "effort": "24h (10h+8h+6h)",
    "description": "## 架构依据\n- roadmap.md Phase 6a: pkm_temporal_demo PDK 骨架 (ITemporalClient + Mock + CLI + pdk_entry)\n- roadmap.md Phase 6a: Demo 项目 (main.cpp + 4 场景 + config.json)\n- roadmap.md Phase 6a: 测试 (unit + e2e mock, ≥8 test cases)\n\n## 范围\n- ITemporalClient 抽象接口 + MockTemporalClient 实现\n- 4 个演示场景 DSL (blocking/async-poll/signal/idempotent)\n- ctest -R temporal ≥8 cases\n\n## 验收标准\n- ctest -R temporal 全绿\n- ./pkm_temporal_demo --mock 4 场景全部 PASS"
  },
  {
    "name": "adr-0002-eventbus-queue",
    "priority": "P1",
    "source": "ADR-0002 EventBus 有界队列架构 (❌ Not Implemented)",
    "status": "created",
    "phase": "phase-6a",
    "category": "架构对齐",
    "effort": "5h",
    "description": "## 架构依据\n- ADR-0002 §决策 2: 事件优先级与背压策略\n- 当前 InMemoryBus 为简化实现 (mutex + queue)\n- 提取 EventBus Core 有界队列组件供 InMemoryBus 内部使用\n\n## 范围\n- EventBusQueue: 有界队列 + 优先级 + 背压\n- InMemoryBus 内部 queue 替换\n- 不修改 IInteractionBus 公开接口\n\n## 验收标准\n- ctest -R test_event_bus_core 全绿\n- ctest -R test_interaction_bus 全绿 (零回归)"
  },
  {
    "name": "adr-0019-iinteractionbus",
    "priority": "P1",
    "source": "ADR-0019 IInteractionBus (🟡 Partial, P0 review 触发)",
    "status": "created",
    "phase": "phase-6a",
    "category": "架构对齐",
    "effort": "3h",
    "description": "## 架构依据\n- ADR-0019 §状态变更日志 (2026-07-06): ADR-0046 要求 topic-based subscribe\n- 当前接口仅 session-based, 缺 subscribe_topic / unsubscribe\n\n## 范围\n- IInteractionBus 新增 subscribe_topic(topic_pattern, callback)\n- InMemoryBus 实现 topic-based dispatch (glob 匹配)\n\n## 验收标准\n- ctest -R test_interaction_bus_topic 全绿\n- ctest -R test_interaction_bus 全绿 (零回归)"
  },
  {
    "name": "adr-0037-causal-ordering",
    "priority": "P2",
    "source": "ADR-0037 因果排序 (🔍 Proposed, 2026-06-26)",
    "status": "created",
    "phase": "phase-6a",
    "category": "架构对齐",
    "effort": "3h",
    "description": "## 架构依据\n- ADR-0037 §决策 1: 单进程逻辑时钟 + 因果向量 (非 Lamport)\n- 解决跨 Worker 事件 happens-before 判定\n\n## 范围\n- CausalClock: std::atomic<uint64_t> 单增\n- BusEvent 新增 causal_time 字段\n- InMemoryBus emit 时自动 tick + attach\n\n## 验收标准\n- ctest -R test_causal_clock 全绿\n- ctest -R test_interaction_bus 全绿 (零回归)"
  }
]