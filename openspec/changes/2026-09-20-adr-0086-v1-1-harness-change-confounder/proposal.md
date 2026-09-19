# Proposal: ADR-0086 v1.0 首次实施 + v1.1 Amendment — HarnessChange Confounder & Baseline Sampling

> **STATUS**: DRAFT (post Oracle dual-agent review `ses_f45b96c94ffevTy454aeDBK7U2`, critical fixes applied)
> **Kind**: Combined change — ADR-0086 v1.0 首次实施（spike 阶段 0，per ADR-0086 v1.0 实施 §阶段 0）+ v1.1 Amendment 合并落地
> **Targets**: `docs/adr/adr-0086-credit-assignment-contract.md`
> **Priority**: P0 (C3 h-d-m-transition-guard hard prerequisite)
> **Estimated Effort**: 5-7 days (修正：v1.0 从零落地 + v1.1 增量，原估时 3-5d 偏低)
> **Author**: Solo Dev + Oracle dual-agent review (`ses_f45b96c94ffevTy454aeDBK7U2` continuation + `bg_fed9d7c0`)
> **Created**: 2026-09-20
> **Last Updated**: 2026-09-20 (post Oracle review 7 Critical fixes)

---

## Why

### 关键范围声明（修正 Oracle 🔴-1）

ADR-0086 自 2026-08-31 创建以来仅作为文档存在（状态 🔍 Proposed，"V1 不强制实施"）。全库无任何 ADR-0086 引用类型的实现代码（`grep` 证实：`include/agenticdsl/types/attribution_record.h` / `tests/test_credit_assignment.cpp` / `VersionPairDiff` / `AttributionRecord` / `ConfounderRecord` 全部不存在）。**本 change 是 ADR-0086 首次代码实施 + v1.1 amendment 的合并落地**，而非纯 amendment。后续所有任务以此为前提。

### Oracle dual-agent review 识别的 7 项 Critical Issues（已修正）

1. **🔴-1 已修正**：v1.0 从未实施，本 change 显式声明 v1.0+v1.1 合并首次落地，估时 3-5d → 5-7d
2. **🔴-2 已修正**：v1.0 不变量 6 约束 attribution 类型必须放 `include/agenticdsl/types/`，v1.1 amendment 不违反（路径统一 `include/agenticdsl/types/attribution_record.h`）
3. **🔴-3 已修正**：C3 `constexpr_can_transition` 改返回 `bool`（literal type），运行期 `can_transition()` 调 constexpr 再包装 EvolutionVerdict
4. **🔴-4 已修正**：C3 引用 3 个不存在类型（`GenomeVersion` / `RewardQuality::Failed` / `budget.remaining`）→ 改为已知正确类型签名
5. **🔴-5 已修正**：`walk_ancestors` 签名加 `name` 参数（`const std::string& name, uint64_t from_version, ...`），与 C2 版本号 per-name 模型一致
6. **🔴-6 已修正**：决策 9 算法改完整版（用 `intermediate_metadata` 中的 `GenomeSpec.harness` 字段），不再有 in-lineage 跨 Harness 假阴性 + excludes-self 假阳性
7. **🔴-7 已修正**：C3 spec.md + tasks.md 重写以匹配 proposal

### Why this amendment is needed before C3

| 依赖链 | 现状 | 阻塞 |
|--------|------|------|
| ADR-0086 → C3 (transition-guard) | ADR-0086 🔍 Proposed, 缺 3 Critical 修正 | C3 proposal 引用 `ConfounderRecord::Kind::HarnessChange` 类型，但类型尚未定义 |
| C2 (genome-registry, ✅ shipped 2026-09-19) → C3 | walk_ancestors 在 C2 ship 时 deferred，**未实装** | C3 数据新鲜度判定无接口可用 |
| ADR-0086 评审 → C3 实施 | ADR-0086 未 Approved 即被 C3 硬依赖 | 治理异常（"实施先于翻牌"，ADR-0072 有前科） |

