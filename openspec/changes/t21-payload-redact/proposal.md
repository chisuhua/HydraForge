# t21-payload-redact

## Why

T21 Prompt Evidence Gate (commit `abd3bd3`, OpenSpec change `t21-prompt-evidence-gate` 已 ship) 发射 `llm.dsl.*` / `prompt.token_limit_exceeded` 3 主题事件时，**事件 payload 携带完整 prompt 全文**（`args(nlohmann::json{{"prompt", prompt}, ...})`），构成敏感数据泄露面。

**Oracle 横切审查发现** (cross-cutting-architecture v1.2 §11):
- **位置**: `src/modules/prompt/evidence_gate.cpp:64,71` + `src/modules/prompt/prompt_assembler.cpp:85`
- **风险**: 违反 ADR-0080 D10 PII 约束（蒸馏数据 capture 模式禁止原始 prompt 持久化）
- **对比**: G11 Mutation Governance 严格遵守 "args 仅 key 不 value" 防御原则（`mutation.governor.cpp:140` 用 `.meta(trace_id)` 而非 `.args(prompt)`）

**审计依据**:
- ADR-0080 D10 Capture/Scrub 解耦: prompt 原始内容应仅出现在 trace 路径，不应入事件流
- ADR-0068 §决策 4: 事件 payload 字段最小化原则
- G11 mutation.* 4 主题全部 hash/trace_id only（无完整 payload）
- ComplianceDecorator 已 ship hash-only 范式（`src/common/llm/compliance_decorator.cpp`）

**前置依赖**（全部已满足）:
- ✅ ADR-0068 v1.4 附录 A 27+ 主题注册（含 `llm.dsl.*` + `prompt.token_limit_exceeded`）
- ✅ T21 ship 19 cases / 338 assertions PASS
- ✅ EventBuilder API 已 ship（`include/agenticdsl/contract/event_builder.h`）

## What Changes

**核心修复**: 3 处 payload 改为 hash-only 模式

1. **`src/modules/prompt/evidence_gate.cpp:64`** — `llm.dsl.parse_failed` 事件：
   - 移除：`"prompt": prompt`（完整文本）
   - 改为：`"prompt_hash"` (SHA-256 hex, 16 chars) + `"prompt_length"` (int)
   - 保留：error_position / retry_count（schema 不变）

2. **`src/modules/prompt/evidence_gate.cpp:71`** — `llm.dsl.schema_validation_failed` 事件：
   - 移除：`"prompt": prompt`
   - 改为：`"prompt_hash"` + `"prompt_length"` + `"violation"`
   - 保留：violation 字段（结构化错误信息，非原文）

3. **`src/modules/prompt/prompt_assembler.cpp:85`** — `prompt.token_limit_exceeded` 事件：
   - 移除：`"prompt": prompt`
   - 改为：`"prompt_hash"` + `"token_estimate"`（保留可观测性）
   - 保留：stage（1/2）+ actual_tokens（int）

**新增 helper**: `src/modules/prompt/prompt_hash.{h,cpp}`:
- `std::string hash_prompt(const std::string& prompt)` — SHA-256 hex 16 chars (前 16 字符足够 PII 识别)
- `int estimate_tokens(const std::string& prompt)` — chars/4 (V1 简化)
- 不引入新依赖（使用现有 nlohmann/json + openssl SHA-256 或 std::hash fallback）

**测试修正**:
- `tests/test_prompt_evidence_gate.cpp` 中 3 个事件断言（parse_failed / schema_validation_failed / token_limit_exceeded）
- 删除 `payload.data["prompt"]` 断言
- 添加 `payload.data["prompt_hash"]` + `payload.data["prompt_length"]` 断言
- 添加新测试 `prompt_hash_uniqueness`（相同 input → 相同 hash, 不同 input → 不同 hash）

**ADR-0068 附录 A 同步修正**:
- `llm.dsl.parse_failed`: payload 字段从 `{"prompt", "error_position", "retry_count"}` → `{"prompt_hash", "prompt_length", "error_position", "retry_count"}`
- `llm.dsl.schema_validation_failed`: 同上 + `{"violation"}`
- `prompt.token_limit_exceeded`: `{"prompt"}` → `{"prompt_hash", "token_estimate", "stage", "actual_tokens"}`

## Impact

**影响范围**:
- `src/modules/prompt/` 仅 3 文件改动（evidence_gate.cpp + prompt_assembler.cpp + 新增 prompt_hash.h/cpp）
- `tests/test_prompt_evidence_gate.cpp` 19 cases 中 3 个事件断言修正
- `docs/adr/adr-0068-event-emission-contract.md` 附录 A 3 主题 payload 字段更新
- `openspec/specs/t21-prompt-evidence-gate/spec.md` 已 archive（spec delta 已 ship）
- **零契约变更**: IInteractionBus / EventBuilder / ToolResult 等公开 API 0 改动

**下游影响**:
- 任何订阅 `llm.dsl.*` / `prompt.token_limit_exceeded` 主题的下游 consumer（横切架构审查中提到的 metrics/audit/SIEM 等）需改用 `prompt_hash` 关联 + 查 log 恢复原文
- 这是**安全改进**而非 breaking change：原文不应入事件流（合规要求）

**V1 边界**:
- ✅ SHA-256 hash（足够 PII 识别且不可逆）
- ✅ 保留可观测性指标（length / token_estimate / stage / violation）
- ❌ V1 不实施：log 端原文持久化（V2 deferred，需 AppendOnlyEventLog 集成）
- ❌ V1 不实施：差分隐私 / k-anonymity（V2 deferred）

**Breaking Changes**: 无（payload 字段属于事件 schema 内部细节，schema 视为 additive change per ADR-0068 §决策 5）

## ship gate 验证

- `python3 tools/adr_lint.py` 通过
- `python3 tools/docs_drift_audit.py` 通过（无新增 CRITICAL drift）
- `openspec validate --changes --strict` PASS
- `ctest --output-on-failure` 全量 0 回归（动态基线，约 190+）
- `ctest -R test_prompt_evidence_gate` 19 cases 全部 PASS（3 个事件断言已修正）
- `grep 'args({{"prompt"' src/modules/prompt/` 应为 0 行（payload 全文已清除）
- `grep 'prompt_hash' src/modules/prompt/` ≥ 3 行（hash-only 已应用）
- ADR-0068 附录 A 3 主题 payload 字段已同步更新

## 关联文档

- `src/modules/prompt/evidence_gate.cpp:64,71`（待修复）
- `src/modules/prompt/prompt_assembler.cpp:85`（待修复）
- `tests/test_prompt_evidence_gate.cpp`（3 个事件断言待修正）
- `docs/adr/adr-0068-event-emission-contract.md` 附录 A v1.4 → v1.5
- `openspec/specs/t21-prompt-evidence-gate/spec.md`（已 ship, 引用为依据）
- `openspec/changes/archive/2026-08-27-t21-prompt-evidence-gate/`（原 T21 archive）
- ADR-0080 D10（Capture/Scrub 解耦约束）
- G11 `mutation.governor.cpp`（hash/trace_id only 范式参考）
- `docs/architecture/cross-cutting-hooks-architecture-2026-08.md` v1.2 §11（Oracle 审查报告触发）
- `docs/adr/adr-0085-cross-cutting-pattern-pdk.md`（未来 Pattern 化 V2 HookPattern 依据）
