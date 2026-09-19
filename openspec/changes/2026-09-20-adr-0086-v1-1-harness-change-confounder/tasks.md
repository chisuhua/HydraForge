# Tasks: ADR-0086 v1.0 首次实施 + v1.1 Amendment

> **STATUS**: DRAFT (post Oracle dual-agent review `bg_fed9d7c0`, 7 Critical fixes applied to proposal + spec + tasks)
> **Target**: ADR-0086 v1.0 首次实施 + v1.1 amendment (合并落地)
> **总估时**: 5-7 days (修正 Oracle 🟠-2: 原 3-5d 偏低，v1.0 从零实施 + v1.1 增量合并)
> **Last Updated**: 2026-09-20 (post spot-check Oracle review `ses_f45456c38ffeO6cmQwiUQwlcdC`)

---

## 1. Pre-flight (0.5d)

- [ ] 1.1 决策 4 HarnessChange schema 字段对齐 `include/agenticdsl/genome/genome.h` `GenomeMetadata` 字段
  - 验证: `name + version + parent` 三个字段足够生成 `source_id = "name@old_version→new_version"`
  - 验证: `capture_mode` 字段是否需要记录在 HarnessChange 中 (当前 decision: 不需要，由 attribution 层推断)
- [ ] 1.2 决策 2 kMinBaselineSamples=5 与 T14 Hotelling T² 实现细节对齐
  - 读 `tests/test_behavioral_regression.cpp` 确认 Hotelling T² 方差估计使用的 sample_count 下界
  - 验证 5 次重复是否足够（文献支撑：Hasan 2025 prompts 1% coverage, Demystifying 2025 90% bottleneck）
- [ ] 1.3 决策 9 walk_ancestors 接口签名与 C3 amendment 协调（修正 Oracle 🔴-5/🔴-6）
  - **签名**: `virtual Result<LineageWalk, GenomeError> walk_ancestors(const std::string& name, uint64_t from_version, std::optional<uint64_t> to_version = std::nullopt) = 0;`
  - **LineageWalk schema** (修正 Oracle 🔴-6 类型): `{ std::vector<uint64_t> intermediate_versions; std::vector<agenticdsl::genome::Genome> intermediate_metadata; }` ← **Genome 含 spec.harness**，NOT GenomeMetadata
  - **Walk 行为契约**: closest-first / excludes-self / 10000 depth cap / 每版本 HMAC 校验 / 失败映射 NotFound | BrokenLineage | IntegrityViolation
  - 通知 C3 fill author 使用该签名 + struct 类型

## 2. Dual-agent review (Metis + Oracle) ✅ DONE

- [x] 2.1 派 Metis background `bg_ba048665`（intentionality / spec ambiguity / AI failure modes）— DONE 2026-09-20
- [x] 2.2 派 Oracle background `bg_fed9d7c0`（architecture / implementation feasibility / physical viability）— DONE 2026-09-20
- [x] 2.3 收集 2 份报告，输出 Critical/Major/Minor 分级修正清单 — DONE 2026-09-20
- [x] 2.4 应用所有 Critical 修正（7 项）至 proposal + spec + tasks — DONE 2026-09-20
- [x] 2.5 轻量 spot-check Oracle review `ses_f45456c38ffeO6cmQwiUQwlcdC`（针对 7 Critical 修正）— DONE 2026-09-20
- [ ] 2.6 应用 spot-check 修正（本 tasks.md 5 处 stale + C3 tasks.md GenomeVersion 复用）

## 3. Implementation (2.5d，修正 Oracle 🟠-2)

### 3.0 v1.0 首次实施 — 基线类型 + compare() 算法（修正 Oracle 🔴-1）

- [ ] 3.0.1 `include/agenticdsl/types/attribution_record.h` 首次创建
  - namespace `agenticdsl::evolution`（修正 Oracle 🟠-3 统一）
  - 5 类型：`AttributionRecord` / `AttributionMethod` / `AttributionVerdict` / `ConfounderRecord` / `VersionSnapshot`（per ADR-0086 v1.0 决策 1）
  - **GenomeVersion struct 定义**（C3 复用本文件，避免 ODR 违规，per spot-check New Issue 1）
    ```cpp
    struct GenomeVersion {
        std::string name;
        uint64_t version;
    };
    ```
- [ ] 3.0.2 `include/agenticdsl/types/attribution_version_pair_diff.h` 首次创建
  - `VersionPairDiff::compare(child, parent, confounders)` 实现 per ADR-0086 v1.0 决策 2
  - 集成 sample_count fail-fast 检查（决策 2 v1.1）
- [ ] 3.0.3 `src/evolution/attribution_record.cpp` 实现 v1.0 类型
- [ ] 3.0.4 `src/evolution/version_pair_diff.cpp` 实现 compare() 算法

### 3.1 文档修订 — `docs/adr/adr-0086-credit-assignment-contract.md`

