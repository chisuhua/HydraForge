# Tasks: Phase 5 Batching Schema (C15 — 精简版)

> **STATUS: ACTIVE** 🟡 (精简版 — 按 Adversarial Review D2 决策，BatchingQueue 接口推迟)
> **关联 proposal**: `proposal.md`
> **关联 spec**: `specs/batching-queue-plugin/spec.md`
> **关联 master plan**: `docs/superpowers/plans/2026-07-03-phase5-self-bootstrapping.md` §五
> **关联 decision**: `docs/adversarial-reviews/decisions-2026-07-07.md` D2
> **前置依赖**: 无
> **后续依赖**: BatchingQueue 接口推迟到第二个推理后端出现时
> **估时**: ~2 小时
> **最后更新**: 2026-07-07

---

## 1. lib/inference/batching.md schema

- [ ] 1.1 创建 `lib/inference/batching.md` (~40 行)
  - 顶部 `> ⚠️ PLACEHOLDER` 标记：`实现在 Phase 5 Stage 2+`
  - YAML signature: `(prompt: string, timeout_ms: int) -> (request_id: int, result: string)`
  - tool_call 节点：`batching.submit_and_wait` 工具
  - 参数：prompt (string), timeout_ms (默认 30000)
  - 说明：实际 batching 实现在 engine plugin 内部，BatchingQueue 接口推迟

## 2. 文档同步

- [ ] 2.1 更新 `docs/active-status.md` 活跃变更看板
  - Phase 5 Stage 1 进度更新（batching.md schema ship）
- [ ] 2.2 更新 `docs/superpowers/plans/2026-07-03-phase5-self-bootstrapping.md`
  - §五 标记 C15 为 schema-only ship
  - §十一.2 Adjustment Log: C15 精简记录

## 3. 验证

- [ ] 3.1 `lib/inference/batching.md` 存在并包含 PLACEHOLDER 标记
- [ ] 3.2 `python3 tools/adr_lint.py` exit 0
- [ ] 3.3 `python3 tools/docs_drift_audit.py` 0 DRIFT
- [ ] 3.4 `openspec validate phase5-batching-queue-plugin` exit 0

## 4. 提交与归档

- [ ] 4.1 Git 提交: `feat(phase5-stdlib): add batching.md schema placeholder`
- [ ] 4.2 `openspec archive phase5-batching-queue-plugin`

---

## 验证检查清单 (C15 ship gate)

- [ ] 1. `lib/inference/batching.md` schema ship（40 行 + PLACEHOLDER 标记）
- [ ] 2. docs_drift_audit 0 DRIFT
- [ ] 3. adr_lint exit 0
- [ ] 4. openspec validate exit 0
- [ ] 5. roadmap/master plan 文档同步

## 关联 change 状态

- ✅ C9 (ADR impl-scope audit) — archived 2026-07-03
- ✅ C10 (Lazy ModuleState) — archived 2026-07-03
- ✅ C11 (SessionRegistry) — archived 2026-07-04
- ✅ C12 (YIELD/STREAM) — archived 2026-07-04
- 🟡 C13 (B2 Architecture Schemas) — ACTIVE
- 🟡 C14 (Llama Engine Plugin) — ACTIVE
- 🟡 **C15 (Batching Schema)** — ACTIVE (schema-only, 按 D2 精简)
