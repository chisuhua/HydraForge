---
name: master-plan-driven-changes
description: Create a Master plan + N OpenSpec changes for multi-Sprint work. Use when planning 3+ related changes across multiple Sprints, when batch-creating placeholder changes with dependencies, or when bootstrapping a new phase/milestone with parallel work tracks. Triggers: "plan Sprint N+1 to N+M", "create a roadmap", "batch create changes", "Master plan 驱动", "基于 Master plan 启动一批 changes".
license: MIT
compatibility: Requires openspec CLI >= 1.4, project with docs/adr/ structure.
metadata:
  author: hydraforge
  version: "1.0"
  parent_skill: openspec-propose
  related_skill: open-spec-placeholder-fill
---

# Master Plan Driven Changes

Create a **Master plan** (one Markdown tracker) + **N OpenSpec changes** (some immediate, some placeholder) for multi-Sprint work with dependencies.

## When to Use

✅ **Use this skill when:**
- User says "plan the next N Sprints"
- User says "create a Master plan"
- User says "batch create these changes"
- Multiple related changes need to ship across 2+ Sprints
- Some changes are blockers/dependencies for others
- User wants both immediate execution + future placeholders

❌ **Don't use when:**
- Single change (use `openspec-propose` instead)
- User just wants to plan 1 Sprint (use existing `phase-execution.md` pattern)
- No clear roadmap_goals

---

## Input Schema

| Field | Required | Type | Description |
|-------|----------|------|-------------|
| `roadmap_goals` | ✅ | string[] | Atomic goals to achieve (e.g., "Phase 2 异步架构", "Phase 3 安全策略") |
| `target_sprints` | ✅ | int | How many Sprints this plan covers (typical: 3-8) |
| `existing_changes` | ⬜ | string[] | Reference changes to inherit patterns from (e.g., archive changes) |
| `external_constraints` | ⬜ | string[] | Team/tech constraints (e.g., "no new external deps", "Linux-only") |
| `start_sprint_number` | ⬜ | int | Default: next available Sprint number |
| `skip_review_gates` | ⬜ | bool | Default: false (always include §9 Review Gates) |

---

## Output Schema

```
docs/superpowers/plans/<YYYY-MM-DD>-<roadmap-slug>.md     # Master plan tracker
openspec/changes/<YYYY-MM-DD>-<change-slug>/              # × N changes
  ├── .openspec.yaml
  ├── proposal.md
  ├── tasks.md
  └── specs/<capability>/spec.md
```

**Master plan must include**:
- §1 Baseline
- §2 Dependency Graph
- §3 Change Overview
- §4 Detailed Tracking
- §5 Sprint Breakdown
- §6 Risks
- §7 Maintenance
- §8 References
- §9 Review Gates (4 types) ← **key innovation**
- §10 Drift Log
- §11 Adjustment Log
- §12 Strategic Pivots
- §13 Response Change Types (fix/retro/redirect)

**Each change must include**:
- `.openspec.yaml` (schema: spec-driven, created: <date>)
- `proposal.md` (Why/What/Capabilities/Impact/Non-goals)
- `tasks.md` (Day-grouped tasks, TBD for placeholders)
- `specs/<capability>/spec.md` (≥1 ADDED Requirements + Scenarios)

---

## Workflow (12 Steps)

### Phase 1: Context Gathering (Steps 1-3)

#### Step 1: Read project state

```bash
# Required reads (parallel)
1. docs/roadmap-status.md (§一 总进度 + §四 实施日志 + §五 验证状态)
2. openspec list (current active changes)
3. docs/adr/ + docs/archive/adr/ + docs/adr/plugin/ (all ADR statuses)
4. docs/implementation-roadmap.md (static blueprint)
```

Output: Establish baseline — current Phase, active ADR count, test pass count.

#### Step 2: Read recent archive patterns

```bash
# Look at 3-5 most recent archive changes to understand proposal format
ls -t openspec/changes/archive/ | head -5
# Read each: proposal.md, design.md (if exists), tasks.md structure
```

