# Proposal: H→D→M Transition Guard —— MetaRSI-v1 关键规则强制

> **STATUS**: DRAFT (PLACEHOLDER removed, ready for dual-agent review)
> **Type**: hard-placeholder → 完整 proposal（实施待 C2 + ADR-0086 v1.1 ship 后启动）
> **优先级**: P0 (Wave 2, Sprint 35+, blocked by ADR-0086 v1.1 amendment ship)
> **估时**: 3-4 天（受 walk_ancestors 接口扩展决策影响）
> **追溯范围**: `docs/roadmap/2026-09-16-pdk-chat-demo-evolution-roadmap.md` §四 C3
> **关联 ADR**: ADR-0083 (✅ V2 Shipped), ADR-0084 (✅ V1 Shipped), **ADR-0086 (✅ Approved v1.1, ship 2026-09-20)**, ADR-0080 (✅ v1.2), ADR-0061-02 (✅ T14 Shipped)
> **依赖上游**:
>   - C2 genome-registry ✅ Shipped 2026-09-19 (`archive/2026-09-19-2026-09-16-genome-registry/`)
>   - **ADR-0086 v1.1 ✅ Shipped 2026-09-20** (`openspec/changes/archive/2026-09-20-adr-0086-v1-1-harness-change-confounder/`)

> **2026-09-20 ADR-0086 v1.1 ship 通知** (per OpenSpec Task 6.4):
> - 本 change 现可 fill — 不再阻塞于 ADR-0086 v1.1 ship
> - 可引用 `agenticdsl::evolution::ConfounderKind::HarnessChange` (类型已 ship, include 路径 `agenticdsl/types/attribution_record.h`)
> - 可引用 `judge_data_freshness(data, current, registry)` 函数 (算法 stub 已 ship, 完整版依赖本 change 实装的 `walk_ancestors` — C3 必须扩展该接口)
> - **GenomeVersion struct 单一所有权** — 已在 `include/agenticdsl/types/attribution_record.h` 定义, 本 change **必须复用本文件, 不得重复定义** (避免 ODR 违规, per spot-check New Issue 1)
> - **walk_ancestors 签名契约** (per ADR-0086 v1.1 决策 9 + Oracle 🔴-5 修正):
>   - 签名: `virtual Result<LineageWalk, GenomeError> walk_ancestors(const std::string& name, uint64_t from_version, std::optional<uint64_t> to_version = std::nullopt) = 0;`
>   - LineageWalk schema: `{ std::vector<uint64_t> intermediate_versions; std::vector<agenticdsl::genome::Genome> intermediate_metadata; }` ← **Genome 含 spec.harness**, NOT GenomeMetadata
>   - ADR-0086 v1.1 amendment → Ship 前置（`openspec/changes/2026-09-20-adr-0086-v1-1-harness-change-confounder/`）
> **下游**: C4 Harness-RSI Pilot
> **Oracle 评审**: `ses_f45b96c94ffevTy454aeDBK7U2` (continuation, 2026-09-20) + `ses_f55f307f6ffeRJ9SIny8iUbZ8Y` (M2 评审)

---

## Why（背景）

### MetaRSI-v1 关键规则（external framework, unverified）

- **禁止 H→M 直跳**：用旧 Harness 数据训练新能力 → 训练目标混乱（字节 Seed HarnessDev 论文实证：64 次版本迭代仅 53.1% 修改方向一致）
- **必须 H→D→M**：先改 Harness，再跑 D 生成与新能力匹配的新数据，再 M 训练
- **验证逻辑与生成分离**：确定性代码，模型无权改

### HydraForge 现状（per Oracle M2 + continuation 评审）

| 组件 | 状态 | C3 消费点 |
|------|------|----------|
| ADR-0083 IEvaluator V2 | ✅ Shipped | 条件 2（回归门 PASS）输入 |
| ADR-0084 Mutation Governance V1 | ✅ Shipped | L1-L4 变异等级白名单 |
| ADR-0086 Credit Assignment v1.1 | 🟡 Amendment in flight | 条件 1（归因 Attributed）+ 数据新鲜度算法 |
| ADR-0080 AppendOnlyEventLog v1.2 | ✅ Shipped | `evolution.scheduler.denied` 事件发射 |
| C2 IGenomeRegistry | ✅ Shipped 2026-09-19 | walk_ancestors 待扩展 |
| `ExecutionBudget` + `IBudgetController` | ✅ Shipped Sprint 11 | 条件 3（预算充足）输入 |
| `Hotelling T²` (T14) | ✅ Shipped | VersionPairDiff 方差估计基线 |

