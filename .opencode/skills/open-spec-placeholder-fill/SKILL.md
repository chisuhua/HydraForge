---
name: open-spec-placeholder-fill
description: Convert a placeholder OpenSpec change into a full-fidelity change ready for /opsx-apply. Use when trigger condition (e.g., upstream change ship) is met for a placeholder change. Triggers: "fill in placeholder", "detailed化 placeholder", "占位 change 详细化", "准备启动 Sprint X change".
license: MIT
compatibility: Requires openspec CLI >= 1.4, parent skill master-plan-driven-changes (or similar planning context).
metadata:
  author: hydraforge
  version: "1.0"
  parent_skill: master-plan-driven-changes
  prerequisite: change marked STATUS: PLACEHOLDER in proposal.md
---

# OpenSpec Placeholder Fill

Convert a placeholder change (created by `master-plan-driven-changes`) into a **full-fidelity change** ready for `/opsx-apply`. The placeholder has `STATUS: PLACEHOLDER` in its `proposal.md` and minimal `tasks.md` / `specs/<capability>/spec.md`. This skill fills in all 4 artifacts with concrete content.

## When to Use

✅ **Use this skill when:**
- A placeholder change's trigger condition is met (e.g., upstream change shipped)
- User says "fill in placeholder for <change-name>"
- User says "detailed化 <change-name>"
- Master plan §9 Dependency Refresh Gate passed for this change

❌ **Don't use when:**
- Change is already full-fidelity (no STATUS: PLACEHOLDER in proposal.md)
- Trigger condition NOT met (wait for upstream change)
- No Master plan context (user wants ad-hoc single change — use `openspec-propose`)

---

## Input Schema

| Field | Required | Type | Description |
|-------|----------|------|-------------|
| `placeholder_change_path` | ✅ | string | Path like `openspec/changes/2026-06-26-adr-0030-v2-async-runtime/` |
| `upstream_change_outputs` | ⬜ | object | What upstream change(s) actually produced (vs assumption) |
| `oracle_consultation_results` | ⬜ | string[] | Decisions from prior Oracle consultations |
| `business_confirmations` | ⬜ | string[] | User confirmations on assumptions |
| `master_plan_path` | ⬜ | string | Path to Master plan (default: latest in docs/superpowers/plans/) |

---

## Output Schema

A **full-fidelity change** ready for `/opsx-apply`:

```
openspec/changes/<date>-<slug>/
  ├── .openspec.yaml      (unchanged)
  ├── proposal.md         (expanded: removed PLACEHOLDER, full What Changes, real Impact)
  ├── design.md           (NEW: 5 Decisions)
  ├── tasks.md            (expanded: TBD sections → concrete Day-grouped tasks)
  └── specs/<capability>/spec.md   (expanded: placeholder Requirements → concrete)
```

**Validation**: `openspec validate <change>` MUST exit 0.

**Master plan update**: §四 <change-id> row status: `⚪ placeholder` → `🟡 active`.

---

## Workflow (8 Steps)

### Step 1: 决策前置 (Decision Prerequisites)

Before writing any artifact, gather inputs:

#### 1.1 Read upstream change outputs (if any)

```bash
# For each upstream change in placeholder proposal's "依赖" section:
openspec show <upstream-change-name>

# Read their proposal.md to see what was actually built
# Compare against placeholder's assumptions in "What Changes" section
```

Output: List of `[ASSUMPTION_MATCH]` or `[ASSUMPTION_MISMATCH]` for each interface/behavior.

#### 1.2 Run Oracle consultation (if needed)

For any open questions in placeholder proposal:

```bash
# Use Oracle subagent for hard architecture decisions
task(subagent_type="oracle", prompt="...")
```

Example open questions (from placeholder proposals):
- ADR-0030 V2: "Is 双层架构 still needed, or std::jthread enough?"
- ADR-0031: "4 vs 6 virtual functions?"
- ADR-0033: "SubtaskSession vs lighter SubtaskContext?"

Record Oracle responses in `oracle_consultation_results` input.

#### 1.3 Get business confirmations (if needed)

For business decisions in placeholder proposal, ask user:

```
Examples:
- "Plan/Agent/YOLO 模式定义是否需要扩展?" (ADR-0031)
- "Fleet 模式 16 路并行业务场景是否真实?" (ADR-0030)
- "Layer × Tool Category 权限矩阵默认值?"
```

