# ADR-0072 D4 env_vars 字段 secrets redaction 协议

> **来源**: dual-agent 调研 (2026-10-14) — Metis 意图 D 隐藏缺口识别 + 提案 `.rddf/improvements/adr-0072-d4-env-vars-redaction.md`
> **关联**: ADR-0068 §3.1 api_key redaction 协议 + ADR-0072 D4 env_vars 字段 + ADR-0075 EnvBackend
> **生成**: 2026-10-15 via rdd-arch → rdd-planner → rdd-builder P1 (5-segment format)

## Why

### 当前 ship 行为 (无 redaction 协议)

```yaml
# DSL 节点 (test fixture test_dsl_extensions.cpp:410-509)
tool_call:
  tool: shell/exec
  env_vars:
    API_TOKEN: secret_abc123  # secrets in node metadata
  backend: local
```

```cpp
// src/modules/parser/node_factory.cpp:41-47 (per 早 session ship)
if (json.contains("env")) {
  ctx.metadata["env_vars"] = json["env"];  // 原样存储
}
if (json.contains("env_vars")) {
  ctx.metadata["env_vars"] = json["env_vars"];  // 原样存储
}
```

### 风险场景 (per Metis 真实分析)

1. **CI 日志泄漏**: 测试失败输出 metadata, secrets 进 stderr → 持久化到 CI log
2. **Audit 工具读取 trace**: 任何 `Tracer` / `event_log_query_perf` 工具读 trace 都暴露 secrets
3. **PG-13 context 写盘**: event_log 是 JSONL, secrets 永久写入磁盘
4. **跨进程 SKILL 调用**: `SkillInterpreter` IPC 传递 metadata 时 secrets 可能 leak
5. **未来真 LLM 测试**: 现在测试 fixture 用假 secret (`secret_abc123`) 可接受, **但未来真 token 测试会泄漏 CI 日志**

### ADR-0068 §3.1 协议 (借鉴目标)

> "api_key 不入日志, ToolResult::meta JSON 只填 env name (`api_key_env = "DEEPSEEK_API_KEY"`), 不填 key 值"

`env_vars` 字段**未在协议覆盖范围内**, 是 R13.4 之外的另一个 redaction 缺口。

## What Changes

**新增** 5 个 production 文件 + 1 个 test binary:

1. **`include/agenticdsl/contract/redact_env_vars.h`** (新文件, ~30 lines): `redact_env_vars(json metadata) -> json redacted` 公开 helper, 借鉴 `redact_trace_fields` (per evolution_session.cpp:582-602) 的 provider-agnostic 整字段替换模式

2. **`src/common/security/redact_env_vars.cpp`** (新文件, ~40 lines): implementation, **白名单模式** (per ADR-0075 backend policy) — `allowed_env_vars` 白名单 + 其他 keys 整字段替换为 `"[REDACTED]"`

3. **`src/modules/parser/node_factory.cpp:41-47`** (修改, ~5 lines): 解析 `env:` / `env_vars:` 字段后调 `redact_env_vars(ctx.metadata)` 防止 secrets 进 metadata

4. **`src/common/hooks/env_validation_hook.cpp:51-56`** (修改, ~10 lines): 现有 `allowed_env_vars` 检查通过后, 输出 `env_vars_audit` event (per ADR-0068 Appendix A v2.5 新主题, 详见 spec R1) 含 keys 列表不含 values

5. **`docs/specs/env-redaction.md`** (新文件, ~80 lines): env_vars redaction 协议契约文档 + `allowed_env_vars` 白名单配置 + audit event schema

6. **`tests/test_redact_env_vars.cpp`** (新测试 binary, ~150 lines): 6 cases — Case 1 happy path (白名单 keys 保留) + Case 2 非白名单 keys 整字段替换 + Case 3 混合 keys (白名单 + 非白名单) + Case 4 嵌套值 (object/array) + Case 5 ADR-0068 audit event 发射 + Case 6 性能 benchmark (<10µs/op)

## Acceptance (per AGENTS.md Reverse Indicator Rule)

**AC-1**: `tests/test_redact_env_vars.cpp` 6 cases / 18+ assertions 全 PASS, 含 Case 1-5 functional + Case 6 performance <10µs/op

**AC-2**: `src/modules/parser/node_factory.cpp:41-47` 调用 `redact_env_vars` 后, `ctx.metadata["env_vars"]` 只含白名单 keys, 其他 keys 替换为 `"[REDACTED]"`

**AC-3**: `src/common/hooks/env_validation_hook.cpp:51-56` 输出 `env_vars_audit` event 含 keys 列表不含 values, 验证通过 `InMemoryBus` capture

**AC-4**: `docs/specs/env-redaction.md` 契约文档完整, 含:
  - §白名单机制 (per ADR-0075 backend policy)
  - §audit event schema (per ADR-0068 Appendix A v2.5)
  - §迁移指南 (现有 12 fixture 改造步骤)

**AC-5**: `tests/test_dsl_extensions.cpp:410-509` 现有 fixture 改造 — `secret_abc123` fixture 验证为 `[REDACTED]`, 不在 stderr / trace JSONL 输出

