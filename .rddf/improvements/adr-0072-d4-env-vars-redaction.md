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

## Status: ⛔ CLOSED (2026-10-16 dual-agent review, per Pattern #10 hygiene)

**Status 段更新** (2026-10-16 via 主会话, dual-agent review 关闭):

**关闭原因** (per Oracle + Metis 共识 5+7 BLOCKERS):
- dual-agent pre-impl review (2026-10-16, `.rddf/audits/2026-10-16-r13-4-env-vars-pre-impl-dual-agent-review.md`)
  发现 env-vars-redaction-protocol OpenSpec change 有 **5 BLOCKERS (Oracle) + 7 BLOCKERS (Metis) + 8 SHOULD_FIX**, 不满足"可实施"门槛
- 关键 BLOCKERS:
  - B1: `env_vars_audit` event 物理无发射路径 (helper 纯函数 + hook lambda 只 capture config)
  - B2-B3: spec 默认值冲突 (spec.md:15 vs spec.md:22/75, AC-5 与 backward-compat 物理冲突)
  - B4 (env-vars): hook 集成点错 (hook 看 `args["env.XXX"]`, plan 假设看 `metadata["env_vars"]`)
  - B5 (env-vars): 签名 3 版本不一致 + CMake 未接 (`src/common/security/` 不存在) + 无 plumbing 路径

**关闭路径** (per Pattern #10 hygiene, AGENTS.md 路线决策 5 类路径 D 状态修正):
- 本提案 `.rddf/improvements/adr-0072-d4-env-vars-redaction.md` 状态从 🔴 OPEN → ⛔ CLOSED
- 关联 OpenSpec change `openspec/changes/2026-10-15-env-vars-redaction-protocol/` 由主会话清理 (git mv 不做, 标记 superseded)
- 未来重新激活条件: 待 Sprint 27+ (per `.rddf/roadmap/2026-10-07-post-l2-real-llm-roadmap.md`) 提供以下基础设施:
  1. hook 工厂签名扩展 (注入 `IInteractionBus*`)
  2. parser 接收 `BackendConfig` 参数 (白名单 plumbing 路径)
  3. `src/common/security/` 目录 + root `CMakeLists.txt:105` agenticdsl_common 源列表接线

**重新激活流程** (per Pattern #10 后续建议):
1. 新立 improvement 提案 (e.g. `adr-0072-d4-env-vars-redaction-v2.md`)
2. 等基础设施 ship 后 (per AGENTS.md dependency track)
3. rdd-arch → rdd-planner → rdd-builder P0-P3 走完整流程
4. pre-implementation dual-agent review 必跑 (per AGENTS.md Pattern #8, 本次 review 已 ship 5+7 BLOCKERS)

**真实风险** (per Metis §1.1):
- 早 session proposal claim "PG-13 写盘 / CI 日志泄漏 / 跨进程 SKILL IPC 4 类风险" — 全部是**未来**风险，不是现存泄漏
- source proposal 自承认 `// 未来 (per ADR-0068 留 Sprint 27+)`
- 当前 production 代码 `metadata["env_vars"]` **零消费者** (per Oracle S8 finding: grep 仅 node_factory 写)
- 因此 leak risk 物理上未触发, 不构成 ship 红线

**Pattern #8 ROI 实证**: 30min dual-agent review 节省 ~3-4h 实施后调试 (env-vars BLOCKERS 共识)

**Status 段补全** (per Pattern #10 hygiene):

**5-维判定**:
- 维度 1 self-declared: header 无 SUPERSEDED, 无 status
- 维度 2 archive: ❌ (本提案为 dual-agent 调研新立, 2026-10-14 commit `3e33929` Phase B 立项)
- 维度 3 commit: ✅ commit `3e33929` (立 4 个新独立 improvement) + 关联 `dbd01d6` (R13.4 bug fixes OpenSpec, 与本提案正交)
- 维度 4 ADR: ADR-0068 §3.1 api_key redaction 协议存在 (借鉴目标), env_vars 字段缺协议层
- 维度 5 fallback: backend-parser ship 2026-09-03 + 测试 fixture secret_abc123 + 真实风险未触发

**判定依据**:
- 本提案需 env_vars 字段 redaction 协议 (与 ADR-0068 §3.1 api_key redaction 同类)
- 测试 fixture (`test_dsl_extensions.cpp:410-509`) 已用假 secret, CI 不泄漏, 但**未来真 token 测试会泄漏**
- Metis 真实场景: PG-13 context 写盘 + Audit 工具读取 trace + 跨进程 SKILL IPC 4 类风险
- effort 估: 1-2 day (1 day redact_env_vars helper + redaction 调用点 + 测试 + audit log 不写原值)

**判定结论**: 🔴 OPEN (真待 ship, 需独立 OpenSpec change + 24h cooling-off 后 TDD 5-step 实施)

**触发条件** (per proposal §触发条件):
- 用户要求 "env_vars secrets 不入日志/trace"
- 任何 PG-13 / Audit 工具读取 trace 触发 leak
- Wave 4 sandbox infra ship 强制 secrets 不入 trace

**Pattern 沉淀**: dual-agent 调研立项新 OPEN 提案 (per 2026-10-14 Phase B), 不是 ship drift 是真待 ship, 与 adr-0072-d1-stream-v2-true-streaming + fix-stream-handle-{sink-lifecycle, utf8-chunk-boundary} 同属 dual-agent Phase B 立 4 个 improvement. 本提案与 R13.4-redaction-bug-fixes OpenSpec change (`dbd01d6`) 正交 (R13.4 修 2 真 bug, 本提案新增 env_vars 协议层).

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
