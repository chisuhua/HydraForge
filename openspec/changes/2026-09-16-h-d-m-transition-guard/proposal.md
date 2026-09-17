# Proposal: H→D→M Transition Guard —— MetaRSI-v1 关键规则强制

> **STATUS: PLACEHOLDER** ⚠️
> **依赖 C2 (genome-registry) ship 后启动** — Genome 版本号是判断"过期数据"的基础
> **关联 ADR**: ADR-0083 (IEvaluator), ADR-0084 (Mutation Governance), ADR-0086 (Credit Assignment, 🔍 Proposed)
> **追溯范围**: `docs/roadmap/2026-09-16-pdk-chat-demo-evolution-roadmap.md` §四 C3
> **优先级**: P1 (Wave 2 中期, Sprint 35)
> **估时**: 2-3 天

---

## Why（背景概要）

**MetaRSI-v1 论文 (unverified)** 关键规则:
- **禁止 H→M 直跳**（用旧 Harness 数据训练新能力 → 训练目标混乱）
- **必须 H→D→M**（先改 Harness，再跑 D 生成与新能力匹配的新数据，再 M 训练）
- **验证逻辑与生成分离**（确定性代码，模型无权改）

**HydraForge 现状** (Oracle 评审 M2):
- ADR-0083 IEvaluator 已 ship ✅ (评估)
- ADR-0084 Mutation Governance 已 ship ✅ (变异门禁)
- ADR-0086 Credit Assignment 🔍 Proposed (归因)
- 但**无**统一的 H→D→M 状态机或转换守卫

**没有守卫的后果**:
- Harness-RSI 与 Model-RSI 可被任意组合（违反 MetaRSI 规则）
- 验证逻辑散落多个 ADR，无统一入口
- 自进化闭环（per `self-evolution-architecture-2026-08.md` §三）无法落地

**Oracle 评审 (M2) 关键决策**:
- **取消**原计划的 3 算子接口 (IDataRSI/IHarnessRSI/IModelRSI) — 与既有 ADR-0083/0084/0086 契约栈重复
- 改为**轻量状态机** + `can_transition()` 强制 H→D→M
- 复用既有契约，**不**新建平行接口
- YAGNI 原则：pilot (C4) 验证价值前不造重型框架

## What Changes（待起草时详细制定）

### 1. TransitionGuard 类
- TBD: `include/agenticdsl/evolution/transition_guard.h` (~200 行)
- TBD: `can_transition(from_state, to_state, current_genome_version, last_harness_change_version) -> EvolutionVerdict`
- TBD: 核心规则: `H → M` 直跳返回 `{can_proceed: false, reason: "H→M forbidden, must run H→D→M"}`

### 2. evaluate_readiness 4 条件矩阵
- TBD: 归因 Attributed (复用 ADR-0086 接口)
- TBD: 回归门 PASS (复用 ADR-0083 IEvaluator)
- TBD: 预算充足 (复用 ADR-0019 ExecutionBudget)
- TBD: 无未控制混杂 (复用 ADR-0086 ConfounderRecord)

### 3. 与既有契约的集成点
- TBD: ADR-0083 IEvaluator 评估结果输入
- TBD: ADR-0084 MutationGovernance 变异授权检查
- TBD: ADR-0086 CreditAssignment 归因结果（降级为可选，因 ADR-0086 仍 Proposed）

### 4. 状态机范围
- TBD: 简单 4 状态 (Idle / H / D / M) vs 复杂 (Ready/NotReady/Blocked)
- TBD: 推荐简单（per YAGNI）

### 5. 错误处理
- TBD: 守卫失败时 emit `evolution.scheduler.denied` 事件
- TBD: 错误码与 ADR-0023 对齐

### 6. 测试覆盖
- TBD: 12 个 test case: H→D ✓, H→M ✗, D→M ✓, D→H ✓, M→D ✓, M→H ✓
- TBD: evaluate_readiness 4 条件 × 2 状态矩阵 (8 case)
- TBD: 编译期+运行期双重断言 (can_transition 在 constexpr context 也能用)

## Capabilities（待详细制定）

### ADDED Requirements (placeholder)
- `h-d-m-transition-rule`: H→D→M 强制规则 (PLACEHOLDER)
- `evaluate-readiness-matrix`: evaluate_readiness 4 条件矩阵 (PLACEHOLDER)
- `existing-contract-integration`: 与 ADR-0083/0084/0086 集成点 (PLACEHOLDER)
- `evolution-scheduler-denied-event`: 守卫失败时 emit 事件 (PLACEHOLDER)

## Non-goals
- ❌ **不**新建 3 算子接口 (IDataRSI/IHarnessRSI/IModelRSI) — Oracle 评审取消
- ❌ **不**实现 Model-RSI 实际执行（依赖 ADR-0078）
- ❌ **不**实现 IModelRSI（仅在 ADR-0078 下登记占位）
- ❌ **不**实现完整自进化闭环（待 C4 pilot 验证）

## Estimated Effort
**总计**: 2-3 天（~200 行实现 + 12 test case + 双重断言验证）

## 详细制定 TODO（待 C2 ship 后）
- [ ] 1. 决策前置: 状态机范围 (4 状态 vs Ready/NotReady) + 与 ADR-0086 集成方式（可选依赖）
- [ ] 2. 写完整 design.md（can_transition 算法 + 4 条件矩阵 + 复用现有契约）
- [ ] 3. 写完整 tasks.md（~200 行实现 + 12 case 测试）
- [ ] 4. 写完整 spec.md（R1-R4 见 Capabilities 章节）
- [ ] 5. 移除 PLACEHOLDER 标记
- [ ] 6. openspec validate
- [ ] 7. 更新 master plan §四 C3 状态
- [ ] 8. 启动 Sprint 35 实施

## 依赖
- **上游**: C2 (genome-registry) ship — Genome 版本号是判断"过期数据"的基础
- **下游**: C4 (harness-rsi-pilot) 需要 transition guard 就绪

## 关联文档
- MetaRSI-v1 论文 H→D→M 关键规则 (unverified)
- `docs/architecture/self-evolution-architecture-2026-08.md` §三
- `docs/adr/adr-0083-evaluator-reward-contract.md`
- `docs/adr/adr-0084-mutation-governance-contract.md`
- `docs/adr/adr-0086-credit-assignment-contract.md` (🔍 Proposed)
- Oracle 评审: `task_id=ses_f55f307f6ffeRJ9SIny8iUbZ8Y` §4 Change 3
- Master plan: `docs/roadmap/2026-09-16-pdk-chat-demo-evolution-roadmap.md`