**AC-6**: `ctest --test-dir build -L security -j1` 全 PASS (新增 `test_redact_env_vars` binary + 既有 security tests 零回归)

**AC-7**: `docs/adr/adr-0068-event-emission-contract.md` Appendix A v2.5 amendment — 新增 1 主题 `env_vars_audit` (per AGENTS.md Reverse Indicator Rule 同步)

**AC-8**: `docs/adr/adr-0075-env-backend-multi-execution.md` §不变量 4 扩展 — 引用 `redact_env_vars` 协议 (env_vars 仅透传白名单, 不入日志/trace)

**AC-9**: `openspec validate` exit 0 (per Day-5 5-file integrity lesson)

**AC-10**: baseline 零回归 — `ctest --test-dir build -LE must_realllm` 267 → 267+ PASS, drop_ratio=0%

## Capabilities

**新增 capability** (per spec-driven schema):
- `secrets-redaction`: env_vars 字段 secrets 整字段替换为 `"[REDACTED]"`, 与 api_key redaction 同类

## Impact

**直接影响**:
- `src/modules/parser/node_factory.cpp` (+5 lines, 调 `redact_env_vars`)
- `src/common/hooks/env_validation_hook.cpp` (+10 lines, emit `env_vars_audit` event)
- `include/agenticdsl/contract/` (新文件, +30 lines)
- `src/common/security/` (新文件, +40 lines)
- `tests/test_redact_env_vars.cpp` (新 binary, +150 lines)
- `docs/specs/env-redaction.md` (新文件, +80 lines)
- `docs/adr/adr-0068-event-emission-contract.md` Appendix A v2.5 amendment (~10 lines)
- `docs/adr/adr-0075-env-backend-multi-execution.md` §不变量 4 扩展 (~5 lines)
- `tests/test_dsl_extensions.cpp` 既有 fixture 改造 (~5 lines)

**总计**: 9 files, +335 lines / -5 lines, 1-2 day effort (per proposal 估算)

**Security impact**:
- ✅ CI 日志 secrets 泄漏防护
- ✅ Audit 工具读 trace 安全
- ✅ PG-13 context 写盘安全
- ✅ 跨进程 SKILL IPC 安全
- ✅ 未来真 LLM 测试 (用真 token) 防护

**Compatibility impact**:
- ⚠️ 现有 12 fixture 测试可能需要改造 (per AC-5) — 仅 `secret_abc123` 等假 secret fixture
- ✅ 公开 API 零变化 (`redact_env_vars` 是新公开 helper, 不替换既有 API)
- ✅ backward-compatible (默认 `allowed_env_vars = {"*"}` 行为不变, 白名单需 explicit 配置)

## Why Not [Alternative]

**Alternative A**: 在 `ToolCoordinator::execute` 入口 redaction
- ❌ 太晚 — metadata 已在 `node_factory.cpp` 写入 + 已传 `tool_metadata_` + 已 emit event
- ❌ 多处重复 (每个 emit point 都要 redaction)

**Alternative B**: 让用户在 DSL 显式声明 `redact: true`
- ❌ 增加 DSL 复杂度
- ❌ 用户容易忘记声明
- ❌ 与 ADR-0068 §3.1 精神不符 (默认 redaction)

**Alternative C**: 仅 redaction `value` 不替换 keys
- ❌ keys 也可能泄漏 (e.g. `DATABASE_PASSWORD` key 名本身敏感)
- ❌ 借鉴 `redact_trace_fields` 是整字段替换模式

**Chosen**: helper 在 `node_factory.cpp:41-47` 解析时立即 redaction (早 + 一次性), 与 `redact_trace_fields` 同模式 (per evolution_session.cpp:582-602 LLM-independent 整字段替换)。

## Pre-Issues (must resolve before P0 case 1 approve)

per `pre_impl_dual_agent_review` metadata 标记 5 个 pre-issue, per Metis + Oracle 早期共识:

1. **Case 1 happy path 测试**: 模拟白名单 keys (`PATH`, `HOME`, `USER`, `LANG`) 保留, 非白名单 keys (`API_TOKEN`, `DB_PASSWORD`) 替换为 `[REDACTED]`
2. **Case 2 nested values**: `env_vars.API_TOKEN.value = nested_object` 嵌套场景 redaction
3. **Case 3 mixed**: 白名单 + 非白名单 keys 共存, 验证选择性 redaction
4. **Case 4 audit event 发射**: `env_vars_audit` event 含 keys 列表不含 values (per ADR-0068 §3.1 协议)
5. **Case 5 backward-compat**: `allowed_env_vars = {"*"}` 默认行为不变, 既有 12 fixture 零行为变化

**Performance 考虑**: redact_env_vars 必须 <10µs/op (per AC-1 Case 6), 否则影响 node_factory.cpp 解析性能 (1000 nodes DSL 解析 = <10ms)

## 相关 subagent sessions

`ses_ec5f36ffaffeT3KEsoFqAUPq43` (Metis 2026-10-14 dual-agent 调研, 意图 D 隐藏缺口识别)

Per AGENTS.md Reverse Indicator Rule 5 字段格式 + rdd-builder P1 5-segment 格式 + 早 session 2026-10-15 R13.4 proposal 模板 + Day-5 5-file integrity lesson.