- [ ] 3.1.1 修改决策 2 — 加入 `kMinBaselineSamples=5` 常量与前置 fail-fast 检查伪代码
- [ ] 3.1.2 修改决策 4 — `ConfounderKind` enum 追加 `HarnessChange` 值（namespace `agenticdsl::evolution`）
- [ ] 3.1.3 决策 4 表格追加 HarnessChange 行（含 5 字段）
- [ ] 3.1.4 新增决策 8 — Cross-framework alignment (documentation-only)
- [ ] 3.1.5 新增决策 9 — Data-freshness judgment algorithm（**完整版 + fast-path**，含 walk_ancestors 依赖）
- [ ] 3.1.6 **不变量编号修正**：v1.1 新增 8/9（避免与 v1.0 不变量 5/6 冲突，per spot-check）
  - 追加不变量 8 (kMinBaselineSamples 责任归属)
  - 追加不变量 9 (HarnessChange source_id 格式)
- [ ] 3.1.7 头部"状态"字段 🔍 Proposed → ✅ Approved (v1.1)
- [ ] 3.1.8 文末追加 ship 证据段: "v1.0+v1.1 合并 shipped 2026-09-XX, commit `<hash>`, 10 tests PASS (6 v1.0 + 4 v1.1), dual-agent review approved"

### 3.2 C++ 实现 — `include/agenticdsl/types/attribution_record.h`（修正 Oracle 🔴-2：types/ 而非 contract/）

- [ ] 3.2.1 在 `namespace agenticdsl::evolution` 新增 `constexpr uint32_t kMinBaselineSamples = 5;`
- [ ] 3.2.2 修改 `enum class ConfounderKind { TaskDifficulty, Environment, Opponent, EvaluatorDrift, ResourceChange, HarnessChange };`
- [ ] 3.2.3 新增 `struct HarnessChangeRecord { std::string source_id; ControlStatus control_status; std::string detection_method; std::string evidence_refs; };`
- [ ] 3.2.4 新增 `enum class ControlStatus { Controlled, Uncontrolled };`
- [ ] 3.2.5 `ConfounderRecord` 新增 optional `HarnessChangeRecord harness_change;` 字段
- [ ] 3.2.6 新增 `GenomeVersion struct { std::string name; uint64_t version; };`（**单一所有权**，C3 复用本文件避免 ODR）
- [ ] 3.2.7 新增 `judge_data_freshness(data, current, registry)` 函数声明（per 决策 9 完整版）

### 3.3 实现 `judge_data_freshness` 算法（修正 Oracle 🔴-6 完整版）

- [ ] 3.3.1 `src/evolution/attribution_record.cpp` 实现 `judge_data_freshness`
  - **Fast-path**: `data.name == current.name && data.version == current.version` → Attributed（不调 walk，避免 excludes-self 假阳性）
  - **Step 1**: `registry.walk_ancestors(current.name, current.version, nullopt)` → Result<LineageWalk, GenomeError>
  - **失败映射**: walk 返回 failure → Insufficient
  - **Step 2**: 在 `lineage.intermediate_versions` 中定位 data.version 索引
  - **不在线 → Confounded**
  - **Step 3**: 比较 `lineage.intermediate_metadata[data_idx].spec.harness` 与后续所有 `lineage.intermediate_metadata[i > data_idx].spec.harness`
  - **任一不同 → Confounded** + reason="harness changed after data generation" + 自动构造 HarnessChange confounder
  - **全部相同 → Attributed**

### 3.4 测试 — `tests/test_credit_assignment.cpp`（10 测试用例 = 6 v1.0 + 4 v1.1，修正 Oracle 🔴-1）

#### 3.4.1 v1.0 首次实施测试（6 case，per ADR-0086 v1.0 实施 §阶段 0）

- [ ] 新增 TEST_CASE "AttributionRecord 全字段 round-trip"
- [ ] 新增 TEST_CASE "ConfounderRecord 5 种基础 kind 序列化"
- [ ] 新增 TEST_CASE "VersionPairDiff::compare 正常返回 eval_delta"
- [ ] 新增 TEST_CASE "VersionPairDiff::compare insufficient verdict 路径"
- [ ] 新增 TEST_CASE "VersionPairDiff::compare confounded verdict 路径"
- [ ] 新增 TEST_CASE "VersionPairDiff::compare not_attempted 默认 fail-closed"

#### 3.4.2 v1.1 增量测试（4 case）

- [ ] 新增 TEST_CASE "ConfounderKind::HarnessChange serialization round-trip"
  - 断言 enum 值可序列化 → 反序列化 → Kind == HarnessChange
- [ ] 新增 TEST_CASE "VersionPairDiff fail-fast when sample_count < kMinBaselineSamples"
  - 构造 parent.sample_count = 4, 断言 verdict == Insufficient, reason 含 "kMinBaselineSamples"
- [ ] 新增 TEST_CASE "VersionPairDiff proceeds when sample_count >= kMinBaselineSamples"
  - 构造 parent.sample_count = 5, 断言正常计算 eval_delta
- [ ] 新增 TEST_CASE "HarnessChangeRecord schema 完整性"
  - 断言 source_id 包含 "@" + "→" 分隔符
