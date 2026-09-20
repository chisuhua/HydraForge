# adr-0086-v1-1-harness-change-confounder

**优先级**: P0 | **来源**: ADR-0086 v1.0 首次实施 + v1.1 amendment (combined)
**阶段**: phase-6c | **分类**: rsi-meta-cognitive
**类型**: feature
**主题**: HarnessChange confounder 检测 + baseline sampling + data-freshness 判定算法

## 架构依据

ADR-0086 自 2026-08-31 创建以来仅作为文档存在（状态 🔍 Proposed, V1 不强制实施）。全库无任何 ADR-0086 引用类型的实现代码（`grep` 证实：`include/agenticdsl/types/attribution_record.h` / `tests/test_credit_assignment.cpp` / `VersionPairDiff` / `AttributionRecord` / `ConfounderRecord` 全部不存在）。本 change 是 ADR-0086 首次代码实施 + v1.1 amendment 的合并落地。

**关键依赖链**（per Oracle M2 + continuation 共识）：

```
ADR-0086 (本 change) ─→ C3 h-d-m-transition-guard (硬阻塞, 待 fill)
                                  ─→ C4 harness-rsi-pilot
```

C3 spec 引用 `ConfounderKind::HarnessChange` 类型 + `judge_data_freshness()` 数据新鲜度算法。两类型尚未定义 = 编译失败 = C3 无法启动。本 change 是 Phase 6c MetaRSI-v1 唯一硬前置。

**Oracle dual-agent review 已 ship**（2026-09-20）：
- Metis `bg_ba048665` — 5 Critical + 5 Major
- Oracle `bg_fed9d7c0` — 7 Critical + 6 Major + 8 Minor
- Oracle spot-check `ses_f45456c38ffeO6cmQwiUQwlcdC` — 5 stale + New Issue 1 (GenomeVersion 重复定义)

所有 7 Critical 修正已应用至 proposal + spec + tasks。

**External framework alignment**（决策 8，documentation-only）：
- 月谈AI "验证是 RSI 生命线" ↔ 决策 3 治理绑定 + 决策 7 fail-closed
- MetaRSI-v1 H→D→M 交通规则 ↔ HarnessChange kind 是 C3 TransitionGuard 首要消费者

## 范围

- **In Scope**:
  - v1.0 首次实施基线（5 类型 + VersionPairDiff.compare() 算法） — `include/agenticdsl/types/attribution_record.h` + `attribution_version_pair_diff.h` + `src/evolution/{attribution_record,version_pair_diff}.cpp`
  - v1.1 增量（enum HarnessChange + kMinBaselineSamples=5 constant + judge_data_freshness 完整版算法）
  - ADR-0086 文档修订 — 决策 2/4 修订 + 决策 8/9 新增 + 不变量 8/9 新增 + 状态翻转 🔍 → ✅ (v1.1)
  - `tests/test_credit_assignment.cpp` 首次创建 — 10 测试用例（6 v1.0 + 4 v1.1）
  - 文档同步 — `docs/research/rsi-three-operators-hydraforge-mapping-2026-09.md` + `docs/architecture/self-evolution-architecture-2026-08.md`
- **Out of Scope**:
  - ❌ 不实现 `walk_ancestors` 接口（C3 范围，本 change 仅定义算法契约 + 类型 stub）
  - ❌ 不实现 C3 transition guard（C3 范围）
  - ❌ 不实现 DistillationRecord.genome_version 扩展（盲点 #6 闭环需独立 follow-up change）
  - ❌ 不修改 ConfounderRecord 序列化 schema（C++ 编译期 enum 不影响磁盘格式）
  - ❌ 不重写 v1.0 既有决策语义（仅 additive v1.1 决策 8/9 + 不变量 8/9）

## 关键场景

- GIVEN v1.0 首次实施 ship（10 tests PASS）+ v1.1 HarnessChange schema + kMinBaselineSamples 守卫就位
  WHEN C3 fill 时引用 `agenticdsl::evolution::ConfounderKind::HarnessChange` 类型
  THEN 编译通过，C3 spec 与 C++ 代码契约一致

- GIVEN v1.1 `judge_data_freshness({name="g", version=3}, {name="g", version=5}, registry)` 实施
  WHEN 数据版本在谱系中且后续 Harness 变更（v4 改了 harness B，data v3 harness A）
  THEN 返回 `AttributionVerdict::Confounded` + reason="harness changed after data generation" + 自动构造 HarnessChange confounder（修正 Oracle 🔴-6 假阴性）

