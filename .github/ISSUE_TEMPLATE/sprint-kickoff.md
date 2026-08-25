---
name: Sprint Kickoff
about: Sprint 启动串联 issue (single-developer mode)
title: "[Sprint XX] Kickoff: <theme>"
labels: ["sprint-kickoff", "self-review"]
assignees: []
milestone: "Sprint XX"
---

## Sprint 目标 (YYYY-MM-DD → YYYY-MM-DD)

<!-- 1 段说明本 Sprint 的核心目标, ≤200 字 -->

主目标: <一句话目标>

---

## 启动周任务 (Week 1)

<!-- Week 1 (启动周) 的具体任务, 每项 ≤1 sprint 估时 -->

- [ ] **T17** <task name> (X sprint)
      前置: <前置条件>
      OpenSpec change: `openspec/changes/<path>/`
      估时: <sprint 数>

- [ ] **<Task>** <name> (X sprint)
      前置: <前置>
      ...

---

## 排期表

| Sprint 周次 | 启动任务 | ship 目标 |
|---|---|---|
| Sprint XX 启动周 | T17 骨架 + ... | ... |
| Sprint XX 末 | T19 ... | ... |
| Sprint XX+1 启动周 | T15 + ... | ... |
| Sprint XX+1 末 | ... | Phase X 完整 |

---

## 风险与备选

### 风险 1: <risk name>

**描述**: <描述>
**影响**: <影响>
**备选**: <备选方案>

### 风险 2: <risk name>

...

---

## 自审清单 (Sprint 启动前)

- [ ] 前置 Sprint 决议 issue 全部冷却期已结束
- [ ] capability-map vX.X 已更新 (引用 §X)
- [ ] X 个 ADR 状态字段已翻转 (✅ Approved)
- [ ] T17 OpenSpec change tasks.md Phase 1 已启动
- [ ] 排期表与能力地图 §四 一致
- [ ] 风险评估与备选方案记录完整

---

## 关联文档

- **Capability Map**: `docs/architecture/capability-application-map-2026-08.md`
- **Sprint Pre-Launch Plan**: `openspec/changes/2026-08-25-sprint-24-pre-launch-self-review/` (或当前 Sprint)
- **Master Plan**: `docs/superpowers/plans/<current>-*.md`
- **相关 ADRs**: `docs/adr/<path>/adr-XXXX-*.md`

---

## 签发

- **创建**: solo-dev @ YYYY-MM-DD
- **关联 Milestone**: Sprint XX
- **下一修订**: Sprint XX 收官 (Sprint XX+1 启动前)