### 没有守卫的后果

- Harness-RSI 与 Model-RSI 可被任意组合（违反 MetaRSI-v1 规则）
- 验证逻辑散落多个 ADR，无统一入口
- 自进化闭环（per `self-evolution-architecture-2026-08.md` §三）无法落地
- ADR-0086 v1.1 amendment 暴露的 H→D→M 规则无状态机落地（仅文字契约）

### Oracle M2 关键决策（继承 + 本次更新）

- **取消**原计划的 3 算子接口 (IDataRSI/IHarnessRSI/IModelRSI) — 与既有 ADR-0083/0084/0086 契约栈重复
- 改为**轻量状态机** + `can_transition()` + `evaluate_readiness()` 双入口
- 复用既有契约，**不**新建平行接口
- YAGNI 原则：pilot (C4) 验证价值前不造重型框架
- **本 change 新增**：Oracle continuation `ses_f45b96c94ffevTy454aeDBK7U2` 识别 7 项 action 全部落地（见 §What Changes）

---

## What Changes

### 1. TransitionGuard 类（核心实现，修正 Oracle 🔴-3/4 + 🟠-3/5）

**文件**: `include/agenticdsl/evolution/transition_guard.h` + `src/modules/evolution/transition_guard.cpp` (~280 行)

```cpp
namespace agenticdsl::evolution {

// D1 状态枚举（per YAGNI，简单 4 状态 + Done）
enum class EvolutionState {
    Idle,        // 未启动或完成
    Harness,     // Harness 变更中
    Data,        // 数据采集中（与新 Harness 匹配）
    Model,       // 模型训练中
    Done         // 全部完成（→等价 Idle，禁止→非 Idle）
};

// D2 状态机判定结果
struct EvolutionVerdict {
    bool can_proceed = false;
    EvolutionState recommended_next = EvolutionState::Idle;
    std::string reason;
    std::vector<std::string> failed_conditions;
};

// D3 编译期判定（修正 Oracle 🔴-3：literal type，返回 bool）
// 编译期+运行期双重断言的物理可行拆分
constexpr bool constexpr_can_transition(
    EvolutionState from, EvolutionState to) noexcept;

// D4 运行期主入口 1: 状态转换判定（修正 Oracle 🔴-4：GenomeVersion name+version）
EvolutionVerdict can_transition(
    EvolutionState from,
    EvolutionState to,
    const agenticdsl::evolution::GenomeVersion& current,
    const agenticdsl::evolution::GenomeVersion& last_harness_change);

// D5 运行期主入口 2: 综合 readiness 评估（修正 Oracle 🟠-5：删除冗余 confounders 参）
EvolutionVerdict evaluate_readiness(
    const AttributionRecord& attribution,    // ADR-0086 v1.1（attribution 内嵌 confounders）
    const RewardSignal& eval_signal,         // ADR-0083 V2
    const ExecutionBudget& budget);          // core type

// D6 GenomeVersion 定义（修正 Oracle 🔴-4a）
struct GenomeVersion {
    std::string name;
    uint64_t version;
};

}  // namespace agenticdsl::evolution
```

**核心规则实现**：

```cpp
// 编译期查表（literal type）
constexpr bool constexpr_can_transition(EvolutionState from, EvolutionState to) noexcept {
    // H → M 禁跳（MetaRSI-v1 关键规则）
    if (from == EvolutionState::Harness && to == EvolutionState::Model) {
        return false;
    }
    // 其他合法转换（H→D, D→M, D→H, M→D, M→H 全部允许）
    return true;
}

// 运行期 can_transition 调 constexpr + 包装完整 Verdict
EvolutionVerdict can_transition(EvolutionState from, EvolutionState to, ...) {
    if (!constexpr_can_transition(from, to)) {
        return {false, EvolutionState::Idle,
                "H→M forbidden, must run H→D→M",
                {"h_to_m_direct_skip"}};
    }
    return {true, to, "transition allowed", {}};
}
```

