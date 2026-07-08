# Proposal: ADR 文档对齐 Hotfix (fix-adr-doc-alignment-hotfix-2026-07-08)

> **STATUS: ACTIVE** 🔵
> **创建日期**: 2026-07-08
> **创建者**: Sisyphus
> **关联审查报告**: 2026-07-08 架构文档对齐审查 (3 P0 + 4 P1 + 3 P2 级别问题)
> **关联 Metis 审查**: session `ses_0bf414b4affe5zB7zN06vHudKN` (Conditional Go + 4 项关键修正)
> **原始 mega-change**: `openspec/changes/fix-adr-doc-alignment-2026-07-08/` (47 tasks) — 拆分为本 hotfix + 2 个 follow-up
> **关联 OpenSpec changes**:
>   - 原始 change `fix-adr-doc-alignment-2026-07-08` (47 tasks) — 标记为拆分记录
>   - 后续 follow-up: `fix-adr-naming-policy-2026-07-08` (Change B, P0-2/3 SLASH 统一, 2h)
>   - 后续 follow-up: `fix-adr-doc-alignment-p2-cleanup-2026-07-08` (Change C, P2 清理, 2h)
> **关联 ADRs**: ADR-0030 (Async V2), ADR-0034 (Model Router), ADR-0036 (3-Layer Service, 软归档)
> **前置依赖**: 无
> **后续依赖**: 解锁 Phase 5 B2 (C13/C14/C15) 实施前置条件
> **估时**: ~30 min (15 tasks, 0 代码变更)

## Why

2026-07-08 架构文档对齐审查发现 10 处对齐问题 (3 P0 + 4 P1 + 3 P2)。其中 **4 个问题属于"hotfix"级** (零依赖、低风险、纯文档)，可独立 ship 而不阻塞后续命名政策变更 (Change B) 与 P2 清理 (Change C)：

1. **P0-1 (部分)**: `STATUS-GLOSSARY.md` 5 处状态错误 (ADR-0021/0022/0023/0030/0034 错为 Proposed/Partial/Not Implemented；ADR-0036 状态在本 hotfix 拆分为归档处理 — 见 P1-2)
2. **P1-1**: `docs/adversarial-reviews/README.md` line 84 文件名拼写错误 (`oopenspec` → `openspec`)
3. **P1-3**: `decisions-2026-07-07.md` D5 step 2/step 3 重复 + 签字状态矛盾
4. **P1-2**: ADR-0036 软归档 (renumber 到 ADR-0045/0046 后原文件未归档)

**为什么 now + 拆分**: 原 mega-change (47 tasks) 包含 P0-2/3 命名政策变更，需同步改 PDK 代码 + C13/C14/C15 spec/tasks (跨多 owner 协调)。将 hotfix 拆为独立 change 允许:
- 本 hotfix **30 分钟内 ship**，立即解锁 B2 启动
- Change B (P0-2/3 命名政策) 可与 B2 owner 协商时序，2h 落地
- Change C (P2 清理) 推到 Sprint 21 follow-up，2h 完成

**Metis 审查修正 (应用)**:
- **Q4**: ADR-0036 状态 → `⛔ Superseded` + 从 STATUS-GLOSSARY 活跃表移除 (与 §1.6 任务冲突的修正)
- **Q5**: spec.md 改为 "README 表格用 📋 Audit, ADR 文件保留实际状态" (事实修正)
- 删除原 §5 任务 (C16 proposal SLASH 化) — 因 `inference.*` 在 C16 是合法事件 topic notation (ADR-0043 §9)

## What Changes

### 1. [P0-1] 修正 `docs/adr-management/STATUS-GLOSSARY.md` 5 处状态

按 ADR 文件实际状态修正：

| ADR | 当前错误 | 正确 |
|-----|---------|------|
| ADR-0021 (PDK) | 🔍 Proposed | ✅ Approved (2026-06-24) |
| ADR-0022 (Plugin Loading) | 🔍 Proposed | ✅ Approved (2026-06-24) |
| ADR-0023 (ToolResult) | 🟡 Partial | ✅ Approved (2026-06-24) |
| ADR-0030 (Async Runtime V2) | ❌ Not Implemented | 🔍 Proposed (2026-06-26) |
| ADR-0034 (Model Router) | ❌ Not Implemented | ✅ Approved (2026-07-02) |

(ADR-0036 不在本节修正 — 由 §4 软归档任务处理,标 `⛔ Superseded` 后从活跃表移除)

### 2. [P0-1 配套] 追加 STATUS-GLOSSARY 维护规则 #2 说明

在 STATUS-GLOSSARY.md 维护规则 #2 段追加:
> 同步方向: From `## 状态` 字段 → STATUS-GLOSSARY 状态表 (单向),任何 ADR 状态变更时 MUST 在同一次 commit 中同步。

### 3. [P1-1] 修正 README 拼写

`docs/adversarial-reviews/README.md` line 84:
```
| `ref-1-b2-oopenspec-arch.md` | ... |
```
→ 改为:
```
| `ref-1-b2-openspec-arch.md` | ... |
```

### 4. [P1-3] 修正 decisions-2026-07-07.md D5 实施步骤

