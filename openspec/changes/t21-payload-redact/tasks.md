# Tasks: t21-payload-redact

> **TDD 5 步结构**: 每任务按 Write failing test → Verify fail → Implement → Verify pass → Commit
> **关键不变量**: IInteractionBus / EventBuilder / ToolResult 等公开 API **零修改**；仅修改事件 payload 内部字段
> **V1 简化**: SHA-256 hash + chars/4 token estimate（V2 升级 AppendOnlyEventLog 集成 + 真实 tokenizer）

## Phase 0: Prompt Hash Helper（估时 0.2 sprint）

- [x] **T0.1** Write failing test: `tests/test_prompt_hash.cpp` 骨架（≥ 6 cases）
  - `hash_same_input_same_output`（确定性）
  - `hash_different_input_different_output`（唯一性）
  - `hash_empty_string_returns_valid_hex`（边界）
  - `hash_long_input_handles_correctly`（性能）
  - `estimate_tokens_chars_div_4`（V1 简化算法）
  - `estimate_tokens_empty_string_returns_zero`（边界）
- [x] **T0.2** Verify fail: 编译失败（`fatal error: 'prompt/prompt_hash.h' file not found`）
- [x] **T0.3** Implement minimal: `include/agenticdsl/prompt/prompt_hash.h`:
  ```cpp
  namespace agenticdsl {
  // SHA-256 hex 前 16 字符 (足够 PII 识别 + 不可逆)
  std::string hash_prompt(const std::string& prompt);
  // V1 简化: chars / 4 (V2 升级真实 tokenizer)
  int estimate_tokens(const std::string& prompt);
  }
  ```
- [x] **T0.4** Implement: `src/modules/prompt/prompt_hash.cpp`:
  - 使用 std::hash（避免新依赖）或 OpenSSL SHA-256（若已 vendor）
  - hex encode 前 16 字符 = 64-bit collision space (PII 识别足够)
  - chars/4 token estimate + round-up
- [x] **T0.5** Verify pass: 6 cases PASS
- [x] **T0.6** Commit: `feat(prompt): prompt_hash + estimate_tokens helpers (T0)`

## Phase 1: 3 事件 Payload 脱敏（估时 0.3 sprint）

- [x] **T1.1** Write failing test: `tests/test_prompt_evidence_gate.cpp` 修正 3 个事件断言：
  - 删除 `payload.data["prompt"]` 断言（保留原断言为注释）
  - 添加 `payload.data["prompt_hash"]` 断言（16 char hex）
  - 添加 `payload.data["prompt_length"]` 断言（int）
  - `parse_failed` 保留 `error_position` + `retry_count`
  - `schema_validation_failed` 保留 `violation`
  - `token_limit_exceeded` 保留 `stage` + `actual_tokens`，添加 `token_estimate`
- [x] **T1.2** Verify fail: 3 个事件测试 FAIL（payload 字段已变更）
- [x] **T1.3** Modify: `src/modules/prompt/evidence_gate.cpp:64`（parse_failed）
  ```cpp
  // 旧: .args(nlohmann::json{{"prompt", prompt}, {"error_position", pos}, {"retry_count", n}})
  // 新:
  bus_->emit(EventBuilder(llm_dsl_topics::kParseFailed)
      .meta(trace_meta)
      .args(nlohmann::json{
          {"prompt_hash", hash_prompt(prompt)},
          {"prompt_length", prompt.size()},
          {"error_position", pos},
          {"retry_count", n}
      })
      .build());
  ```
- [x] **T1.4** Modify: `src/modules/prompt/evidence_gate.cpp:71`（schema_validation_failed）
  - 同上模式 + `{"violation", violation_msg}` 保留
- [x] **T1.5** Modify: `src/modules/prompt/prompt_assembler.cpp:85`（token_limit_exceeded）
  ```cpp
  // 新: payload 仅含 hash + length + token_estimate + stage + actual_tokens
  bus_->emit(EventBuilder(prompt_topics::kTokenLimitExceeded)
      .args(nlohmann::json{
          {"prompt_hash", hash_prompt(prompt)},
          {"prompt_length", prompt.size()},
          {"token_estimate", estimate_tokens(prompt)},
          {"stage", stage},
          {"actual_tokens", actual}
      })
      .build());
  ```
- [x] **T1.6** Verify pass: 3 个事件测试 PASS + 既有 16 cases 零回归
- [x] **T1.7** Commit: `fix(prompt): T21 payload redact - 3 events hash-only (security fix)`

