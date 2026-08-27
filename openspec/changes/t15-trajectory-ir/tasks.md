# Tasks: t15-trajectory-ir

> **TDD 5 步结构**: 每任务按 Write failing test → Verify fail → Implement → Verify pass → Commit
> **关键不变量**: ParsedGraph **零修改**。TrajectoryIR 是独立类，通过 `from_parsed_graph()` 单向 Converter 桥接。
> **设计依据**: ADR-0061-06 v1.1 amendment ✅ Approved (Oracle session `ses_fcba5e477ffeG9wEBHVhU64J0o`)

## Phase 0: 契约类型声明（估时 0.2 sprint）

- [ ] **T0.1** Write failing test: `tests/test_trajectory_ir.cpp` 骨架（≥ 8 cases 占位，引入 `agenticdsl::ir::TrajectoryIR` 命名空间 + 三级 IR 结构）
- [ ] **T0.2** Verify fail: 编译失败（`fatal error: 'agenticdsl/ir/trajectory_ir.h' file not found`）
- [ ] **T0.3** Implement minimal: `include/agenticdsl/ir/trajectory_ir.h`
  - `namespace agenticdsl::ir { class TrajectoryIR { ... }; }`
  - 三级 IR: `RawIR` / `ParsedIR` / `CanonicalIR`（struct）
  - 关联结构: `NodeRecord` / `EdgeRecord` / `StepRecord`（struct）
  - 单向 Converter: `static ParsedIR from_parsed_graph(const ParsedGraph& pg)`（声明）
  - V1 Backends: `static nlohmann::json to_sft_data(const CanonicalIR&)` + `static nlohmann::json to_otel_spans(const CanonicalIR&)`（声明）
  - V1 Pass: `class ConstantFoldingPass { CanonicalIR run(const CanonicalIR&) const; }`（声明）
- [ ] **T0.4** Verify pass: 编译成功，8 cases 编译通过（运行时仍 FAIL，断言占位）
- [ ] **T0.5** Commit: `feat(ir): TrajectoryIR contract + 3-level IR structure (T0)`

## Phase 1: Converter 单向实现（估时 0.2 sprint）

- [ ] **T1.1** Write failing test: `converter_from_parsed_graph_basic`（空 ParsedGraph → 空 ParsedIR；1 node + 1 edge → 1 NodeRecord + 1 EdgeRecord）
- [ ] **T1.2** Write failing test: `converter_unidirectional_invariant`（修改 ParsedGraph 不影响已生成的 ParsedIR 快照）
- [ ] **T1.3** Verify fail: 2 cases FAIL（converter 未实现）
- [ ] **T1.4** Implement: `src/core/parsed_graph_to_trajectory_ir.cpp` — `TrajectoryIR::from_parsed_graph()` 单向转换
  - Nodes → NodeRecord（浅拷贝，值类型，type=字符串无 enum 依赖）
  - Edges → EdgeRecord（from/to + weight=1.0 V1 简化）
  - Steps: V1 从 ExecutionSession TraceRecord 推（占位，V2 集成 ADR-0061-13 DistillationRecord.reward）
- [ ] **T1.5** Verify pass: 2 cases PASS
- [ ] **T1.6** Commit: `feat(ir): TrajectoryIR::from_parsed_graph Converter V1 (T1)`

## Phase 2: V1 Backends + Pass 占位（估时 0.3 sprint）

- [ ] **T2.1** Write failing test: `to_sft_data_basic`（CanonicalIR → JSON 包含 nodes/edges/steps/sft_metadata 字段）
- [ ] **T2.2** Write failing test: `to_otel_spans_basic`（CanonicalIR → JSON OTLP 格式 spans 数组）
- [ ] **T2.3** Write failing test: `constant_folding_pass_passthrough`（V1 占位：Pass 输入输出等价）
- [ ] **T2.4** Verify fail: 3 cases FAIL（backends + pass 未实现）
- [ ] **T2.5** Implement: `src/modules/ir/trajectory_ir_backend.cpp`
  - `to_sft_data()`: 序列化为 SFT 训练数据 JSON schema（含 node/edge/step/reward 字段）
  - `to_otel_spans()`: 序列化为 OTLP spans JSON（含 trace_id/span_id/parent_span_id/timestamps）
- [ ] **T2.6** Implement: `src/modules/ir/trajectory_ir_pass.cpp`
  - `ConstantFoldingPass::run()` V1 占位：直接返回输入（V2 扩展：常量折叠 + 死代码消除）
- [ ] **T2.7** Implement: `src/modules/ir/CMakeLists.txt` — 注册新源（**不修改既有任何 CMakeLists**）
- [ ] **T2.8** Verify pass: 3 cases PASS + ctest 全量 0 回归（动态基线）
- [ ] **T2.9** Commit: `feat(ir): V1 backends (to_sft_data + to_otel_spans) + ConstantFoldingPass placeholder (T2)`

## Phase 3: SkillCompiler 占位符升级（估时 0.1 sprint）