### 2. evaluate_readiness 3+1 条件矩阵（Oracle 修正，类型修正 🔴-4）

> **重要修正（per Oracle 评审）**：原"4 条件"逻辑冗余，条件 1（Attributed）蕴含条件 4（无未控制混杂），实际 3 个独立条件 + 1 派生注释。

| # | 条件 | 契约源 | 验证规则（修正 Oracle 🔴-4/🟠-2） |
|---|------|--------|------------------------|
| 1 | **归因 Attributed** | ADR-0086 v1.1 决策 9 | `attribution.verdict == AttributionVerdict::Attributed`（sample_count 检查已内嵌于 ADR-0086 v1.1 决策 2 的 compare()） |
| 2 | **回归门 PASS** | ADR-0083 IEvaluator V2 | `eval_signal.quality != RewardSignal::Quality::Poor` AND Hotelling T² 偏差 < 阈值（修正 🔴-4：实际枚举值是 Poor，无 Failed） |
| 3 | **预算充足** | `ExecutionBudget` + `IBudgetController` | `budget.max_evolution_llm_calls == -1 \|\| budget.evolution_llm_calls_used + estimated_llm_calls <= budget.max_evolution_llm_calls`（修正 🔴-4：实际无 remaining 字段，使用显式维度） |

**派生注释（条件 4）**：
- "无未控制混杂" 不是独立门，其检查已内嵌于条件 1 的算法
- ADR-0086 v1.1 决策 2 算法：`未控制混杂 → verdict=Confounded`
- 因此 `Attributed` 判定必然通过混杂检查

**`estimated_consumption` 单位锁定**（修正 Oracle Open Question 2）：本 change 锁定为 **LLM 调用次数**（`estimated_llm_calls`），如未来扩展到 token/duration 需另行 ADR。`budget.max_evolution_llm_calls` 是 T3 已 ship 的 evolution 维度。

### 3. 与既有契约的集成点

| 集成点 | 契约 | C3 使用方式 |
|--------|------|------------|
| 评估层 | `IEvaluator::evaluate()` (ADR-0083 V2) | 直接读取 `RewardSignal` 作为条件 2 输入 |
| 治理层 | `MutationGovernance::authorize()` (ADR-0084 V1) | V1 **不绑定** MutationGovernor.commit()（YAGNI + Oracle M2 spirit）；guard 与 governor 职责分离 |
| 归因层 | `CreditAssignment::attribute()` (ADR-0086 v1.1) | 读取 `AttributionRecord` 作为条件 1 输入 |
| Genome 谱系 | `IGenomeRegistry::walk_ancestors()` (C2 amendment) | ADR-0086 v1.1 决策 9 数据新鲜度算法依赖 |

**Enforcement point 绑定（Oracle 评审决策）**：

| 路径 | 是否绑定 guard | 理由 |
|------|----------------|------|
| C4 Harness-RSI Pilot 入口 | **必须**强制调 `evaluate_readiness()` | 阻断 H→M 直跳进入生产 |
| MutationGovernor.commit() | **不绑定**（V1） | 职责分离：governor 管变异授权，guard 管跨算子时序；YAGNI |
| 训练管线入口（ADR-0078 立项时） | **必须**调 guard verdict 作为前置 | 前向约束条款：任何 Model-RSI 提案必须引用 guard |
| 无 Genome 版本锚点 | **fail-closed 返回 NotReady** | spec 硬性要求，非注释 |

### 4. walk_ancestors 接口扩展（Oracle 关键发现，修正 Oracle 🔴-5 + 🟠-4 + 🔴-6）

**问题**：C2 ship 时 `walk_ancestors` 被 deferred，**未实装**（`include/agenticdsl/genome/genome.h:87` 仅 5 public virtual）。

**决策**：**C3 范围内给 IGenomeRegistry 加 public `walk_ancestors`**（contract extension on shipped/archived C2 interface）。BREAKING-lite 治理声明：纯虚添加，唯一实现 FilesystemGenomeRegistry 同步改，无第二实现者。

