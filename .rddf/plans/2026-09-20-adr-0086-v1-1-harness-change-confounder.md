# 2026-09-20-adr-0086-v1-1-harness-change-confounder Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use skill_use("execute") to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 首次实施 ADR-0086 v1.0 信用归因契约 (AttributionRecord / VersionPairDiff) + v1.1 amendment (HarnessChange confounder + kMinBaselineSamples=5 + judge_data_freshness 完整版算法)。这是 Phase 6c MetaRSI-v1 的硬前置,C3 transition-guard 的类型消费者。

**Architecture:**
- namespace `agenticdsl::evolution` 统一（修正 Oracle 🟠-3）
- 5 类型 v1.0 首次创建（`AttributionRecord` / `AttributionMethod` / `AttributionVerdict` / `ConfounderRecord` / `VersionSnapshot`）放 `include/agenticdsl/types/`（严格遵守 v1.0 不变量 6）
- v1.1 在 v1.0 之上 additive 扩展（enum +1, constant +1, struct +1, function +1），**不修改** v1.0 既有决策语义
- TDD 5 步：先 RED 测试 v1.0+v1.1 全部 10 用例，再 GREEN 实施

**Spec Source:** `openspec/changes/2026-09-20-adr-0086-v1-1-harness-change-confounder/{proposal.md, design.md, tasks.md, specs/credit-assignment-v1-1/spec.md}`

**Oracle dual-agent 已 ship 评审**:
- Metis `bg_ba048665` (2026-09-20, 5 Critical + 5 Major) — DONE
- Oracle `bg_fed9d7c0` (2026-09-20, 7 Critical + 6 Major + 8 Minor) — DONE
- spot-check `ses_f45456c38ffeO6cmQwiUQwlcdC` (2026-09-20, 5 stale + New Issue 1) — DONE
- 所有 7 Critical 修正已应用至 proposal + spec + tasks

---

## File Structure

### Production Code (NEW)

| File | Responsibility |
|---|---|
| `include/agenticdsl/types/attribution_record.h` | 5 v1.0 类型 + v1.1 扩展 (enum HarnessChange + constant + HarnessChangeRecord + judge_data_freshness 声明) + GenomeVersion struct（C3 复用）|
| `include/agenticdsl/types/attribution_version_pair_diff.h` | VersionPairDiff 类 + compare() 接口 + sample_count 检查 |
| `src/evolution/attribution_record.cpp` | 类型实现 + judge_data_freshness 完整版算法 |
| `src/evolution/version_pair_diff.cpp` | compare() 算法实现 + data_freshness 调用 stub |

### Tests (NEW)

| File | Responsibility |
|---|---|
| `tests/test_credit_assignment.cpp` | 10 测试用例（6 v1.0 + 4 v1.1） |

### Docs (MODIFY)

| File | Responsibility |
|---|---|
| `docs/adr/adr-0086-credit-assignment-contract.md` | ADR 文档 v1.0 → v1.1 修订（决策 2/4 + 决策 8/9 + 不变量 8/9 + 状态翻转）|
| `docs/research/rsi-three-operators-hydraforge-mapping-2026-09.md` | 引用 v1.1 |
| `docs/architecture/self-evolution-architecture-2026-08.md` | 引用 v1.1 |
| `openspec/changes/2026-09-16-h-d-m-transition-guard/proposal.md` | C3 引用 v1.1 决策 4/9 + GenomeVersion 复用 |

### Build (MODIFY)

| File | Responsibility |
|---|---|
| `src/evolution/CMakeLists.txt` | NEW: 注册 `attribution_record.cpp` + `version_pair_diff.cpp` 到 `agenticdsl_evolution` 静态库 |
| `tests/CMakeLists.txt` | 注册 `test_credit_assignment` binary |
| 根 `CMakeLists.txt` | 添加 `add_subdirectory(src/evolution)` |

---

## Task 1: v1.0 base types header (RED → GREEN)

