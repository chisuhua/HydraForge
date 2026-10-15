# Design: ADR-0072 D4 env_vars 字段 secrets redaction 协议

> **关联 proposal**: `proposal.md` (5-segment format)
> **设计时间**: 2026-10-15
> **关联 ADR**: ADR-0068 §3.1 (api_key redaction 借鉴) + ADR-0072 D4 (env_vars 字段定义) + ADR-0075 EnvBackend
> **模式依据**: `redact_trace_fields` (evolution_session.cpp:582-602) LLM-independent 整字段替换模式 + `clean_metadata` (per ToolResult::result) pattern

## Decision 1: redact 时机 — 解析时 vs 落地时

**选项 A**: `node_factory.cpp:41-47` 解析时立即 redaction (early binding)
- ✅ metadata 一进入 ctx 就 redact, 后续所有路径 (audit / event / IPC) 自动安全
- ✅ 借鉴 `redact_trace_fields` (per evolution_session.cpp:582-602) 同样模式
- ✅ 一次性 redaction, 不需要每个 emit point 重复
- ⚠️ 测试 fixture 改造 (现有 12 fixture 含 secret_abc123 等假 secret)

**选项 B**: `ToolCoordinator::execute` 入口 redaction (late binding)
- ❌ 太晚 — metadata 已传 `tool_metadata_` + 已 emit event, 仍可能泄漏
- ❌ 多处重复 (每个 emit point 都要 redaction)

**选项 C**: 每个 emit point 单独 redaction (per-event)
- ❌ 4-5 处 emit point 全改, 易遗漏
- ❌ 不符合 ADR-0068 §3.1 helper 抽象精神

**Chosen**: 选项 A (early binding), 借鉴 `redact_trace_fields` 同模式。

## Decision 2: 白名单机制 — explicit allowlist vs implicit blocklist

**选项 A**: 默认 deny, `allowed_env_vars` 白名单 (per ADR-0075 backend policy)
- ✅ 与 ADR-0075 §不变量 4 一致 (env_vars 仅透传白名单)
- ✅ 默认安全 (secrets 默认 redact)
- ⚠️ 现有 fixture 需 explicit 配置白名单

**选项 B**: 默认 allow, `blocked_env_vars` 黑名单
- ❌ 默认不安全 (secrets 默认 pass through)
- ❌ 难以穷举所有 secrets 模式

**选项 C**: 默认 `{"*"}` (向后兼容), explicit 配置时白名单
- ✅ backward-compatible (现有 fixture 零行为变化)
- ✅ 渐进迁移 (用户显式开启白名单)
- ⚠️ 默认不安全, 文档必须强调建议白名单

**Chosen**: 选项 C (默认 `{"*"}`), 提供 backward-compat + 文档强调安全建议。

## Decision 3: redact 实现 — 整字段替换 vs 部分 mask

**选项 A**: 整字段替换 (`{"API_TOKEN": "secret_abc123"}` → `{"API_TOKEN": "[REDACTED]"}`)
- ✅ 借鉴 `redact_trace_fields` 同样模式 (per evolution_session.cpp:582-602)
- ✅ 实现简单 (直接 `value = "[REDACTED]"`)
- ✅ keys 也保护 (per Case 2 nested values, keys 名可能含敏感词如 DATABASE_PASSWORD)

**选项 B**: 部分 mask (`{"API_TOKEN": "***abc123"}` 保留前缀)
- ❌ keys 仍可见, keys 可能泄漏
- ❌ 实现复杂 (字符串截断 + 前缀保留)

**选项 C**: 完全删除 (`{"API_TOKEN": "secret_abc123"}` → `{}`)
- ❌ 失去 keys 列表 (audit 工具无法知道哪些 keys 存在)
- ⚠️ 后端 `env_validation_hook.cpp:51-56` 依赖 keys 列表存在

**Chosen**: 选项 A (整字段替换, value → `"[REDACTED]"`), 保持 keys 列表完整供 audit。

## Decision 4: 嵌套值处理 — recursive vs shallow

**选项 A**: recursive (嵌套 object/array 递归 redaction)
- ✅ 防御深度 secret (e.g. `env_vars.DB.value.password = "secret"`)
- ⚠️ 性能略慢 (递归遍历)

**选项 B**: shallow (仅顶层 keys)
- ❌ 嵌套 secret 仍泄漏
- ❌ 用户预期整字段安全

**Chosen**: 选项 A (recursive), 防御深度 secret, 性能 <10µs/op 可接受 (per AC-1 Case 6)。

## Decision 5: audit event 协议 — emit vs no-emit