## Phase 2: ADR-0068 附录 A 同步（估时 0.2 sprint）

- [x] **T2.1** Modify: `docs/adr/adr-0068-event-emission-contract.md` 附录 A v1.4 → v1.5：
  - `llm.dsl.parse_failed`: payload 字段从 `{"prompt", "error_position", "retry_count"}` → `{"prompt_hash", "prompt_length", "error_position", "retry_count"}`
  - `llm.dsl.schema_validation_failed`: 同上 + `{"violation"}`
  - `prompt.token_limit_exceeded`: payload 字段从 `{"prompt"}` → `{"prompt_hash", "prompt_length", "token_estimate", "stage", "actual_tokens"}`
- [x] **T2.2** Verify: `grep "prompt_hash\|prompt_length" docs/adr/adr-0068-event-emission-contract.md | wc -l` ≥ 3
- [x] **T2.3** Verify: `grep -E '("prompt"|prompt.*:.*prompt[^_])' docs/adr/adr-0068-event-emission-contract.md` 不应包含 llm.dsl.* / prompt.token_limit_exceeded 主题的 payload 描述
- [x] **T2.4** Commit: `docs(adr-0068): v1.4 → v1.5 - 3 event payloads hash-only (PII defense)`

## Phase 3: 文档同步 + ship（估时 0.2 sprint）

- [x] **T3.1** Modify: `docs/architecture/cross-cutting-hooks-architecture-2026-08.md` §十一:
  - 标注："T21 Prompt Evidence Gate 事件 payload 泄露面已修复 (commit t21-payload-redact ship)"
  - 状态从 "HIGH 风险未修复" → "已修复"
- [x] **T3.2** Verify: `python3 tools/adr_lint.py` PASS
- [x] **T3.3** Verify: `python3 tools/docs_drift_audit.py` 0 NEW CRITICAL
- [x] **T3.4** Verify: `ctest --output-on-failure` 全量 0 回归（动态基线，禁止硬编码）
- [x] **T3.5** Verify: `grep -rE 'args\(nlohmann::json\{\{"prompt"' src/modules/prompt/` 应为 0 行（payload 全文已清除）
- [x] **T3.6** Commit: `docs(architecture): 标注 T21 payload redact 修复完成`
- [x] **T3.7** `openspec archive t21-payload-redact`

## 总估时

- Phase 0: 0.2 sprint
- Phase 1: 0.3 sprint
- Phase 2: 0.2 sprint
- Phase 3: 0.2 sprint
- **总估时: ~0.9 sprint**（最小化、零契约变更）

## 关键不变量（强制遵守）

- ❌ IInteractionBus / EventBuilder / ToolResult 公开 API 零修改
- ❌ ADR-0068 附录 A 主题名（llm.dsl.* + prompt.token_limit_exceeded）零修改
- ❌ 既有 19 cases test_prompt_evidence_gate 中非事件断言零修改
- ❌ T19 GEPA / T20 MCTS / G11 Mutation Governance 等其他任务零修改
- ❌ 在测试失败时强行 commit
- ❌ 硬编码 ctest 数字

## 明确 out of scope (V2 deferred)

- AppendOnlyEventLog 集成（V2: 原文持久化到 log，事件流仅含 hash + log_id）
- 真实 LLM tokenizer（V2: tiktoken 或类似，替代 chars/4 估算）
- 差分隐私 / k-anonymity 增强（V2: 大规模 prompt 集合的 PII 保护）
- HookPattern 化（T21 全局 PII scrub hook via CompositionPattern, ADR-0085 实施）
- 审计日志原文恢复工具（V2: 通过 prompt_hash 关联 log_id 恢复原文）

## 安全保证

✅ SHA-256 hex 前 16 字符 = 64-bit collision space
- 误判概率: 1/2^64 ≈ 5.4 × 10^-20（实际不可达）
- 不可逆: 无法从 hash 还原原文
- 可识别: 相同 prompt → 相同 hash（用于事件关联 + log 恢复）

✅ PII 不入事件流
- 原始 prompt 仅保留在调用方内存（暂态）
- V1 无原文持久化路径（合规要求）
- V2 可选：AppendOnlyEventLog 集成时原文仅写入受控 log

✅ 操作可观测性保留
- prompt_length: int（识别异常长度 prompt）
- token_estimate: int（监控 token 消耗）
- stage / actual_tokens: 监控装配流程
- violation / error_position: 结构化错误信息（非原文）