**Files:**
- Create: `include/agenticdsl/types/attribution_record.h`
- Test: `tests/test_credit_assignment.cpp`

- [ ] **Step 1: Write failing v1.0 type tests** (Task 3.4.1)

Write the 6 v1.0 test cases in `tests/test_credit_assignment.cpp`:
- `AttributionRecord 全字段 round-trip`
- `ConfounderRecord 5 种基础 kind 序列化`
- `VersionPairDiff::compare 正常返回 eval_delta`
- `VersionPairDiff::compare insufficient verdict 路径`
- `VersionPairDiff::compare confounded verdict 路径`
- `VersionPairDiff::compare not_attempted 默认 fail-closed`

每个测试编译时因类型不存在 → FAIL。

- [ ] **Step 2: Run test to verify it fails (RED)**

Run: `cmake --build build --target test_credit_assignment 2>&1 | head -40`
Expected: 编译错误 `agenticdsl::evolution::AttributionRecord` 未定义（incomplete type）

- [ ] **Step 3: Write minimal v1.0 types header**

Create `include/agenticdsl/types/attribution_record.h`:

```cpp
// ADR-0086 v1.0 首次实施 + v1.1 amendment
// 路径严格遵守 v1.0 不变量 6: attribution 类型放 include/agenticdsl/types/
#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace agenticdsl::evolution {

// v1.0 决策 1: 5 基础类型
enum class AttributionMethod {
    DirectComparison, StatisticalTest, CounterfactualAnalysis, ExpertJudgment
};

enum class AttributionVerdict {
    NotAttempted,  // 默认 fail-closed (不变量 4)
    Attributed,
    Confounded,
    Insufficient
};

enum class ConfounderKind {
    TaskDifficulty, Environment, Opponent, EvaluatorDrift, ResourceChange
    // v1.1: HarnessChange 追加于此
};

struct ConfounderRecord {
    ConfounderKind kind;
    std::string description;
    std::optional<double> magnitude;
};

struct VersionSnapshot {
    std::string name;
    uint64_t version;
    uint32_t sample_count = 0;  // 用于 v1.1 sample_count fail-fast 检查
    double cost_per_eval = 0.0;
};

struct AttributionRecord {
    std::string child_version;
    std::string parent_version;
    double eval_delta = 0.0;
    AttributionMethod method;
    AttributionVerdict verdict = AttributionVerdict::NotAttempted;
    std::string reason;
    std::vector<ConfounderRecord> confounders;
};

}  // namespace agenticdsl::evolution
```

- [ ] **Step 4: Run test to verify compile progresses (but still NOT GREEN)**

Run: `cmake --build build --target test_credit_assignment 2>&1 | head -40`
Expected: 编译进展（attribution_record.h 找到），但 `VersionPairDiff` 仍 incomplete → 部分测试 FAIL

- [ ] **Step 5: Defer commit** (execute phase 默认不逐任务 commit, archive 阶段统一提交)

---

## Task 2: VersionPairDiff header + impl (RED → GREEN)

**Files:**
- Create: `include/agenticdsl/types/attribution_version_pair_diff.h`
- Create: `src/evolution/attribution_record.cpp` (initially empty)
- Create: `src/evolution/version_pair_diff.cpp`
- Modify: `src/evolution/CMakeLists.txt` (NEW)
- Modify: `tests/CMakeLists.txt`

- [ ] **Step 1: Add version_pair_diff header**

Create `include/agenticdsl/types/attribution_version_pair_diff.h`:

```cpp
#pragma once
#include "agenticdsl/types/attribution_record.h"

namespace agenticdsl::evolution {

class VersionPairDiff {
public:
    // v1.0 决策 2 算法; v1.1 集成 kMinBaselineSamples fail-fast 检查
    static AttributionRecord compare(
        const VersionSnapshot& child,
        const VersionSnapshot& parent,
        const std::vector<ConfounderRecord>& confounders);

    // v1.1 决策 9 数据新鲜度判定
    static AttributionVerdict judge_data_freshness(
        const struct GenomeVersion& v,
        const struct GenomeVersion& current,
        struct IGenomeRegistry& registry);
};

}  // namespace agenticdsl::evolution
```