```cpp
// C3 范围新增 (include/agenticdsl/genome/genome.h)
class IGenomeRegistry {
public:
    // ... 现有 5 methods ...
    
    // C3 新增: 谱系遍历（修正 Oracle 🔴-5: name 参数 + 版本 per-name 模型）
    struct LineageWalk {
        std::vector<uint64_t> intermediate_versions;      // closest-first (per C2 ship spec)
        std::vector<agenticdsl::genome::Genome> intermediate_metadata;  // 修正 Oracle 🔴-6: 类型为 Genome（含 spec.harness），NOT GenomeMetadata
    };
    
    virtual Result<LineageWalk, GenomeError> walk_ancestors(
        const std::string& name,                                  // ← 修正 Oracle 🔴-5
        uint64_t from_version,
        std::optional<uint64_t> to_version = std::nullopt) = 0;
    
    // Walk 语义契约（C2 ship 既有 + C3 增量）：
    // - closest-first 排序（C2 ship spec 已定）
    // - excludes-self（from_version 不在返回集中）
    // - 10000 depth cap（C2 ship M1 修复）
    // - 每版本 HMAC 校验（修正 Oracle 提及的 walk 安全审计）
    // - 失败映射: NotFound | BrokenLineage | IntegrityViolation
};
```

**Walk 性能与并发（Oracle 评审 OK）**：
- 每跳 = 1 次目录读 + YAML parse + HMAC verify，O(depth)。实际深度 <100，每次归因判定 <<10ms。
- 仅 commit 由 `commit_mutex_` 串行；walk 为纯读，配合 C2 M1 sig-first 原子写序，并发 walk-during-commit 最坏读到 NotFound → 决策 9 映射 Insufficient（fail-closed 方向正确）。无数据竞争点。

**Genome-registry spec delta（修正 Oracle 🟠-4）**：C3 change 增补 `specs/genome-registry/spec.md`：
- **MODIFIED Requirement**: "5 public methods" → "6 public methods"
- **NEW Scenario**: walk_ancestors(name, from, to) 返回 closest-first LineageWalk，包含 spec.harness 字段可被 decision 9 使用
- **NEW Scenario**: walk 失败返回 Result::failure 对应 3 种 GenomeError
- **NEW Scenario**: walk 10000 depth cap + cycle detection（C2 M1 修复语义）

**治理声明**：本扩展是 C3 范围内的 contract amendment，须在 C3 proposal 显式声明 BREAKING-lite（pure virtual 添加）。C2 已 archive，C2 唯一实现（FilesystemGenomeRegistry）同步改。

**Walk 行为契约**：
- 默认 `to_version = nullopt` → 追溯到 root
- 循环检测：`visited = name@version` pair（per C2 ship 修复）+ 10000 depth cap
- HMAC 验证：每个 intermediate version 都需通过签名校验
- 失败 → `Result::failure(GenomeError::BrokenLineage | NotFound | IntegrityViolation)`

**Workaround（V1 fallback，仅在 IGenomeRegistry 扩展未 ship 时）**：
- 用 `list_versions` + 逐个 `load` 模拟 walk
- 性能差（O(N) load vs O(1) walk），但功能等价
- **不推荐**：推迟 C3 ship 直到扩展 ship

### 5. 数据新鲜度判定（ADR-0086 v1.1 决策 9 的 C3 集成，修正 Oracle 🔴-4）

C3 `evaluate_readiness()` 条件 1 调用 ADR-0086 v1.1 决策 9。修正点：

