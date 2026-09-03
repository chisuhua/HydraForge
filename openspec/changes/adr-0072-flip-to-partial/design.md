## Context

Phase 6c (LLM-native Execution Plane) 于 2026-09-02 实质 ship 收官（8/8 change archived per `iteration.json`），其中 ADR-0072 D3 (declarative style) + D5 (dual syntax) 已 ship，C6/C7 实施层落地。但 ADR-0072 整体状态从 🔍 Proposed 翻牌至 🟡 Partial 的决策**未走完整 single-developer 治理流程**：

**当前异常状态**：
- ADR-0072 §状态字段已显示 `🟡 Partial (2026-09-02)`（见 `docs/adr/adr-0072-dsl-node-extensions.md:5`）
- 但 `openspec/changes/` 中**不存在**对应 change 目录
- 无 GitHub issue 用 `adr-review.md` 模板
- 无 Self-Review Checklist 12 项勾选记录
- 无 24h cooling-off 计时证据

**背景**：
- single-dev 模式治理范式（per AGENTS.md "Single-Developer Mode" + `.github/ISSUE_TEMPLATE/adr-review.md` 模板）要求重大 ADR 状态翻转必须走完整流程：issue 创建 → 12 项 Self-Review → 24h cooling-off → OpenSpec change archive
- Phase 6c 期间同时完成 C6+C7 实施 + ADR-0072 状态字段翻牌 = "实施先于翻牌"治理异常
- 该异常已在 ADR-0072 §治理异常段（line 532 footer）自我承认："治理异常已文档化，待建 OpenSpec change `2026-09-02-adr-0072-flip-to-partial`"

## Goals / Non-Goals

**Goals:**
1. 创建完整 OpenSpec change artifacts (proposal/design/tasks/specs)
2. 创建 GitHub issue 用 adr-review 模板
3. 勾选 Self-Review Checklist 12 项（按 `docs/architecture/adr-self-review-checklist.md`）
4. 等待 ≥24h cooling-off（issue 创建到 archive 时间差 ≥86400s）
5. archive change 至 `openspec/changes/archive/2026-09-02-adr-0072-flip-to-partial/`
6. 同步 `iteration.json`（+1 archived entry）
7. 同步 `proposal-suggestions.md` §3.4 + `proposal-approved.md` 标记本治理补建完成

**Non-Goals:**
1. **不**实施 ADR-0072 D1+D4 代码改动（→ Change #3+#4 阶段 A）
2. **不**再次翻牌 ADR-0072（保持 🟡 Partial 不再翻，避免反复）
3. **不**修改 ADR-0072 §状态字段（已翻牌，本次仅补证据链）
4. **不**修复其他 ADR 的"实施先于翻牌"异常（仅 ADR-0072，其他留 follow-up）
5. **不**修改 `roadmap.md` / `active-status.md`（Q2a/Q3 修订已完成）

## Decisions

### Decision 1: archive 路径使用完整日期前缀

- **选择**: `openspec/changes/archive/2026-09-02-adr-0072-flip-to-partial/`
- **理由**: 
  - 与其他 archive 命名约定一致（`2026-08-13-adr-0073-partial-flip/` 等）
  - 保留时间戳便于按时间线追溯治理证据
- **替代考虑**: 
  - ❌ `archive/adr-0072-flip-to-partial/` — 丢失 ship 日期信息
  - ✅ `archive/2026-09-02-adr-0072-flip-to-partial/` — 保留日期便于审计

### Decision 2: change name 不带日期前缀（openspec CLI 限制）

- **选择**: `openspec/changes/adr-0072-flip-to-partial/`（无日期前缀）
- **理由**:
  - openspec CLI 要求 change name 以字母开头（`openspec new change <name>` 报错 "name must start with a letter"）
  - archive 时再追加日期前缀作为目录名
- **替代考虑**:
  - ❌ `2026-09-02-adr-0072-flip-to-partial/` — CLI 拒绝
  - ✅ `adr-0072-flip-to-partial/` — CLI 接受 + archive 时加日期

### Decision 3: 24h cooling-off 跨 Sprint 25 Day 1-2

- **选择**: Day 1 (周一) 创建 issue → Day 2 (周二) archive
- **理由**:
  - 利用 sprint 内自然工作日冷却，不阻塞其他 change 并行
  - Sprint 25 +1 周末前完成治理底线
- **替代考虑**:
  - ❌ cooling-off 跨整周（5+ 工作日）— 过度延迟，浪费容量
  - ✅ cooling-off 跨 1 自然日（24h+）— 满足 single-dev 治理要求
  - ❌ 跳过 cooling-off — 违反治理范式，先例风险

### Decision 4: 不修复其他 ADR 的"实施先于翻牌"异常

- **选择**: 仅修复 ADR-0072，其他 ADR 异常留 follow-up
- **理由**:
  - Sprint 25 容量有限（~18h 实质工作）
  - ADR-0072 是 Oracle 标识的最严重异常（最近一次翻牌 + 治理证据完全缺失）
  - 其他 ADR 异常可作为 Sprint 26+ follow-up
- **替代考虑**:
  - ❌ 一次性修复所有 6 个 ADR 异常 — 范围过大，可能分散精力
  - ✅ 仅 ADR-0072 + 文档化其他 follow-up 列表 — 优先级清晰

## Risks / Trade-offs

| 风险 | 缓解 |
|------|------|
| [cooling-off 24h 期间其他 change 抢先 ship 导致依赖反序] → 4 个其他 P0/P1 change 已经在 #1 后启动，但 #1 的 archive 不阻塞它们 |
| [Self-Review Checklist 12 项发现遗漏] → Sprint 25 capacity 有 36h buffer 应对额外 fix |
| [archive 后 git log 无法区分"补建"vs"首次 ship"] → 在 commit message 显式声明 "governance补建" + iteration.json archived_at 字段 |
| [24h cooling-off 跨周末导致 Sprint 25 收官延迟] → Day 1 创建 + Day 2 archive 避免跨周末 |

## Migration Plan

**无迁移需求**（纯治理动作，zero 代码改动）。

**Rollback 策略**:
- 若 archive 后发现治理证据不完整 → `git revert <commit-hash>` 删除 archive 目录 + iteration.json entry
- 但通常不需要（治理证据本身是事后记录，可重新补建）

## Open Questions

**无开放问题**（所有设计决策已由 Oracle 优先级排序 + Metis B 评级 + 修订后 roadmap.md 决策树锁定）。