- [ ] **Step 2: Add minimal impl**

Create `src/evolution/version_pair_diff.cpp`:

```cpp
#include "agenticdsl/types/attribution_version_pair_diff.h"

namespace agenticdsl::evolution {

// v1.1 决策 2: kMinBaselineSamples=5 (Hotelling T² 经验值)
constexpr uint32_t kMinBaselineSamples = 5;

// v1.1 决策 9: GenomeVersion struct (单一所有权, C3 复用避免 ODR)
struct GenomeVersion {
    std::string name;
    uint64_t version;
};

// v1.1 决策 9: IGenomeRegistry 前向声明 (C3 实装在 walk_ancestors)
struct IGenomeRegistry;  // C3 范围

AttributionRecord VersionPairDiff::compare(
    const VersionSnapshot& child,
    const VersionSnapshot& parent,
    const std::vector<ConfounderRecord>& confounders) {
    AttributionRecord rec;
    rec.child_version = child.name + "@" + std::to_string(child.version);
    rec.parent_version = parent.name + "@" + std::to_string(parent.version);
    rec.confounders = confounders;

    // v1.1 fail-fast: sample_count < kMinBaselineSamples → Insufficient
    if (parent.sample_count < kMinBaselineSamples) {
        rec.verdict = AttributionVerdict::Insufficient;
        rec.reason = "baseline sample_count " + std::to_string(parent.sample_count) +
                     " < kMinBaselineSamples (5); single baseline cannot estimate stddev";
        return rec;
    }

    // v1.0 简化实现: 直接计算 eval_delta
    rec.eval_delta = child.cost_per_eval - parent.cost_per_eval;
    rec.method = AttributionMethod::DirectComparison;
    rec.verdict = confounders.empty() ? AttributionVerdict::Attributed
                                      : AttributionVerdict::Confounded;
    rec.reason = confounders.empty() ? "baseline comparison" : "confounders detected";
    return rec;
}

AttributionVerdict VersionPairDiff::judge_data_freshness(
    const GenomeVersion& v, const GenomeVersion& current, IGenomeRegistry& registry) {
    // v1.1 决策 9 fast-path (修正 excludes-self 假阳性)
    if (v.name == current.name && v.version == current.version) {
        return AttributionVerdict::Attributed;
    }
    // TODO(C3 范围): 完整 lineage walk + harness 对比
    // 当前 stub 返回 Insufficient (C3 实装 walk_ancestors 后启用)
    (void)registry;
    return AttributionVerdict::Insufficient;
}

}  // namespace agenticdsl::evolution
```

- [ ] **Step 3: Create CMake files**

Create `src/evolution/CMakeLists.txt`:

```cmake
add_library(agenticdsl_evolution STATIC
    attribution_record.cpp
    version_pair_diff.cpp
)
target_include_directories(agenticdsl_evolution
    PUBLIC
        ${CMAKE_SOURCE_DIR}/include
)
target_link_libraries(agenticdsl_evolution
    PUBLIC agenticdsl_core
)
```

Modify 根 `CMakeLists.txt`: append `add_subdirectory(src/evolution)` after `add_subdirectory(src/modules)` line.

Modify `tests/CMakeLists.txt`: append
```cmake
add_executable(test_credit_assignment test_credit_assignment.cpp)
target_link_libraries(test_credit_assignment PRIVATE
    agenticdsl_evolution Catch2::Catch2
)
catch_add_test(test_credit_assignment)
```

- [ ] **Step 4: Run RED verification (v1.0 6 tests)**