```cpp
// C3 集成代码 (transition_guard.cpp)
#include <agenticdsl/types/attribution_record.h>  // 修正 Oracle 🔴-2: types/ 而非 contract/
#include <agenticdsl/contract/reward_signal.h>     // 修正 Oracle 🔴-4: 实际是 reward_signal.h
#include <core/types/budget.h>                      // ExecutionBudget

EvolutionVerdict evaluate_readiness(
    const AttributionRecord& attribution,    // ADR-0086 v1.1（含内嵌 confounders）
    const RewardSignal& eval_signal,         // ADR-0083 V2
    const ExecutionBudget& budget) {         // 修正 Oracle 🔴-4: 3 参，无冗余 confounders

    // 条件 1: 归因 Attributed（修正 Oracle 🔴-4/🟠-2）
    // - 实际枚举名: AttributionVerdict（修正笔误 AttributedVerdict）
    // - sample_count 检查已内嵌于 ADR-0086 v1.1 决策 2 的 compare()（无需 C3 重复）
    // - 数据新鲜度判定已内嵌于 ADR-0086 v1.1 决策 9（walk_ancestors + harness 对比）
    if (attribution.verdict != AttributionVerdict::Attributed) {
        return {false, EvolutionState::Idle,
                attribution.reason,
                {"attribution_not_attributed"}};
    }
    
    // 条件 2: 回归门 PASS（修正 Oracle 🔴-4: 实际枚举值 Poor 而非 Failed）
    if (eval_signal.quality == RewardSignal::Quality::Poor) {
        return {false, EvolutionState::Idle,
                "regression gate FAILED",
                {"regression_gate_failed"}};
    }
    
    // 条件 3: 预算充足（修正 Oracle 🔴-4: ExecutionBudget 无 remaining 字段，用显式维度）
    // estimated_llm_calls 是 C3 锁定的 estimated_consumption 单位（LLM 调用次数）
    const int64_t estimated_llm_calls = /* 由 C3 调用方传入或默认 1 */;
    if (budget.max_evolution_llm_calls != -1 &&
        budget.evolution_llm_calls_used + estimated_llm_calls > budget.max_evolution_llm_calls) {
        return {false, EvolutionState::Idle,
                "budget insufficient",
                {"budget_insufficient"}};
    }
    
    // 注: 条件 4 "无未控制混杂" 不是独立门
    // ADR-0086 v1.1 决策 2 算法: 未控制混杂 → verdict=Confounded
    // 因此 Attributed 判定已通过混杂检查
    
    return {true, EvolutionState::Model, "all conditions met", {}};
}
```

### 6. 状态机范围决策（per YAGNI）

**选择简单 5 状态（Idle/Harness/Data/Model/Done）**（per Oracle M2）：
- 不用复杂的 Ready/NotReady/Blocked 9 状态机
- Verdict 内含 `can_proceed` bool + `recommended_next` enum + `reason` string + `failed_conditions` list
- 测试：6 状态转换（H→D/H→M✗/D→M/D→H/M→D/M→H）+ 2 边界（Idle→任意/任意→Idle）+ 自环（any→any no-op）+ Done→Idle 等价 = **11-13 cases**
- 状态机完备性缺口：**Done→非 Idle 未定义**——建议 Done 等价 Idle（重启新一轮合法）；spec 明确化

### 7. 错误处理 + 事件发射（修正 Oracle 🟠-5：幻影事件主题）

**C3 emit 的事件主题需在 ADR-0068 v1.2.1 Appendix A 注册**（Oracle 提及幻影主题风险）：

| 事件主题 | Payload | 触发条件 |
|---------|---------|---------|
| `evolution.transition.denied` | `{from, to, reason, current_genome, last_harness_change}` | `can_transition()` 返回 can_proceed=false |
| `evolution.readiness.denied` | `{failed_conditions, attribution_verdict, eval_quality, budget_state}` | `evaluate_readiness()` 返回 can_proceed=false |

**事件发射契约**（修正 Oracle 提及的 `evolution.scheduler.denied` 幻影主题）：
- 原 proposal 写 `evolution.scheduler.denied` —— **该主题在 ADR-0068 Appendix A 中不存在**，是幻影主题
- 修正为 `evolution.transition.denied` + `evolution.readiness.denied` 两个明确主题
- **注册要求**：C3 change 需在 `openspec/changes/2026-09-16-h-d-m-transition-guard/specs/event-emission-contract/spec.md` 增补 MODIFIED Requirement（ADR-0068 Appendix A 注册）

**其他错误处理**：
- 错误码与 ADR-0023 对齐（NotReady → ErrorCode::NotReady）
- 不抛异常（EvolutionVerdict 是值类型）
- 日志走 `agenticdsl::ILogger`（per ADR-0068 §3.1 facade）

