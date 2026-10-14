# adr-0072-d4-env-vars-redaction

**优先级**: 🟠 P1 | **来源**: dual-agent 调研 (2026-10-14) — Metis 意图 D 隐藏缺口
**阶段**: post-6c | **分类**: execution-security
**类型**: feature
**主题**: env_vars 字段 secrets redaction 协议 (与 ADR-0068 §3.1 api_key 同类)

## 架构依据

per 2026-10-14 dual-agent 调研 (`ses_ec5f36ffaffeT3KEsoFqAUPq43`):
- **`backend-parser` 已 ship (2026-09-03)**: `node_factory.cpp:37-47` 解析 `env:` / `env_vars:` 字段
- **测试有 secrets fixture**: `API_TOKEN: secret_abc123` (`test_dsl_extensions.cpp:410-509`)
- **ADR-0068 §3.1 协议**: "api_key 不入日志, ToolResult::meta JSON 只填 env name (api_key_env), 不填 key 值"
- **缺口**: `env_vars` 字段**未在协议覆盖范围内** — 未来 event_log / trace 写入会泄漏 secrets
- **Metis 真实风险**: 测试 fixture 用了假 secret 可接受, 但未来真测试若用真实 token 会泄漏 CI 日志

## Why

### 当前 ship 行为 (无 redaction)

```yaml
# DSL 节点
tool_call:
  tool: shell/exec
  env_vars:
    API_TOKEN: secret_abc123  # secrets in node metadata
  backend: local
```

```cpp
// node_factory.cpp:37-47
ctx.metadata["env_vars"] = {"API_TOKEN": "secret_abc123"};  // 原样存储
```

```cpp
// 未来 (per ADR-0068 留 Sprint 27+):
// event_log.write(trace_event{
//   ...,
//   meta: ctx.metadata,  // 包含 secret_abc123
//   ...
// });
```

### 风险场景

1. **CI 日志泄漏**: 测试失败输出 metadata, secrets 进 stderr
2. **Audit 工具读取 trace**: 任何 `Tracer` 或 `event_log_query_perf` 工具读 trace 都暴露 secrets
3. **PG-13 context 写盘**: event_log 是 JSONL, secrets 永久写入磁盘
4. **跨进程 SKILL 调用**: `SkillInterpreter` IPC 传递 metadata 时 secrets 可能 leak

## What Changes (proposal scope)

- **修复 1**: `node_factory.cpp:37-47` env_vars 解析时, 标记 secrets key 集合 (e.g. `*_TOKEN`, `*_KEY`, `*_SECRET`, `*_PASSWORD`, `API_KEY`)
- **修复 2**: 新增 `metadata["env_vars_redacted"]` 字段, 值是 secrets keys 列表 (e.g. `["API_TOKEN"]`), 不含值
- **修复 3**: `Tracer::record_phase` 等 event 路径遇到 env_vars 时, 自动替换为 `env_vars_redacted` 字段
- **修复 4**: `ToolResult::meta` 类似 api_key_env 协议, 改用 env name 列表
- **修复 5**: regression test 覆盖 secrets 在 event_log / trace / IPC 路径不泄漏

## Out of Scope

- 工具内部 secrets 处理 (per 工具 own 协议, e.g. `pdk/provider_agent/`)
- 用户级 encryption (e.g. `secrets: encrypted_env_file: path`)
- api_key 字段本身 (已在 ADR-0068 §3.1 ship)

## Dependencies

- **前置 ship**: `adr-0072-d4-backend-parser` 已 ship (2026-09-03)
- **ADR-0068 Appendix A amendment**: 新增 redaction 协议主题 (或现有 `redacted` 主题)
- **`Tracer` / `event_log` API**: 需统一 redaction 钩子

## Acceptance Criteria (建议)

- AC-1: `node_factory.cpp` env_vars 解析时标记 secrets keys (regex `.*_TOKEN|.*_KEY|.*_SECRET|.*_PASSWORD|API_KEY`)
- AC-2: `metadata["env_vars_redacted"]` 字段包含 secrets keys 列表 (无值)
- AC-3: `Tracer::record_phase` 写入 trace 时, env_vars 自动替换为 env_vars_redacted
- AC-4: `ToolResult::meta` env_vars 字段同样 redaction
- AC-5: regression test 验证 secrets 不出现在 trace / event_log / IPC 输出 (用 grep -r 验证)
- AC-6: 既有 `test_dsl_extensions.cpp` W5 测试零回归 (不破坏 API)

## Estimated Effort

- 修复 1-2: 2-4h (parse + metadata 标记)
- 修复 3-4: 4-6h (Tracer + event_log 集成)
- 修复 5-6: 2-4h (regression test)
- 总: 1-2 天

## Reference

- ADR-0068 §3.1: api_key 协议模板
- `node_factory.cpp:37-47`: 当前 env_vars 原样存储
- `test_dsl_extensions.cpp:410-509`: W5 测试 (含 API_TOKEN fixture)
- Dual-agent 调研: `.rddf/audits/2026-10-14-p0-proposals-dual-agent-review.md`

## Status

📋 **登记但未立项** (per 2026-10-14 dual-agent 调研, Metis 意图 D 隐藏缺口发现). 待 ADR-0068 后续 amendment ship 或独立 OpenSpec change 立项实施.
