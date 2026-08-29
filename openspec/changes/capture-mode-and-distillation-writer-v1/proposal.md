# capture-mode-and-distillation-writer-v1

## Why

**不变量链**：ADR-0080 v1.2 (CaptureMode 三态) ✅ Approved 但 Training 模式零代码 → ADR-0061-13 (IDistillationWriter) ✅ Approved 但 IDistillationWriter 不存在 → **T19 GEPA / T20 AFlow** 已 ship (2026-08-28, 14 cases + 17 cases) 却无蒸馏数据可消费 → 闭环 7 环断在第 1 环（采集）。

**Oracle 二次审查发现** (2026-08-29, session `ses_fb4e00320ffeqQVZ2S61tF3dZi`):
- **P0**: ADR-0080 v1.2 + ADR-0061-13 互为依赖且双双 "Approved 但零代码/幻影 change"，蒸馏数据面闭环的实际阻塞点
- 两 ADR 均 2026-08-25 Approved 且超 24h 无 tracking change（single-dev 治理缺口实例，见 ADR-TRACKING-01 规则）
- 合并 ship 共享 EventLog 改动面 + 测试夹具 + ship gate，比串行 2 change 省 ~0.5 sprint

**审计依据**:
- ADR-0080 v1.2 amendment (D10 Capture 与 Scrub Hook 解耦) ✅ Approved (评审通过 2026-08-25)
- ADR-0061-13 distillation output format ✅ Approved (评审通过 2026-08-25)
- ADR-0083 IEvaluator ✅ Approved + ship (V1 + V2)
- ADR-0084 Mutation Governance ✅ Approved + ship (V1 gate-and-audit)
- ADR-0068 Event Emission Contract ✅ Approved (v1.6, 27+ 主题注册)
- ADR-0074 Prompt Evidence Gate ✅ Approved + ship (T21)
- ADR-0008 LayeredContext (5-层结构化)
- T21 payload redact 范式（hash-only PII defense）
- `src/core/types/event_log_config.h`（实际路径，ADR-0080 v1.2 §实施路径漂移修正）

**前置依赖**（全部已满足）:
- ✅ ADR-0083 `RewardSignal` 已 ship (`include/agenticdsl/contract/reward_signal.h`)
- ✅ T15 TrajectoryIR 已 ship (`include/agenticdsl/ir/trajectory_ir.h`)
- ✅ EventLogConfig v1.1 (`src/core/types/event_log_config.h` line 21 `bool capture_prompt_bytes`)
- ✅ T21 payload redact 已 ship（hash-only PII 范式参考）
- ✅ nlohmann::json 已 vendor
- ✅ `--allow-training-capture` mock-mode guard 模式（pdk_chat_demo model_switching 已有先例）

## What Changes

### Phase 0 抽象层（2 个新头文件 + 默认实现骨架）

1. **`include/agenticdsl/types/capture_mode.h`** (新)
   - `enum class CaptureMode : uint8_t { Off = 0, Online = 1, Training = 2 }`
   - `to_string(CaptureMode)` / `parse_capture_mode(string)` helpers
   - 常量: `kDefaultCaptureMode = CaptureMode::Off`

2. **`include/agenticdsl/types/distillation_record.h`** (新)
   - `struct DistillationRecord` (trajectory/policy/meta 三文件分离, ADR-0061-13 §决策 1)
   - `struct StepRecord` (step 元数据 + reward 复用 ADR-0083 RewardSignal)
   - `struct ConvergenceMeta` (agent_id, teacher_version, capture_mode 字段)

3. **`include/agenticdsl/contract/idistillation_writer.h`** (新)
   - `class IDistillationWriter` (纯虚接口)
   - `virtual void write(DistillationRecord)` + `virtual void finalize()` + `virtual ~IDistillationWriter() = default`

4. **`tests/test_capture_mode.cpp`** (新, ≥3 cases)
   - `enum_serialization` (to_string/parse round-trip)
   - `parse_invalid_string_throws` (M1: schema 非法 throw)
   - `default_value_is_off`

### Phase 1 迁移 + 默认实现（BREAKING-internal）

