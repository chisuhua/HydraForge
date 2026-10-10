# real-llm-loop-level-e2e-followup (CLOSED 2026-10-09 by consolidate-loop-phases-to-shared-helpers)

**优先级**: ✅ CLOSED | **阶段**: ✅ closed | **分类**: 测试覆盖追踪
**类型**: debt-tracking | **主题**: Real LLM loop-level E2E coverage (react steps / PlanExecute 3-phase / ForkJoin synthesize)
**状态**: ✅ **closed** (2026-10-09, by consolidate-loop-phases-to-shared-helpers ship)
**关闭原因**: consolidate-loop-phases 5 atomic commits (1cfe82a/04273ad/bed7995/b06cf60/fbda908) + HARD pause override (per builder-handoff-v1.5.json) ship 后, 主会话 critical self-examination 验证 **10/10 must_realllm tests PASS** with DEEPSEEK_API_KEY:
  - test_e2e_real_llm (含 ChatSession case, 1.60s) ✅
  - test_e2e_real_llm_generate_subgraph (9.03s) ✅
  - test_e2e_real_llm_multi_turn (2.10s, 跨 turn context preservation) ✅
  - test_e2e_real_llm_errors (0.32s) ✅
  - test_distillation_capture_training_real_llm (4.00s) ✅
  - test_gepa_loop_real_llm (29.61s) ✅
  - test_harness_mutation_proposal_real_llm (0.84s) ✅
  - test_plan_execute_realllm (14.50s, **关键: 验证 PlanExecute 3-phase 真 LLM 端到端**)
  - test_react_loop_real_llm (71.92s, **关键: 验证 react think→decide→end steps 真 LLM**)
  - test_skill_compiler_real_llm (2.38s) ✅
**生成时间**: 2026-09-25
**关闭时间**: 2026-10-09
**关联**: `openspec/changes/2026-09-18-chat-real-llm-coverage-phase-h/` (Phase H shipped) + `openspec/changes/archive/consolidate-loop-phases-to-shared-helpers/` (consolidation closed gap)
**Oracle reference**: ses_f2b41e215 (Stage 2 SHIP-with-fixes verdict Major 1)
**AGENTS.md Pattern**: #10 hygiene (systematic recording of deferred work) + #12 (Loop phase consolidation via shared helpers + thin shells, consolidation 触发 re-validation 真实路径)
**METHODOLOGY.md**: §9.2 (worker fabrication defense, 主会话 critical self-examination 触发 10/10 PASS 验证)

## 背景

Phase H (commit `786d8cc`) ship 后 Oracle Stage 2 verdict (`ses_f2b41e215`) 揭示：

**当前 Phase H 实现覆盖 provider-level dispatch smoke**（7 cases）：
- R1: `provider->generate("Reply with one word: OK")` → text 非空
- R2-jthread: `provider->generate("用一句话解释 std::jthread...")` → 含 "jthread" 关键词
- R3: 多轮 `provider->generate()` 顺序 dispatch（relaxed — stateless 不共享 context）
- R4: `provider->generate("Plan a 3-step...")` → 含 "step" 关键词
- R5: 3 次 sequential `provider->generate()` → ≥1/3 成功
- R6: 100 次 sequential `provider->generate()` → ≥95% 成功率 + median ≤5s

**未覆盖**（Oracle 标记的真正 gap）：
- **React loop think→decide→end steps 验证**（R1 实际仅 provider-level）
- **PlanExecute 3-phase (plan → execute → verify) 真实端到端**（R4 实际仅 capability assertion）
- **ForkJoin parallel + synthesize node 聚合**（R5 实际仅 sequential dispatch）

这些**真正的 loop-level E2E** 需要：
- DSLEngine 实例化（`lib/loop/react.agent.md` / `lib/loop/plan_execute.agent.md` / `lib/loop/fork_join.agent.md`）
- ChatSession 11-param ctor (per L2 spec)
- LoopAgent infrastructure (`pdk/loop_agent/` 已 ship)
- Context 跨 turn 传递（仅 ChatSession 支持，stateless generate 不行）

## Why (为什么需要跟踪)

1. **Truthfulness gap**: 当前 Phase H name claims "react loop / plan_execute / fork_join" but implementation 仅 provider-level。Oracle Major 1 fix 改正了 naming + scope clarification，但 **真正 loop-level E2E 仍是 unverified gap**。
2. **AGENTS.md Reverse Indicator Rule**: 只报涨不报掉属选择性披露，不予通过。Phase H ship 报告"real LLM E2E coverage"是涨，但 loop-level gap 是未报的掉。
3. **AGENTS.md Pattern #10 hygiene**: 此类 deferral 必须有可执行追踪机制，避免未来 reader 误以为 Phase H 已覆盖 loop-level。
4. **Phase H+ 立项载体**: Wave 4 / Phase 2 D4+ / 未来 Loop-RSI 实现时需此 coverage。

## Acceptance (重新启动条件)

- [ ] **Loop-level react E2E**: `lib/loop/react.agent.md` 通过 DSLEngine + ChatSession 真实 LLM 端到端，react steps ≥ 3 (think → decide → act → observe → end) 触发验证
- [ ] **Loop-level plan_execute E2E**: `lib/loop/plan_execute.agent.md` 三阶段 (plan → execute → verify) 全部触发，LoopResult.success == true
- [ ] **Loop-level fork_join E2E**: `lib/loop/fork_join.agent.md` 3 分支 parallel + synthesize node 聚合验证
- [ ] **Cross-turn context awareness**: ChatSession 多轮 "My name is Alice" → "What is my name?" → Assistant 含 "Alice"
- [ ] **F1 fail-fast integration**: 真实 LLM 在 ChatSession loop 内空响应时，node_executor fail-fast 触发验证

## Reverse Indicator (R8 per AGENTS.md)

```
[Reverse Indicator]
+ new_up: real-LLM loop-level E2E coverage 缺口记录 (per Pattern #10 hygiene)
- old_down: drop_ratio=0% (pure debt-tracking, no behavior change)
- failure_traces: N/A (planning/tracking entry)
- ablation: N/A
- context_ids: N/A
```

## 关联文档

- **Phase H (shipped)**: `openspec/changes/2026-09-18-chat-real-llm-coverage-phase-h/`
- **Oracle verdict**: `ses_f2b41e215` (Stage 2 SHIP-with-fixes verdict, 2026-09-25)
- **Loop infrastructure**: `pdk/loop_agent/` (已 ship, 3 loop types)
- **ChatSession**: `include/agenticdsl/pdk/chat_session.h` (11-param ctor, per L2 spec)
- **AGENTS.md Pattern #10**: post-acceptance hygiene — systematic recording of deferred work
- **AGENTS.md Reverse Indicator Rule**: 不能选择性披露，只报涨不报掉