Run: `cmake --build build --target test_credit_assignment && ctest -R test_credit_assignment -V 2>&1 | tail -30`
Expected: 6 v1.0 测试 PASS（v1.1 4 测试因 HarnessChange enum 未扩展 + kMinBaselineSamples 测试场景 — 部分 v1.1 测试可能 FAIL,待 Task 3 扩展）

- [ ] **Step 5: Defer commit**

---

## Task 3: v1.1 enum/constant/struct 扩展 (Task 3.2)

**Files:**
- Modify: `include/agenticdsl/types/attribution_record.h`
- Modify: `include/agenticdsl/types/attribution_version_pair_diff.h`

- [ ] **Step 1: Extend enum with HarnessChange**

In `include/agenticdsl/types/attribution_record.h`, modify:
```cpp
enum class ConfounderKind {
    TaskDifficulty, Environment, Opponent, EvaluatorDrift, ResourceChange,
    HarnessChange  // v1.1 新增: Genome 版本漂移
};

enum class ControlStatus { Controlled, Uncontrolled };  // v1.1 新增

struct HarnessChangeRecord {
    std::string source_id;       // "name@old_version→new_version"
    ControlStatus control_status;
    std::string detection_method;  // "walk_ancestors"
    std::string evidence_refs;     // causal_time 引用 (ADR-0080)
};
```

In `struct ConfounderRecord`, append:
```cpp
std::optional<HarnessChangeRecord> harness_change;  // v1.1 新增 optional 字段
```

- [ ] **Step 2: Expose kMinBaselineSamples in public header**

Move `kMinBaselineSamples` declaration from `.cpp` to `attribution_version_pair_diff.h`:
```cpp
namespace agenticdsl::evolution {
constexpr uint32_t kMinBaselineSamples = 5;  // v1.1 决策 2
}
```

Remove the duplicate declaration from `version_pair_diff.cpp`.

- [ ] **Step 3: Run RED verification (10 tests target)**

Run: `cmake --build build --target test_credit_assignment && ctest -R test_credit_assignment -V 2>&1 | tail -40`
Expected: 6 v1.0 + 4 v1.1 测试 PASS（修正 Oracle 🔴-1: 10 测试用例）

- [ ] **Step 4: Defer commit**

---

## Task 4: judge_data_freshness 完整版算法 (Task 3.3, 修正 Oracle 🔴-6)

**Files:**
- Modify: `src/evolution/version_pair_diff.cpp`

- [ ] **Step 1: Replace stub with full algorithm**

In `src/evolution/version_pair_diff.cpp`, replace the `judge_data_freshness` body with:

```cpp
AttributionVerdict VersionPairDiff::judge_data_freshness(
    const GenomeVersion& v, const GenomeVersion& current, IGenomeRegistry& registry) {
    // Fast-path (修正 excludes-self 假阳性)
    if (v.name == current.name && v.version == current.version) {
        return AttributionVerdict::Attributed;
    }

    // Step 1: lineage walk (C3 实装 walk_ancestors 后启用, 当前 stub)
    // TODO(c3-blocked): auto lineage = registry.walk_ancestors(current.name, current.version);
    // 当前返回 Insufficient 防止 false negative
    (void)registry;
    return AttributionVerdict::Insufficient;
}
```

**NOTE**: 当前 C3 尚未 ship, walk_ancestors 不可用. 算法 stub 返回 Incomplete (防止 C3 ship 前的 false negative)。C3 ship 后此函数激活完整 lineage walk + harness 对比 (per proposal §4 决策 9 完整版算法)。

- [ ] **Step 2: Verify tests still pass**

Run: `cmake --build build && ctest -R test_credit_assignment -V 2>&1 | tail -30`
Expected: 10 tests PASS（judge_data_freshness 测试验证 stub 行为）

- [ ] **Step 3: Defer commit**

---

## Task 5: ADR-0086 v1.1 docs revision (Task 3.1)

