# Proposal: ADR 文档对齐修复 (fix-adr-doc-alignment-2026-07-08) — **拆分记录**

> **STATUS: SUPERSEDED** 🟡
> **创建日期**: 2026-07-08
> **创建者**: Sisyphus (基于 2026-07-08 架构文档对齐审查产出)
> **状态**: **本 change 已被 Metis 审查后拆分, 不再 apply**
> **拆分结果**:
>   - **Change A (hotfix)**: `openspec/changes/fix-adr-doc-alignment-hotfix-2026-07-08/` (30 min ship, 15 tasks)
>   - **Change B (naming policy)**: `openspec/changes/fix-adr-naming-policy-2026-07-08/` (2h, 待与 B2 协调时序)
>   - **Change C (P2 cleanup)**: `openspec/changes/fix-adr-doc-alignment-p2-cleanup-2026-07-08/` (2h, 推到 Sprint 21)
> **拆分原因**: Metis 审查 `ses_0bf414b4affe5zB7zN06vHudKN` 指出 47 tasks 单 change 风险过高, 拆分后每个 change 独立 ship 验证风险更可控
> **本文件保留**: 保留原 47 tasks 完整 proposal + design + spec + tasks 作为历史审计记录, 防止 review 上下文丢失
> **关联审查报告**: `docs/adversarial-reviews/main-report.md` + 专项审查 (3 P0 + 4 P1 + 3 P2 级别问题)
> **关联 OpenSpec changes**:
>   - `phase5-b2-arch-schemas` (C13) — 命名约定修复直接消费者
>   - `phase5-llama-engine-plugin` (C14) — 命名约定修复直接消费者
>   - `phase5-batching-queue-plugin` (C15) — 命名约定修复直接消费者
>   - `phase5-illmprovider-call-chain-v2` (C16) — 命名章节对齐
> **关联 ADRs**: ADR-0021 (PDK), ADR-0022 (Plugin Loading), ADR-0023 (ToolResult), ADR-0030 (Async V2), ADR-0034 (Model Router), ADR-0036 (3-Layer Service, 待归档), ADR-0043 (PDK Naming), ADR-0045/0046 (renumbered from 0036/0037)
> **前置依赖**: 无 (审查产出)
> **后续依赖**: B2 三 Change (C13/C14/C15) 启动前置 + C16 命名章节修订
> **估时**: ~4 h (P0 + P1 全部) + ~2 h (P2)

## Why

2026-07-08 架构文档对齐审查发现 10 处文档对齐问题，按严重度分 3 级 (P0 阻塞 + P1 关键 + P2 一般)。其中 **3 个 P0 问题直接阻塞 Phase 5 B2 (C13/C14/C15) 实施**：

1. **STATUS-GLOSSARY.md 状态表严重过时** — 6 个 ADR (0021/0022/0023/0030/0034/0036) 的"权威状态"与 ADR 实际状态字段冲突。STATUS-GLOSSARY 自身维护规则 #2 要求"任何 ADR 状态变更时，README、relationships.md、SPECS-ALIGNMENT.md **必须同步**"，但 STATUS-GLOSSARY 自身从未同步。

2. **PDK 工具命名约定 3-way 矛盾** — ADR-0034 (P0 已 ship) + ADR-0043 (新起草) 明确"slash (`/`) 是唯一合法分隔符"；`lib/inference/engine.md`/`model.md` (C14 实际代码) 使用 SLASH ✅；但 C13 5 个 .md 文件 (`prefix_cache.md`/`kv_cache.md`/`decoding.md`/`cloud_engine.md`/`batching.md`) 仍用 DOT ❌；`decisions-2026-07-07.md` D3 决策文件用 DOT ❌；`C16 proposal` 命名章节用 DOT ❌。这意味着 B2 实施启动后，要么 5 个 C13 .md 文件破坏 ADR-0034 已 ship 的命名规范，要么与 decisions 决策冲突。

3. **`decisions-2026-07-07.md` D3 映射表与 C14 实际实施脱节** — D3 第 38-49 行"原名 `llama_engine.init` → 新名 `inference.engine_init`"声称与 C14 proposal 同步；但 C14 proposal (2026-07-07 18:03 更新版) 实际工具名是 `inference/engine/init` (SLASH 风格)。映射表"原名"和"新名"双双错误，决策文件未能反映其自身的实施结果。

**为什么 now**: B2 三 Change (C13/C14/C15) 计划 2026-07-08 后启动实施。如果不先修复 P0-1/2/3，C13 实施时会因命名约定矛盾而返工，STATUS-GLOSSARY 状态错误也会让 `adr_lint.py` CI 检查失败，CI 红绿灯阻塞 merge。

## What Changes

### 1. [P0-1] 修正 `docs/adr-management/STATUS-GLOSSARY.md` 状态表