### 8. ADR-0086 v1.1 行为矩阵（依赖可用性，修正 Oracle 🔴-7：fail-closed）

| ADR-0086 状态 | C3 条件 1 行为 | C3 整体 verdict |
|--------------|---------------|---------------|
| ✅ Approved (v1.1 ship 后) | 正常走归因判定 | 正常 |
| 🟡 Proposed / ❌ 不可用 | **fail-closed**：条件 1 恒 NotReady（reason="attribution_unavailable"），**不允许跳过放行** | 恒 NotReady |

**关键约束（修正 Oracle 🔴-7：原占位 spec 写 "degraded mode skip attribution check" 是 fail-open 反模式，本 proposal 明确否定）**：
- ADR-0086 不可用状态下，C3 guard **拒绝所有 H→D→M 转换**（"宁可误拒，不可误放"）
- 这是治理上的 fail-closed 设计，与 ADR-0084 fail-closed 一致
- spec.md 必须**禁止**"degraded skip" scenario（修正 Oracle 抓到的 spec 矛盾）

---

## Capabilities

### ADDED Requirements

- `h-d-m-transition-rule`: H→M 禁止规则（**编译期 `constexpr bool` + 运行期 `EvolutionVerdict` 双重断言**，修正 Oracle 🔴-3 literal type）
- `evaluate-readiness-3-plus-1`: **3+1** 条件 evaluate_readiness 矩阵（修正 Oracle 🔴-7：原 spec 写"4 条件"是矛盾）
- `existing-contract-integration`: 与 ADR-0083 V2 / ADR-0084 V1 / ADR-0086 v1.1 / IBudgetController / IGenomeRegistry v2 (walk_ancestors) 集成
- `enforcement-point-binding`: C4 强制调 guard + ADR-0078 立项前向约束 + 无 Genome fail-closed
- `walk-ancestors-extension`: IGenomeRegistry::walk_ancestors 公开接口扩展（**含 name 参数 + LineageWalk.intermediate_metadata: std::vector<Genome>**）
- `data-freshness-integration`: 条件 1 内嵌 ADR-0086 v1.1 决策 9 数据新鲜度判定（**完整版 + fast-path**）
- `evolution-transition-denied-event`: guard 失败 emit `evolution.transition.denied` 事件（**修正 Oracle 🟠-5：注册到 ADR-0068 Appendix A**）
- `evolution-readiness-denied-event`: evaluate_readiness 失败 emit `evolution.readiness.denied` 事件（同上）
- `fail-closed-default`: ADR-0086 不可用时 guard 恒 NotReady（修正 Oracle 🔴-7：禁止 degraded-skip）

### MODIFIED Requirements

- `evolution-state-machine`: **5 状态**枚举（Idle/Harness/Data/Model/Done）+ Verdict struct + Done 等价 Idle 语义
- `ig-genome-registry-contract-extension`: `IGenomeRegistry::walk_ancestors` 公开方法新增（**修正 Oracle 🟠-4：genome-registry spec delta**）
- `adr-0068-event-emission-contract-appendix-a`: 新增 2 个事件主题（修正 Oracle 🟠-5）

### REMOVED Requirements (none)

---

## Non-goals

- ❌ **不**新建 3 算子接口 (IDataRSI/IHarnessRSI/IModelRSI) — Oracle M2 评审取消
- ❌ **不**实现 Model-RSI 实际执行（依赖 ADR-0078）
- ❌ **不**实现 IModelRSI（仅在 ADR-0078 下登记占位）
- ❌ **不**实现完整自进化闭环（待 C4 pilot 验证）
- ❌ **不**绑定 MutationGovernor.commit()（V1，YAGNI；governor 与 guard 职责分离）
- ❌ **不**修改 `ConfounderRecord` 序列化 schema（C++ 编译期 enum 不影响磁盘格式；属 ADR-0086 v1.1 范围）
- ❌ **不**实现 walk_ancestors 之外的 IGenomeRegistry 扩展

---

## Estimated Effort（修正 Oracle 🟠-2）