5. **`src/core/types/event_log_config.h`** (修改)
   - **BREAKING**: 删除 `bool capture_prompt_bytes = false` 字段
   - 新增 `CaptureMode capture_mode = CaptureMode::Off`
   - `effective_capture_enabled()` helper（Online + Training 均返回 true）
   - 不保留 bool 兼容层（消费者迁移面仅 5 文件）

6. **5 消费者迁移**（grep 验证 0 命中 `capture_prompt_bytes`）:
   - `src/core/engine.h`（PIMPL 字段引用更新）
   - `src/core/engine.cpp`（构造默认值更新）
   - `src/common/llm/tracing_decorator.h`（装饰基类字段引用）
   - `src/common/llm/tracing_decorator.cpp`（捕获开关判断更新）
   - `src/core/types/event_log_config.h`（如上）

7. **`src/core/event_log.cpp`** (修改)
   - 启动校验: Training 三重保护（agent_id 必填 + 路径含 `train|distill` + WARNING stderr）
   - Online→Training 静默降级检测 + `event_log.capture_mode_downgrade` 审计事件发射（ADR-0068 §决策 2）
   - 任何 mode mismatch → 强制 emit + 不阻断启动（V1: log + 继续，V2 可配 fail-closed）

8. **`src/modules/distillation/file_writer.cpp`** (新, V1 默认实现)
   - `class FileDistillationWriter : public IDistillationWriter`
   - 同步 write + `fsync` + finalize meta.json
   - 文件命名: `<agent_id>_<seq>.distill.v1.jsonl`
   - ≤1.5MB/record 硬上限（不变量 1, 超出 throw `std::length_error`）

### Phase 2 集成（CLI flag + 桥接）

9. **`examples/pdk_chat_demo/cli_args_parser`** (修改)
   - 新增 `--allow-training-capture` flag
   - **mock-mode guard**: `if (provider_mode == "mock" && flag_enabled) throw runtime_error + stderr warning`（参考 model_switching 模式）
   - **路径漂移修正**: ADR-0080 v1.2 §实施写 `engine.cpp`，实际应在 `cli_args_parser`

10. **`src/modules/distillation/trajectory_bridge.cpp`** (新)
    - `DistillationRecord from_trajectory_ir(const TrajectoryIR::CanonicalIR&)` 桥接
    - 复用 ADR-0061-06 v1.1 `to_sft_data()` 输出作为 trajectory.jsonl 内容
    - payload redact 复用 T21 `hash_prompt()` helper（不新造 hash）

### Phase 3 测试 + 文档 + archive（≥10 cases 实际 ≥12）

11. **`tests/test_event_log_capture_mode.cpp`** (新, ≥5 cases)
    - `capture_mode_off_no_capture`
    - `capture_mode_online_capture_no_pii_warning`（pdk_chat_demo 默认）
    - `capture_mode_training_three_protections_required`（三重保护逐项验证）
    - `capture_mode_downgrade_emits_audit_event`（V1 silent + log）
    - `capture_mode_invalid_string_throws`

12. **`tests/test_distillation_writer.cpp`** (新, ≥5 cases)
    - `file_writer_write_read_roundtrip`
    - `file_writer_finalize_meta_hash`
    - `file_writer_record_too_large_throws`（不变量 1, ≤1.5MB）
    - `file_writer_filename_uniqueness`（`<agent_id>_<seq>.distill.v1.jsonl`）
    - `idistillation_writer_contract_pure_virtual`（契约层接口完整性）

13. **文档同步**:
    - `docs/adr/adr-0080-v1-2-amendment-d10-decouple.md` 头部追加 `⏳ tracking: in-progress` + 链接本 change
    - `docs/adr/skill/adr-0061-13-distillation-output-format.md` 头部追加同样链接
    - `docs/architecture/capability-application-map-2026-08.md` §七 changelog v2.5 + §一 +1（新能力 #31 Distillation Data Plane V1）
    - `docs/adr/adr-0068-event-emission-contract.md` 附录 A 追加 `event_log.capture_mode_downgrade` 主题（v1.7）
    - `tools/adr_lint.py` ADR-TRACKING-01 WARNING 应自动消失（ADR-0080 v1.2 / ADR-0061-13 现在有 tracking change 目录）