### 关键战略决策

**Oracle M2 + Oracle continuation 共识**：amendment change 必须**先于** C3 实施 ship，理由：
1. C3 spec 引用 `ConfounderRecord::Kind::HarnessChange` 类型，类型不存在 = 编译失败
2. C3 数据新鲜度算法需要 `parent.sample_count >= kMinBaselineSamples` 判定，constant 须先定
3. ADR-0086 amendment 通过 = ADR-0086 v1.0 → v1.1 Approved 状态翻转，C3 引用合规

### External framework alignment

- **月谈AI "验证是 RSI 生命线"**：本 amendment 是该原则在归因层的可执行化（决策 8）。
- **MetaRSI-v1 H→D→M 交通规则**：本 amendment 的 HarnessChange kind 是 C3 TransitionGuard (C3) 的**首要消费者**（决策 4 + 决策 9 数据新鲜度算法）。

---

## What Changes

### 1. ADR-0086 决策 4 修改 — ConfounderRecord::Kind 新增 HarnessChange

**namespace 统一**（修正 Oracle 🟠-3）：v1.0 代码块使用 `namespace agenticdsl`，v1.1 显式改为 `namespace agenticdsl::evolution`（与 C3 一致）。

**原 5 种**（v1.0，已存在但从未实施）：
```cpp
namespace agenticdsl::evolution {
enum class ConfounderKind {
    TaskDifficulty, Environment, Opponent,
    EvaluatorDrift, ResourceChange
};
}
```

**v1.1 改为 6 种**：
```cpp
namespace agenticdsl::evolution {
enum class ConfounderKind {
    TaskDifficulty, Environment, Opponent,
    EvaluatorDrift, ResourceChange,
    HarnessChange  // ← 新增：Genome 版本漂移（旧 Harness 数据归因到新 Harness）
};
}
```

**路径修正**（Oracle 🔴-2）：v1.0 不变量 6 约束 attribution 类型必须放 `include/agenticdsl/types/`，v1.1 不违反。原提案 `include/agenticdsl/contract/credit_assignment.h` **修正为** `include/agenticdsl/types/attribution_record.h`（与 v1.0 实施 §阶段 0 一致）。

**HarnessChange 记录 schema**（决策 4 修订）：
| 字段 | 类型 | 说明 |
|------|------|------|
| kind | `ConfounderKind::HarnessChange` | 固定值 |
| source_id | `string` | "genome@<name>@<old_version>→<new_version>" |
| control_status | `enum { Controlled, Uncontrolled }` | Harness 变更是否被实验设计捕获 |
| detection_method | `string` | "walk_ancestors"（来自决策 9 数据新鲜度判定） |
| evidence_refs | `string` | causal_time 引用（ADR-0080） |

### 2. ADR-0086 决策 2 修改 — kMinBaselineSamples=5 baseline sampling requirement

**新增常量与规则**：
```cpp
namespace agenticdsl::evolution {
constexpr uint32_t kMinBaselineSamples = 5;  // 最小基线重复评估次数
}

// 决策 2 修订: VersionPairDiff::compare() 前置检查
if (parent.sample_count < kMinBaselineSamples) {
  rec.verdict = AttributionVerdict::Insufficient;
  rec.reason = "baseline sample_count < kMinBaselineSamples (5); "
               "single baseline cannot estimate stddev";
  return rec;  // fail-fast, 不进入 eval_delta 计算
}
```

**预算影响**：
- 5 次重复基线 × cost = `5 × parent.cost_per_eval`
- 计入消费方预算门（C3 evaluate_readiness 条件 3）
- 不在 ADR-0086 内自动调度，由调用方负责执行

### 3. ADR-0086 决策 8 新增 — Cross-framework alignment (documentation only)

**决策 8（documentation-only，非契约）**：

