# analysis-status-snapshot-sync

## Why

三方独立审查（Sisyphus + Oracle + Metis）确认 3 份架构分析文档（`adr-implementation-status-gap-analysis.md` / `llm-native-blueprint-vs-code-gap-analysis.md` / `layer-based-missing-capabilities-analysis.md`）存在 **ADR 状态过时**与**定位声明过度**两类问题：

**A 类：状态过时（ADR 状态已变更但分析文档未同步）**
1. **`adr-implementation-status-gap-analysis.md` L166-168**：声称 ADR-0068/0069/0070 均"🔍 Proposed (Wave 1 在审)"，实际 ADR-0068 ✅ Approved（2026-08-03，后经 v1.1-v2.0 持续修订）、ADR-0069/0070 🟡 Partial（2026-08-04）。Header 声称"最后更新 2026-08-28"（git 实证确实持续更新）却漏刷这些行 —— 文档自我矛盾。
2. **`adr-implementation-status-gap-analysis.md` §一 表 L16**：把 ADR-0074/0075 列为 Proposed，实际 ADR-0075 ✅ Approved（2026-08-18）、ADR-0074 ✅ Approved（2026-08-25）。
3. **`llm-native-blueprint-vs-code-gap-analysis.md` §一 L14/L23**："8 个 ADR 全部 🔍 Proposed，0 Approved"，§2.1 L34 ADR-0071、§2.4 L92 ADR-0074 均标 Proposed；实际 0071/0074 ✅ Approved（2026-08-25）、0075 ✅ Approved（2026-08-18）。git 实证该文件 ec27bad 后从未刷新。
4. **`layer-based-missing-capabilities-analysis.md`**：L1-1/L1-2/L804-806 仍写"建议: 新增 ADR-0068/A tool-coordinator middleware"（现在式），实际 ADR-0068 ✅ Approved（08-03）、ADR-0069 🟡 Partial（08-04 middleware ship）；ADR-0085（08-28 Approved）0 引用。

**B 类：定位声明过度（"唯一事实源"自封）**
5. **`adr-implementation-status-gap-analysis.md`** 被 `docs/GOVERNANCE.md:68` + `docs/README.md:152` + `docs/architecture/README.md:17` 三处称为"**ADR 状态唯一事实源**"，但该文档自身状态滞后（见 A1/A2）——自封权威与实况不符。
6. **数据陈旧**：gap-analysis 内 `ctest 97/98`（L417）等计数停留在 07-30，当前实际 184/185；"73 个 ADR"计数过时。

## What Changes

**核心原则**：分析文档是"滚动快照"而非"事实源"；ADR 状态以 `docs/adr/*.md` 各自状态行为准。

1. **`docs/architecture/adr-implementation-status-gap-analysis.md`**：
   - L166-168 三行状态更新：0068 → ✅ Approved (2026-08-03)；0069/0070 → 🟡 Partial (2026-08-04)
   - §一 总表计数重算（Approved/Partial/Proposed 数量与占比）
   - L16 表更新（0074/0075 → Approved）
   - Header 加快照横幅："⚠️ 本文档为滚动快照，ADR 状态以 docs/adr/*.md 各自状态行为准；本表滞后风险自负"
   - L417 `ctest 97/98` → 当前实测（以 doc_metrics / AGENTS.md 184/185 为准）
   - "ADR 状态唯一事实源"措辞降级为"ADR 状态权威参照"
2. **`docs/architecture/llm-native-blueprint-vs-code-gap-analysis.md`**：
   - §一 总表 + §2.1/§2.4 状态更新（0071/0074 → ✅ Approved 2026-08-25；0075 → ✅ Approved 2026-08-18）
   - 文首加快照横幅："2026-08-03 基线快照，状态已过期，决策请以 ADR 文件为准"
3. **`docs/architecture/layer-based-missing-capabilities-analysis.md`**：
   - L1-1/L1-2/L804-806 行尾加落地标注："→ 已落地：ADR-0068 ✅ Approved（2026-08-03）/ ADR-0069 🟡 Partial（2026-08-04 middleware ship）"
   - 文首快照横幅 + 新增 ADR-0085 交叉引用（置于 L1-2 旁）
4. **引用同步（4 处）**：`docs/GOVERNANCE.md:68` + `docs/README.md:152` + `docs/architecture/README.md:17` 的"唯一事实源"措辞 → "ADR 状态权威参照（以 docs/adr/*.md 状态行为准）"
5. **附带修复**：`docs/README.md` 中 ADR-0070 行 🔍 Proposed → 🟡 Partial（与 adr-0070 文件实际一致）

## Capabilities

### New Capabilities

无（纯文档状态同步，不引入新能力）

### Modified Capabilities

无（架构分析文档不定义 spec 级别行为）

## Impact

- **文档**：3 份架构分析文档 + `docs/GOVERNANCE.md` + `docs/README.md` + `docs/architecture/README.md`，共 6 个文件
- **代码**：零改动
- **测试**：零改动
- **验证**：`adr_lint.py` / `docs_drift_audit.py` / `openspec validate` 全部通过
- **注意**：`docs/GOVERNANCE.md` 的"其他文档禁止维护状态副本表"数据纪律条款**不改**，仅改"唯一事实源"指向声明