**总估时修正**: 3-4 天 → **4-5 天**（Oracle 修正：walk_ancestors 公开接口扩展含 cycle detection + HMAC 逐版本校验 + 11-13 tests，估时 1.5d → 2.5d）

| 阶段 | 估时 | 工作量 |
|------|------|--------|
| Pre-flight (依赖 C2 + ADR-0086 v1.1 ship) | 0.5d | 锁定 walk_ancestors 签名（含 name 参数 + LineageWalk 类型）+ ADR-0086 v1.1 决策 4/9 接口 + genome-registry spec delta |
| Dual-agent review (Metis + Oracle) | 0.5d | per AGENTS.md §模式 8（已 done：bg_ba048665 + bg_fed9d7c0） |
| Implementation | **2.5d**（修正 1.5d → 2.5d） | (a) IGenomeRegistry::walk_ancestors 扩展（contract extension on C2 archived interface，~80 行 impl + cycle detection + HMAC 逐版本校验）; (b) TransitionGuard 实现（~280 行 + 11-13 tests）; (c) ADR-0068 Appendix A 注册 2 个新事件主题 |
| Ship-with-fixes | 0.5d | Oracle 实施审查 + critical/major 修正 |
| Doc sync | 0.25d | mapping doc + self-evolution doc 同步 |
| Archive | 0.25d | openspec archive + 6 文件 git ls-files 验证（Day-5 lesson） |

---

## Dependencies

### Upstream（必须 ship，本 change 才能实施）

| 依赖 | 状态 | 关键阻塞 |
|------|------|---------|
| C2 genome-registry | ✅ Shipped 2026-09-19 | walk_ancestors 公开接口缺失，C3 必须扩展 |
| ADR-0086 v1.1 amendment | 🟡 In flight (`openspec/changes/2026-09-20-...`) | HarnessChange kind 缺失，C3 条件 1 子例程无类型可调 |

### Downstream（consumers）

| 消费者 | 依赖点 |
|--------|--------|
| C4 Harness-RSI Pilot | 双门禁集成（guard + MutationGovernanceVerdict） |
| ADR-0078 Model-RSI Pilot（Wave 3） | 前向约束：训练管线必须调 guard verdict |

---

## Open Questions（已解决）

1. ✅ **walk_ancestors 签名扩展决策**（Oracle 🔴-5 已定）：A 方案 + name 参数 `walk_ancestors(const std::string& name, uint64_t from_version, std::optional<uint64_t> to_version)`，详见 §4
2. ✅ **`estimated_consumption` 单位锁定**（Oracle Open Question 2 已定）：**LLM 调用次数**（`estimated_llm_calls`），使用 `budget.max_evolution_llm_calls` 维度，详见 §2
3. ✅ **Done 状态语义**（Oracle 评审发现缺口）：Done 等价 Idle（重启新一轮合法），spec 明确化

---

## References

- **Oracle review sessions**（dual-agent review completed）:
  - `bg_ba048665` (2026-09-20, Metis: 5 Critical + 5 Major)
  - `bg_fed9d7c0` (2026-09-20, Oracle: 7 Critical + 6 Major + 8 Minor)
- **Prior Oracle reviews**:
  - `ses_f45b96c94ffevTy454aeDBK7U2` (continuation, 7 项 action 列表)
  - `ses_f55f307f6ffeRJ9SIny8iUbZ8Y` (2026-09-16, M2 评审取消 3 算子接口)
- **Project documents**:
  - `docs/roadmap/2026-09-16-pdk-chat-demo-evolution-roadmap.md` §四 C3
  - `docs/research/rsi-three-operators-hydraforge-mapping-2026-09.md` §3.2 C3 段 + §1.2/§1.3
  - `docs/architecture/self-evolution-architecture-2026-08.md` §三 + §五
  - `docs/adr/adr-0083-evaluator-reward-contract.md` (✅ Approved V2)
  - `docs/adr/adr-0084-mutation-governance-contract.md` (✅ Approved V1)
  - `docs/adr/adr-0086-credit-assignment-contract.md` (🟡 Amendment in flight → ready for re-review post 修正)
  - `docs/adr/adr-0061-02-behavioral-regression.md` (✅ Approved + T14 Shipped)
  - `docs/adr/adr-0080-append-only-event-log.md` (✅ Approved v1.2)
  - `docs/adr/adr-0068-event-emission-contract.md` (✅ Approved v1.2.1, **C3 需增补 2 个新事件主题**)
