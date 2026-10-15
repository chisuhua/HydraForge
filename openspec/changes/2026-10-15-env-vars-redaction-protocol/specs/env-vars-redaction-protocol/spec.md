# Spec: ADR-0072 D4 env_vars 字段 secrets redaction 协议

> **关联**: proposal.md + design.md + tasks.md (3 文档 5-file integrity per Day-5 lesson)
> **架构依据**: ADR-0068 §3.1 (api_key redaction 借鉴) + ADR-0072 D4 (env_vars 字段定义) + ADR-0075 EnvBackend (env_vars 透传)

## ADDED Requirements

### Requirement: env_vars 字段 secrets 整字段替换为 `[REDACTED]`

`redact_env_vars` helper 必须对 env_vars 字段值进行整字段替换, 当 keys 不在 `allowed_env_vars` 白名单时。

#### Scenario: 白名单 keys 保留 + 非白名单 keys 替换

- **GIVEN** 用户 DSL `tool_call` 节点含 `env_vars: {PATH: /usr/bin, API_TOKEN: secret_abc123}`
- **AND** `allowed_env_vars = {"PATH", "HOME", "USER", "LANG"}` (默认白名单)
- **WHEN** `node_factory.cpp` 解析节点调 `redact_env_vars(ctx.metadata)`
- **THEN** `ctx.metadata["env_vars"]` 必须等于 `{"PATH": "/usr/bin", "API_TOKEN": "[REDACTED]"}`
- **AND** SHELL API 仅对非白名单 keys 整字段替换, 保留 keys 列表完整

#### Scenario: 默认 `{"*"}` 行为 backward-compat

- **GIVEN** `allowed_env_vars = {"*"}` (默认配置, 全部允许)
- **AND** DSL 节点含 `env_vars: {API_TOKEN: secret_abc123, DB_PASSWORD: pwd}`
- **WHEN** `redact_env_vars` 被调
- **THEN** 所有 keys value 不替换 (backward-compat, 现有 12 fixture 零行为变化)
- **AND** SHELL API 默认 `{"*"}` 行为需文档化安全建议

#### Scenario: 嵌套值递归 redaction

- **GIVEN** DSL 节点含 `env_vars: {DB: {password: "secret_nested", host: "localhost"}}`
- **AND** `allowed_env_vars = {}` (默认 deny, 全部 redact)
- **WHEN** `redact_env_vars` 被调
- **THEN** 递归遍历嵌套 object/array
- **AND** `result["DB"]["password"] == "[REDACTED]"` + `result["DB"]["host"] == "[REDACTED]"`
- **AND** SHELL API 防御深度 secret (e.g. `env_vars.DB.value.password`)

#### Scenario: 混合 keys 选择性 redaction

- **GIVEN** DSL 节点含 `env_vars: {PATH: /usr/bin, API_TOKEN: secret_abc123, DB_PASSWORD: pwd}`
- **AND** `allowed_env_vars = {"PATH"}` (显式白名单仅 PATH)
- **WHEN** `redact_env_vars` 被调
- **THEN** `result["PATH"] == "/usr/bin"` (白名单保留)
- **AND** `result["API_TOKEN"] == "[REDACTED]"` (非白名单替换)
- **AND** `result["DB_PASSWORD"] == "[REDACTED]"` (非白名单替换)
- **AND** SHELL API keys 列表完整 (PATH / API_TOKEN / DB_PASSWORD 均存在)

### Requirement: env_vars_audit event emit (per ADR-0068 Appendix A v2.5)

每次 `redact_env_vars` 调用必须 emit `env_vars_audit` event, 含 keys 列表不含 values。

#### Scenario: env_vars_audit event 含 keys 不含 values

- **GIVEN** `env_validation_hook.cpp:51-56` 现有白名单检查通过
- **WHEN** `redact_env_vars` 完成
- **THEN** emit `env_vars_audit` event via IInteractionBus
- **AND** payload 必须含 `keys: list<string>` (env_vars 所有 keys, redact 后)
- **AND** payload **MUST NOT** 含 `values: list<string>` (per ADR-0068 §3.1 api_key redaction 协议)
- **AND** payload 含 `node_id: string` (per InMemoryBus capture 验证)
- **AND** SHELL API event emit 频率需 throttle (per design decision 9 风险回滚)

### Requirement: 性能预算 <10µs/op