**Step 编号去重** (line 99-105): 当前 step 2 与 step 3 重复 ("新增 DSLEngine::load_plugin(...) 公开方法" 出现 2 次),拆分为 5 步:
1. 删除 `DSLEngine` 构造中默认注入逻辑
2. 新增 `DSLEngine::load_plugin(const std::string& name)` 公开方法
3. 添加 `test_load_plugin.cpp` 单元测试
4. 迁移现有测试/示例
5. 更新 `lib/dsl-reference.md` §3.2 记录 API 变更

**签字状态修正** (line 105): "待签字确认" → `🟡 待签字 (2026-07-08)`, 后续如找到 D5 决策 author 再更新为 `✅ 已签字`。

### 5. [P1-2] ADR-0036 软归档 + 旧链接同步

**a. 软归档**:
- `git mv docs/adr/adr-0036-three-layer-service-protocol.md docs/archive/adr/`
- 归档文件头部追加 `> **⛔ DEPRECATED (2026-07-08)**` 横幅, 引用新 ADR-0045

**b. STATUS-GLOSSARY 同步**:
- ADR-0036 状态标注为 `⛔ Superseded` (被 ADR-0045 替代)
- 从活跃 ADR 状态表移除 (仅在 superseded 列表保留)

**c. README.md 同步**:
- `docs/README.md` 删除 `adr-0036-three-layer-service-protocol.md` 行

**d. plugin/README.md 同步**:
- `docs/adr/plugin/README.md` 追加 ADR-0036 renumber 注记

**e. 旧链接同步**:
- `docs/adr/adr-0030-async-runtime-v2.md:318` (引用 ADR-0036): 更新为 ADR-0045 或删除
- `docs/handoff/2026-07-06-architecture-completion.md:51` (保留 0036/0037 引用): 更新

## Non-goals

- **不修改**任何 C++/CMake 代码
- **不创建**新 ADR 文件 (仅修订现有 ADR 状态字段)
- **不修改** PDK 工具名 (P0-2 SLASH 统一 → Change B)
- **不修改** `lib/inference/*.md` (DOT→SLASH → Change B)
- **不修改** `decisions-2026-07-07.md` D3 命名映射 (P0-3 → Change B)
- **不修改** `decisions-2026-07-07.md` D5 描述段 (D5 实施步骤仅 step 去重, 不改 D5 决策内容)
- **不修改** STATUS-GLOSSARY.md 📋 双语义扩展 (P1-4 → Change C)
- **不修改** C16 proposal (经 Metis 审查, `inference.*` 是合法事件 topic notation,非违规)
- **不重跑** `tools/adr_relationships.py` (P2-1 → Change C)
- **不验证** C13 4 个 schema ship 状态 (P2-2 → Change C)
- **不修复** `pdk/llama_engine/` 缺 `llama.h` 的 LSP 错误 (pre-existing,与本 change 无关)

## Capabilities

### New Capabilities

- `adr-doc-alignment-hotfix`: 本 hotfix 仅作为"首次手动对齐"产出, 不引入长期治理规则。后续 Change C 将基于本 hotfix 落地 cap, 添加 `tools/adr_lint.py` 验证支持。

### Modified Capabilities

无 (本 hotfix 不修改现有 spec-level behavior)

## Impact

### 文档文件 (6 个)

| 文件 | 变更类型 | 估时 |
|------|---------|:----:|
| `docs/adr-management/STATUS-GLOSSARY.md` | 5 处状态修正 + 维护规则 #2 同步方向说明 + ADR-0036 Superseded 标注 | 15 min |
| `docs/adversarial-reviews/README.md` | line 84 拼写修正 (1 处) | 1 min |
| `docs/adversarial-reviews/decisions-2026-07-07.md` | D5 step 编号去重 (1 处) + 签字状态标注 (1 处) | 5 min |
| `docs/archive/adr/adr-0036-three-layer-service-protocol.md` | `git mv` + DEPRECATED 横幅 | 5 min |
| `docs/README.md` | 删除 ADR-0036 行 (1 处) | 1 min |
| `docs/adr/plugin/README.md` | 追加 ADR-0036 renumber 注记 (1 处) | 1 min |
| `docs/adr/adr-0030-async-runtime-v2.md` | 更新 ADR-0036 旧链接 (1 处) | 2 min |
| `docs/handoff/2026-07-06-architecture-completion.md` | 更新 ADR-0036/0037 旧引用 (1 处) | 2 min |

### 系统影响

- **零代码变更** — 本 change 100% 文档
- **零 ctest 影响** — 无 src/ 修改
- **零 API breaking** — 无 C++ 公开 API 变更
- **零数据迁移** — 无运行时数据

### 阻塞/解锁关系

- **解锁** Phase 5 B2 (C13/C14/C15) 实施前置 (命名一致性)
- **解锁** `tools/adr_lint.py` 持续集成检查通过 (STATUS-GLOSSARY 一致)
- **不阻塞** Change B/C — 本 hotfix 独立 ship
- **不阻塞** Sprint 21 后续工作

### 关联 Sprint 决策

- **Sprint 21**: 本 hotfix 立即 ship (~30 min)
- **Sprint 21 follow-up**: Change B (P0-2/3 SLASH 统一) 与 B2 owner 协调时序
- **Sprint 21 follow-up**: Change C (P2 清理) 由独立 owner 实施