- **OpenSpec changes**:
  - `openspec/changes/archive/2026-09-19-2026-09-16-genome-registry/` (C2 ✅ Shipped, **genome-registry spec 待 MODIFIED delta by C3**)
  - `openspec/changes/2026-09-20-adr-0086-v1-1-harness-change-confounder/` (ADR-0086 v1.1 amendment, **已修正 7 Critical**)
  - `openspec/changes/2026-09-16-harness-rsi-pilot/` (C4 placeholder, 依赖 C3 ship)
- **External frameworks (unverified, 引用需标注)**:
  - MetaRSI-v1 论文 (清华/北大/斯坦福)
  - 字节 Seed HarnessDev 论文 (64 次版本迭代 53.1% 一致)
- **AGENTS.md patterns**:
  - §模式 8: OpenSpec dual-agent review（实施前硬性门）

---

## TODO Checklist（Draft → Ship）

### Pre-flight (0.5d)
- [ ] 1.1 确认 ADR-0086 v1.1 已 ship（含 HarnessChange kind + 决策 9 数据新鲜度算法）
- [ ] 1.2 锁定 walk_ancestors 接口签名（与 C2 amendment + ADR-0086 v1.1 决策 9 协调）
- [ ] 1.3 锁定 estimated_consumption() 实现细节（Oracle 评审决策 1）
- [ ] 1.4 设计 transition_guard.h 完整 API + Verdict struct + 测试 mock strategy

### Dual-agent review (0.5d)
- [ ] 2.1 派 Metis background（intentionality + spec ambiguity + AI failure modes）
- [ ] 2.2 派 Oracle background（architecture + implementation feasibility + physical viability）
- [ ] 2.3 30min 内收集 2 份报告，应用 Critical/Major 修正

### Implementation (1.5d)
- [ ] 3.1 include/agenticdsl/genome/genome.h 加 walk_ancestors public method + LineageWalk struct（C2 唯一实现同步改）
- [ ] 3.2 include/agenticdsl/evolution/transition_guard.h 完整 API（5 个 enum/struct + 2 主入口 + 1 constexpr helper）
- [ ] 3.3 src/modules/evolution/transition_guard.cpp 完整实现（~250 行）
- [ ] 3.4 tests/test_transition_guard.cpp 11-13 test cases：
  - can_transition: 6 状态转换（H→D, H→M ✗, D→M, D→H, M→D, M→H）+ 2 边界（Idle→*, *→自身）
  - evaluate_readiness: 3 独立条件 × 2 状态 = 6 cases
  - attribution_unavailable: 1 case (fail-closed 行为)
  - 编译期 constexpr: 1 case (sanity check)
- [ ] 3.5 RED → GREEN 验证

### Ship-with-fixes (0.5d)
- [ ] 4.1 Oracle 实施审查 background
- [ ] 4.2 应用 Critical 修正（独立 commit 保持原子性）
- [ ] 4.3 spec amendments (如有)
- [ ] 4.4 跑全量 ctest → 0 regression

### Doc sync (0.25d)
- [ ] 5.1 mapping doc §3.2 + §1.2 同步 C3 状态 ✅ → ⚪ → 🟡（fill）/ ✅（ship）
- [ ] 5.2 self-evolution doc §五 / §七 同步
- [ ] 5.3 ADR-0061 附录 A 同步（如已写）
- [ ] 5.4 跑 adr_lint + docs_drift_audit

### Archive (0.25d)
- [ ] 6.1 openspec archive → archive/2026-09-20-2026-09-16-h-d-m-transition-guard/
- [ ] 6.2 git ls-files 验证 6 文件完整（Day-5 lesson）
- [ ] 6.3 commit: `feat(transition-guard): h-d-m-state-machine-3+1-conditions`
- [ ] 6.4 通知 C4 fill author 引用 guard API