**修复 6 处状态错误**：

| ADR | 错误 | 正确 |
|-----|------|------|
| ADR-0021 (PDK) | 🔍 Proposed | ✅ Approved (2026-06-24) |
| ADR-0022 (Plugin Loading) | 🔍 Proposed | ✅ Approved (2026-06-24) |
| ADR-0023 (ToolResult) | 🟡 Partial | ✅ Approved (2026-06-24) |
| ADR-0030 (Async Runtime V2) | ❌ Not Implemented | 🔍 Proposed (2026-06-26) |
| ADR-0034 (Model Router) | ❌ Not Implemented | ✅ Approved (2026-07-02) |
| ADR-0036 (3-Layer Service) | ❌ Not Implemented | 🔍 Proposed (2026-05-28) |

### 2. [P0-2] 统一 PDK 工具命名为 SLASH 格式

按 ADR-0034 §命名约定 + ADR-0043 §1 决策 ("**唯一合法分隔符是 `/` (slash)**")：

**a. 重写 C13 5 个 `lib/inference/*.md` 工具名 (DOT → SLASH)**：
- `prefix_cache.configure` → `prefix_cache/configure`
- `kv_cache.configure` → `kv_cache/configure`
- `decoding.configure` → `decoding/configure`
- `cloud_engine.configure` → `cloud_engine/configure`
- `batching.submit_and_wait` → `batching/submit_and_wait`

**b. 重写 `docs/adversarial-reviews/decisions-2026-07-07.md` D3 整章**：
- 删 DOT 风格映射表 (8 行)
- 明确"统一 SLASH 格式 `inference/engine/init`"
- 删除"C13 架构工具命名边界"小节 (DOT 论证与 ADR-0034/0043 矛盾)
- 修正"保持与现有 `lib/inference/engine.md` 占位文件的 `inference.engine_init` 风格对齐" → 实际是 `inference/engine/init` (SLASH)

**c. 修正 `openspec/changes/phase5-illmprovider-call-chain-v2/proposal.md` 命名章节**：
- "命名统一"段：DOT → SLASH
- "文档修订"段：DOT → SLASH

### 3. [P0-3] 修正 `decisions-2026-07-07.md` D3 映射表

D3 第 38-49 行映射表替换为正确版本：

| 原名 | 新名 |
|------|------|
| `llama_engine/init` | `inference/engine/init` |
| `llama_engine/generate` | `inference/engine/generate` |
| `llama_engine/stream` | `inference/engine/stream` |
| `llama_engine/status` | `inference/engine/status` |
| `llama_model/load` | `inference/model/load` |
| `llama_model/unload` | `inference/model/unload` |
| `llama_model/list` | `inference/model/list` |
| `llama_model/switch` | `inference/model/switch` |

### 4. [P1-1] 修正 `decisions-2026-07-07.md` D5 实施步骤

- step 2 与 step 3 重复 ("新增 DSLEngine::load_plugin(...)" 出现 2 次)
- 修正为 5 步清晰序列：删除默认注入 / 新增 API / 添加单测 / 迁移示例 / 文档更新
- 决策签字状态：`待签字确认` → 标注实际签字日期 (如已签) 或明确为 `🟡 待签字`

### 5. [P1-2] 解决 ADR-0036/0037 编号冲突

归档 `docs/adr/adr-0036-three-layer-service-protocol.md` 至 `docs/archive/adr/`：
- 在归档文件头部加 DEPRECATED 横幅 (referenced by ADR-0045/0046 renumber 注释)
- 在 README.md 同步更新 (删除该 ADR 引用)
- 验证 `tools/adr_relationships.py` 重跑后无 ADR-0036 节点

### 6. [P1-3] 修正 `docs/adversarial-reviews/README.md` 文件名拼写

- line 84: `ref-1-b2-oopenspec-arch.md` → `ref-1-b2-openspec-arch.md` (去双 "o")

### 7. [P1-4] STATUS-GLOSSARY.md 增补 📋 双语义

在 STATUS-GLOSSARY.md 状态表中：
- 📋 Reserved | Reserved | 编号预留，无内容
- **新增** 📋 Audit | 审计补充 | impl-scope-audit 文档专用 (与 docs-code-drift-audit 配套使用)

README.md 中 12 个 `adr-*-impl-scope.md` 引用状态保持 📋 审计补充 (语义由 Audit 标签承担)。

### 8. [P2-1] 重跑 `tools/adr_relationships.py`

脚本当前缺 16 个 ADR 节点 (0035/0038-0046/0014/0029/0032 等)。重跑后：
- 验证 `relationships.md` 包含 38 个文件
- 检查脚本是否过滤了 `docs/adr/plugin/` 目录 (确认 ADR-0034 包含)
- 更新 `docs/adr-management/relationships.md` 状态统计表 (13 → 16 Approved etc.)

