# r13-4-confidential-redaction-real-llm

> **来源**: 2026-10-06 全量回归测试完备性审查 (`AGENTS.md §Recent Changes` entry 自识别盲点 G6)
> **生成**: 2026-10-06 via completeness-audit
> **AGENTS.md Pattern**: #10 hygiene (systematic recording of deferred work)

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