- [ ] 新增 TEST_CASE "judge_data_freshness data==current fast-path"（修正 🔴-6）
- [ ] 新增 TEST_CASE "judge_data_freshness in-lineage 跨 Harness → Confounded"（修正 🔴-6 假阴性）
- [ ] 新增 TEST_CASE "judge_data_freshness data 不在 lineage → Confounded"
- [ ] 新增 TEST_CASE "judge_data_freshness walk failure → Insufficient"

### 3.5 RED → GREEN

- [ ] 3.5.1 RED: 跑 `cmake --build build --target test_credit_assignment && ctest -R test_credit_assignment`
  - 预期: 10+ 新测试全部 FAIL（v1.0+v1.1 实现未到位）
- [ ] 3.5.2 GREEN: 实施 3.0 + 3.1 + 3.2 + 3.3 全部内容
- [ ] 3.5.3 跑 `ctest -R test_credit_assignment` → 10+ 新测试 PASS

## 4. Ship-with-fixes (0.5d)

- [ ] 4.1 派 Oracle 实施审查 background agent
  - prompt: "审查 ADR-0086 v1.0+v1.1 实施 diff + 10 测试结果，输出 ship-with-fixes 清单"
- [ ] 4.2 应用 Critical 修正（不修改 baseline commit，新建独立 commit 保持原子性）
- [ ] 4.3 spec amendments（若有）
- [ ] 4.4 跑全量 `ctest --output-on-failure`
  - 预期: 0 regression（除已知 pre-existing failures）

## 5. Doc sync (0.5d)

- [ ] 5.1 `docs/research/rsi-three-operators-hydraforge-mapping-2026-09.md`
  - §1.2 行 28 ADR-0086 引用从 "🔍 Proposed" → "✅ Approved (v1.1)"
  - §3.2 C3 段 ADR-0086 引用同步
- [ ] 5.2 `docs/architecture/self-evolution-architecture-2026-08.md`
  - §一.1.3 引用 ADR-0086 ✅ Approved (v1.1)
  - §七 #6 引用 ADR-0086 ✅ Approved (v1.1) — 同步 status 翻转
- [ ] 5.3 `docs/adr/adr-0061-agent-evolution-and-solidification.md`
  - 附录 A 引用 ADR-0086 v1.1（如已写）
- [ ] 5.4 跑 `python3 tools/adr_lint.py` + `python3 tools/docs_drift_audit.py`
  - 预期: 0 errors + 0 DRIFT

## 6. Archive (0.5d)

- [ ] 6.1 `openspec archive 2026-09-20-adr-0086-v1-1-harness-change-confounder`
  - 移动到 `openspec/changes/archive/2026-09-20-adr-0086-v1-1-harness-change-confounder/`
- [ ] 6.2 Day-5 lesson 验证: `git ls-files openspec/changes/archive/2026-09-20-adr-0086-v1-1-harness-change-confounder/ | wc -l`
  - 预期: 4 文件（.openspec.yaml + proposal.md + tasks.md + specs/<name>/spec.md × 1）
- [ ] 6.3 git commit: `feat(adr-0086-v1-1): v1.0-first-impl + harness-change-confounder + baseline-sampling + data-freshness`
- [ ] 6.4 通知 C3 fill author 引用 v1.1 决策 4 + 决策 9 + GenomeVersion 复用本文件

## 7. Out-of-scope

- ❌ 不实现 walk_ancestors 接口（C3 范围，本 change 仅定义算法契约 + 类型 stub）
- ❌ 不实现 C3 transition guard（C3 范围）
- ❌ 不实现 DistillationRecord.genome_version 扩展（盲点 #6 闭环需独立 follow-up change）
- ❌ 不修改 ConfounderRecord 序列化 schema（C++ 编译期 enum 不影响磁盘格式）
- ❌ 不重写 v1.0 既有决策语义（仅 additive v1.1 决策 8/9 + 不变量 8/9）

## 8. References

- **Oracle review sessions**:
  - `bg_ba048665` (2026-09-20, Metis: 5 Critical + 5 Major)
  - `bg_fed9d7c0` (2026-09-20, Oracle: 7 Critical + 6 Major + 8 Minor)
  - `ses_f45456c38ffeO6cmQwiUQwlcdC` (2026-09-20, Oracle spot-check: 5 stale in tasks.md + New Issue 1 GenomeVersion 重复定义)
- **Prior Oracle reviews**:
  - `ses_f45b96c94ffevTy454aeDBK7U2` (continuation, 7 项 action 列表)
  - `ses_f55f307f6ffeRJ9SIny8iUbZ8Y` (2026-09-16, M2 评审)
- **ADR-0086 v1.0**: `docs/adr/adr-0086-credit-assignment-contract.md`
- **C2 ship**: `openspec/changes/archive/2026-09-19-2026-09-16-genome-registry/`
- **C3 placeholder**: `openspec/changes/2026-09-16-h-d-m-transition-guard/proposal.md`
- **T14 Hotelling T²**: `tests/test_behavioral_regression.cpp` (Sprint 25 ship)
- **AGENTS.md §模式 8**: OpenSpec Change pre-implementation dual-agent review
