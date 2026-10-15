# r13-4-confidential-redaction-real-llm

> **来源**: 2026-10-06 全量回归测试完备性审查 (`AGENTS.md §Recent Changes` entry 自识别盲点 G6)
> **生成**: 2026-10-06 via completeness-audit
> **AGENTS.md Pattern**: #10 hygiene (systematic recording of deferred work)

> ⛔ **SUPERSEDED 2026-10-15** — dual-agent 调研 (Metis + Oracle 并行) 识别 R9 误标"弱变体":
>
> **R13.4 mock 已 ship** (commit `1fd1450` 2026-09-26, `test_reverse_indicators.cpp:265-301`), 但**真实 LLM 验证零增量信号**:
> - `redact_trace_fields` (`evolution_session.cpp:582-602`) 是**纯 JSON 变换** (provider-agnostic 整字段替换)
> - LLM 真在回路 (`evolution_session.cpp:201 provider->generate`), 但**断言目标与 LLM 无关**
> - Case 1/2 断言恒真: 真 LLM 复述 turn_input 不影响 `response == "[REDACTED-confidential]"`
> - Case 2 物理不可测: EvolutionSession 是 N 个独立 context 复用 ChatSession, **非 turn1→turn2** 多轮
> - Case 3 幻影需求: `redact_trace_fields` 不含 tool_calls, trace schema 不含 tool_calls 字段
> - Case 4 spec 冲突: spec R13.4 保留 context_id/session_id/sensitivity 供 audit trail
>
> **must_realllm 误标反模式第 4 次** (前 3 次: 2026-09-29 fork_join + 2026-10-15 提案 4 P2 7af59b7 + 2026-10-15 rsi-anti-cheat 强变体). 提案 0 处引用 `provider->generate` 断言依赖.
>
> **关键发现 — 真 bug 清单** (per dual-agent 共识):
> 1. `expected_eval_quality` 漏脱敏 (per spec R13.4 `:409` 要求, 实现 `582-602` 漏字段)
> 2. **跨 context 泄漏** (共享 ChatSession, public ctx2 response 可能复述 confidential ctx1 内容且不脱敏)
> 3. **wire-level 明文 by design** (spec NOTE `:417` 显式豁免, 需 Recording Provider case 固化设计决策)
>
> **后续** (per dual-agent Oracle 路径 1 + 用户 2026-10-15 决策):
> - 关闭本提案 (本文件)
> - 新建 deterministic 静态守卫 (`test_r13_4_redaction_static_guard.cpp`, mock only, 复述 secret 验证)
> - Recording Provider case 固化 wire-level 明文设计 (合规审计 trail)
> - 立独立 OpenSpec change `2026-10-15-r13-4-real-bug-fixes` 修 2 个真 bug (`expected_eval_quality` + 跨 context 泄漏)
>
> 详细 dual-agent 5 维分析 + 收敛信号见 `.rddf/audits/2026-10-15-r13-4-confidential-redaction-real-llm-dual-agent-review.md`
> 决策登记: 用户 2026-10-15 显式选择"Oracle 路径 1: 关闭+静态守卫 (推荐)"

**优先级**: 🟠 P1 | **阶段**: 自由 (phase-n/a) | **分类**: 真实 LLM 验证盲点
**类型**: debt-tracking | **主题**: R13.4 confidential 0% leak 红线真实 LLM 路径验证
**状态**: pending (mock 覆盖已 ship 2026-09-26 `1fd1450`, real LLM 未验证)
**关联**:
- `rsi-architecture-2026-09.md` §十二 R13.4
- `L2 spec §R13.4 confidential 0% leak`
- `tests/test_reverse_indicators.cpp` (R13.4 mock 已 ship)
- `openspec/changes/archive/2026-09-26-l2-evolution-real-execution-chain/` (`1fd1450`)

## 背景

R13.4 confidential redaction 已 ship (2026-09-26 `1fd1450` 修复 dead code):
- `evolution_session.cpp` redact_trace_fields helper 在 trace emit 前脱敏
- `turn_input` / `response` 字段 sensitivity=confidential → "[REDACTED-confidential]"
- mock 测试验证: confidential context → trace emit `"[REDACTED-confidential]"` (was cleartext)

但**真实 LLM 路径下脱敏验证未覆盖**:
- LLM 可能在 response 中意外泄露 confidential 信息
- LLM 可能对 confidential prompt 输出 cleartext 敏感字段
- 真实 LLM trace payload 可能含未脱敏的 turn_input 间接引用

