## Why

2026-08-25 对 `docs/architecture/capability-application-map-2026-08.md`（v1.3，最后验证日 2026-08-25）做了一次结构化审查。基于：

- 文件内一致性比对（章节顺序 / 状态词汇 / 计数 / cross-reference）
- 实证 ctest (`ctest -N` 报告 185 个 tests)
- 关联 ADR 文件抽查（ADR-0071 / ADR-0074 / ADR-0083 / ADR-0080 v1.2 / ADR-0061-13 / ADR-0061-06 v1.1）
- OpenSpec archive 路径对照（2 个 behavioral-regression / slm-routing changes 实际在 `archive/2026-08-25-2026-08-24-...` 而非 `changes/2026-08-24-...`）

发现 **12 处文档 drift**，按严重程度分 **3 级（🔴 P0 × 7 / 🟠 P1 × 3 / 🟡 P2 × 3）**。最大风险为：

1. **章节顺序错乱**：`§七 变更记录`（line 585）排到 `§八 蒸馏专题`（line 434）之后，正常阅读流应为 "六 → 七 → 八"。当前结构出现 "六 → 八 → 七" 断流，目录/索引会被跳过 §七。
2. **`§六.6 验证命令` 章节位置错误**：应在 `§六.5` (line 407) 之后，但当前插入到 `§八` 后（line 564）。同时 line 421 验证命令路径前缀错误，缺 `archive/` 与 `-2026-08-25-` 时间戳段。
3. **状态词汇表缺 ✅ Closed**：`§二` line 90 定义 4 个状态（Open / Blocked / Partial / 架构契约缺失），但表格 line 107-112 已用 `✅ Closed (评审通过 2026-08-25)`。词汇表未补充。
4. **§二 标题 "15 项未 ship" 已过时**：v1.3 后 5 项已 Closed，标题与实际不符。
5. **§八 闭环 1 line 463 仍称 G15 "新 ADR 需求"** — 但 ADR-0061-13 已于 2026-08-25 评审通过。Stale 文本导致 "60% 评估高估" 的论据失效。

**Why now**:
- 本文档是 **Sprint 24 启动前的 Step 3 任务**（AGENTS.md "Single-Developer Mode" 章节："24h cooling-off + 更新 capability-map §七 v1.3"）。
- v1.3 经过 v1.0 → v1.1 → v1.1.2 → v1.1.3 → v1.2 → v1.3 六次增量累积，结构性 drift 累积到需收口阈值。
- capability-map §一/§二/§三/§四 的计数是 `sprint-closeout.sh` Step 8 与 `docs-drift-detect.sh` B.2 计划的输入，P0 #1-7 漂移会影响自动审计脚本的预期输出。
- 经 24h 冷却期（per Single-Dev Mode 治理）后即可执行 ship，预计 ~45 分钟工作量，符合 Sprint 24 启动前可消化范围。

**Non-Goals（明确范围边界，避免 scope creep）**：

- ❌ 不修改任何源代码（`src/` `include/` `pdk/` `tests/` `examples/`）
- ❌ 不重编号任何工程任务（T1-T22 保持不变）
- ❌ 不改变计数本身的真实性（仅同步措辞与时效标签，不重写 §一/§二/§三 表格）
- ❌ 不创建新 ADR、不修改现有 ADR 状态字段（drift-7 仅同步 footer 状态注记）
- ❌ 不修改 OpenSpec spec 已 approved 部分（仅本 change 的 specs/ 子目录）
- ❌ 不修复 Oracle 评审方法论本身（仅文档同步，不重新评估）
- ❌ 不解决其他文档的 drift（`docs-cleanup-phase-3` 之前的 cleanup 已归档；本文档之外留作 follow-up change）

## What Changes

本 change **不修改任何源代码、不修改任何 ADR 编号、不修改任何现有 spec**。仅做 12+4 处 capability-application-map drift 修复（局部文本修订 + 1 处章节位置调整 + 后续 ship 时发现 4 项同类 drift 扩展）。

### P0 — 严重 drift（7 处，~30 分钟）

- **[drift-1] capability-map §七/§八 章节位置互换**：`## 七、变更记录` (原 line 585) 移到 §八 之前。
- **[drift-2] capability-map §六.6 章节位置 + 路径修复**：(a) `### 6.6` 移到 `§六.5` 之后；(b) §六.5 line 421 验证命令路径修正为 `openspec/changes/archive/2026-08-25-2026-08-24-adr-0061-02-...`。
- **[drift-3] capability-map §二 line 90 词汇表补充**：状态词汇表增加 `✅ Closed（评审通过，已 ship 不需再实施，如 G10/G12/G13/G14/G15）`。
- **[drift-4] capability-map §二 标题措辞更新**：line 87 标题去 "15 项未 ship" → "15 项追踪（含 5 项已 ✅ Closed）"。
- **[drift-5] capability-map §八 闭环 1 + 闭环 2 G10/G12 stale 行同步**：(a) 闭环 1 line 463 G15 表格行；(b) 闭环 1 line 506 G12 row (ADR-0080 v1.2 解耦)；(c) 闭环 1 row 2 ADR-0074 Promotion 状态；(d) 闭环 1 row 3 G10 (ADR-0083 Approved)；(e) 闭环 2 row 3 G10 同类 stale 同步。
- **[drift-6] capability-map §八.5 line 538-547 重复排期删除**：保留评审通过后 Sprint 24/25/26 排期表，删除 v1.2 阶段 "下个 Sprint" / "Phase 6 中后期（spike 模式）" 旧排期。
- **[drift-7] ADR-0071 / ADR-0074 文件内部 footer + header 同步**："(待架构组评审)" → "(Promotion 评审通过 2026-08-25)"。

### P1 — 中等 drift（3 处，~10 分钟）

- **[drift-8] capability-map §六.1.2 / §六.3 ctest 计数同步**：line 325 + line 385 `184/184` → `185/185`。
- **[drift-9] capability-map §一 line 31 覆盖范围 + line 28 标题**：`Sprint 22` → `Sprint 23 (含 T14/T16 后置增补)`；`（22 项）` → `（23 项，L4 含 #23 T14 v1.2 后置增补）`。
- **[drift-10] capability-map §三 line 124 + 131 "零工程"段计数**：`22 项` → `23 项`。

### P2 — 轻微 drift（3 处，~5 分钟）

- **[drift-11] capability-map §二 line 91 词汇对齐**：`**分层标记**` → `**性质标记**`。
- **[drift-12] capability-map §一 L4 表头加注**：`（4 项）` → `（4 项，#23 T14 v1.2 后置增补）`。

### §七 变更记录追加

- **v1.3.1 行** 记录本 change ship。

**BREAKING**: 无。

## Capabilities

### New Capabilities

- `capability-application-map-v1-3-drift-fix`: capability-application-map v1.3 文档结构同步 — 锁定 12+4 处 drift 修复范围，明确"不修改源代码/不重编号 ADR/不修改现有 spec"的 Non-goals。

### Modified Capabilities

无。本 change 不修改任何现有 capability 的 REQUIREMENTS。

## Impact

- **代码**: 0 文件
- **ADR**: 2 文件（adr-0071 + adr-0074 仅 footer + header 状态注记同步）
- **docs/architecture/**: 1 文件（`capability-application-map-2026-08.md`，约 30 处局部文本 + 2 处章节位置调整 + 1 处 §七 追加）
- **OpenSpec specs/**: 0 文件（现有 spec 未触及）
- **OpenSpec archive/**: 0 文件（保留归档目录不动）
- **git history**: 1 commit (`docs(cap-map): v1.3.1 drift fix`)
