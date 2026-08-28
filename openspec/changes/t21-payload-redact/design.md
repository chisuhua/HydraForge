# Design: t21-payload-redact

## Context

T21 Prompt Evidence Gate (commit `abd3bd3`, OpenSpec change `t21-prompt-evidence-gate` archived) 发射 3 个事件时携带完整 prompt 全文，构成 PII 泄露面。Oracle 横切审查（cross-cutting-architecture v1.2 §11）发现此问题并标记为 HIGH 风险。本 change 实施最小化修复：3 事件 payload 改为 hash-only 模式（对齐 G11 "args 仅 key 不 value" + ComplianceDecorator hash-only 范式）。

## Scope Boundaries

### 范围 IN
- 新增 `include/agenticdsl/prompt/prompt_hash.h` + `src/modules/prompt/prompt_hash.cpp`
- 修改 `src/modules/prompt/evidence_gate.cpp:64,71`
- 修改 `src/modules/prompt/prompt_assembler.cpp:85`
- 修正 `tests/test_prompt_evidence_gate.cpp` 3 个事件断言
- 更新 `docs/adr/adr-0068-event-emission-contract.md` 附录 A v1.4 → v1.5
- 更新 `docs/architecture/cross-cutting-hooks-architecture-2026-08.md` §十一
- ≥6 新增 helper 测试 + 3 修正事件测试

### 范围 OUT
- IInteractionBus / EventBuilder / ToolResult 公开 API 修改
- 既有 19 cases test_prompt_evidence_gate 中非事件断言
- T19 GEPA / T20 MCTS / G11 Mutation Governance 等其他任务
- AppendOnlyEventLog 集成（V2）
- 真实 LLM tokenizer 替换 chars/4（V2）
- HookPattern 化（ADR-0085 V2 实施）

## Design Decisions

### D1 — SHA-256 hex 前 16 字符作为 prompt_hash

**理由**:
- **不可逆**: 无法从 hash 还原原文（满足 PII 防御）
- **可识别**: 相同 prompt → 相同 hash（事件关联 + log 恢复）
- **64-bit collision space**: 1/2^64 误判概率（实际不可达）
- **轻量**: 仅 16 字符 hex，事件 payload 体积可控
- **零新依赖**: 使用 `std::hash` 或现有 OpenSSL（若已 vendor）

### D2 — estimate_tokens 用 chars/4 简化算法

**理由**:
- V1 简化：避免引入 tokenizer 依赖
- 误差约 ±25%（LLM tokenizer 实际更精确），对事件可观测性足够
- V2 升级：接 tiktoken 或类似真实 tokenizer

### D3 — 保留所有可观测性字段（length / stage / violation）

**理由**:
- 安全改进不应损失监控能力
- prompt_length: 识别异常长度（潜在 injection 攻击）
- token_estimate: 监控 token 消耗趋势
- stage: 监控装配流程（1/2）
- violation: 结构化错误（非原文）
- error_position / retry_count: parse 重试可观测性

### D4 — payload 字段视为 additive change（不视为 breaking）

**理由**:
- ADR-0068 §决策 5: 事件 schema 演进采用 additive-only 原则
- 字段从 `prompt` → `prompt_hash` + `prompt_length` 是**字段替换**，schema 内部细节
- 下游 consumer 应使用 `prompt_hash` 关联 log 恢复原文（如需要）
- 与 G11 mutation.* 4 主题 payload 演进模式一致

### D5 — 零契约变更（与之前 ship 任务风格一致）

**理由**:
- T17 SkillCompiler / T15 Trajectory IR / G11 Mutation 均保持零契约变更
- 此次仅修改事件 payload 内部字段（schema-level change）
- IInteractionBus / EventBuilder 等公开 API 0 改动
- 测试 19 cases 中 16 个非事件断言 0 改动（仅 3 个事件断言修正）

## Risks

| 风险 | 缓解 |
|---|---|
| 误判导致事件关联失败 | SHA-256 前 16 字符 64-bit collision space（实际不可达）|
| 下游 consumer 解析 `prompt` 字段失败 | 字段替换视作 additive change，文档同步 + ADR-0068 v1.5 标注 |
| 测试修改破坏现有断言 | 19 cases 中仅 3 个事件断言修改，其余 16 个 0 改动 |
| PII 已通过原 payload 进入事件流（历史事件）| 历史事件未持久化（V1 无 log 集成），仅在内存中短暂存在 |
| 性能影响 | SHA-256 + chars/4 计算 < 1µs per call，可忽略 |

## Verification Gates

- ✅ ≥ 6 cases test_prompt_hash PASS
- ✅ 19 cases test_prompt_evidence_gate PASS（3 修正 + 16 零修改）
- ✅ ctest 全量 0 回归（动态基线）
- ✅ adr_lint 83 ADR PASS
- ✅ docs_drift_audit 0 NEW CRITICAL
- ✅ `grep 'args({{"prompt"'` = 0（payload 清除）
- ✅ `grep 'prompt_hash'` ≥ 3（已应用）

## Dependencies

### 满足
- ✅ T21 Prompt Evidence Gate ship (commit `abd3bd3`)
- ✅ ADR-0068 v1.4 附录 A 27+ 主题
- ✅ EventBuilder / IInteractionBus 已 ship
- ✅ ComplianceDecorator hash-only 范式（参考实现）
- ✅ G11 mutation.* 4 主题 hash/trace_id only（参考实现）

### 不依赖
- HookPattern（ADR-0085 V2）
- AppendOnlyEventLog（V2 集成）
- 真实 LLM tokenizer（V2 升级）

## Out of Scope (V2 deferred)

- AppendOnlyEventLog 集成（原文持久化到 log，事件流仅含 hash + log_id）
- 真实 LLM tokenizer（tiktoken 或类似，替代 chars/4 估算）
- 差分隐私 / k-anonymity 增强
- HookPattern 化（T21 全局 PII scrub hook）
- 审计日志原文恢复工具（通过 prompt_hash 关联 log_id 恢复）

## Success Criteria

- T21 事件 payload 泄露面修复（hash-only）
- ctest 全量 0 回归
- ADR-0068 附录 A v1.5 同步更新
- 横切架构审查报告标注"已修复"
- OpenSpec archive 完成