## 缺口

R13.4 红线 `confidential 0% leak` 当前仅 mock 验证。真实 LLM:
- response 字段可能含 turn_input 复述 (即使 turn_input 已脱敏)
- metadata 字段可能含 unredacted 子字段
- 多轮对话中后续 turn 可能引用已脱敏的早期 turn (LLM 自身不能保证)
- LLM 工具调用 arguments 可能含 cleartext (需 ToolRegistry 层脱敏)

## 触发条件

任何下列条件满足即升级 P0 → 立即立项:
1. 用户要求 "R13.4 真实 LLM 端到端脱敏验证"
2. Wave 4 (sandbox) 启动需要真实 LLM 脱敏合规
3. 任何生产 trace payload 含 confidential cleartext bug 发现

## 实施建议 (非实施, 待立项 OpenSpec change)

新增 `openspec/changes/2026-XX-r13-4-real-llm-redaction/`:
- 新增 `tests/test_r13_4_redaction_real_llm.cpp` (must_realllm label + l2-evolution)
  - Case 1: ContextRequest sensitivity=confidential → LLM 真实响应 → trace emit 0% cleartext leak
  - Case 2: 多轮对话 turn 1 confidential → turn 2 LLM 不能复述 turn 1 cleartext
  - Case 3: LLM 工具调用 arguments 含 turn_input 引用 → ToolRegistry 脱敏
  - Case 4: metadata 字段子字段 (domain/tags) 不能绕过 redact_trace_fields
- 验证手段: grep trace JSONL for cleartext patterns, 断言 0 命中
- 预估 effort: 1-2 天 (脱敏 helper 复用 + 真实 LLM adapter)
- 关键: 必须用真实 LLM (deepseek), 因为 mock 不能生成真实 LLM 输出模式

## Status: ⛔ SUPERSEDED (2026-10-15, Oracle 路径 1 + dual-agent 调研阻断)

**Status 段补全** (per Pattern #10 hygiene, 2026-10-15 audit verification):

本提案于 2026-10-15 经 dual-agent 调研 (Metis `ses_ec1545fcdffe79LuLYcKsBH38D` + Oracle `ses_ec1540a00ffeoqYH8dp7b32Zcx`) 识别为 **must_realllm 误标反模式弱变体** — 4 强命中 + 3 独占发现.

**关键发现** (Oracle + Metis 收敛):
1. **Case 1/2 断言恒真**: `redact_trace_fields` 整字段替换 response, 真 LLM 复述 turn_input 不影响断言
2. **Case 2 物理不可测**: EvolutionSession 是 N 个独立 context 复用 ChatSession, **非 turn1→turn2** 多轮
3. **Case 3 幻影需求**: `redact_trace_fields` 582-602 不含 tool_calls, trace schema 不含 tool_calls 字段
4. **Case 4 spec 冲突**: spec R13.4 保留 context_id/session_id/sensitivity 供 audit trail, 脱敏违反 spec

**must_realllm 误标反模式第四次** (前 3 次: 2026-09-29 fork_join + 2026-10-15 提案 4 P2 7af59b7 + 2026-10-15 rsi-anti-cheat-real-llm 强变体). 本提案为**弱变体**: LLM 在回路但断言 LLM-independent (provider-agnostic 整字段替换).

**真 bug 清单** (per dual-agent 共识, 立项修复):
1. `expected_eval_quality` 漏脱敏 (per spec R13.4 :409 要求, 实现 582-602 漏字段)
2. **跨 context 泄漏** (共享 ChatSession, public ctx2 复述 confidential ctx1)
3. **wire-level 明文 by design** (spec NOTE :417 显式豁免, 需 Recording Provider case 固化设计决策)

**建议** (per dual-agent 调研审计 `.rddf/audits/2026-10-15-r13-4-confidential-redaction-real-llm-dual-agent-review.md`):
- 关闭本提案 (Oracle 路径 1, per commit `7321154` 2026-10-15)
- 立独立 OpenSpec change `2026-10-15-r13-4-redaction-bug-fixes` 修 2 个真 bug (per commit `dbd01d6` 2026-10-15 立项, 5-file integrity + 24h cooling-off)

**Pattern 沉淀**: Pattern #13 must_realllm 误标反模式谱系第 4 实例 (弱变体). 提案文件头注 (`> ⛔ SUPERSEDED 2026-10-15`) 已标注, 本 Status 段补全治理字段标准化.
