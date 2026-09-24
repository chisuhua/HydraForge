# pdk-chat-demo-evolution-reference-example

**优先级**: P1 | **来源**: 用户决策 2026-09-23 "用 pdk-chat-demo 作为承载例, 给三方 SoT 架构做可运行的 reference example" + C4 Decision Record §3 第 6 项 (post-hoc closure gate) + 3 份 SoT 已 ship ([self-evolution v1.5 §十一], [harness v1.0 §十一], [rsi v1.0 §十一])

**阶段**: phase-6c | **分类**: examples + reference

**类型**: feature

**主题**: 端到端 6 段 pipeline reference example: ContextRequest → ChatSession → apply_harness_mutation → load(genome@N) → 重建 ChatSession → IEvaluator V2 → trace JSONL;消费 C2/C3/C4/G1/G2/G3/G4/Wave 3 Phase 1 全部 ship 接口,零新增公共 API。

## 架构依据
- 3 份 SoT 已 ship:`self-evolution-architecture-2026-08.md` v1.5 + `harness-architecture-2026-09.md` v1.0 + `rsi-architecture-2026-09.md` v1.0, §十一 pdk_chat_demo traceback 已就位。
- C4 (harness-rsi-pilot) 已 ship `apply_harness_mutation` 5 参自由函数 (`harness_rsi.h:73-78`) + 5-tier gate。
- G4 (genome-wiring-harness-rsi-gepa) 已 ship `IGenomeRegistry::commit/load/fork/walk_ancestors` 6 方法。
- ADR-0023 (ToolResult), ADR-0061-13 (IDistillationWriter), ADR-0083 (IEvaluator V2) 均已 ship。
- Wave 3 Phase 1 D7 `LLMProviderFactory::register_dynamic` stub provider 已注册。
- L2 是 **reference example 入口设施**,**不**是 autonomous evaluator (per user original quote)。

