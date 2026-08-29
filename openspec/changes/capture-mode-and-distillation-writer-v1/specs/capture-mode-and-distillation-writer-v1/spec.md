# capture-mode-and-distillation-writer-v1 Specification

## ADDED Requirements

### Requirement: CaptureMode 三态强类型枚举

The `CaptureMode` enum MUST provide exactly 3 states (`Off` / `Online` / `Training`) with stable integer values. MUST provide `to_string()` and `parse_capture_mode()` round-trip helpers.

#### Scenario: 枚举三态完整

- **WHEN** 静态检查 `grep -E 'Off|Online|Training' include/agenticdsl/types/capture_mode.h`
- **THEN** 3 个枚举值全部出现

#### Scenario: to_string/parse round-trip

- **WHEN** 运行 `test_capture_mode::enum_serialization`
- **THEN** `parse_capture_mode(to_string(m)) == m` 对所有 3 个值成立

#### Scenario: 默认值 Off

- **WHEN** 构造 `EventLogConfig`（默认字段）
- **THEN** `capture_mode == CaptureMode::Off`（生产路径零开销）

> **实现注记 (Metis A3 修正)**:
> - **Phase 0 ship** (commit `11d3515`): 仅测 `kDefaultCaptureMode` 常量本身（不构造 EventLogConfig，因 Phase 0 不 touch `src/core/types/event_log_config.h`）
> - **Phase 1 ship**: 该 Scenario 在 EventLogConfig 字段迁移后真实验证（`bool capture_prompt_bytes → CaptureMode capture_mode`）
> - **测试代码位置**: `tests/test_capture_mode.cpp::default_value_and_static_asserts`
> - **Oracle 决策 2**: 禁止 bool 兼容层，迁移彻底后默认值校验自动满足

### Requirement: BREAKING 字段迁移彻底

The V1 implementation MUST NOT leave any `capture_prompt_bytes` references in `src/`, `include/`, or `examples/`. Migration MUST touch all 5 consumer files (engine.h, engine.cpp, tracing_decorator.h, tracing_decorator.cpp, event_log_config.h).

#### Scenario: grep 0 命中

- **WHEN** 运行 `grep -rn "capture_prompt_bytes" src/ include/ examples/`
- **THEN** 0 命中（迁移彻底）

#### Scenario: EventLogConfig 新字段

- **WHEN** 静态检查 `grep "CaptureMode capture_mode" src/core/types/event_log_config.h`
- **THEN** 新字段定义存在

#### Scenario: 5 消费者更新

- **WHEN** 静态检查 `git diff HEAD -- src/core/engine.h src/core/engine.cpp src/common/llm/tracing_decorator.h src/common/llm/tracing_decorator.cpp`
- **THEN** 全部有 diff（每文件至少 1 行变更）

### Requirement: Training 三重保护

The `Training` mode MUST enforce 3 protections at startup: (1) `agent_id` non-empty, (2) output path contains `train` or `distill` substring, (3) stderr WARNING printed.

#### Scenario: agent_id 为空触发 throw

- **WHEN** 配置 `capture_mode = Training` 但 `agent_id == ""`
- **THEN** 启动 MUST throw `std::invalid_argument`

#### Scenario: 路径不含 train/distill 触发 throw

- **WHEN** 配置 `capture_mode = Training` 但 output path = `/tmp/data.jsonl`
- **THEN** 启动 MUST throw `std::invalid_argument`

#### Scenario: 三重保护全过 emit WARNING

- **WHEN** Training 配置全部合法
- **THEN** stderr MUST 含 `WARNING.*Training mode` 子串

### Requirement: IDistillationWriter 纯虚契约

The `IDistillationWriter` MUST be a pure virtual interface with **3 pure virtual methods + 1 static factory**, **完全对齐 ADR-0061-13 §决策 3**:
- `write_record(const DistillationRecord&)` — 写入单条 record
- `close()` — flush + 关闭（析构时自动调用）
- `finalize(const DistillationMetadata&)` — 收尾写 meta.json
- `make_file_writer(const std::filesystem::path&, const std::string&)` — 静态工厂