Output: Identify any conventions to follow (e.g., spec-driven schema, ## ADDED Requirements).

#### Step 3: Read user constraints

From user message, extract:
- `roadmap_goals` (must parse to N atomic goals)
- `target_sprints` (must be int)
- `existing_changes` (must verify exist on disk)
- `external_constraints` (must be concrete)

If anything unclear → **ask ONE clarifying question** using `question` tool.

---

### Phase 2: Goal Decomposition (Steps 4-5)

#### Step 4: Decompose goals to atomic sub-goals

For each roadmap_goal, break down into 1-3 atomic sub-goals (each = 1 change):

```
roadmap_goal = "Phase 2 异步架构"
  → sub_goal_1: "写 ADR-0030 V2 取代 V1" (1 change, 1 day)
  → sub_goal_2: "实施并行 DAG executor" (1 change, 1.5 weeks)
  → sub_goal_3: "16 路 LLM 并行 (Fleet 模式)" (1 change, 1 week)
```

**Atomic sub-goal test**: Can it ship in 1-2 Sprints and produce ≥1 ctest pass?

#### Step 5: Assign change types

For each sub_goal, assign one of 3 types:

| Type | When | Output |
|------|------|--------|
| **immediate** | Can start now (no blocker) | Full artifacts (proposal + design + tasks + spec) |
| **placeholder-soft** | Soft dependency on other change | Minimal proposal + STATUS: PLACEHOLDER + 10-step TODO |
| **placeholder-hard** | Hard dependency on other change (interface/blocking) | Minimal proposal + STATUS: PLACEHOLDER + dependency chain in proposal |

---

### Phase 3: Dependency Analysis (Steps 6-7)

#### Step 6: Identify dependencies

For each pair of changes, classify:

| Dependency Type | Meaning | Sprint Assignment |
|----------------|---------|-------------------|
| **Hard (compile/interface)** | B compiles only after A ships | B in next Sprint |
| **Soft (logical)** | B is more natural after A | B can parallel, but A first preferred |
| **None (parallel)** | Independent | Same Sprint |

Use this template to record:

```
A → B: hard (B's design depends on A's interface)
C → D: soft (D's testing cleaner if C done)
E ⊥ F: parallel (no dependency)
```

#### Step 7: Topological sort + Sprint assignment

```python
# Pseudocode
sprints = []
remaining = all_changes
while remaining:
    ready = [c for c in remaining if all deps of c are in (sprints_done or current)]
    if not ready:
        raise CycleError
    sprints.append(ready)  # all parallel in this Sprint
    remaining -= ready
```

Output: Sprint plan table (which changes in which Sprint).

---

### Phase 4: Master Plan Generation (Step 8)

#### Step 8: Write Master plan

File: `docs/superpowers/plans/<YYYY-MM-DD>-<roadmap-slug>.md`

Use this **mandatory structure** (14 sections):

| § | Title | Content |
|---|-------|---------|
| 一 | Baseline | Current project state (Phase, ADR count, tests, date) |
| 二 | Dependency Graph | ASCII diagram of change dependencies |
| 三 | Change Overview | Table: # / name / type / estimate / dep / status |
| 四 | Detailed Tracking | Per-change deep dive (10 lines each) |
| 五 | Sprint Breakdown | Sprint N-M table with ship gates |
| 六 | Risks | Risk table with mitigation strategies |
| 七 | Maintenance | Rules for keeping plan current |
| 八 | References | Links to docs/adr/, docs/specs/, etc. |
| **九** | **Review Gates** | **4 types: Sprint/Drift/Dependency/Strategic** |
| **十** | **Drift Log** | **Historical drift events table** |
| **十一** | **Adjustment Log** | **Placeholder adjustment history** |
| **十二** | **Strategic Pivots** | **Major direction changes** |
| **十三** | **Response Types** | **fix/retro/redirect change workflow** |
| 十四 | Maintenance Rules | Add Review Gates enforcement rules |
| 附录 A | Physical Layout | Tree diagram of all changes |

**§9 Review Gates is the key innovation** — see "Review Gates 4 Types" section below.

---

### Phase 5: Change Creation (Steps 9-10)

#### Step 9: Create immediate changes (full artifacts)

For each `immediate` change:

```bash
mkdir -p openspec/changes/<date>-<slug>/specs/<capability>

# Write 4 files
write .openspec.yaml       # schema: spec-driven, created: <date>
write proposal.md          # full Why/What/Capabilities/Impact/Non-goals
write tasks.md             # Day-grouped, all checked [ ]
write specs/<capability>/spec.md   # ≥1 ## ADDED Requirements + Scenarios
```

**Spec format** (must pass `openspec validate`):
```markdown
## ADDED Requirements

### Requirement: <name>
<description>

#### Scenario: <name>
- **WHEN** <condition>
- **THEN** <result>
- **AND** <additional> (optional)
```

#### Step 10: Create placeholder changes (minimal artifacts)

For each `placeholder-*` change:

```bash
mkdir -p openspec/changes/<date>-<slug>/specs/<capability>

write .openspec.yaml
write proposal.md          # STATUS: PLACEHOLDER + Why + What (overview) + 10-step TODO
write tasks.md             # All TBD sections, 5-10 sections each with TBD bullets
write specs/<capability>/spec.md   # 3-5 placeholder Requirements with TBD Scenarios
```

**Placeholder proposal.md template**:
```markdown
# Proposal: <Change Name>

> **STATUS: PLACEHOLDER** ⚠️
> **本 change 详细 design/spec/tasks 待 <trigger_condition> 后填充**
> **关联 ADR**: <path>
> **追溯范围**: docs/superpowers/plans/<master-plan>.md §四 <change-id>

## Why
<brief rationale based on user input>

## What Changes (待 <trigger_condition> 后详细制定)
### <Section 1> (placeholder)
- TBD: <what needs to be decided>
- TBD: <implementation scope>

## Capabilities (待详细制定)
### ADDED Requirements (placeholder)
- `<req-name>` (PLACEHOLDER): <one-line description>

## Non-goals (placeholder)
- <what this change won't do>

## Estimated Effort (placeholder)
**总计**: <N> 周 (<Sprint> 主体)

## 详细制定 TODO (待 <trigger_condition> 后执行)
- [ ] 1. 决策前置 (Oracle 咨询 / 业务确认)
- [ ] 2. 写完整 design.md
- [ ] 3. 写完整 tasks.md
- [ ] 4. 写完整 spec.md
- [ ] 5. 移除 PLACEHOLDER 标记
- [ ] 6. openspec validate
- [ ] 7. 更新 master plan §四 状态: ⚪ placeholder → 🟡 active
- [ ] 8. 启动 Sprint 实施
```

---

### Phase 6: Validation & Summary (Steps 11-12)

#### Step 11: Validate all changes

```bash
for change in $(openspec list --format ids); do
  result=$(openspec validate "$change" 2>&1)
  if echo "$result" | grep -q "valid"; then
    echo "✅ $change"
  else
    echo "❌ $change: $(echo "$result" | tail -2 | head -1)"
  fi
done
```

**Common validation errors**:
| Error | Fix |
|-------|-----|
| `No delta sections found` | Change `## Requirements` to `## ADDED Requirements` in specs/*.md |
| `No `#### Scenario:` block` | Add at least one Scenario per Requirement |
| `Change must have at least one delta` | Ensure specs/<capability>/spec.md exists |

#### Step 12: Final summary to user

```markdown
## ✅ N 个 OpenSpec Changes 创建完成

### Master Plan
- **路径**: docs/superpowers/plans/<date>-<slug>.md (<lines> 行, <size>)
- **覆盖**: Sprint <N> - Sprint <M> (<N> Sprints, ~<weeks> 周)

### Changes (N total, all `openspec validate` pass)
| # | Change | Type | Status | Tasks | Dep |
|---|--------|------|--------|-------|-----|
| ... | ... | immediate/placeholder | 🟡/⚪ | ... | ... |

### File Structure
```
docs/superpowers/plans/<date>-<slug>.md   [Master tracker]
openspec/changes/
├── <date>-<slug>/    [C0] immediate
└── ... × N
```

### 下一步
1. 审阅 Master plan §一-§四 (baseline + overview)
2. 启动 C0 (或第一个 immediate change) 实施: /opsx-apply <change>
3. 占位 changes 在 trigger condition 满足时调用 `open-spec-placeholder-fill` 子技能
```

---

## Review Gates 4 Types (Master Plan §9)

| Gate | Trigger | Check | Output |
|------|---------|-------|--------|
| **🔄 Sprint Review** | Each Sprint end | Behavior as expected? Bugs? Hypothesis errors? Time drift > 30%? | Add row to §10 Drift Log |
| **🧭 Architecture Drift** | Every 2-3 Sprints | ADR descriptions still match? Status updates needed? | ADR-fix change (like C0) |
| **🔗 Dependency Refresh** | Before each placeholder starts | Dep really shipped? Interface matches? | Adjust placeholder → §11 |
| **🎯 Strategic Alignment** | Quarter / milestone | Backlog still serves goals? New ADR? Phase 5 ready? | Major pivot → §12 + new Master plan |

**Detail**: See `<YYYY-MM-DD>-sprint-11-to-18-roadmap.md` §9 in this project for full table.

---

## Failure Modes

| Failure | Detection | Recovery |
|---------|-----------|----------|
| Goal too vague | User says "improve performance" | Ask: "Which perf dimension? Latency / throughput / memory?" |
| 2+ changes have circular dep | Topo sort fails | Ask user: which to defer / split |
| ADR contradicts goal | Compare ADR status vs goal | Suggest doc-alignment sub-change first (like C0) |
| Change name invalid | `openspec new change` rejects | Manual mkdir + .openspec.yaml (date-prefix accepted) |
| Spec delta syntax wrong | `openspec validate` errors | Run `openspec change show <id> --json --deltas-only` to inspect |

---

## Integration with Sub-Skills

This skill **creates** the Master plan + placeholder changes.
The sub-skill `open-spec-placeholder-fill` **fills in** placeholder details when trigger condition is met.

```
master-plan-driven-changes (this skill)
  ↓ creates
Master plan + N changes (some placeholders)
  ↓ user starts Sprint X
[immediate changes] → /opsx-apply → ship
[placeholder changes] → open-spec-placeholder-fill → ship
```

---

## Example Invocation

```
User: "Plan Sprint 11-18 with 9 changes: doc alignment, tech debt execution, then 7 phase 2-4 changes"

Skill:
1. Reads roadmap-status (Phase 0+1 = 100%, Phase 2-5 = 0%, 34/34 tests)
2. Decomposes to 9 atomic sub-goals
3. Analyzes: C0 → C1 → C2; C3 → C4 → C6/C8; C5 ⊥ C7
4. Writes docs/superpowers/plans/2026-06-26-sprint-11-to-18-roadmap.md (590 lines, 14 sections + 4 Review Gates)
5. Creates 9 changes:
   - C0, C1: full artifacts (immediate)
   - C2-C8: minimal + STATUS: PLACEHOLDER + 10-step TODO
6. Validates all 9 → all pass
7. Outputs summary
```

---

## Output Location

After completion, list created files:

```bash
echo "=== Master Plan ==="
ls -la docs/superpowers/plans/<date>-<slug>.md

echo "=== Changes ==="
for d in openspec/changes/<date>-*/; do
  echo "  $d ($(find "$d" -type f | wc -l) files, $(du -sh "$d" | cut -f1))"
done

echo "=== Validation Status ==="
openspec list
```

---

**Last Updated**: 2026-06-26 (initial)
**Tested With**: openspec 1.4.1, this project's ADR/spec structure
**Related Skills**: `open-spec-placeholder-fill` (placeholder fill-in), `openspec-propose` (single change)