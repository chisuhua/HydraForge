# Sprint Closeout Checklist Guide

> **目的**: 详细说明每个 Sprint 收官前必须执行的检查项，避免"ship 后才发现偏离"的常见陷阱。
>
> **创建日期**: 2026-06-26 (Roadmap-Driven Development 启用)
>
> **设计依据**:
> - `docs/superpowers/plans/2026-06-26-sprint-11-to-18-roadmap.md` §9 (4 种 Review Gates)
> - `docs/active-status.md` §六 (Sprint 结束前检查)
> - `tools/check_roadmap_drift.py` (Drift Detection 工具)
>
> **关联工具**: `scripts/sprint-closeout.sh` (一键 wrapper)

---

## 1. 为什么需要 Sprint Closeout Checklist

### 1.1 问题背景

Sprint 6 (2026-06-22) 的惨痛教训：
- 4 个 commit 全部 ship + 行为保持 + 测试通过
- 但 Oracle 深度审查发现验收标准**系统性未达**（execute 222 行 vs ≤60、15 个测试 0 交付等）
- 最终需要 Sprint 7 follow-up 单独 1 个 Sprint 来清理

**根因**：缺少"重新检查节点"，sprint 收官只看 commit 表面（行为保持 + ctest 通过），未深挖 spec 验收项。

### 1.2 Roadmap-Driven Development 解决方案

引入 **4 种 Review Gates**（每 Sprint 强制执行）：

| Gate | 触发时机 | 检查目的 |
|------|---------|---------|
| 🔄 **Sprint Review** | 每个 Sprint 收官 | 已 ship change 是否达到预期效果 |
| 🧭 **Architecture Drift** | 每 2-3 Sprint | 已 ship change 实际行为是否仍符合 ADR 描述 |
| 🔗 **Dependency Refresh** | 占位 change 启动前 | 占位假设是否仍成立 |
| 🎯 **Strategic Alignment** | 季度 | 当前 backlog 是否仍服务项目核心目标 |

详细定义见 Master plan §9。

---

## 2. Sprint Closeout 完整流程

### 2.1 一键执行（推荐）

```bash
cd /workspace/project/HydraForge
./scripts/sprint-closeout.sh
```

输出示例：
```
============================================================
Sprint Closeout — 2026-06-26 14:45:00
Repo: /workspace/project/HydraForge
============================================================

============================================================
Step 1/6: 🔍 Drift Detection (Master Plan §9 Review Gates)
============================================================

🔴 [CRITICAL] ADR_CONTRADICTION
   ADR `adr-0030-async-runtime-dual-layer` 状态矛盾: ...
🚨 有 CRITICAL drift. 应立即处理 (创建 fix change).

[... 6 步详细输出 ...]

============================================================
Step 6/6: 📊 Summary
============================================================
  ✅ PASSED:   5
  ⚠️  WARNINGS: 1
  ❌ FAILED:   1
  ⏱️  耗时:     45s

🚨 Sprint Closeout 有失败项, 必须先修复.
```

### 2.2 单独运行 Drift Detection（快速检查）

```bash
./scripts/sprint-closeout.sh --drift-only
# 或
python3 tools/check_roadmap_drift.py
```

### 2.3 跳过 ctest（CI 失败调试时）

```bash
./scripts/sprint-closeout.sh --no-ctest
```

### 2.4 包含 ASan/TSan（耗时较长但彻底）

```bash
./scripts/sprint-closeout.sh --with-asan-tsan
```

---

## 3. 详细检查项（按 Step 顺序）

### Step 1: 🔍 Drift Detection（**强制**，Roadmap-Driven Development 核心）

**目的**：自动检测 4 类 drift，避免"Sprint 6 惨剧"重演。

**检测项**：
1. **占位 change 依赖偏离**：依赖不存在或已删除
2. **Master plan §三 状态不一致**：plan 标注 vs 实际 openspec 状态
3. **ADR 状态矛盾**：archive 标注 vs 实际测试/代码状态
4. **Master plan §9-§13 完整性**：4 种 Review Gates 是否齐全