Record responses in `business_confirmations` input.

---

### Step 2: 写 design.md (Create Design with 5 Decisions)

Create `design.md` (does NOT exist in placeholder). Each Decision has 2-3 alternatives + chosen + rationale.

**Template**:

```markdown
# Design: <Change Name>

> **范围来源**: proposal.md Why section + 上游 change 实际产出
> **目标**: <one-line goal>

## Decision 1: <Topic>

**方案对比**:
- **A. <approach>**: <pros> / <cons>
- **B. <approach>**: <pros> / <cons>

**选择**: <方案 X>. 理由 — <rationale based on upstream reality + Oracle input>

**实施**: <concrete step-by-step>

## Decision 2: <Topic>
...

## Decision 3: <Topic>
...

## Decision 4: <Topic>
...

## Decision 5: <Topic>
...

## 实施顺序 (按依赖)

1. <Step 1> (Day 1-3)
2. <Step 2> (Day 4-5)
...

## 测试策略

- 单元测试: <scope>
- 集成测试: <scope>
- TSan/ASan: <verify commands>
```

**5 Decisions typical topics** (customize per change):
1. 架构方案选择 (Taskflow vs std::jthread / 4 vs 6 虚函数 / etc.)
2. 集成方式 (替换 vs 包装 / 立即重构 vs 渐进迁移)
3. 错误处理 / 异常策略
4. 兼容性策略 (breaking vs backward-compat)
5. 测试 + ship gate 策略

---

### Step 3: 写 tasks.md (Replace TBD with Concrete Tasks)

Replace each `[ ] TBD:` line with `[ ] <N.M.K>` concrete task with Day grouping.

**Template per section**:

```markdown
## <N>. <Section Title> (Sprint <X> Day <Y>-<Z>)

### <N.1> <subsection>

- [ ] <N.1.1> 编辑 `<file>:<line>`, <具体改动>
- [ ] <N.1.2> 添加 <新文件/方法/类>
- [ ] <N.1.3> 验证: `<command>` 期望输出
- [ ] <N.1.4> 提交: `git commit -m "<type>(<scope>): <message>"`
- [ ] <N.1.5> 更新 `<tasks-tracker>` 标 `[x]`
```

**Granularity rule**: Each task MUST be completable in 1-3 tool calls. If larger, split.