**选项 A**: emit `env_vars_audit` event (per ADR-0068 Appendix A v2.5)
- ✅ audit trail 完整 (keys 列表可追溯)
- ✅ 借鉴 `mutation.committed` / `genome.committed` 等审计事件模式
- ⚠️ ADR-0068 Appendix A 需新增 v2.5 amendment

**选项 B**: 不 emit event, 仅 metadata redact
- ❌ 失去 audit trail
- ❌ 与 ADR-0068 §3.1 协议精神不符 (api_key redaction 不 emit, 但 env_vars 是结构性变化)

**Chosen**: 选项 A (emit `env_vars_audit`), 借鉴 `mutation.*` / `genome.*` 审计事件模式, 与 ADR-0068 一致。

## Decision 6: backward-compat — 默认 `{"*"}` vs 默认 `{}`

**策略**: 默认 `allowed_env_vars = {"*"}` (向后兼容, 现有 fixture 零行为变化)
- ✅ 现有 12 fixture 零改造
- ✅ 渐进迁移 (用户显式开启白名单配置)
- ⚠️ 默认不安全, 文档强调

**迁移路径**:
- Sprint 26: ship redact_env_vars helper + default `{"*"}` (本 change 范围)
- Sprint 27+: 文档化白名单建议 (per ADR-0075 backend policy 配置)
- Sprint 28+: 评估是否默认 deny (per drop_ratio 监控)

## Decision 7: 性能预算

**目标**: `redact_env_vars` <10µs/op (per AC-1 Case 6)
- 1000 nodes DSL 解析 = <10ms (可接受)
- 递归 nlohmann::json 遍历 + map insert ≈ 0.5µs/key × 20 keys = 10µs
- 实测 baseline 预估 (per decision 4 递归实现)

**测试**: `tests/test_redact_env_vars.cpp:Case 6` benchmark (1000 iter + std::chrono::high_resolution_clock)

## Decision 9: 风险回滚

**风险 1**: 默认 `{"*"}` → secrets 默认通过
**回滚**: Sprint 28+ 改默认 deny (per decision 6 迁移路径)

**风险 2**: `redact_env_vars` 性能影响 node_factory 解析
**回滚**: Cache redact 结果 (per node_id + env_vars hash), 仅首次调用 redaction

**风险 3**: `env_vars_audit` event 频率过高 (每次解析 emit)
**回滚**: Throttle per 5 秒 batch (借鉴 `event_log capture_mode_downgrade` v1.7 amendment 模式)

## Implementation Timeline (1-2 day)

**Day 1 morning** (4h):
1. `include/agenticdsl/contract/redact_env_vars.h` 公开 API 设计
2. `src/common/security/redact_env_vars.cpp` 实现 (recursive + 整字段替换)
3. `src/modules/parser/node_factory.cpp:41-47` 调 `redact_env_vars` 集成
4. `src/common/hooks/env_validation_hook.cpp:51-56` emit `env_vars_audit` event

**Day 1 afternoon** (4h):
5. `tests/test_redact_env_vars.cpp` 6 cases / 18+ assertions
6. `tests/CMakeLists.txt` add_catch_test 注册新 binary
7. `tests/test_dsl_extensions.cpp:410-509` fixture 改造 (AC-5)
8. 本地 ctest `-L security` 全 PASS 验证

**Day 2 morning** (2h):
9. `docs/specs/env-redaction.md` 契约文档
10. `docs/adr/adr-0068-event-emission-contract.md` Appendix A v2.5 amendment
11. `docs/adr/adr-0075-env-backend-multi-execution.md` §不变量 4 扩展

**Day 2 afternoon** (2h):
12. Oracle SHIP-with-fixes review (per AGENTS.md Pattern #4)
13. 修正 Oracle 评审发现 + ship-with-fixes 修正
14. ctest `-LE must_realllm` 全量验证 + drop_ratio 确认

**Total**: 12h (1.5 day) effort, 9 files / +335/-5 lines

## 关联文档

- `.rddf/improvements/adr-0072-d4-env-vars-redaction.md` (源提案)
- `openspec/changes/archive/2026-09-24-pdk-chat-demo-evolution-reference-example/specs/pdk-chat-demo-evolution/spec.md` (L2 spec R13.4 redaction 借鉴模式)
- `examples/pdk_chat_demo_evolution/evolution_session.cpp:582-602` (`redact_trace_fields` 实现参考)
- `src/modules/parser/node_factory.cpp:37-47` (现有解析代码, 修改点)
- `src/common/hooks/env_validation_hook.cpp:51-56` (现有白名单检查, 修改点)

Per AGENTS.md Pattern #8 (OpenSpec pre-impl dual-agent review) + Pattern #4 (SHIP-with-fixes flow) + Day-5 5-file integrity lesson.