FileDistillationWriter MUST implement all 3 virtual methods and the factory.

> **历史变更（spec 修订）**:
> - 原 spec v0.1: 2 methods (`write` + `finalize()`)
> - **修订为 3 methods + 工厂**（Oracle/Metis 修正 D3, 2026-08-29）— 与 ADR-0061-13 §决策 3 完全对齐，避免 Phase 1 因 `finalize()` 缺参数导致接口返工
> - Phase 0 ship commit `11d3515` 已应用此修订

#### Scenario: 契约层接口完整性

- **WHEN** 静态检查 `grep "virtual.*write_record\|virtual.*close\|virtual.*finalize" include/agenticdsl/contract/idistillation_writer.h`
- **THEN** **3 个虚函数声明全部存在**（修正 D3 后由 2 个扩为 3 个）

#### Scenario: 静态工厂存在性

- **WHEN** 静态检查 `grep "make_file_writer" include/agenticdsl/contract/idistillation_writer.h`
- **THEN** 静态工厂签名必须存在（修正 D3 新增）

#### Scenario: FileDistillationWriter 实现

- **WHEN** 运行 `test_distillation_writer::file_writer_write_read_roundtrip`
- **THEN** 写入 + 读取 JSONL round-trip 成功

#### Scenario: ≤1.5MB 硬约束

- **WHEN** 写入 record > 1.5MB
- **THEN** MUST throw `std::length_error`

#### Scenario: 文件命名唯一性

- **WHEN** 多次调用 `write()`
- **THEN** 文件命名 `<agent_id>_<seq>.distill.v1.jsonl` 中 seq MUST 单调递增

### Requirement: payload redact 复用 T21

The DistillationRecord MUST reuse T21 `hash_prompt()` helper instead of duplicating hash logic. JSONL output MUST contain `prompt_hash` field instead of full prompt text.

#### Scenario: hash_prompt 复用

- **WHEN** 静态检查 `grep "hash_prompt" src/modules/distillation/trajectory_bridge.cpp`
- **THEN** MUST 调用现有 helper（不重新实现 hash）

#### Scenario: JSONL 仅含 hash

- **WHEN** 运行 `test_distillation_writer::file_writer_no_prompt_in_jsonl`
- **THEN** 输出的 JSONL 行 MUST NOT 含 `"prompt":` 字段

### Requirement: Mock-mode hard rejection

The `--allow-training-capture` CLI flag MUST be rejected when `provider_mode == "mock"`. Error message MUST explain the reason.

#### Scenario: mock + training 拒绝

- **WHEN** 启动时 `--allow-training-capture` 但 `provider == "mock"`
- **THEN** MUST throw `std::runtime_error`，stderr 含 "requires real LLM provider"

#### Scenario: real provider + training 接受

- **WHEN** 启动时 `--allow-training-capture` 且 `provider == "deepseek"`
- **THEN** 配置生效，无 throw

### Requirement: capture_mode_downgrade 审计事件

When `Online → Training` mode mismatch is detected at runtime (V1: 仅启动校验检测路径 mismatch), the system MUST emit `event_log.capture_mode_downgrade` event via `IInteractionBus`.

#### Scenario: 路径不匹配 emit 事件

- **WHEN** 配置 `capture_mode = Training` 但路径不含 train/distill
- **THEN** emit `event_log.capture_mode_downgrade` 事件 payload 含 `original_mode=Training, detected_mode=Off, reason=path_mismatch`

### Requirement: ADR-0068 附录 A v1.7 主题注册

After V1 ship, ADR-0068 附录 A MUST contain `event_log.capture_mode_downgrade` topic entry.

#### Scenario: 附录 A 主题登记

- **WHEN** 静态检查 `grep "event_log.capture_mode_downgrade" docs/adr/adr-0068-event-emission-contract.md`
- **THEN** MUST ≥ 1 命中

### Requirement: cap-map §一 +1 新能力