## 范围
- **In Scope**:
  - `examples/pdk_chat_demo_evolution/` 新建子项目,主 demo 零改动 (N1/R6 强制)
  - 6 段端到端 happy path demo (启动 → session 初始化 → baseline → mutation → reload → compare → emit JSONL)
  - 14 fixture 文件 (3 SHIPPED reference + 11 test,per P0-10 spec §3.1.6)
  - `--context-file <path.jsonl>` 必填 (R13 零 hardcode);6 顶层 + 4 metadata 子字段 schema 校验 (S28-S31)
  - R8 反向指标门 (3 flag: `--release-metrics` / `--regression-test-suite` / `--ablation-mode=full`) — 机制演示 + 静态契约守卫,**不**真实回归度量 (per C5 P0 fix + rsi §11.8.1 治理边界)
  - R9 反作弊 3 模式降级 (R9.1 parser-side hint + R9.2 prefix-rejection + R9.3 keyword-rejection defer Wave 4) — **不**声称真实反作弊
  - R13 ContextRequest 零 hardcode contract (≥3 类 code/research/debug 实证)
  - 10 个 test binary (per P0-7 fix, 含 LABELS "l2-evolution") + hermetic HOME env fixture (per P0'-4 fix)
- **Out Scope**:
  - 修改 `examples/pdk_chat_demo/` 主体 (N1)
  - 新增 ChatSession / ChatConfig / Genome / Mutation / Provider / IEvaluator 公共 API (N2/R7 freeze,`git diff include/` = 0)
  - Sandbox network isolation / 真实反作弊能力 (defer Wave 4)
  - Wave 3 Phase 2 D4-D7 (LoRA + 评估 + AgenticMind + serving 完整化, 独立立项)
  - S4 Agent-Agent 协同进化 (research 路径)

## 关键场景
- GIVEN `--mock --context-file examples/pdk_chat_demo_evolution/fixtures/contexts/code-class-context.jsonl` 启动,WHEN 跑 6 段链,THEN 30 秒内 exit 0,JSONL 含 4 段事件 (baseline/mutation/reload/compare) × 8 顶层字段 + meta 12 字段 (per P2-1),`meta.context_id` 进每段事件。
- GIVEN Phase 4 reload (load(genome@N) + ChatSession 11 参 ctor rebuild),WHEN 同 turn_input 再次输入,THEN ChatSession 响应有可见变化 (mutation 生效)。
- GIVEN `metadata.is_hidden=true` ContextRequest,WHEN 处理,THEN 接受进 hidden 桶 + emit `hidden_context_accepted_info` + `meta.is_hidden=true` + `meta.hidden_bucket=true` 双字段 (per R13.4 P0 fix,公开/隐藏集分离评测,非拒绝)。
- GIVEN `task_class: "mutation_metric_*"` ContextRequest,WHEN 处理,THEN prefix-rejection 先于闭枚举校验,emit `mutation_metric_rejected` + exit non-zero (per P0'-1 ordering fix + P0-7 prefix-rejection)。
- GIVEN R9.3 turn_input 含 `fetch http://`,WHEN 处理,THEN keyword-rejection + emit `turn_input_network_keyword_rejected` + exit non-zero (defer 真 sandbox 至 Wave 4)。
- GIVEN R8 `--release-metrics` + 注入 fixture drop_ratio > 5%,WHEN 运行,THEN exit non-zero (mechanism 演示,injected fixture 验证)。

## 技术约束
- MUST 消费现有 API,零新增 public contract:`ChatSession` 11 参 ctor (`chat_session.h:210-230`) + `ChatConfig::override_provider` + `ChatConfig::override_system_prompt` 2 方法 (Sprint 19) + `apply_harness_mutation` 5 参自由函数 (`harness_rsi.h:73-78`,非 MutationGateChain 类) + `IGenomeRegistry` 6 方法 + `LLMProviderFactory::register_dynamic` + `IDistillationWriter` + `IEvaluator` V2。
- MUST `--context-file` 必填 (R13.2 零 hardcode);`task_class` 闭枚举 `code_gen | research | summary | debug | classify | other` (P0-7 fix 加 `other` 兜底);`invocation_mode` `mock | real_llm_deepseek | real_llm_custom`,默认 `mock`。
- MUST ContextRequest JSONL 每行 1 个 object;schema 校验失败 → exit non-zero + 行号 + 字段名。
- MUST prefix-rejection (`mutation_metric_*`) 先于 closed-enum validation (per P0'-1 ordering)。
- MUST R9.1 parser-side regex detection `R"(the answer is \w+)"` 拒绝 + emit `hint_containment_rejected`,LLM 不介入 (per P0'-2 fix,消除 vacuous 静态断言)。
- MUST L2 启动时构造 FilesystemGenomeRegistry 之前 hermetic HOME fixture (`HOME=/tmp/l2-test-<uuid>` + `mkdtemp` + teardown),mock + real-llm 双模式均走 hermetic HOME (per P0'-4 fix,守 N2 + AGENTS.md 模式 #10 fresh-deploy hygiene)。
- MUST L2 不计算 eval_quality (per P0-4 C5 fix);输出 `attribution_verdict` 来自 `BehavioralEquivalenceEvaluator` V2 (已 ship);R8.3 消融用 `attribution_verdict` 分布 + `response_edit_distance`,**不**用 eval_quality diff (per P0'-3 fix)。
- MUST `to_agent_config` 落点 `pdk_chat_demo_evolution::detail::to_agent_config` (per P2-2 fix,避免污染 `agenticdsl::genome` namespace)。
- MUST L2 trace meta 字段 12 个统一 (per P2-1 fix);`hidden_context_accepted_info` MUST 发射 (per P2-3 fix,design 与 spec MUST 统一)。
- MUST redaction 3-tier policy (per P2-4 fix):public=none / internal=redact turn_input+response / confidential=+expected_eval_quality+tags+domain。
- MUST NOT 任何 `git diff include/` 改动;MUST NOT 修改 `examples/pdk_chat_demo/` 主体。
- SHOULD ctest LABELS "l2-evolution" 全部 10 binary;baseline 211 零回归 (`ctest -LE l2-evolution`)。

## 验收标准
- Stage 4 Oracle 最终 verdict = **APPROVE**(已 ship,commit `c3b4844` baseline + `78b6098` follow-up,Stage 0/1/2/4 完整 ship-with-fixes 闭环)。
- `openspec validate pdk-chat-demo-evolution-reference-example --strict` PASS。
- 14 fixture 文件 (3 SHIPPED + 11 test) JSON-legal + schema 6+4 合规 (除 `invalid_missing_context_id.jsonl` 按设计无 context_id)。
- 10 binary ctest LABELS "l2-evolution" 全过 (per P0-7 fix);`ctest -LE l2-evolution` baseline 211 零回归。
- `--context-file` 必填契约 (S28):无 flag → exit non-zero。
- 4 个 ADR-0068 v2.4 事件注册 + 发射 (`mutation_metric_rejected` / `turn_input_network_keyword_rejected` / `hidden_context_accepted_info` / `hint_containment_rejected`)。
- `git diff include/` = 0;`git diff examples/pdk_chat_demo/` = 0;commit message 含 [Reverse Indicator] 5 字段 (`new_up` / `old_down` / `failure_traces` / `ablation` / `context_ids`)per AGENTS.md Reverse Indicator Rule (2026-09-23)。
- 3 份 SoT §十一 "L2 ✅ ship" 行已加;harness-arch §3.1 API 表面校正 (11 参 ctor + 2 override 方法 + 5 参自由函数,同步 commit `78b6098` 后);SoT 三件套 (README/rsi/self-evolution) 路径全部归一至 `examples/pdk_chat_demo_evolution/fixtures/contexts/`。