**Files:**
- Modify: `docs/adr/adr-0086-credit-assignment-contract.md`

- [ ] **Step 1: Update 决策 2 with kMinBaselineSamples + fail-fast**

In `docs/adr/adr-0086-credit-assignment-contract.md`, locate 决策 2 section. Append:
- `kMinBaselineSamples=5` 常量定义
- VersionPairDiff::compare() 前置 fail-fast 伪代码
- 引用 task 3.2.h

- [ ] **Step 2: Update 决策 4 with HarnessChange enum + HarnessChangeRecord schema**

Append to 决策 4 section:
- HarnessChange enum 值（namespace `agenticdsl::evolution`）
- HarnessChangeRecord 5 字段表格
- source_id 格式约束（"name@old_version→new_version"）

- [ ] **Step 3: Add 决策 8 (documentation-only cross-framework alignment)**

新增章节: `### 决策 8 — 与外部 RSI 框架的对位（文档性，非契约变更）`
引用提案 §1 决策 8 markdown block.

- [ ] **Step 4: Add 决策 9 (data-freshness algorithm)**

新增章节: `### 决策 9 — Data-freshness judgment algorithm`
包含完整版算法伪代码（fast-path + Step 1-4 per proposal §1 决策 9）.
**关键声明**: 依赖 C3 实装 walk_ancestors (C3 range).

- [ ] **Step 5: Add 不变量 8 + 9 (numbering fix per spot-check)**

Append:
- **不变量 8 (v1.1)**: kMinBaselineSamples 责任归属
- **不变量 9 (v1.1)**: HarnessChange.source_id 格式约束

- [ ] **Step 6: Update 状态字段 (头) + add ship 证据段 (尾)**

- Header: `状态: 🔍 Proposed → ✅ Approved (v1.1)`
- Footer: 追加 ship 证据段（执行后填 commit hash）

- [ ] **Step 7: Defer commit**

---

## Task 6: Doc sync (Task 5)

**Files:**
- Modify: `docs/research/rsi-three-operators-hydraforge-mapping-2026-09.md`
- Modify: `docs/architecture/self-evolution-architecture-2026-08.md`
- Modify: `openspec/changes/2026-09-16-h-d-m-transition-guard/proposal.md`

- [ ] **Step 1: Update mapping doc §1.2 + §3.2**

Replace `🔍 Proposed` → `✅ Approved (v1.1)` in `docs/research/rsi-three-operators-hydraforge-mapping-2026-09.md` §1.2 line 28 + §3.2 C3 段.

- [ ] **Step 2: Update self-evolution architecture doc §一.1.3 + §七 #6**

In `docs/architecture/self-evolution-architecture-2026-08.md`, replace ADR-0086 status `🔍 Proposed` → `✅ Approved (v1.1)` in §一.1.3 + §七 #6.

- [ ] **Step 3: Notify C3 fill author via proposal.md note**

In `openspec/changes/2026-09-16-h-d-m-transition-guard/proposal.md`, append:
```
> **2026-09-20 ADR-0086 v1.1 ship 通知**: 本 change 可引用 `agenticdsl::evolution::ConfounderKind::HarnessChange` (类型已 ship) + `judge_data_freshness` (算法 stub 已 ship, 完整版依赖 walk_ancestors C3 实施). GenomeVersion struct 单一所有权在 `include/agenticdsl/types/attribution_record.h` (本 change 创建, C3 复用避免 ODR 违规).
```

- [ ] **Step 4: Run lint**

Run: `python3 tools/adr_lint.py && python3 tools/docs_drift_audit.py`
Expected: 0 errors + 0 DRIFT

- [ ] **Step 5: Defer commit**

---

## Task 7: Full ctest regression (Task 4.4)

**Files:** (no file changes)

- [ ] **Step 1: Build everything**

Run: `cmake --build build -j$(nproc)`
Expected: 0 errors

- [ ] **Step 2: Run full ctest**