**Number of tasks**: Typically 40-80 per change (matching archive patterns like Sprint 7 follow-up's 142 tasks).

---

### Step 4: 写 specs/<capability>/spec.md (Replace Placeholder Requirements)

Replace each `### Requirement: <name> (PLACEHOLDER)` with concrete requirement:

**Template**:

```markdown
## ADDED Requirements

### Requirement: <name>

`<API/Component>` MUST <behavior>. <additional constraints>

#### Scenario: <scenario-name>

- **WHEN** <condition>
- **THEN** MUST <expected result>
- **AND** <additional constraint>

#### Scenario: <scenario-name-2>
...
```

**Common patterns**:
- File-level: `src/<module>/<file>.cpp` MUST ...
- API-level: `class <Name>` MUST ...
- Behavior-level: `<action>` MUST result in ...
- Performance: `<metric>` MUST be ≤ <threshold>

**Number of Scenarios per Requirement**: ≥ 1 (validation requirement), typically 2-4.

**Number of Requirements**: 5-8 per change (matching Sprint 7 follow-up's 10 requirements).

---

### Step 5: 移除 PLACEHOLDER 标记

In `proposal.md`:

```diff
- > **STATUS: PLACEHOLDER** ⚠️
- > **本 change 详细 design/spec/tasks 待 ... 后填充**
+ > **STATUS: ACTIVE** (filled <YYYY-MM-DD>)
+ > **Trigger condition met**: <evidence>
+ > **Filled from placeholder**: <date>
```

Replace `STATUS: PLACEHOLDER` → `STATUS: ACTIVE` in all 4 files.

---

### Step 6: openspec validate

```bash
openspec validate <change-name>

# Expected output: Change '<change-name>' is valid
# If errors: see "Common Validation Errors" below
```

**Common Validation Errors**:

| Error | Fix |
|-------|-----|
| `No delta sections found` | Change `## Requirements` to `## ADDED Requirements` in specs/*.md |
| `Each requirement MUST include at least one Scenario` | Add at least one `#### Scenario:` per Requirement |
| `Invalid Requirement name format` | Use lowercase + hyphens (e.g., `scheduler-fork-dedup`) |

---

### Step 7: 更新 Master plan 状态

Edit Master plan §四 <change-id> row:

```diff
- | **<C-ID>** | `<change-name>` | 占位 (...) | ⚪ **placeholder, 待 ... 完成后详细制定** |
+ | **<C-ID>** | `<change-name>` | 实施 (filled <date>) | 🟡 **active, 准备启动 Sprint <X>** |
```

If upstream change dependency was actually different than expected, also update §十一 Change Adjustment Log:

```markdown
| <YYYY-MM-DD> | <change-id> | <Oracle 上游 change 实际产出> | <调整点> | <status> |
```

---

### Step 8: 启动 Sprint 实施

Output ready signal:

```markdown
## ✅ Placeholder Fill Complete

**Change**: <change-name>
**Status**: 🟡 active, ready for implementation
**Artifacts**:
- proposal.md: <lines> (expanded from <previous>)
- design.md: <lines> (NEW)
- tasks.md: <lines> (expanded from <previous>)
- specs/<capability>/spec.md: <lines> (expanded from <previous>)

**Next Step**: Run `/opsx-apply <change-name>` to start implementation.

**Or invoke the implementation subagent**:
```bash
task(
  subagent_type="general",
  category="deep",
  prompt="Implement OpenSpec change <change-name>. Follow design.md 5 Decisions + tasks.md Day-grouped tasks. Run ctest after each Day. Archive when complete."
)
```
```

---

## Example: Filling ADR-0030 V2 Placeholder

**Trigger**: C1 (`2026-06-26-sprint-7-tech-debt-execution`) shipped (engine.cpp include ≤3 achieved).

**Workflow**:

1. Read C1 outputs: `openspec show 2026-06-26-sprint-7-tech-debt-execution` → get list of artifacts
2. Run Oracle consultation: "Is Taskflow + async_simple dual-layer still needed, given C1 simplified engine.cpp dependencies?"
3. Get business confirmation: "Is Fleet 16-way parallelism a real requirement?"
4. Create design.md with 5 Decisions:
   - D1: dual-layer vs std::jthread replacement
   - D2: parallel DAG executor design (Taskflow / custom executor)
   - D3: 16-way Fleet mode (real use case?)
   - D4: streaming yield (coroutine / IGenerationStream)
   - D5: testing strategy
5. Expand tasks.md: ~40 concrete Day-grouped tasks
6. Expand spec.md: 5-8 concrete Requirements with Scenarios
7. Remove PLACEHOLDER mark
8. `openspec validate` → exit 0
9. Update Master plan §四 C2 row + §十一 log entry
10. Output "ready for /opsx-apply"

---

## Integration Notes

**This skill is downstream of `master-plan-driven-changes`**. The parent skill creates the placeholders; this skill fills them.

**Sequential dependencies** (cannot parallel):
1. Master plan exists → placeholder exists
2. Trigger condition met → can fill placeholder
3. Placeholder filled → /opsx-apply can start

**Parallel opportunities** (multiple placeholders can be filled simultaneously):
- If multiple placeholders have same trigger condition (e.g., all depend on same upstream change)
- Then run this skill in parallel via multiple `task` calls

---

## Output Summary Template

After completion, output:

```markdown
## ✅ Placeholder Filled: <change-name>

| Field | Before | After |
|-------|--------|-------|
| Status | ⚪ placeholder | 🟡 active |
| proposal.md | <X> lines, TBD | <Y> lines, full |
| design.md | (absent) | <Z> lines, 5 Decisions |
| tasks.md | <A> lines, TBD | <B> lines, ~40 tasks |
| specs/... | <C> lines, placeholder | <D> lines, 5-8 Requirements |

**Validation**: `openspec validate` exit 0 ✅

**Master Plan Updated**: §四 row + §十一 log entry

**Next**: `/opsx-apply <change-name>` to start implementation.
```

---

**Last Updated**: 2026-06-26 (initial)
**Tested With**: this project's 9 placeholder changes (C2-C8)
**Related Skills**: `master-plan-driven-changes` (creates placeholders), `openspec-apply-change` (implement after fill)