# skill-compiler Specification

## ADDED Requirements

### Requirement: SkillCompiler 是纯函数式 SKILL.md 编译器

The SkillCompiler MUST accept a SKILL.md (Anthropic Skills format) input with performance metrics (Trajectory IR + IEvaluator output) and return an optimized skill.md output, without side effects on the original skill.

#### Scenario: 基础 SKILL.md 编译

- **GIVEN** 一个 SKILL.md 输入（Anthropic Skills 格式）
- **AND** 性能指标：IEvaluator 输出 RewardSignal.quality = "Acceptable" 或更高
- **WHEN** 调用 `SkillCompiler::compile(skill, metrics)`
- **THEN** 返回优化后的 skill.md（含改进 prompt + 验证 evidence）
- **AND** 原 SKILL.md 文件未被修改（纯函数式）

#### Scenario: 编译产物通过行为回归

- **GIVEN** 编译前 SKILL.md (baseline) + 编译后 SKILL.md (candidate)
- **AND** 2 个 BehaviorFingerprint（来自 Trajectory IR 执行结果）
- **WHEN** 调用 `hotelling_t2_test(baseline_fp, candidate_fp, budget)` (T14)
- **THEN** 返回 Verdict (Pass/Fail/Inconclusive)
- **AND** 若返回 Fail → 编译产物被拒绝（rollback）

#### Scenario: 编译失败回滚

- **GIVEN** IEvaluator 评估编译产物为 quality = "Poor"
- **WHEN** 调用 `SkillCompiler::rollback()`
- **THEN** 恢复原 SKILL.md 状态
- **AND** emit `skill.compilation.failed` 事件（payload 含 reason="quality_poor"）

### Requirement: SkillCompiler 必须通过变异治理审计

The SkillCompiler MUST integrate with the mutation governance contract (G11) to emit audit events and prevent unauthorized prompt injection.

#### Scenario: 编译过程 emit 审计事件

- **WHEN** SkillCompiler 处理任意 skill 编译请求
- **THEN** 必须按顺序 emit：
  - `skill.compilation.started` (payload: `{skill_id, original_version, compiler_version}`)
  - `skill.compilation.succeeded` 或 `skill.compilation.failed`（**只 emit 一个**）

#### Scenario: 审计事件包含 causal_time

- **GIVEN** SkillCompiler 处理第 N 次编译
- **WHEN** emit `skill.compilation.*` 事件
- **THEN** `causal_time` 必须严格递增（与 ADR-0068 事件契约对齐）

### Requirement: SkillCompiler 输入数据格式对齐 Trajectory IR

The SkillCompiler MUST accept Trajectory IR (ADR-0061-06 v1.1) as input data format for compilation decisions.

#### Scenario: 编译输入包含 Trajectory IR

- **GIVEN** TrajectoryIR::CanonicalIR（含 ≥ 1 个执行轨迹）
- **WHEN** 调用 `SkillCompiler::compile(skill_md, trajectory_ir)`
- **THEN** SkillCompiler 使用 trajectory 数据优化 prompt（如增加 few-shot examples）

#### Scenario: 编译输入不依赖 ParsedGraph（解耦）

- **GIVEN** SkillCompiler 编译流程
- **WHEN** 输入数据解析
- **THEN** 不读取/修改 `ParsedGraph`（与 ADR-0061-06 v1.1 amendment G14 对齐）

### Requirement: SkillCompiler 是 EventLog 一等公民

The SkillCompiler MUST emit events through IInteractionBus and EventLog (ADR-0080 D10.v1.2 Training mode optional capture).

#### Scenario: Training 模式下 prompt 字节落盘

- **GIVEN** `EventLogConfig.capture_mode == Training` (ADR-0080 v1.2)
- **WHEN** SkillCompiler emit `skill.compilation.succeeded` 含 compiled prompt
- **THEN** `payload.prompt_text` 字段被 EventLog 落盘（v1.2 Training mode 不 scrub）

#### Scenario: Online 模式下 prompt 字节不落盘

- **GIVEN** `EventLogConfig.capture_mode == Online`
- **WHEN** SkillCompiler emit 事件
- **THEN** `payload.prompt_text` 字段被 scrub hook (ADR-0081) 处理后**仅存 hash**
- **AND** 不暴露原始 prompt 字节

### Requirement: SkillCompiler 与 SLM 路由协同

The SkillCompiler MUST leverage T16 SLM routing policy (ADR-0061-04) for compilation cost optimization.

#### Scenario: 编译过程使用 SLM 路由

- **WHEN** SkillCompiler 调用 LLM 生成改进 prompt
- **THEN** 路由策略优先选择 fast/slm 模型（与 T16 一致）
- **AND** 编译成本被纳入 budget 控制

### Requirement: SkillCompiler 输出可审计

The SkillCompiler MUST produce an audit trail of compilation decisions for compliance and debugging.

#### Scenario: 编译 metadata 持久化

- **WHEN** SkillCompiler 完成编译
- **THEN** 输出 skill.md 必须包含 front-matter metadata:
  ```yaml
  ---
  compiled_at: <ISO timestamp>
  compiler_version: "skill-compiler-v1.0.0"
  baseline_skill_id: <UUID>
  trajectory_ir_hash: <SHA256>
  ievaluator_score: <float 0.0-1.0>
  regression_verdict: "Pass" | "Fail" | "Inconclusive"
  ---
  ```

#### Scenario: 编译失败原因分类

- **WHEN** SkillCompiler 回滚
- **THEN** emit `skill.compilation.failed` payload 含 `reason` 字段：
  - `"quality_poor"`: IEvaluator 评分低于基线
  - `"regression_fail"`: T14 行为回归返回 Fail
  - `"budget_exceeded"`: 编译成本超过 budget
  - `"infrastructure_error"`: 内部异常（LLM 调用失败等）

### Requirement: SkillCompiler 集成点

The SkillCompiler MUST integrate with existing HydraForge architecture components.

#### Scenario: 与 CognitiveWorker 集成

- **WHEN** CognitiveWorker 任务完成时调用 SkillCompiler
- **THEN** 编译输入来自 CognitiveWorker 的 ExecutionTrace（与 T14 BehaviorFingerprint 同源）
- **AND** 编译输出替换 worker 的 active skill

#### Scenario: 与 DomainWorkerPool 集成

- **WHEN** DomainWorkerPool 中任一 worker 触发 skill 更新
- **THEN** SkillCompiler 编译该 worker 的 skill
- **AND** 编译过程是线程安全的（编译调用方持有 worker 锁）

#### Scenario: 与 PluginLoader 集成

- **GIVEN** 一个 .so plugin 包含 SKILL.md
- **WHEN** PluginLoader::load_plugin() 调用
- **THEN** 可选触发 SkillCompiler 优化（CLI 标志 `--optimize-skills`）