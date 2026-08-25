# from-roadmap-phase-6c-evidence-gate Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use skill_use("execute") to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.
>
> **⚠️ SCOPE-OPEN**: 此 plan 是 MOMUS REJECT 后的第二次骨架,scope 决策 (X/Y/Z/W) 锁定后 fill Task 4-7。具体见 `docs/audits/2026-08-25-evidence-gate-momus-decision-summary.md`。

**Goal:** 通过 evaluate_gate() 纯函数 + 决议文档模板 + 合规门禁 ship,产出 ADR-0074 §决策 D5 实证字段新增。**实际 Go/No-Go 决议数据消费 scope 由 user 决策 Task 4 决定**。

**Architecture:**
- **代码层**: `src/common/prompts/evidence_gate.h` header-only 纯函数(无 IO,无全局 state),4 状态枚举,4 阈值输入 → 决策输出
- **测试层**: `tests/test_evidence_gate.cpp` Catch2 ≥5 case(4 boundary parse-valid + 1 Abort 数据完整性)
- **文档层**: `docs/audits/<date>-evidence-gate-v1.md.template` 5 章节 skeleton(YAML 占位块)
- **同步层**: `docs/active-status.md` §一 ADR-0074 状态行(无变更)+ §四 Phase 7 启动条件项 #1 翻牌 (Task 4 决议后)
- **合规层**: adr_lint + docs_drift_audit + openspec validate(8.4-8.6,与 Wave 1 baseline §6 同模式)

**Tech Stack:**
- C++20 header-only
- Catch2 (test framework, ≥5 case ≥20 assertions)
- 无新依赖 (yaml-cpp / Catch2 已 vendor)
- Markdown report(YAML 占位嵌块格式)

---

## File Structure

### Production Code (新增)

| File | Responsibility |
|------|----------------|
| `src/common/prompts/evidence_gate.h` | Header-only: `enum class GateStatus` + `evaluate_gate(double, double, double, double) -> GateStatus` |
| `docs/audits/<date>-evidence-gate-v1.md.template` | 决议文档 5 章节 skeleton + YAML 占位块 |

### Tests (新增)

| File | Responsibility |
|------|----------------|
| `tests/test_evidence_gate.cpp` | Catch2 ≥5 case: 4 boundary + 1 Abort |

---

### Task 1: evaluate_gate 函数实现 (TDD 5 步)

- [ ] **1.1 Write failing test** — `tests/test_evidence_gate.cpp` 创建,5 case
- [ ] **1.2 Verify failure** — `ctest -R test_evidence_gate --output-on-failure` 显示 5 case 全部 FAIL (linker error)
- [ ] **1.3 Implement** — 创建 `evidence_gate.h`: enum class + 4 参函数 + D-3 数据完整性 (parse_valid sentinel) + D-4 左闭右开
- [ ] **1.4 Verify pass** — 5 case 全 PASS
- [ ] **1.5 Commit** — `feat(prompts): add evaluate_gate pure function (5 boundary cases)`

---

### Task 2: Abort data integrity test case

- [ ] **2.1 Write failing test** — Abort case
- [ ] **2.2 Verify failure** (skip if Task 1.3 覆盖)
- [ ] **2.3 Implement** (Task 1.3 已含)
- [ ] **2.4 Verify pass** — 5 case 全 PASS
- [ ] **2.5 Commit amend**

---

### Task 3: Decision document skeleton template

- [ ] **3.1 Static create** — `docs/audits/<date>-evidence-gate-v1.md.template` 5 章节 + YAML 占位
- [ ] **3.2 Verify** — grep 章节 + 占位存在
- [ ] **3.3 Commit** — `docs(change): add evidence-gate decision doc template skeleton`

---

### Task 4: ⚠️ SCOPE-OPEN — 等待用户 X/Y/Z/W 决策后 fill

| 选项 | Task 4 实施内容 |
|---|---|
| **X: Ship 模板 + evaluate_gate;真实决议 Sprint 25+** | Task 4 = no-op;跳至 Task 5 |
| **Y: 真实 3 模型测量纳本 change** | 实施 GPT-4 / Claude / DeepSeek × 51 × 3 models,measure + evaluate_gate + 决议 |
| **Z: 默认 ABORT,诚实记录 mock data 限制** | Verdict = Abort + §决议 含 "数据局限说明" |
| **W: 暂停 evidence-gate,改 Sprint 24 W1** | 整个 change archive 为 deferred |

---

### Task 5-7: Sync (X 路线跳过 / Y/Z 实施)

- Task 5: active-status.md §四 Phase 7 启动条件项 #1 翻牌
- Task 6: ADR-0074 §决策 D5 实证字段追加
- Task 7: adr_lint + docs_drift_audit + openspec validate 强制 gate

---

### Task 8: Commit + merge

- [ ] **8.1** Atomic commits (3-5)
- [ ] **8.2** PR 创建 (`gh pr create`)

---

### Task 9: Archive

- [ ] **9.1** `openspec archive from-roadmap-phase-6c-evidence-gate --yes`

---

## 必须 DO

1. TDD 5 步 (test fail → impl → test pass → commit)
2. header-only (无 IO, 无全局 state)
3. file:line cite
4. atomic commits
5. **scope 不偷换** — Task 4 需 user 决策

## 必须 NOT DO

1. 不实施决议数据消费 (MUST-FIX 2 blocking)
2. 不改 proposal.md / design.md scope-dependent sections (Locked-Wait)
3. 不翻牌 ADR-0074 状态 (已 Approved)
4. 不引入新依赖

## 关联文档

- **MOMUS 评审**: `docs/audits/2026-08-25-evidence-gate-momus-decision-summary.md`
- **前置 baseline 数据**: `docs/audits/2026-08-18-execution-baseline-v1.md` (mock mode)
- **父 ADR**: `docs/adr/adr-0074-prompt-evidence-gate.md` (✅ Approved)
- **Wave 1 baseline plan template**: `.rddf/plans/from-roadmap-phase-6c-execution-baseline.md`

## 元数据

- 创建: 2026-08-25
- 估算: Task 1+2+3+7+8+9 = 2-3 天 (scope-independent); Task 4 dep = 0-3 周