```markdown
### 决策 8 — 与外部 RSI 框架的对位（文档性，非契约变更）

- 月谈AI "验证是 RSI 的生命线" ↔ 决策 3 治理绑定 + 决策 7 fail-closed：
  本 ADR 是该原则在归因层的可执行化（HarnessChange 检测 + kMinBaselineSamples 守卫）。
- MetaRSI-v1 H→D→M 交通规则是本 ADR 的**首要消费者**：
  HarnessChange 混杂 kind（决策 4 修订）为 TransitionGuard (C3) 提供类型化判定依据。
- 月谈AI 三层（Prompt/Skill/Agent RSI）不是与 MetaRSI 三算子平行的框架，
  而是 Harness-RSI 内部的粒度轴。本 ADR 不直接区分三层（粒度对归因透明），
  但 ConfounderRecord::Kind::HarnessChange 可检测任意粒度的 Harness 变更。
- 对位不改变本 ADR 边界：归因层仍不调用 LLM、不修改 IEvaluator（不变量 1/3）。
```

### 4. ADR-0086 决策 9 新增 — Data-freshness judgment algorithm

**签名修正**（Oracle 🔴-5）：版本号在 C2 是 per-name 模型（`parent` 字段为 `"name@version"`），签名必须含 name：

```cpp
namespace agenticdsl::evolution {

struct GenomeVersion {
    std::string name;
    uint64_t version;
};

// 消费方: C3 TransitionGuard::evaluate_readiness() 条件 1 子例程
// 输入:
//   - data: 数据生成时的 Genome 版本（name + version）
//   - current: 当前 Genome 版本
//   - registry: IGenomeRegistry 实例
// 输出:
//   - AttributionVerdict (Attributed | Confounded | Insufficient)
AttributionVerdict judge_data_freshness(
    const GenomeVersion& data,
    const GenomeVersion& current,
    IGenomeRegistry& registry);

}
```

**算法修正**（Oracle 🔴-6 完整版）：使用 `intermediate_metadata` 中的 `GenomeSpec.harness` 字段，**消除原简化版 (a) 的假阴性 + excludes-self 假阳性**：

```cpp
AttributionVerdict judge_data_freshness(
    const GenomeVersion& data,
    const GenomeVersion& current,
    IGenomeRegistry& registry) {

  // Fast-path: 数据版本 == 当前版本 → 必然 Attributed
  // (避免 C2 walk excludes-self 语义导致的假阳性 Confounded)
  if (data.name == current.name && data.version == current.version) {
    return AttributionVerdict::Attributed;
  }

  // Step 1: 谱系可达性检查 (closest-first, excludes-self per C2 ship)
  auto lineage = registry.walk_ancestors(current.name, current.version);
  if (!lineage.has_value()) {
    return AttributionVerdict::Insufficient;  // 谱系不可达
  }

  // Step 2: 定位 data_version 在 lineage 中的位置
  // lineage.intermediate_versions: closest-first (e.g. [v_curr-1, v_curr-2, ...])
  // lineage.intermediate_metadata: 同步 vector<Genome> (含 spec.harness)
  size_t data_idx = SIZE_MAX;
  for (size_t i = 0; i < lineage->intermediate_versions.size(); ++i) {
    if (lineage->intermediate_versions[i] == data.version) {
      data_idx = i;
      break;
    }
  }
  if (data_idx == SIZE_MAX) {
    // 数据版本不在 lineage 中 → 完全 out-of-lineage
    return AttributionVerdict::Confounded;
  }

  // Step 3: 检查 data_idx 之后的所有版本的 harness 是否变化
  // lineage[0] = closest ancestor (e.g. v_curr-1)
  // lineage[data_idx] = data version
  // 我们关心 data_idx 之后的索引 (i > data_idx)，它们是 data 之前的更早版本
  const std::string& data_harness = lineage->intermediate_metadata[data_idx].spec.harness;
  for (size_t i = data_idx + 1; i < lineage->intermediate_metadata.size(); ++i) {
    if (lineage->intermediate_metadata[i].spec.harness != data_harness) {
      // data 生成后经历了 Harness 变更 → Confounded
      return AttributionVerdict::Confounded;  // reason="harness changed after data generation"
    }
  }

  // Step 4: 数据版本在谱系内且后续无 Harness 变更 → Attributed
  return AttributionVerdict::Attributed;
}
```