### 9. [P2-2] 验证 C13 4 个 schema 实际 ship 状态

`ref-3-lib-inference-state.md` (2026-07-06) 报告 C13 0/32 tasks complete，但 2026-07-07 后 lib/inference/ 出现 4 个新 .md 文件 (prefix_cache/kv_cache/decoding/cloud_engine)。需要：
- git log 检查 commit hash 确认 ship 状态
- 若已 ship，更新 master plan §十六.5 "7/7 子图覆盖率" 数字
- 若未 ship，记录为 7-子图状态对应表

### 10. [P2-3] 同步 ADR-0021 状态字段与 decisions D1

在 `docs/adr/adr-0021-pdk-design.md` ## 状态 段追加：
```
2026-07-08 update: §8 SamplerStrategy 接口被 decisions-2026-07-07.md D1 决策撤销 (B2 实施前对齐)。
```

## Non-goals

- **不修改**任何代码 (C++/CMake) — 本 change 是纯文档修复
- **不创建**新 ADR — 仅修订现有 ADR 状态字段
- **不重启**B2 三 Change (C13/C14/C15) — 仅修复其前置文档
- **不修改**master plan 内容 — 仅修正 master plan 中文档漂移数字
- **不增加**新 status label — STATUS-GLOSSARY 双语义扩展在 P1-4 范围内

## Capabilities

### New Capabilities

- `adr-doc-alignment`: ADR 文档对齐规范 (命名约定统一 + 状态同步规则 + 编号一致性 + 拼写/引用准确性)。该 capability 形式化以下契约：
  - **命名规则**: 所有 PDK 工具 MUST 使用 SLASH 分隔符 (`{plugin_namespace}/{component}/{action?}`)，DOT 仅允许 C++ method names
  - **状态同步**: STATUS-GLOSSARY.md MUST 与 ADR 文件实际状态字段保持一致 (autogen 校验)
  - **编号一致性**: ADR 编号 MUST 唯一；renumber 决策 MUST 同步归档旧编号文件
  - **文档引用**: 任何文档 MUST 使用现行文件路径；历史引用 MUST 在归档后更新

### Modified Capabilities

无 (本 change 是新 capability 引入，不修改现有 spec-level behavior)

## Impact

### 文档文件 (15+ 个)

| 文件 | 变更类型 | 估时 |
|------|---------|:----:|
| `docs/adr-management/STATUS-GLOSSARY.md` | 6 处状态修正 + P1-4 双语义 | 30 min |
| `docs/adr-management/relationships.md` | 重跑 (P2-1) | 30 min |
| `lib/inference/prefix_cache.md` | DOT→SLASH 工具名 | 10 min |
| `lib/inference/kv_cache.md` | DOT→SLASH 工具名 | 10 min |
| `lib/inference/decoding.md` | DOT→SLASH 工具名 | 10 min |
| `lib/inference/cloud_engine.md` | DOT→SLASH 工具名 | 10 min |
| `lib/inference/batching.md` | DOT→SLASH 工具名 | 10 min |
| `docs/adversarial-reviews/decisions-2026-07-07.md` | D3 整章重写 + D5 step 修正 | 45 min |
| `docs/adversarial-reviews/README.md` | line 84 拼写修正 | 1 min |
| `openspec/changes/phase5-illmprovider-call-chain-v2/proposal.md` | 命名章节 DOT→SLASH | 15 min |
| `docs/adr/adr-0021-pdk-design.md` | 状态字段追加 D1 决策注记 | 5 min |
| `docs/archive/adr/adr-0036-three-layer-service-protocol.md` | 移动 + DEPRECATED 横幅 | 15 min |
| `docs/README.md` | 删除 ADR-0036 引用 (P1-2) | 5 min |
| `docs/specs/architecture.md` | (可选) 命名规则段补充 | 15 min |

### 系统影响

- **零代码变更** — 本 change 100% 文档
- **零 ctest 影响** — 无 src/ 修改
- **零 API breaking** — 无 C++ 公开 API 变更
- **零数据迁移** — 无运行时数据

### 阻塞/解锁关系

- **解锁** Phase 5 B2 (C13/C14/C15) 实施前置
- **解锁** C16 命名章节一致性
- **解锁** `tools/adr_relationships.py` CI 检查通过

### 关联 Sprint 决策

- **Sprint 21 (待启动)**: B2 实施前必须先完成 P0 全部 3 项
- **不阻塞** Sprint 19/20 已 ship 内容
- **影响** master plan `docs/superpowers/plans/2026-07-03-phase5-self-bootstrapping.md` §5.5 编号重定义 (后续 P0 修复完成后回填)
