# HydraForge 文档治理方案

> **核心原则**: 每一个 `.md` 文件要么**驱动一个任务**，要么被**一个任务更新**，要么被**归档**。
>
> **最后更新**: 2026-07-22
> **下次审视**: 2026-09-30 (季审视)

---

## 一、文档分层与权限

```
                    ┌──────────────────────┐
                    │   proposals/         │  ← 自由探索层
                    │   "如果...会怎样?"    │     任何人都可以写，不需评审
                    └──────┬───────────────┘
                           │ 需要 ADR Sponsor 提升
                           ▼
               ┌───────────────────────┐
               │   docs/adr/           │  ← 决策层
               │   "我们决定..."        │     必须 adr_lint + Oracle 评审
               │                      │     只有 Approved 才能进入实现
               └──────┬────────────────┘
                      │ 必须有一份 spec 引用该 ADR
                      ▼
          ┌───────────────────────┐
          │   docs/specs/         │  ← 契约层
          │   "系统必须..."        │     用 REQ-XXX 编号，可被测试验证
          └──────┬────────────────┘
                 │ 自动映射到 Master Plan
                 ▼
    ┌─────────────────────────┐
    │   Master Plan           │  ← 执行层
    │   "何时做、谁做、怎么做"  │     每个 Sprint 启动时创建/更新
    └──────┬──────────────────┘
           │ 每日同步
           ▼
    ┌───────────────────────┐
    │   active-status.md    │  ← 追踪层
    │   "现在在哪?"          │     数据必须从实际工具输出验证
    └───────────────────────┘
```

### 各层合并条件

