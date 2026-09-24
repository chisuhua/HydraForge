# intent-classification-router (PLACEHOLDER archived)

**优先级**: P3 (deferred) | **阶段**: Wave 2 P1 (stale roadmap) | **分类**: 技术债追踪
**类型**: debt-tracking | **主题**: ChatSession intent classification + loop type routing
**状态**: placeholder-archived (2026-09-25)
**生成时间**: 2026-09-25
**关联 ADR**: ADR-0071 §D5 (LLM-native DSL architecture), ADR-0074 (Prompt + Evidence Gate)
**原 OpenSpec change**: `openspec/changes/2026-09-17-intent-classification-router/` (PLACEHOLDER 状态，2026-09-17 创建，2026-09-25 归档)

## 背景

原 OpenSpec change 是 PLACEHOLDER 状态（仅 proposal.md 6 行 + tasks.md 25 行），创建于 2026-09-17 至 2026-09-25 期间未进展。

**触发根源**: 关联 master plan `docs/roadmap/2026-09-16-pdk-chat-demo-evolution-roadmap.md` §三 P1 row
**实施方案**: A'' (Oracle `ses_f4fd88215ffeUWSe2StAWtFPSQ` 推荐) — DSL 分类 + ChatSession dispatch
**估时**: 0.5-1 sprint (2-3d impl + 3-5d real LLM test)

## In Scope

- Phase A: DSL 子图 (~30 行 YAML)
  - 创建 `lib/loop/intent_classify.agent.md` (subgraph, ~30 lines)
    - start → switch (rule-based 入口分流: trivial/math/coding/research)
    - think (llm_call 调 classify_intent tool)
    - dispatch (tool_call: loop/run_subgraph 桥接)
  - 4 intent types + complexity 字段 (4-level: trivial/simple/complex/multi-step)
- Phase B: C++ 工具 + ChatSession wiring (~50 行 C++)
  - `pdk/loop_agent` 注册 `loop/classify_intent` (~30 行)
  - ChatSession "auto" routing (~20 行, 两次平级 loop/run 调用)
  - 测试覆盖: 6 cases (3 happy + 3 error)
- Phase C: 真实 LLM E2E (chat-real-llm-coverage 模式)
  - `[realllm]` tag cases: 3 happy paths (trivial → simple loop, complex → react, multi-step → plan_execute)
  - `[realllm]` tag cases: 3 error paths (parse failure / unknown intent / timeout)
  - CI 默认 skip, manual `HYDRAFORGE_SKIP_REAL_LLM=0` 启用
- Phase D: ship-with-fixes
  - Metis dual-agent review (mandatory per master plan §九)
  - Oracle review (mandatory for cross-file)
  - ctest 245/245 零回归
  - openspec archive

## Why (为什么现在归档)

1. **信息保留**: 8 天 PLACEHOLDER 状态未推进，proposal 内容（Oracle session、估时、4 intent types、DSL 子图设计）有价值
2. **Wave 2 语境已变**: 原引用 2026-09-16 旧 roadmap §三 P1 row — Wave 2 plan 已被 L2 系列（pdk-chat-demo-evolution-reference-example + l2-evolution-deferred-follow-up）吸收
3. **重启时需重新校准触发条件**: intent-classification-router 设计基于 Loop Agent 三类（React / PlanExecute / ForkJoin），但当前 `pdk/loop_agent` 已 ship 且 ChatSession 集成已稳定 — 重启时需要先重新评估 routing 必要性
4. **优先级 P3**: 与 `provider-llm-tool-empty-passthrough` (P2, F1 Latent Site #3) 相比，意图分类是 UX 改进而非 bug fix

## Why (为什么现在不实施)

- 当前 P1 工作流已被 Oracle 推荐路径占满：
  - DECISION B(a): ProviderLLMTool 空文本守卫 (1-2h, P2)
  - DECISION B(c): chat-real-llm-coverage Phase H (3-5h, P2)
  - DECISION C: Wave 3 Phase 2 D4 规划
- Wave 3 Phase 2 D4 完成后才会有真实 LLM routing 数据验证意图分类必要性
- L2 ref example 已是 loop routing 的 reference implementation，再次扩展意图分类需要先证伪 L2 4 phase pipeline 的实际使用率

## Acceptance (重新启动条件)

- [ ] 真实 LLM 测试基础设施（chat-real-llm-coverage Phase H）ship 后
- [ ] Wave 3 Phase 2 D4 (LoRA pipeline) 完成后，收集 routing 决策数据
- [ ] 任何生产 telemetry 显示 4-loop 分流选择 < 95% 准确率即触发
- [ ] pdk_chat_demo `/model` 升级（D2 from `pdk-chat-demo-followups.md`）ship 后，验证 routing 与 /model 交互

## Reverse Indicator (R8 per AGENTS.md)

```
[Reverse Indicator]
+ new_up: 8 天 PLACEHOLDER 信息保留至 improvement 跟踪
- old_down: drop_ratio=0% (纯文档归档，无行为变更)
- failure_traces: N/A (no code change)
- ablation: N/A
- context_ids: N/A
```

## 关联文档

- 原 placeholder: `openspec/changes/2026-09-17-intent-classification-router/` (待 archive)
- Roadmap 2026-09-16: `docs/roadmap/2026-09-16-pdk-chat-demo-evolution-roadmap.md` §三 P1 (stale)
- ADR-0071 §D5 (LLM-native DSL), ADR-0074 (Prompt + Evidence Gate)
- Loop Agent 现状: `pdk/loop_agent/` (已 ship, 3 loop types)