After V1 ship, capability-application-map MUST add new capability #31 Distillation Data Plane V1 in §一.

#### Scenario: 能力登记

- **WHEN** 静态检查 `grep "Distillation Data Plane" docs/architecture/capability-application-map-2026-08.md`
- **THEN** §一表格 MUST 新增 Distillation Data Plane V1 能力行

### Requirement: ADR-TRACKING-01 WARNING 自动消失

After V1 ship, the `tools/adr_lint.py` ADR-TRACKING-01 rule MUST NOT emit WARNING for ADR-0080 v1.2 + ADR-0061-13 (because their tracking change directory exists).

#### Scenario: adr_lint 验证

- **WHEN** 运行 `python3 tools/adr_lint.py 2>&1 | grep "adr-0080-v1-2\|adr-0061-13"`
- **THEN** 0 命中（WARNING 自动消失）

### Requirement: 契约层零修改（Oracle B3 关键不变量）

The V1 implementation MUST NOT modify any existing file under `include/agenticdsl/contract/` (only ADD new files). **20 个 contract 既有头文件**（修正 D8：从 9 个更新为完整 20 个清单）零 diff。

完整 20 个清单（不可修改）:
```
bus_event.h, causal_clock.h, evaluation_events.h, event_builder.h,
i_llm_provider_decorator.h, iagent_composition.h, iagent_hook_registry.h,
iagent_registry.h, icommand_registry.h, ievaluator.h, iinteraction_bus.h,
imutation_governance.h, inmemory_bus.h, iparser.h, iprovider_factory.h,
ischeduler.h, iskill_compiler.h, itool_hook_registry.h, itool_registry.h,
test_double_registry.h
```

✅ 仅允许新增: `idistillation_writer.h`

#### Scenario: 20 个契约文件 git diff 0 行

- **WHEN** `git diff HEAD^ HEAD -- include/agenticdsl/contract/` (commit 后)
- **THEN** **仅含 idistillation_writer.h 新增行，其余 20 文件 0 行修改**（修正 D5：commit 前/后两个不同命令）

> **Phase 0 ship 验证**: commit `11d3515` 实测通过（idistillation_writer.h +59 行, 其余 20 文件 0 行）

### Requirement: ctest 全量零回归

The `ctest --output-on-failure` MUST report ALL tests PASS with zero regressions relative to ADR-TRACKING-01 baseline (191 tests).

#### Scenario: ctest 全量 PASS

- **WHEN** 运行 `ctest --output-on-failure`
- **THEN** 所有测试 PASS, 0 failures（pre-existing 4 失败不计入）
- **AND** 测试计数 ≥ baseline + 13（动态计数, 禁止硬编码）

#### Scenario: test_distillation_* 专项

- **WHEN** 运行 `ctest --output-on-failure -R test_capture_mode\|test_distillation_writer\|test_event_log_capture_mode`
- **THEN** ≥ 13 cases PASS（3 capture_mode + 5 distillation_writer + 5 event_log_capture_mode）

### Requirement: 路径漂移修正文档化

The V1 implementation MUST use the ACTUAL code paths (not the ADR-0080 v1.2 §实施 documented paths). Path drift MUST be documented in ADR-0080 v1.2 amendment PR.

#### Scenario: 实际路径使用

- **WHEN** 静态检查 `ls src/core/types/event_log_config.h examples/pdk_chat_demo/cli_args_parser.cpp`
- **THEN** 2 个文件都存在（本 change 在实际位置修改）

#### Scenario: 路径漂移注记

- **WHEN** V1 ship 后查 `docs/adr/adr-0080-v1-2-amendment-d10-decouple.md`
- **THEN** MUST 含路径漂移修正注记（指向本 change 实施位置）

## Out of Scope

- Online/Training runtime 切换 API（V1 仅启动校验）
- 分布式 capture / cross-cluster distillation
- W3A EvaluationDataStore 流式接口
- NetworkDistillationWriter / StreamDistillationWriter
- Marketplace 第三方 Writer 注册
- 自适应 capture mode（基于 cost/budget 决策）