| 层级 | 谁可以写 | 合并条件 |
|------|---------|---------|
| **proposals/** | 任何人 | 无门槛 |
| **adr/** | 任何人提 PR | `adr_lint` 通过 + Oracle 评审 |
| **specs/** | 从 Approved ADR 派生 | `REQ-XXX` 编号唯一 + 每个需求有对应测试 |
| **Master Plan** | Sprint 启动时 | `check_roadmap_drift` 零 HIGH |
| **active-status.md** | Sprint 内随时 | 数据从实际工具输出验证 |

---

## 二、任务驱动流水线 (5 步法)

### Step 1: Proposal → ADR

```
触发: proposals/ 下提交了新的设计文档 (超过 1 页)
动作: Owner 评审 → 决定是否提升为 ADR
时限: 1 周内
结果: ADR 进入 🔍 Proposed，或 proposal 被标注 "Deferred"
```

**清理规则**: 每月 1 日，超过 3 个月未被提升为 ADR 的 proposal 归档到 `docs/archive/proposals/`。

---

### Step 2: ADR → Spec

```
触发: ADR 状态变为 ✅ Approved
动作: ADR Sponsor 在 Spec 中新增/修改对应的 REQ-XXX
时限: ADR Approved 后 1 周内
结果: spec 中有了可验证的需求条目
```

**REQ-XXX 格式规范**:
```markdown
### REQ-{DOMAIN}-{NNN}: {简短描述}
- **来源**: ADR-XXXX §决策 N
- **行为**: {一句话描述系统应该做什么}
- **验证**: `{test_file}::{test_case}`
- **状态**: ✅ 已实现 | 📋 待实现 | 🔶 部分实现
```

**验证**: 每 2-3 Sprint 的 Drift Gate 检查 `Approved ADR 数量 == spec 中 REQ 来源 ADR 数量`。

---

### Step 3: Spec → Master Plan

```
触发: Spec 中有新的 REQ-XXX 且状态为 📋 待实现
动作: 纳入 Master Plan 的 Sprint backlog
时限: 每个 Sprint 启动时
结果: Master Plan §四 中增加了对应的任务行
```

**验证**: Sprint Review Gate 时检查 `grep "📋 待实现" docs/specs/` 数量 vs Master Plan backlog。

---

### Step 4: Master Plan → Tasks

```
触发: Sprint 启动
动作: 从 Master Plan §四 提取本 Sprint changes → 分解为 tasks
时限: Sprint 启动后 1 天内
结果: 可逐项追踪的 task list
```

**Phase 6 简化模式** (不创建 OpenSpec change):
```bash
# 从 Master Plan 提取本 Sprint task list
grep "🟡 active\|🔨 编码" docs/superpowers/plans/2026-07-16-pdk-chat-demo-implementation.md
```

**验证**: Sprint Review 时对比 Master Plan 声明的 changes 和实际 commit 内容。

---

### Step 5: Tasks → active-status.md

```
触发: 任何 task 完成
动作: 立即更新 active-status.md 的对应行
时限: task 完成后 10 分钟内
结果: active-status 反映真实进度
```

**自动化验证** (Sprint 收官):
```bash
# ctest 计数一致性
actual=$(ctest --test-dir build -N 2>/dev/null | grep "Total Tests" | grep -oP '\d+')
claimed=$(grep "Total ctest" docs/active-status.md | grep -oP '\d+')
[ "$actual" = "$claimed" ] || echo "DRIFT: ctest mismatch ($actual != $claimed)"

# OpenSpec active 计数一致性
actual=$(ls openspec/changes/*/proposal.md 2>/dev/null | wc -l)
claimed=$(grep "OpenSpec active" docs/active-status.md | grep -oP '\d+')
[ "$actual" = "$claimed" ] || echo "DRIFT: openspec active mismatch ($actual != $claimed)"
```

**验证**: Sprint Review Gate 必须验证 3 个关键数字 (ctest/ASan/OpenSpec active) 与实际一致。

---

## 三、治理节奏

| 频率 | 事件 | 检查项 |
|------|------|--------|
| **每月 1 日** | proposals/ 清理 | >3 月未处理的 proposal → archive |
| **每 Sprint 启动** | Step 3: Spec → Plan | 从 spec REQ 生成 backlog |
| **每 Sprint 进行** | Step 4: Plan → Tasks | 提取本 Sprint tasks |
| **每 Sprint 收官** | Sprint Review Gate | active-status.md 3 项数字验证 |
| **每 2-3 Sprint** | Drift Gate | ADR/spec/plan/status 全面一致性 |
| **每 Phase 完成** | Strategic Alignment | 方向评估 + 可能创建新 Master Plan |
| **每季度末** | 治理方案审视 | 评估本方案是否仍然适用 |

### 2026 下半年日程

| 日期 | 事件 |
|------|------|
| 2026-08-01 | 每月 proposals/ 清理 |
| 2026-08-12 | Sprint Review (ctest/ASan/OpenSpec active 验证) |
| 2026-08-19 | Drift Gate (ADR ↔ spec REQ 映射) |
| 2026-09-01 | 每月 proposals/ 清理 |
| 2026-09-30 | 季审视: 本治理方案评估 |

---

## 四、反模式清单

| 反模式 | 症状 | 修复 |
|--------|------|------|
| **幽灵 proposal** | proposals/ 中有超过 3 个月未被评审的文档 | 归档到 archive/proposals/ |
| **孤儿 ADR** | ADR Approved 但 spec 没有对应的 REQ-XXX | 补充 spec REQ 条目 |
| **僵尸 REQ** | spec 中有 📋 待实现 的 REQ 但 Master Plan 无对应任务 | 纳入下个 Sprint |
| **统计漂移** | active-status.md 中的 ctest/ASan 计数与实际不符 | Sprint Review 时强制验证 |
| **影子工作** | Master Plan 声称在做 X，实际 commit 都是 Y | Sprint Review 对比 plan vs commits |

---

## 五、治理方案的维护

1. **Amend 而非 Replace**: 修改本方案时，在文件末尾追加 `## 修订记录` 行
2. **关键变更走 ADR**: 任何涉及文档治理流程的重大变更，创建 ADR 记录
3. **季审视**: 每季度末评估本方案是否仍然适用

---

## 修订记录

| 日期 | 修订 | 理由 |
|------|------|------|
| 2026-07-22 | 初始版本 | Debt audit: 建立 docs → tasks 驱动流程 |