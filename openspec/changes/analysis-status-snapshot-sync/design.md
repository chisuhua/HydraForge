# Design: analysis-status-snapshot-sync

## Context

3 份架构分析文档（gap-analysis / llm-native-blueprint / layer-based-missing）作为"ADR 状态权威参照"已被 3 处文件（GOVERNANCE/README/architecture-README）称为"唯一事实源"，但文档自身状态滞后于 ADR 文件最新状态（08-03 ~ 08-28 期间多次 ship 后未刷新）。本 change 把这些分析文档降级为"滚动快照 + 权威参照"，并把状态指向统一到 `docs/adr/*.md` 各自状态字段。

## Scope Boundaries

### 范围 IN

#### A. `docs/architecture/adr-implementation-status-gap-analysis.md`

- **§一 总览表 L12-22**：重算 Approved/Partial/Proposed 计数与占比（与 README `docs/README.md` ADR 表对齐）
- **§一 L16 起草 6 LLM-native 表**：ADR-0074/0075 状态更新为 ✅ Approved（2026-08-25 / 2026-08-18）
- **§二 2.3 L155-168 表**：ADR-0068/0069/0070 状态更新
- **Header 加快照横幅**："⚠️ 本文档为滚动快照，ADR 状态以 `docs/adr/*.md` 各自状态行为准；本表滞后风险自负" + "本表修订记录见 git log"
- **§决策 5 / §1 文档定位段**："ADR 状态唯一事实源" → "ADR 状态权威参照（最终以 `docs/adr/*.md` 为准）"
- **§数据 L417 等**：`ctest 97/98` 等陈旧计数更新（使用 doc_metrics 实际值）

#### B. `docs/architecture/llm-native-blueprint-vs-code-gap-analysis.md`

- **§一 总表 L14/L23**：8 个 ADR 状态更新（0071/0074/0075 → ✅ Approved）
- **§2.1 L34 ADR-0071** 状态行更新
- **§2.4 L92 ADR-0074** 状态行更新
- **文首加快照横幅**："2026-08-03 基线快照（ec27bad 提交日），状态已过期，决策请以 ADR 文件为准"

#### C. `docs/architecture/layer-based-missing-capabilities-analysis.md`

- **L1-1 L198-210** "建议: 新增 ADR-0068" 段尾加：*"→ **已落地**：ADR-0068 ✅ Approved（2026-08-03）"*
- **L1-2 L212-228** "建议 ToolCoordinator 改 middleware" 段尾加：*"→ **已落地**：ADR-0069 🟡 Partial（2026-08-04 middleware ship）"*
- **L804-806** 拟新增 ADR-0069/ADR-0070 行加：*"→ ADR-0069 🟡 Partial（2026-08-04）；ADR-0070 🟡 Partial（2026-08-04）"*
- **L1-2 旁新增交叉引用**：*"另见 ADR-0085（Cross-Cutting Pattern PDK，2026-08-28 ✅ Approved）提供横切 DSL `*.cc.md` 替代方案，与本节 hook 机制互补"*
- **文首快照横幅**：*"2026-07-31 数据基线 v1.2.1；后续 ship 见 capability-application-map-2026-08.md（当前架构现状权威）"*

#### D. 4 处"唯一事实源"引用同步

- **`docs/GOVERNANCE.md:68`**：`本仓库的 ADR 状态基线 (唯一事实源) ...` → `本仓库的 ADR 状态权威参照（最终以 docs/adr/*.md 各自状态行为准）`
- **`docs/README.md:152`**：同上去重措辞
- **`docs/architecture/README.md:17`**：同上去重措辞
- **`docs/architecture/adr-implementation-status-gap-analysis.md` 自身 §决策 5**：同上去重措辞

#### E. 附带修复

- **`docs/README.md`**：ADR-0070 行 🔍 Proposed → 🟡 Partial（与 `adr-0070-declare-command.md` 文件实际一致）

### 范围 OUT

- 数据纪律条款本身（`docs/GOVERNANCE.md` 的"其他文档禁止维护状态副本表"）：不动
- 文档头四字段规范（生成日期/最后验证/作者/状态）：不动（分析文档已有生成日期/作者/状态，可选加最后验证）
- 3 份分析文档的 §决策 段（仅修状态行，不动分析内容）
- 6 幻影主题机制缺口：与 `adr-0080-and-bus-api-alignment` 同样 defer

## Design Decisions

### D1 — "唯一事实源" → "权威参照（最终以 docs/adr/*.md 为准）"