**严重度**：
- 🔴 **CRITICAL**：必须立即创建 fix change
- 🟠 **HIGH**：应在当前 Sprint 处理
- 🟡 **MEDIUM**：纳入下次 Sprint Review
- 🔵 **INFO**：仅提示

**当前实测**（2026-06-26 baseline）：
```
🔴 [CRITICAL] ADR-0030 V1 archive 理由过时 (依赖已引入)
🔴 [CRITICAL] ADR-0032 标注 ❌ Not Implemented 但 test 已 PASS
```
→ 这 2 个 drift 已被 `C0 doc-alignment-adr-states` 覆盖，ship 后自动消失。

### Step 2: 🧪 ctest（测试套件）

```bash
cd build && ctest --output-on-failure
```

**期望**：
- 所有测试通过
- 0 regression（与 Sprint 开始前的 baseline 对比）

**如果失败**：
- 立即修复（引入新 bug）
- 或回滚（如果失败率 > 5%）

### Step 3: 📋 ADR lint

```bash
python3 tools/adr_lint.py docs/adr/ docs/archive/adr/ docs/adr/plugin/
```

**检查项**：
- ADR 状态字段使用 6 个标准标签
- 跨文件引用指向已存在的 ADR
- 替代关系编号正确

### Step 4: 📚 Docs drift audit

```bash
python3 tools/docs_drift_audit.py
```

**检查项**：
- docs/ 与 src/ 实现一致性
- 已 ship ADR 与代码状态对齐

### Step 5: 📋 OpenSpec validate（所有 active changes）

```bash
openspec validate <each-active-change>
```

**期望**：所有 active changes 通过 `openspec validate`。

**如果失败**：
- 修复 spec.md 格式（必须 `## ADDED Requirements` + `#### Scenario:`）
- 或修复 proposal.md 结构

---

## 4. Drift 类型详解（roadmap-driven 重点）

### 4.1 PLACEHOLDER_DEP_MISSING（占位依赖缺失）

**触发**：占位 change 引用了不存在的 change 名。

**示例**：
```
🔴 [HIGH] PLACEHOLDER_DEP_MISSING
   Change: 2026-06-26-adr-0030-v2-async-runtime
   Dependency: 2026-06-26-some-deleted-change
   占位 change `2026-06-26-adr-0030-v2-async-runtime` 依赖
   `2026-06-26-some-deleted-change`, 但该 change 不存在或已删除。
   需更新占位 assumption 或创建上游 change.
```

**响应**：
- 创建 fix change：调整占位 change 的依赖列表
- 或创建 redirect change：合并/拆分

### 4.2 STATUS_MISMATCH（Master plan 与 openspec 状态不一致）

**触发**：Master plan §三 标注与 `openspec list` 实际状态不符。

**示例**：
```
🟡 [MEDIUM] STATUS_MISMATCH
   Change: 2026-06-26-adr-0031-p3p4-toolcoordinator
   CID: C4
   Master plan C4 标注 `active`, 但实际 proposal 含 STATUS: PLACEHOLDER。
```

**响应**：
- 如果 change 实际上仍是占位：调用 `open-spec-placeholder-fill` 技能详细化
- 如果已 ship 但未更新 plan：更新 Master plan §四 row

### 4.3 ADR_CONTRADICTION（ADR 状态矛盾）

**触发**：ADR 标注 vs 实际代码/测试状态不一致。

**示例**：
```
🔴 [CRITICAL] ADR_CONTRADICTION
   ADR `adr-0030-async-runtime-dual-layer` 状态矛盾: V1 标注
   ❌ Not Implemented 归档原因 = 依赖未引入, 但 Slice 00 已 ship。
```

**响应**：
- 创建 doc-alignment-adr-states 类 change（类似 C0）
- 或写新 ADR（如 V2 取代 V1）

