# Design: t15-trajectory-ir

## Context

ADR-0061-06 v1.1 amendment (✅ Approved 评审通过 2026-08-25, Oracle session `ses_fcba5e477ffeG9wEBHVhU64J0o`) 定义 Trajectory IR 为**独立序列化视图**——与 ParsedGraph 完全分离，通过单向 Converter 桥接。V1 简化：仅 `from_parsed_graph()` Converter + `to_sft_data()` + `to_otel_spans()` 两个 backends + `ConstantFoldingPass` 占位。

## Scope Boundaries

### 范围 IN
- 新增 `agenticdsl::ir::TrajectoryIR` 独立类（namespace 与既有 `agenticdsl::types` / `agenticdsl::contract` 并列）
- 三级 IR 结构 (RawIR / ParsedIR / CanonicalIR) + NodeRecord / EdgeRecord / StepRecord
- 单向 Converter `from_parsed_graph(const ParsedGraph&)` → ParsedIR
- V1 Backends: `to_sft_data(CanonicalIR)` + `to_otel_spans(CanonicalIR)`
- V1 Pass 占位: `ConstantFoldingPass`
- SkillCompiler 占位符升级 (`TrajectoryPlaceholder::hash()` → `TrajectoryIR::hash()`)
- 文档同步 (ADR-0061-06 + v1.1 amendment ship 注记 + cap-map §一 +1)

### 范围 OUT
- ParsedGraph **任何修改**（关键隔离边界，强制不变量）
- TrajectoryIR 与 ParsedGraph 继承/耦合关系
- V2 backends (`to_rl_data` / `to_eval_data`)
- 跨框架 frontend（LangGraph / CrewAI / AutoGen / OpenAI SDK）
- 完整 Pass Pipeline（V2: DeadCodeElim / LoopUnroll）
- 真实 StepRecord 从 ExecutionSession 提取（V2 集成 ADR-0061-13 DistillationRecord）
- 训练工具消费 TrajectoryIR（V2 downstream）

## Design Decisions

### D1 — 命名空间独立

`agenticdsl::ir` 独立命名空间：
- 不污染 `agenticdsl::types`（运行时值类型）
- 不污染 `agenticdsl::contract`（抽象接口）
- 明确边界：`ir::*` 是序列化/转换层，与运行时解耦

理由：训练数据格式不应与运行时共享命名空间，独立演化。

### D3 — 单向 Converter 严格性

`TrajectoryIR::from_parsed_graph(const ParsedGraph& pg)` 单向：
- 输入：const 引用 ParsedGraph
- 输出：TrajectoryIR::ParsedIR 值类型（浅拷贝）
- **禁止**反向 Converter（TrajectoryIR → ParsedGraph）
- **禁止** Parser 持 ParsedGraph 引用

理由：训练方向单向数据流，避免反向耦合污染 L0 运行时。

### D4 — V1 简化策略

V1 仅实现核心契约：
- 3 级 IR 结构（完整 schema）
- 1 个 Converter
- 2 个 V1 backends（SFT + to_otel_spans）
- 1 个 Pass 占位（pass-through）

V2 延后：
- to_rl_data / to_eval_data
- 完整 Pass Pipeline
- 跨框架 frontend
- 真实 StepRecord 提取

理由：V1 ship 后已可支撑 B6/B7 应用层核心需求，避免过度设计。

### D5 — SkillCompiler 集成策略

T17 SkillCompiler（commit `21dd622`）引入 `TrajectoryPlaceholder::hash()` 占位实现。T15 ship 后无缝升级：

```cpp
// 旧（commit 21dd622）:
struct TrajectoryPlaceholder {
    static std::string hash() { return "trajectory_placeholder_v1"; }
};

// 新（T15 ship 后）:
// TrajectoryPlaceholder struct 已删除
// CompiledSkill::trajectory_ir_hash 由 TrajectoryIR::hash(canonical_ir) 生成
```

升级不破坏 SkillCompiler 公开 API（仅替换内部实现）。

### D6 — V1 边界遵守

- ❌ L4 权重支持（ADR-0068 V1 禁止）
- ❌ 真实变异触发（V1 仅 emit 事件，G11 ✅ 已 ship）
- ❌ 修改既有契约（IEvaluator / MutationGovernor / BehaviorRegression 公开 API）

## Risks

| 风险 | 缓解 |
|---|---|
| ParsedGraph 不慎修改 | T0.2 验证 fail (header 缺失) + Phase 4 `git diff HEAD~1 -- src/core/parsed_graph.h` 验收 |
| V2 范围蔓延 | tasks.md 明确 out of scope 清单 + V1 backends 限制 |
| SkillCompiler 集成破坏现有测试 | Phase 3 单独 phase + SkillCompiler 现有 15 cases 验证 |
| ctest 数字硬编码 | tasks.md 明确禁止 + Phase 4 动态基线 |
| docs_drift_audit 引入新 drift | Phase 4 验证 + Scenario 6 pre-existing |

## Verification Gates

- ≥ 8 cases test_trajectory_ir PASS
- ctest 全量 0 回归（动态计数）
- adr_lint 82 ADR PASS
- docs_drift_audit 0 NEW drift
- openspec validate --strict PASS
- ParsedGraph 0 修改
- 既有契约 API 0 修改

## Dependencies

### 满足
- ✅ ADR-0061-06 v1.1 ✅ Approved (独立序列化视图设计)
- ✅ ParsedGraph 既有定义 (`src/core/parsed_graph.h`)
- ✅ nlohmann::json 既有 (external vendor)
- ✅ SkillCompiler ✅ Shipped (commit `21dd622`, 占位符可升级)
- ✅ IEvaluator ✅ Shipped
- ✅ T14 行为回归 ✅ Shipped

### 不依赖
- T19 / T20 / T22 (这些应用层依赖 T15)
- 跨框架 frontend
- V2 backends

## Out of Scope (V2 deferred)

- to_rl_data / to_eval_data
- 跨框架 frontend
- 完整 Pass Pipeline
- StepRecord 真实提取
- TrajectoryIR JSONL 持久化
- 训练工具消费 TrajectoryIR

## Success Criteria

- TrajectoryIR ✅ Shipped
- ParsedGraph 0 修改
- SkillCompiler TrajectoryPlaceholder 升级完成
- ADR-0061-06 + v1.1 amendment ✅ Approved + Shipped
- cap-map §一 24→25 能力 + §八 T15 ✅ Completed
- 8+ cases PASS + ctest 188+ 全量零回归
- OpenSpec archive 完成