`redact_env_vars` 必须 <10µs/op 平均耗时 (per AC-1 Case 6 performance benchmark)。

#### Scenario: 1000 iter × <10µs/op

- **GIVEN** `redact_env_vars` 在 1000 nodes DSL 解析路径
- **WHEN** 1000 iter × 20 keys 平均 (per AC-1 Case 6)
- **THEN** 平均耗时 <10µs/op
- **AND** SHELL API 1000 nodes DSL 解析总耗时 <10ms (可接受)
- **AND** benchmark via `std::chrono::high_resolution_clock`

### Requirement: backward-compat + 渐进迁移 (decision 6)

`redact_env_vars` 默认 `allowed_env_vars = {"*"}` 行为 backward-compat, 现有 12 fixture 零行为变化。

#### Scenario: 现有 12 fixture 零行为变化

- **GIVEN** 现有 `tests/test_dsl_extensions.cpp:410-509` 12 fixture 含假 secret (`secret_abc123` 等)
- **AND** 默认 `allowed_env_vars = {"*"}` (本 change 不改默认值)
- **WHEN** `redact_env_vars` 默认配置调用
- **THEN** 所有 12 fixture 行为不变
- **AND** SHELL API 12 fixture 改造需 explicit `allowed_env_vars` 配置 (Sprint 27+)

#### Scenario: explicit 白名单配置启用 (Sprint 27+)

- **GIVEN** Sprint 27+ 用户配置 `allowed_env_vars = {"PATH", "HOME", "USER", "LANG"}` (per ADR-0075 backend policy)
- **WHEN** `redact_env_vars` 显式配置调用
- **THEN** 非白名单 keys 整字段替换为 `[REDACTED]`
- **AND** audit event 含 redact 后 keys 列表
- **AND** SHELL API 文档化迁移指南 (per docs/specs/env-redaction.md §迁移指南)

## MODIFIED Requirements

### Requirement: ADR-0068 Appendix A v2.5 — env_vars_audit 主题注册

> **MODIFIED**: 现有 ADR-0068 Appendix A v2.4 扩展为 v2.5, 新增 1 主题 `env_vars_audit` (owner=`env_validation_hook`, payload `keys: list<string>` + `node_id: string`)

#### Scenario: env_vars_audit 主题注册 + 发射

- **GIVEN** ADR-0068 Appendix A 当前 v2.4 (per 2026-09-23 pdk-chat-demo-evolution-reference-example ship)
- **WHEN** 本 change ship
- **THEN** Appendix A v2.5 amendment 新增 `env_vars_audit` 主题
- **AND** v2.5 amendment 状态同步: owner=`env_validation_hook` + payload schema + 频率约束
- **AND** SHELL API 现有 4 主题 (v2.4: mutation_metric_rejected / turn_input_network_keyword_rejected / hidden_context_accepted_info / hint_containment_rejected) 保持不变

### Requirement: ADR-0075 §不变量 4 — env_vars 仅透传白名单

> **MODIFIED**: 现有 ADR-0075 §不变量 4 (env_vars 仅透传 `ExecOptions.env` 显式声明, 不继承 parent env) 扩展, 引用 `redact_env_vars` 协议

#### Scenario: env_vars 透传 + redact 双约束

- **GIVEN** ADR-0075 §不变量 4 现有 env_vars 透传约束
- **WHEN** 本 change ship
- **THEN** §不变量 4 扩展为 env_vars 透传 + redact 双约束
- **AND** 引用 `docs/specs/env-redaction.md` 协议文档
- **AND** 同步 commit msg 标注 §不变量 4 扩展

## REMOVED Requirements

无。

## 5-维 ship 判定 (per 2026-10-14 audit methodology)

实施 ship 后, 提案 `.rddf/improvements/adr-0072-d4-env-vars-redaction.md` Status 段更新:
- 维度 1 self-declared: OPEN → 待 ship
- 维度 2 archive: ✅ `openspec/changes/archive/2026-10-15-env-vars-redaction-protocol/`
- 维度 3 commit: ✅ ship commit msg 含 [Reverse Indicator] 5 字段
- 维度 4 ADR: ADR-0068 Appendix A v2.5 + ADR-0075 §不变量 4 扩展
- 维度 5 fallback: tests/test_redact_env_vars 6 cases PASS + ctest -LE must_realllm 267 → 267+

Per AGENTS.md Reverse Indicator Rule 5 字段格式 + Day-5 5-file integrity lesson.