- [ ] **T3.1** Write failing test: `compiled_skill_trajectory_ir_hash`（CompiledSkill.trajectory_ir_hash 由 TrajectoryIR 真实 hash() 生成，而非占位符硬编码）
- [ ] **T3.2** Verify fail: 当前 hash 仍为 TrajectoryPlaceholder::hash() 占位实现
- [ ] **T3.3** Implement: `include/agenticdsl/types/compiled_skill.h`
  - 删除 TrajectoryPlaceholder::hash() 占位（commit `21dd622` 引入）
  - trajectory_ir_hash 字段类型改为 `std::string`，生成逻辑调用 `TrajectoryIR::hash(canonical)`
  - **V1 简化**: 接受 const CanonicalIR& 输入（如已编译 skill 提供）
- [ ] **T3.4** Verify pass: SkillCompiler 测试 + TrajectoryIR 测试均 PASS
- [ ] **T3.5** Commit: `feat(skill-compiler): TrajectoryPlaceholder upgraded to TrajectoryIR (T15 integration)`

## Phase 4: 文档同步 + ship 验证（估时 0.2 sprint）

- [ ] **T4.1** 修改 `docs/adr/skill/adr-0061-06-trajectory-ir.md` 头部 `##状态` 章节：✅ Approved → ✅ Approved + Shipped（追加 ship 证据段：commit hash + 8 cases / 16 assertions + ctest 188+）
- [ ] **T4.2** 修改 `docs/adr/skill/adr-0061-06-v1-1-amendment-trajectory-ir-decouple.md` 头部：✅ Approved → ✅ Approved + Shipped（同步）
- [ ] **T4.3** 修改 `docs/architecture/capability-application-map-2026-08.md`:
  - 头部版本 v1.8 → v1.9 + 最后验证 2026-08-27
  - §一 +1（新能力 #25 Trajectory IR）
  - §八 T15 → Completed（保持 ✅ APPROVED 但新增 "✅ Shipped 2026-08-27"）
  - §七 changelog 新增 v1.9 条目
- [ ] **T4.4** 修改 `docs/architecture/adr-implementation-status-gap-analysis.md` §一 总计行 + Approved 列表追加 ADR-0061-06 ship 证据
- [ ] **T4.5** 修改 `docs/active-status.md` §一 T15 跟踪段：移除"待 ship"标注
- [ ] **T4.6** 修改 `docs/README.md` §adr/ 表 ADR-0061-06 行 + ADR-0061-06 v1.1 amendment 行
- [ ] **T4.7** 验证: `python3 tools/adr_lint.py` + `python3 tools/docs_drift_audit.py` 全部通过
- [ ] **T4.8** 验证: `openspec validate --changes --strict` PASS
- [ ] **T4.9** 验证: `ctest --output-on-failure` 全量 0 回归（动态基线，禁止硬编码）
- [ ] **T4.10** Commit: `feat(ir): ship T15 Trajectory IR V1 - independent serialization view (closes G14)`
- [ ] **T4.11** `openspec archive t15-trajectory-ir`

## 总估时

- Phase 0: 0.2 sprint
- Phase 1: 0.2 sprint
- Phase 2: 0.3 sprint
- Phase 3: 0.1 sprint (SkillCompiler 集成)
- Phase 4: 0.2 sprint
- **总估时: ~1.0 sprint**（符合 ADR-0061-06 文档约定）

## 明确 out of scope (V2 延后)

- `to_rl_data()` / `to_eval_data()` backends（V2 扩展）
- 跨框架 frontend（LangGraph / CrewAI / AutoGen / OpenAI SDK，V2 扩展）
- 完整 Pass Pipeline（V2 扩展：ConstantFolding + DeadCodeElim + LoopUnroll）
- 真实 StepRecord 从 ExecutionSession 提取（V2 集成 ADR-0061-13 DistillationRecord）
- TrajectoryIR JSONL 持久化（V2 集成 SessionManager）
- 训练工具消费 TrajectoryIR（V2：downstream 应用层）

## 关键不变量（强制遵守）

- ❌ ParsedGraph **任何修改**（关键隔离边界）
- ❌ TrajectoryIR 与 ParsedGraph 任何继承/耦合关系（独立类）
- ❌ 修改既有 IEvaluator / MutationGovernor / BehaviorRegression / SkillCompiler 契约
- ❌ 在测试失败时强行 commit
- ❌ 硬编码 ctest 数字

## SkillCompiler 占位符升级详细说明

commit `21dd622` 引入的 `TrajectoryPlaceholder::hash()` 占位实现位于 `include/agenticdsl/types/compiled_skill.h`。Phase 3 将其替换为 TrajectoryIR 真实 hash 集成。

升级步骤：
1. 删除 `TrajectoryPlaceholder` struct（占位专用）
3. 修改 `CompiledSkill::trajectory_ir_hash` 字段类型为 `std::string`
4. `CompiledSkill` 构造或 `compile()` 后注入 `TrajectoryIR::hash(canonical_ir)` 计算结果
5. SkillCompiler 测试更新（如涉及该字段）