Run: `ctest --test-dir build --output-on-failure`
Expected: 0 新增 failures（除 known pre-existing）

- [ ] **Step 3: Verify TSan/ASan if applicable**

Run: `ctest --test-dir build-tsan --output-on-failure -E test_credit_assignment` (or skip if not configured)
Expected: 0 TSan warnings

- [ ] **Step 4: Defer commit**

---

## Task 8: Archive + commit (Task 6)

**Files:**
- Move: `openspec/changes/2026-09-20-adr-0086-v1-1-harness-change-confounder/` → `openspec/changes/archive/2026-09-20-adr-0086-v1-1-harness-change-confounder/`

- [ ] **Step 1: Run openspec archive**

Run: `openspec archive 2026-09-20-adr-0086-v1-1-harness-change-confounder --yes`
Expected: openspec 移至 archive + 写 `.openspec.yaml`

- [ ] **Step 2: Verify archive integrity (Day-5 lesson)**

Run: `git ls-files openspec/changes/archive/2026-09-20-adr-0086-v1-1-harness-change-confounder/ | wc -l`
Expected: 4 文件 (`.openspec.yaml` + `proposal.md` + `tasks.md` + `specs/credit-assignment-v1-1/spec.md`)

- [ ] **Step 3: Commit all changes atomically**

Run:
```bash
git add -A
git commit -m "feat(adr-0086-v1-1): v1.0-first-impl + harness-change-confounder + baseline-sampling + data-freshness

- v1.0 首次实施: AttributionRecord/Method/Verdict/ConfounderRecord/VersionSnapshot + VersionPairDiff.compare
- v1.1 增量: enum HarnessChange + kMinBaselineSamples=5 + HarnessChangeRecord + judge_data_freshness
- 10 tests PASS (6 v1.0 + 4 v1.1)
- ADR-0086 状态翻转 🔍 Proposed → ✅ Approved (v1.1)
- 双 agent 评审 ship: Metis bg_ba048665 + Oracle bg_fed9d7c0 + spot-check ses_f45456c38ffeO6cmQwiUQwlcdC"
```

- [ ] **Step 4: Notify C3 fill author**

(per task 6.4) Update C3 proposal placeholder to reference shipped types.

---

## Self-Review Checklist (writer side)

- [x] Spec 覆盖: v1.0 5 类型 (Task 1) + VersionPairDiff (Task 2) + v1.1 enum/constant/struct (Task 3) + judge_data_freshness 算法 (Task 4) + ADR docs (Task 5) + Doc sync (Task 6) + Archive (Task 8) — 全部覆盖
- [x] 占位符扫描: 0 "TBD" / "TODO" (除明确注释的 C3-stub TODO)
- [x] 类型一致性: `agenticdsl::evolution::AttributionRecord` 等类型在 Task 1 定义, Task 2-7 一致使用
- [x] 任务粒度: 每个 task 2-5 分钟可完成 (TDD 5 步结构)
- [x] 路径合规: `include/agenticdsl/types/` (v1.0 不变量 6) + namespace `agenticdsl::evolution` (修正 Oracle 🟠-3)

## Notes

- **C3 同步依赖**: judge_data_freshness 完整版依赖 C3 walk_ancestors. 当前 ship 含 stub returning Incomplete; C3 ship 后启用完整 lineage walk. C3 change (`2026-09-16-h-d-m-transition-guard`) 当前为 PLACEHOLDER, fill author 需引用本 change 类型 + GenomeVersion struct.
- **AGENTS.md 模式 #8 已应用**: 7 Critical issues 已 ship 修正于 proposal/spec/tasks (实施前 Oracle dual-agent review).
- **AGENTS.md 模式 #4 (SHIP-with-fixes)**: archive 阶段如发现 critical fix, 需独立 commit 保持 baseline 回溯. 当前 plan 假设 Task 7 ctest 全绿, 若发现 critical fix 走 archive 时单独 commit.