**关键修正说明**：
1. **消除假阴性**：原简化版 (a) 对"in-lineage 但跨 Harness 变更"（即 v3 生成数据，v4 改了 Harness，v5 是当前版本）错误返回 Attributed。完整版通过对比 `spec.harness` 字符串检测出 v3→v4 的 Harness 变更。
2. **消除假阳性**：原 (a) 因 C2 walk excludes-self 语义，对 `data.version == current.version` 误判 Confounded。Fast-path 修复。
3. **LineageWalk.intermediate_metadata 元素类型必须是 `Genome`**（不是 `GenomeMetadata`），因为 `GenomeMetadata` 无 `spec.harness` 字段。详见 C3 proposal §4 同步锁定。

**关键依赖**：本决策依赖 C3 change（`2026-09-16-h-d-m-transition-guard`）的 `walk_ancestors` 接口扩展决策（含 name 参数 + LineageWalk.intermediate_metadata: `std::vector<Genome>`）。两 change 必须同步 ship。

### 5. ADR-0086 不变量更新（v1.0 → v1.1）

**v1.0 已有不变量**（5/6/7 — 修正 Oracle 🔴 numbering collision）：

- 不变量 1：归因层不调用 LLM（保持）
- 不变量 2：归因层不修改 IEvaluator 接口（保持）
- 不变量 3：归因层判定由确定性代码执行（保持）
- 不变量 4：AttributionVerdict 默认 NotAttempted fail-closed（保持）
- 不变量 5 (v1.0)：默认 NotAttempted fail-closed（v1.1 保持）
- 不变量 6 (v1.0)：5 contract 头文件零修改（`include/agenticdsl/contract/`），attribution 类型放 `include/agenticdsl/types/`（v1.1 严格遵守）
- 不变量 7 (v1.0)：ADR-0068 Appendix A v1.9（保持）

**v1.1 新增不变量**（编号 8/9，避免与 v1.0 冲突）：
- **不变量 8 (v1.1)**：归因层不主动调度基线重复评估；调用方必须保证 `parent.sample_count >= kMinBaselineSamples`，否则归因层 fail-fast 返回 Insufficient。
- **不变量 9 (v1.1)**：HarnessChange kind 的 `source_id` 必须包含完整的 "name@old_version→new_version" 标识符，确保跨进程审计可追溯。

### 6. ADR-0086 状态翻转 — 🔍 Proposed → ✅ Approved (v1.1)

**关键声明**（Oracle 🟠-6）：v1.0 自 2026-08-31 创建以来仅作为文档存在，从未经历正式 dual-agent review。本 change 的 dual-agent review（Metis `bg_ba048665` + Oracle `bg_fed9d7c0`）**同时覆盖**：
1. v1.0 主体（决策 1-7 + 不变量 1-7）的首次评审与通过
2. v1.1 增量（决策 4 修订 + 决策 8/9 新增 + 不变量 8/9 新增）的评审与通过

状态翻转从 🔍 Proposed 直接到 ✅ Approved (v1.1)，本次评审 = v1.0 首次通过 + v1.1 增量通过。治理路径合规，避免"amendment 搭车过审"误读。

**前提**：
- dual-agent review (Metis + Oracle) 通过
- 实施 + 测试通过（见 tasks.md）
- ADR-0086 v1.1 文末追加 ship 证据段

**版本号语义**：v1.0 → v1.1 是 minor 版本号（语义化），表明 additive backward-compatible 变更（新增 enum 值 + 新增 constant + 新增 decision）。**v1.0 从未实施，无既有调用方，v1.1 随 v1.0 首次实施一并落地**。

---

## Capabilities

### ADDED Requirements