**理由**：
- 真相源排序 = 代码 + ADR 文件各自状态字段；分析文档只是 **view**，不是 source
- 当前 4 处声明散布 GOVERNANCE/README/architecture-README 3 个文件，**必须 4 处同步**（否则会留下 1 处声称"唯一事实源"导致读者继续引用旧版）
- 措辞降级而非删除——仍承认分析文档的"权威参照"地位（提供按维度组织的状态视图，比读者自行 grep 100+ ADR 更方便）

**反方论据**：完全删除"唯一事实源"措辞可能让分析文档失去权威性，被其他文档当普通参考资料
**裁决**：降级为"权威参照"而非删除

### D2 — 计数更新使用 doc_metrics 实测，不硬编码

**理由**：
- `python3 tools/doc_metrics.py` 是项目自带的文档计数工具，可复现
- 避免硬编码数字在下次 ship 后再次过时
- `ctest 184/185` 计数以 `ctest --output-on-failure` 实测为准
- ADR Approved/Partial/Proposed 计数以 `python3 tools/adr_lint.py` 报数为准

**反方论据**：硬编码数字能让读者一眼看清"修复后的状态"（不必跑工具）

**裁决**：在文档中嵌入可复现命令 + 一次实测快照数字（如"截至 2026-09-01：184/185 ctest PASS，52 ✅ + 11 🟡 + 13 🔍 Proposed，详见 doc_metrics 实测"）

### D3 — 快照横幅格式遵循 architecture/ 目录头四字段规范

**理由**：
- `docs/architecture/README.md` 规定分析文档头 4 字段：生成日期/最后验证/作者/状态
- 现有 3 份分析文档均符合；本 change 在 Header 加横幅时**不破坏**四字段
- 横幅作为新段落置于"## 一、..."正文之前

### D4 — layer-based 文档加 ADR-0085 交叉引用

**理由**：
- ADR-0085（Cross-Cutting Pattern PDK，2026-08-28 ✅ Approved）内容与 L1-2（hook 机制）相邻但不重叠
- Metis 审查发现 layer-based 文档对 ADR-0085 0 引用是不合理的（与 hook 机制相关）
- 引用而非改写分析内容（最小动作）

## Risks

| 风险 | 概率 | 影响 | 缓解 |
|---|---|---|---|
| **状态更新数字下次又过时** | 高 | 中 | 嵌入可复现命令 + Last-Verified 日期 |
| **4 处"唯一事实源"引用漏改** | 中 | 中 | tasks.md 显式列 4 处 |
| **GOVERNANCE 数据纪律条款被误改** | 低 | 高 | D1 明确仅改"唯一事实源"措辞，不动数据纪律条款 |
| **快照横幅破坏 architecture/ 头字段规范** | 低 | 中 | D3 明确横幅作为新段落，不动 4 字段 |
| **README 中 ADR-0070 行状态再次与文件不一致** | 中 | 低 | 文档加注释"状态以 `docs/adr/adr-0070-declare-command.md` 为准" |

## Verification Gates

- ✅ `python3 tools/adr_lint.py` exit 0（**新增 ERROR = 0**；20+ warning 允许保留）
- ✅ `python3 tools/docs_drift_audit.py` 0 DRIFT
- ✅ `python3 tools/doc_metrics.py` 实测与文档声明的"截至日期"计数一致
- ✅ `openspec validate analysis-status-snapshot-sync --strict` PASS
- ✅ `grep "唯一事实源" docs/ -r` 0 命中（已降级为"权威参照"）
- ✅ `grep "唯一事实源" docs/GOVERNANCE.md docs/README.md docs/architecture/README.md` 0 命中
- ✅ ADR-0068/0069/0070/0071/0074/0075 状态在 3 份分析文档中表述一致（`docs/adr/*.md` 优先）
- ✅ 3 份分析文档文首均含快照横幅
- ✅ `docs/README.md` ADR-0070 行 🟡 Partial（与 adr-0070 文件一致）
- ✅ `docs/architecture/layer-based-missing-capabilities-analysis.md` 至少 1 处引用 ADR-0085

## Dependencies

### 不依赖代码
- 纯文档同步

### 需遵守
- `docs/architecture/README.md` 文档头四字段规范
- `docs/GOVERNANCE.md` 数据纪律条款（除"唯一事实源"措辞外）

## Out of Scope (V2 / 其他 change)

- 3 份分析文档的 §决策 段重组与重写（仅修状态行）
- 6 幻影主题机制修复（与 `adr-0080-and-bus-api-alignment` 同样 defer）
- 引入自动状态同步脚本（每日 cron 跑 `adr_lint.py` + diff 写入 gap-analysis）

## Success Criteria

- 3 份分析文档 + 4 处引用文件按 D1-D4 决策完成修改
- 9 条 verification gate 全部通过
- 0 改 0 回归（纯文档）
- OpenSpec archive 完成