14. **`openspec archive capture-mode-and-distillation-writer-v1`**

## Impact

**影响范围**:
- **新代码**: 4 新 .h + 2 新 .cpp + 2 新测试文件（≥10 cases）
- **修改代码**: `event_log_config.h` (BREAKING-internal), `event_log.cpp`, 5 消费者, `cli_args_parser`
- **零契约变更**: IInteractionBus / EventBuilder / IApprovalHandler / IAgentRegistry 等公开 API **零修改**
- **零既有测试变更**: 既有 190+ tests 全保留

**下游影响**:
- 解锁 **T19 GEPA Phase 3 commit 路径**（DistillationRecord 输出到 fine-tune pipeline）
- 解锁 **T20 AFlow MCTS Phase 2 evaluation path**（mutation.committed 事件 → DistillationWriter → SFT data）
- 解锁 **ADR-0078 Fine-tune 基模** 输入通道（Wave 5+）
- 解锁 **横切功能 Marketplace** 真实 capture_mode 字段

**V1 边界**（per Oracle 决策 2 + 决策 5）:
- ✅ CaptureMode 三态 + FileDistillationWriter + IDistillationWriter
- ✅ EventLogConfig 字段迁移（BREAKING-internal 5 消费者）
- ✅ `--allow-training-capture` CLI flag + mock-mode guard
- ✅ TrajectoryIR → DistillationRecord 桥接
- ✅ payload redact 复用（不新造 hash）
- ⏸ V1 不实施：Online/Training 切换的 runtime API（仅启动校验）
- ⏸ V1 不实施：分布式 capture（V2 deferred）
- ⏸ V1 不实施：cross-cluster distillation（V2）

**Breaking Changes**:
- EventLogConfig.capture_prompt_bytes 删除（BREAKING-internal，仅影响 5 个内部文件，无外部 API）
- 文档漂移修正: ADR-0080 v1.2 §实施路径（cli_args_parser 而非 engine.cpp）

## ship gate 验证

- `python3 tools/adr_lint.py` 通过（含 ADR-TRACKING-01 WARNING 自动消失验证）
- `python3 tools/docs_drift_audit.py` 通过（无新增 CRITICAL drift）
- `openspec validate --changes --strict` PASS
- `ctest --output-on-failure` 全量 0 回归（动态基线，约 191 → 203+）
- `ctest -R test_capture_mode|test_distillation_writer|test_event_log_capture_mode` ≥ 12 cases PASS
- `grep -rn "capture_prompt_bytes" src/ include/ examples/` 0 命中（迁移彻底）
- `git diff HEAD -- include/agenticdsl/contract/` 0 行（契约层零修改）
- ADR-0080 v1.2 + ADR-0061-13 头部 `⏳ tracking: in-progress` 注记
- cap-map §一 +1（新能力 #31）
- ADR-0068 附录 A v1.7 含 `event_log.capture_mode_downgrade`

## 关联文档

- `docs/adr/adr-0080-v1-2-amendment-d10-decouple.md`（父 ADR）
- `docs/adr/skill/adr-0061-13-distillation-output-format.md`（下游派生）
- `docs/adr/adr-0083-evaluator-reward-contract.md`（RewardSignal 复用）
- `docs/adr/adr-0068-event-emission-contract.md`（事件发射契约）
- `include/agenticdsl/contract/reward_signal.h`（reward 复用）
- `include/agenticdsl/ir/trajectory_ir.h`（T15 输出源）
- `src/core/types/event_log_config.h`（迁移目标）
- `examples/pdk_chat_demo/cli_args_parser.cpp`（CLI flag 实际位置）
- `include/agenticdsl/prompt/prompt_hash.h`（T21 payload redact 复用）
- ADR-TRACKING-01 规则（`tools/adr_lint.py`，2026-08-29 ship）
- Oracle 决策 2 + 决策 5（session `ses_fb4cd8ff8ffeJlYBgU3JogcnfB`）
