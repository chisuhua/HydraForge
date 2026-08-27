# t15-trajectory-ir

## Why

ADR-0061-06 v1.1 amendment (✅ Approved 评审通过 2026-08-25, Oracle session `ses_fcba5e477ffeG9wEBHVhU64J0o`) 定义 Trajectory IR 为**独立序列化视图**——与 ParsedGraph 完全分离，通过单向 Converter 桥接。V1 简化：仅 `from_parsed_graph()` Converter + `to_sft_data()` + `to_otel_spans()` 两个 backends。

**Oracle 评审关键发现**（cap-map §八 G14）：
- ADR-0061-06 v1 标题"升级 ParsedGraph"耦合风险：训练数据格式耦合进运行时图结构
- v1.1 修正：**独立类** + 单向 Converter，不改 ParsedGraph
- TrajectoryIR schema 演化独立于 ParsedGraph
- 蒸馏工具消费 TrajectoryIR（安全），不影响 L0 运行时

**审计依据**：
- ADR-0061-06 v1.1 amendment ✅ Approved (P1 优先级)
- ADR-0061-06 v1 ADR ✅ Approved
- cap-map §八 T15 ✅ APPROVED
- 当前 `include/agenticdsl/ir/trajectory_ir.h` 0 命中（grep 验证）

**前置依赖**（全部已满足）：
- ✅ ADR-0061-06 v1.1 ✅ Approved（关键：独立序列化视图设计）
- ✅ ParsedGraph 已有（src/core/parsed_graph.h, 既有 L0 运行时定义）
- ✅ nlohmann::json 已有（external vendor）
- ✅ SkillCompiler ✅ Shipped（commit `21dd622`，TrajectoryPlaceholder 可无缝替换）
- ✅ IEvaluator ✅ Shipped（用于 quality gate）
- ✅ T14 行为回归 ✅ Shipped（用于 eval backend）

## What Changes

**新增契约 + 值类型**：
- `include/agenticdsl/ir/trajectory_ir.h` — `class TrajectoryIR` 独立类（namespace `agenticdsl::ir`）
  - 三级 IR: `RawIR` / `ParsedIR` / `CanonicalIR`
  - 关联结构: `NodeRecord` / `EdgeRecord` / `StepRecord`
  - 单向 Converter: `static ParsedIR from_parsed_graph(const ParsedGraph& pg)`
  - V1 Backends: `to_sft_data()` + `to_otel_spans()` (V2 扩展 others)
  - V1 Pass: `ConstantFoldingPass` 占位（V2 扩展）

**新增实现**：
- `src/core/parsed_graph_to_trajectory_ir.cpp` — Converter 单向实现
- `src/modules/ir/trajectory_ir_pass.cpp` — Pass Pipeline 占位
- `src/modules/ir/trajectory_ir_backend.cpp` — V1 两个 backends
- `src/modules/ir/CMakeLists.txt` — 注册新源

**新增测试**：
- `tests/test_trajectory_ir.cpp` — ≥ 8 cases
  - Converter 单向性
  - RawIR/ParsedIR/CanonicalIR 互转
  - to_sft_data 序列化
  - to_otel_spans 序列化
  - ConstantFoldingPass 占位
  - ParsedGraph 修改不影响 TrajectoryIR（独立性）
  - 边界情况：空 graph / 大量 nodes / 循环引用

**SkillCompiler 集成**：
- 替换 `include/agenticdsl/types/compiled_skill.h::TrajectoryPlaceholder::hash()` 为 `TrajectoryIR::hash()`（commit `21dd622` 已 ship 的占位符）

## Impact

**影响范围**：
- L0 运行时（ParsedGraph）**零改动**（关键不变量）
- 新增 namespace `agenticdsl::ir`（与 `agenticdsl::types` / `agenticdsl::contract` 并列）
- SkillCompiler 占位符升级（T17 ship 时的 `TrajectoryPlaceholder::hash()`）

**下游解锁**：
- B6 Agent 蒸馏环境（教师轨迹采集 → 学生行为克隆）
- B7 自进化基础（GEPA/AFlow 中间表示层）
- T19 GEPA Phase 2 commit（TrajectoryIR 作为失败→反思→修订 prompt 的输入）
- T20 AFlow MCTS 工作流改写（TrajectoryIR 作为搜索空间）
- 跨框架 trace 兼容（V2 扩展：LangGraph/CrewAI/AutoGen/OpenAI SDK frontends）
- 训练数据后端（V2 扩展：to_rl_data / to_eval_data）

**V1 边界**（per ADR-0061-06 v1.1）：
- ✅ 仅 3 级 IR 结构 + 单向 Converter + 2 个 V1 backends + 1 个占位 Pass
- ⏸ V2 延后：to_rl_data / to_eval_data / 跨框架 frontend / 完整 pass pipeline

**Breaking Changes**：无（仅新增 namespace `agenticdsl::ir`，既有代码 0 修改）

## ship gate 验证

- `python3 tools/adr_lint.py` 通过（≥82 ADR）
- `python3 tools/docs_drift_audit.py` 通过（无新增 CRITICAL drift）
- `openspec validate --changes --strict` PASS
- `ctest --output-on-failure` 全量 0 回归（动态基线，禁止硬编码 188）
- `ctest -R test_trajectory_ir` ≥ 8 cases / ≥ 16 assertions PASS
- `grep "class TrajectoryIR" include/agenticdsl/ir/` 命中
- ADR-0061-06 v1 状态追加 ship 注记：✅ Approved → ✅ Approved + Shipped
- cap-map §一 +1（新能力 #24+）/ §八 T15 → Completed
- SkillCompiler `TrajectoryPlaceholder::hash()` 已升级为 `TrajectoryIR::hash()` 调用
- ADR-0061-06 v1.1 amendment 状态 ✅ Approved + Shipped

## 关联文档

- `docs/adr/skill/adr-0061-06-trajectory-ir.md` v1 (待追加 ship 注记)
- `docs/adr/skill/adr-0061-06-v1-1-amendment-trajectory-ir-decouple.md` (待追加 ship 注记)
- `src/core/parsed_graph.h`（既有 L0 运行时定义，0 修改）
- `include/agenticdsl/types/compiled_skill.h`（SkillCompiler 占位符待升级）
- `tests/test_skill_compiler.cpp`（TrajectoryPlaceholder 相关测试待升级）
- ADR-0068 附录 A（无需 amendment，TrajectoryIR 是**数据格式**而非事件主题）
- Oracle session `ses_fcba5e477ffeG9wEBHVhU64J0o`（v1.1 amendment 决议）
- `docs/architecture/capability-application-map-2026-08.md` §八 T15 + G14
- `docs/active-status.md` §一 T15 跟踪段