### 4.4 MASTER_PLAN_INCOMPLETE（Master plan 缺 Review Gates）

**触发**：Master plan 缺少 §9-§13 4 种 Review Gates 章节。

**响应**：参考 `docs/superpowers/plans/2026-06-26-sprint-11-to-18-roadmap.md` §9-§十三 模板补充。

---

## 5. Sprint Closeout 后必须做的事

### 5.1 更新 Master plan（强制）

每次 Sprint Closeout 后：

1. **§十 Drift Log**：追加本 Sprint 发现的所有 drift
2. **§十一 Adjustment Log**：记录占位 change 内容调整
3. **§十二 Pivots Log**：记录战略转向（如有）
4. **§四 Change Tracking**：更新本 Sprint ship 的 changes 状态为 `✅ archived (date)`

### 5.2 更新项目文档（强制）

- `docs/active-status.md` (快速概览)：更新 Phase 进度/活跃变更状态
- `docs/active-status.md` (最近完成)：追加本 Sprint 实施日志
- `AGENTS.md` § Recent Changes：追加 1 行总结

### 5.3 Git 操作（强制）

```bash
git status                           # 应 clean
git log --oneline -10                # 确认 commit 按 Day 分组
git tag sprint-<N>-ship              # 打 tag 便于追溯
git push --tags                      # 推 tag
```

### 5.4 OpenSpec archive（推荐）

```bash
# 已 ship 的 changes archive
openspec archive <change-name> --yes

# 验证
openspec list                        # 应不显示已 archive
openspec list --specs                # 应显示已合并的 spec
```

---

## 6. 常见问题

### Q1: ctest 失败但属于"pre-existing"问题怎么办？

**答**：创建独立的"tracking change"跟踪，不阻塞本 Sprint 收官。

**参考案例**：`2026-06-25-pre-existing-sanitizer-findings` (Sprint 10)
- 当时 ctest 33/34 + TSan 32/34（2 个 pre-existing）
- 创建独立 change `2026-06-25-pre-existing-sanitizer-findings`
- Sprint 10 优雅降级 ship，但 tracking change 在 Sprint 11 修复

### Q2: Drift Detection 误报怎么办？

**答**：
1. 检查 `tools/check_roadmap_drift.py` 是否误判
2. 如是真实误报，提交 fix PR 到工具
3. 如是真实 drift，创建 fix change 处理

### Q3: 4 种 Review Gates 必须全部跑吗？

**答**：
- 🔄 **Sprint Review**：每 Sprint 必跑
- 🧭 **Drift Detection**：每 Sprint 必跑（已集成到 wrapper）
- 🔗 **Dependency Refresh**：仅占位 change 启动前跑
- 🎯 **Strategic Alignment**：每季度（~6 Sprint）跑 1 次

### Q4: Sprint Closeout 失败，可以 ship 吗？

**答**：**绝对不行**。Sprint Closeout 是 ship gate，失败 = 不 ship。

例外情况（需在 Master plan §十 Drift Log 记录）：
- 已知 pre-existing 问题（已创建独立 tracking change）
- 战略决策延期（已 §十二 Pivots Log 记录）

---

## 7. 参考资料

- **Master Plan**: `docs/superpowers/plans/2026-06-26-sprint-11-to-18-roadmap.md`
- **Drift Detector**: `tools/check_roadmap_drift.py`
- **Wrapper Script**: `scripts/sprint-closeout.sh`
- **Sprint Closeout Checklist**: `docs/active-status.md`
- **Review Gates 详细定义**: Master plan §9
- **Skills**:
  - `.opencode/skills/master-plan-driven-changes/` (批量创建 plan + changes)
  - `.opencode/skills/open-spec-placeholder-fill/` (占位详细化)

---

**最后更新**: 2026-06-26
**下次更新**: Sprint 11 收官后（验证流程有效性）
**维护者**: Sisyphus（创建）→ 用户（持续维护）