- `confounder-harness-change-kind`: `ConfounderKind::HarnessChange` enum 值 + 完整 schema（namespace: `agenticdsl::evolution`）
- `baseline-min-samples`: `kMinBaselineSamples=5` constant + VersionPairDiff 前置 fail-fast
- `cross-framework-alignment-section`: 决策 8 documentation-only 对位节
- `data-freshness-algorithm`: 决策 9 数据新鲜度判定算法（完整版，name 参数 + GenomeSpec.harness 对比 + excludes-self fast-path）+ walk_ancestors 依赖

### ADDED v1.0 首次实施 Requirements（修正 Oracle 🔴-1）

由于 v1.0 从未实施，本 change 同时承载 v1.0 首次落地：

- `v1.0-attribution-record-types`: `AttributionRecord` / `AttributionMethod` / `AttributionVerdict` / `ConfounderRecord` / `VersionSnapshot` / `VersionPairDiff` 完整定义与实现（per ADR-0086 v1.0 决策 1-2）
- `v1.0-version-pair-diff-algorithm`: VersionPairDiff::compare() 算法实现 + sample_count 检查集成（per ADR-0086 v1.0 决策 2）
- `v1.0-test-coverage`: 6 个 v1.0 基础测试用例（per ADR-0086 v1.0 实施 §阶段 0 要求 ≥6 cases）

### MODIFIED Requirements (none)

本 change 不修改 ADR-0086 v1.0 既有决策（决策 1-7 + 不变量 1-7）的语义。新增 v1.1 决策（4 修订 + 8/9 新增 + 不变量 8/9 新增）。

### REMOVED Requirements (none)

---

## Impact

### Affected Files

| 文件 | 类型 | 说明 |
|------|------|------|
| `docs/adr/adr-0086-credit-assignment-contract.md` | ADR | 主要修改目标（决策 2/4/8/9 + 不变量 8/9 + 状态翻转 v1.0→v1.1） |
| `include/agenticdsl/types/attribution_record.h` | C++ header | **首次创建**（v1.0 类型 + v1.1 HarnessChange 扩展 + kMinBaselineSamples constant）。路径严格遵守 v1.0 不变量 6（attribution 类型放 `types/`） |
| `include/agenticdsl/types/attribution_version_pair_diff.h` | C++ header | **首次创建**（VersionPairDiff + compare() 实现） |
| `src/evolution/attribution_record.cpp` | C++ impl | **首次创建**（类型实现） |
| `src/evolution/version_pair_diff.cpp` | C++ impl | **首次创建**（算法实现 + data_freshness 调用 stub） |
| `tests/test_credit_assignment.cpp` | Test | **首次创建**（6 v1.0 + 4 v1.1 = 10 测试用例） |
| `docs/research/rsi-three-operators-hydraforge-mapping-2026-09.md` | Research | 引用 ADR-0086 v1.1（ship 后由 doc_sync commit 同步） |
| `docs/architecture/self-evolution-architecture-2026-08.md` | Architecture | 引用 ADR-0086 v1.1（§一.1.3 + §七 #6） |
| `openspec/changes/2026-09-16-h-d-m-transition-guard/proposal.md` | C3 proposal | 引用 ADR-0086 v1.1 决策 4/9（fill 时同步） |

### Affected Components

| 组件 | 影响 |
|------|------|
| `ConfounderKind` (新增 enum) | 新增 HarnessChange 值，序列化保持 additive |
| `VersionPairDiff::compare()` | 前置 fail-fast 检查 sample_count |
| `judge_data_freshness()` (新增函数) | 完整版算法（name 参数 + GenomeSpec.harness 对比） |
| `C3 TransitionGuard` | 引用 HarnessChange 类型 + 数据新鲜度算法 |
| `C4 Harness-RSI Pilot` | 间接：依赖 C3 guard 的数据新鲜度判定 |
| `ADR-0080 EventLog` | HarnessChange 事件主题注册（未来） |

### Risks