- GIVEN v1.1 `judge_data_freshness({name="g", version=5}, {name="g", version=5}, registry)` 实施
  WHEN data.version == current.version（excludes-self 边界）
  THEN 返回 `AttributionVerdict::Attributed` 直接 fast-path，不调 walk_ancestors（修正 excludes-self 假阳性）

- GIVEN v1.1 `VersionPairDiff::compare(child, parent, confounders)` 实施
  WHEN `parent.sample_count < kMinBaselineSamples (5)`
  THEN fail-fast 返回 `AttributionVerdict::Insufficient` + reason 含 "sample_count" + "5"，不进入 eval_delta 计算

- GIVEN ADR-0086 v1.1 ship（dual-agent review passed + 10 tests PASS + doc sync done）
  WHEN C3 fill author 引用 v1.1 决策 4 + 决策 9
  THEN 治理路径合规，ADR-0086 ✅ Approved (v1.1) 状态翻转可见

## 技术约束

- MUST 路径严格遵守 v1.0 不变量 6：attribution 类型放 `include/agenticdsl/types/`（非 `contract/`）
- MUST namespace 统一 `agenticdsl::evolution`（修正 Oracle 🟠-3，避免与 `agenticdsl` 顶层冲突）
- MUST `GenomeVersion struct` 单一所有权（修正 spot-check New Issue 1：C3 复用本文件避免 ODR 违规）
- MUST `kMinBaselineSamples=5` constant 与 T14 实现细节对齐（基于 Hotelling T² 经验值）
- MUST `judge_data_freshness` 算法完整版：fast-path + lineage walk + harness string comparison（消除 Oracle 🔴-6 假阴性 + excludes-self 假阳性）
- MUST walk_ancestors 签名含 name 参数（修正 Oracle 🔴-5：`const std::string& name, uint64_t from_version, std::optional<uint64_t> to_version = std::nullopt`）
- MUST LineageWalk.intermediate_metadata 元素类型为 `Genome`（非 `GenomeMetadata`，因 GenomeMetadata 无 spec.harness 字段）
- MUST HarnessChange.source_id 包含 "@" + "→" 分隔符（不变量 9）
- MUST NOT 自动调度基线重复评估（不变量 8：调用方负责保证 sample_count >= 5）
- MUST NOT 修改 v1.0 既有决策语义（仅 additive v1.1）
- MUST NOT 引入 walk_ancestors 实现（C3 范围）
- SHOULD 实施分 atomic commits — v1.0 基线 commit + v1.1 增量 commit（保持回溯能力，AGENTS.md 模式 #4）

## 验收标准

- [ ] **Pre-flight (0.5d)**：决策 4 schema + kMinBaselineSamples=5 调研 + walk_ancestors 签名锁定（与 C3 同步）
- [ ] **Dual-agent review DONE**：Metis `bg_ba048665` + Oracle `bg_fed9d7c0` + spot-check `ses_f45456c38ffeO6cmQwiUQwlcdC` 已 ship，7 Critical 修正已应用至 proposal + spec + tasks
- [ ] **v1.0 首次实施**：`include/agenticdsl/types/attribution_record.h` + `attribution_version_pair_diff.h` + `.cpp` 全部创建，5 类型 + VersionPairDiff.compare() 算法 + 6 测试用例（per ADR-0086 v1.0 决策 1/2）
- [ ] **v1.1 增量**：kMinBaselineSamples constant + ConfounderKind::HarnessChange enum + HarnessChangeRecord struct + judge_data_freshness 完整版算法 + 4 测试用例
- [ ] **10 测试 PASS**：6 v1.0 + 4 v1.1（修正 Oracle 🔴-1：原 4 用例不足）
- [ ] **不变量编号修正**：v1.1 不变量 8/9，避免与 v1.0 不变量 5/6 冲突
- [ ] **ADR-0086 状态翻转** 🔍 Proposed → ✅ Approved (v1.1)，文末追加 ship 证据段
- [ ] **Doc sync**：`rsi-three-operators-hydraforge-mapping-2026-09.md` + `self-evolution-architecture-2026-08.md` 引用 v1.1
- [ ] **ctest 全量零回归**：`ctest --output-on-failure` 0 新增 failure（除已知 pre-existing）
- [ ] **tools/adr_lint.py + tools/docs_drift_audit.py**：0 errors + 0 DRIFT
- [ ] **Archive**：openspec archive 移至 `archive/2026-09-20-adr-0086-v1-1-harness-change-confounder/` + `git ls-files` 验证 4 文件完整（Day-5 lesson）
- [ ] **C3 通知**：填 C3 proposal/tasks 引用 v1.1 决策 4 + 决策 9 + GenomeVersion 复用本文件