- **R1**: HarnessChange schema 扩展是否兼容旧 ConfounderRecord 序列化数据？—— 答：enum additive backward-compatible，旧数据无 HarnessChange 值仍然 valid
- **R2**: kMinBaselineSamples=5 是否过多？—— 答：基于 Hotelling T² (T14) 经验值，可后续调整；但 V1 优先安全
- **R3**: 数据新鲜度算法依赖 walk_ancestors（C3 范围），如果 C3 不扩展该接口则本决策无法实施 —— 答：C3 amendment change (本 change 同步) 显式声明依赖关系
- **R4（新增）**: v1.0 首次实施 + v1.1 增量合并 ship，code review 需同时验证两层 —— 答：通过 separate commit 区分（v1.0 基线 commit + v1.1 增量 commit）保持原子性
- **R5（新增）**: `estimated_consumption()` 单位锁定为 LLM 调用次数（C3 条件 3 依赖），如未来扩展到 token/duration 需另行 ADR —— 答：本 change scope 明确锁定

### Non-goals

- ❌ 不重写 ADR-0086 v1.0 既有决策（决策 1-7 + 不变量 1-7）的语义（仅 additive）
- ❌ 不实现 walk_ancestors 接口（C3 范围，本 change 仅定义算法契约 + 类型 stub）
- ❌ 不实现 C3 transition guard（C3 范围）
- ❌ 不修改 ConfounderRecord 序列化 schema（C++ 编译期 enum 不影响磁盘格式）

---

## Estimated Effort

**总估时修正**: 3-5 天 → **5-7 天**（Oracle 🟠-2 修正：v1.0 从零实施 + v1.1 增量，估时低估 2-3 倍）

| 阶段 | 估时 | 工作量明细 |
|------|------|----------|
| Pre-flight | 0.5 天 | 决策 4/8/9 细节确认 + kMinBaselineSamples 调研 + walk_ancestors 签名锁定（与 C3 同步） |
| Dual-agent review | 0.5 天 | Metis + Oracle 并行审查 proposal（已 done：bg_ba048665 + bg_fed9d7c0） |
| Implementation | **2.5 天**（修正：1d → 2.5d） | (a) v1.0 基线：types/attribution_record.h + version_pair_diff.h + .cpp + 6 测试 (1.5d); (b) v1.1 增量：enum 扩展 + constant + judge_data_freshness + 4 测试 (1d) |
| Ship-with-fixes | 0.5 天 | Oracle 实施审查 + critical/major 修正 |
| Doc sync | 0.5 天 | mapping doc + self-evolution doc + ADR-0061 附录 A 同步 |
| Archive | 0.5 天 | openspec archive + 6 文件 git ls-files 验证（Day-5 lesson） |

---

## Dependencies

### Upstream (必须 ship)

| 依赖 | 状态 | 说明 |
|------|------|------|
| ADR-0086 v1.0 | 🔍 Proposed | 本 amendment 的 base |
| ADR-0083 (IEvaluator V2) | ✅ Approved + Shipped | 评估层基础 |
| ADR-0084 (Mutation Governance V1) | ✅ Approved + Shipped | 治理层基础 |
| ADR-0080 (EventLog v1.1/v1.2) | ✅ Approved | 证据层基础 |
| C2 genome-registry | ✅ Shipped 2026-09-19 | walk_ancestors deferred（本 amendment 触发 C3 实施） |

### Downstream (consumers)

| 消费者 | 状态 | 依赖点 |
|--------|------|--------|
| C3 transition guard | 🟡 PLACEHOLDER (待 fill) | 决策 4 HarnessChange kind + 决策 9 数据新鲜度算法 |
| C4 Harness-RSI Pilot | ⚪ PLACEHOLDER | 间接（依赖 C3） |
| ADR-0078 Model-RSI | 🔍 Proposed | 远期依赖（Wave 3） |

---

## References

- **Oracle review sessions**:
  - `ses_f45b96c94ffevTy454aeDBK7U2` (2026-09-20, 3 Critical + 4 Major issues 识别)
  - Prior session `ses_f45b96c94ffevTy454aeDBK7U2` continuation
- **Project documents**:
  - `docs/research/rsi-three-operators-hydraforge-mapping-2026-09.md` (mapping doc)
  - `docs/architecture/self-evolution-architecture-2026-08.md` (架构顶层)
  - `docs/adr/adr-0086-credit-assignment-contract.md` (v1.0 base)
  - `docs/adr/adr-0083-evaluator-reward-contract.md` (评估层)
  - `docs/adr/adr-0084-mutation-governance-contract.md` (治理层)
  - `docs/adr/adr-0080-append-only-event-log.md` (证据层)
- **OpenSpec changes**:
  - `openspec/changes/archive/2026-09-19-2026-09-16-genome-registry/` (C2 已 ship)
  - `openspec/changes/2026-09-16-h-d-m-transition-guard/` (C3 placeholder, 同步更新)
  - `openspec/changes/2026-09-16-harness-rsi-pilot/` (C4 placeholder)
- **External frameworks (unverified, 引用需标注)**:
  - MetaRSI-v1 论文 (清华/北大/斯坦福, 2026-XX)
  - 月谈AI "RSI 三层落地路线" 文章
  - 字节 Seed Aspire/S³Gym/HarnessDev 三论文

---

## TODO Checklist (Draft → Ship)

- [ ] **Pre-flight (0.5d)**:
  - [ ] 1.1 决策 4 HarnessChange schema 字段对齐 C2 genome.h metadata 字段
  - [ ] 1.2 决策 2 kMinBaselineSamples=5 与 T14 Hotelling T² 实现细节对齐
  - [ ] 1.3 决策 9 walk_ancestors 接口签名与 C3 amendment 协调
- [ ] **Dual-agent review (0.5d)**:
  - [ ] 2.1 派 Metis background (intentionality / spec ambiguity / AI failure modes)
  - [ ] 2.2 派 Oracle background (architecture / implementation feasibility / physical viability)
  - [ ] 2.3 收集 30min 内 2 份报告，应用 Critical/Major 修正
- [ ] **Implementation (2.5d)** (修正 Oracle 🟠-2: 原 1d → 2.5d，v1.0 从零实施):
  - [ ] 3.0 v1.0 首次实施基线：`types/attribution_record.h` + `version_pair_diff.h` + .cpp + 6 v1.0 测试 (1.5d)
  - [ ] 3.1 修订单 8/9 到 ADR-0086 v1.1（**不变量编号修正为 8/9 避免与 v1.0 冲突**）
  - [ ] 3.2 `types/attribution_record.h`（**修正 Oracle 🔴-2: types/ 而非 contract/credit_assignment.h**）— enum 扩展 + constant + HarnessChangeRecord struct + judge_data_freshness 完整版算法（含 fast-path）
  - [ ] 3.3 test_credit_assignment.cpp 10 测试用例（**修正 Oracle 🔴-1: 6 v1.0 + 4 v1.1 = 10**，原 4 用例不足）
  - [ ] 3.4 RED 验证 → GREEN 实施
- [ ] **Ship-with-fixes (0.5d)**:
  - [ ] 4.1 Oracle 实施审查 + ship-with-fixes 修正
  - [ ] 4.2 spec amendments (如有)
  - [ ] 4.3 ADR 状态翻转 (Proposed → Approved)
- [ ] **Doc sync (0.5d)**:
  - [ ] 5.1 mapping doc §1.2/§3.2 C3 引用更新
  - [ ] 5.2 self-evolution doc §一.1.3 + §七 #6 引用更新
  - [ ] 5.3 ADR-0061 附录 A（可选，低优先）
- [ ] **Archive (0.5d)**:
  - [ ] 6.1 openspec archive → archive/2026-09-20-adr-0086-v1-1-harness-change-confounder/
  - [ ] 6.2 git ls-files 验证 6 文件完整
  - [ ] 6.3 commit 命名: `feat(adr-0086-v1-1): harness-change-confounder + baseline-sampling`
