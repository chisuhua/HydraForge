# Tasks: capture-mode-and-distillation-writer-v1

> **TDD 5 步结构**: 每任务按 Write failing test → Verify fail → Implement → Verify pass → Commit
> **关键不变量**（Oracle 决策 2）: 契约层 **零修改** — `include/agenticdsl/contract/` 全部 0 diff
> **设计依据**: ADR-0080 v1.2 + ADR-0061-13 + Oracle 决策 2 (session `ses_fb4cd8ff8ffeJlYBgU3JogcnfB`)

## Phase 0: 抽象层（4 .h + 默认实现骨架）

- [ ] **T0.1** Write failing test: `tests/test_capture_mode.cpp` ≥ 3 cases 占位
- [ ] **T0.2** Verify fail: 编译失败（`fatal error: 'agenticdsl/types/capture_mode.h' file not found`）
- [ ] **T0.3** Implement: `include/agenticdsl/types/capture_mode.h`:
  ```cpp
  namespace agenticdsl {
  enum class CaptureMode : uint8_t { Off = 0, Online = 1, Training = 2 };
  constexpr CaptureMode kDefaultCaptureMode = CaptureMode::Off;
  std::string to_string(CaptureMode m);
  CaptureMode parse_capture_mode(const std::string& s);  // throws std::invalid_argument
  }
  ```
- [ ] **T0.4** Implement: `include/agenticdsl/types/distillation_record.h`:
  - `struct StepRecord { std::string node_id; nlohmann::json metadata; double reward; double confidence; }`
  - `struct DistillationRecord { std::string agent_id; std::string teacher_version; CaptureMode capture_mode; std::vector<StepRecord> steps; std::string trajectory_jsonl; ConvergenceMeta meta; }`
  - `struct ConvergenceMeta { std::string task_id; std::string trace_id; std::chrono::system_clock::time_point created_at; }`
- [ ] **T0.5** Implement: `include/agenticdsl/contract/idistillation_writer.h`:
  ```cpp
  namespace agenticdsl {
  class IDistillationWriter {
  public:
    virtual ~IDistillationWriter() = default;
    virtual void write(const DistillationRecord& record) = 0;
    virtual void finalize() = 0;
  };
  }
  ```
- [ ] **T0.6** Verify pass: 3 cases 编译通过（运行时仍 FAIL）
- [ ] **T0.7** Commit: `feat(distillation): CaptureMode + DistillationRecord + IDistillationWriter contracts (T0)`

## Phase 1: BREAKING 字段迁移 + 默认实现

- [ ] **T1.1** Write failing test: `tests/test_distillation_writer.cpp` ≥ 5 cases 占位
- [ ] **T1.2** Verify fail: 5 cases FAIL
- [ ] **T1.3** Implement: `src/core/types/event_log_config.h`:
  - 删除 `bool capture_prompt_bytes = false`
  - 新增 `CaptureMode capture_mode = CaptureMode::Off`
  - `bool effective_capture_enabled() const { return capture_mode != CaptureMode::Off; }`
- [ ] **T1.4** Migrate 5 消费者:
  - `src/core/engine.h` (PIMPL 字段引用)
  - `src/core/engine.cpp` (构造默认值)
  - `src/common/llm/tracing_decorator.h` (字段引用)
  - `src/common/llm/tracing_decorator.cpp` (捕获开关判断)
  - `src/core/types/event_log_config.h` (本身)
- [ ] **T1.5** Verify: `grep -rn "capture_prompt_bytes" src/ include/ examples/` = **0 命中**
- [ ] **T1.6** Implement: `src/core/event_log.cpp` 启动校验 + 降级 + 审计事件:
  - Training 三重保护（agent_id 必填 + 路径含 train|distill + WARNING）
  - Online→Training 降级检测 + `event_log.capture_mode_downgrade` 事件发射（ADR-0068 §决策 2）
- [ ] **T1.7** Implement: `src/modules/distillation/file_writer.cpp`:
  - `class FileDistillationWriter : public IDistillationWriter`
  - write: 同步写 + fsync，命名 `<agent_id>_<seq>.distill.v1.jsonl`
  - finalize: 写 `<agent_id>_<seq>.distill.v1.meta.json`
  - ≤1.5MB/record 硬上限（超出 throw `std::length_error`）
- [ ] **T1.8** Verify pass: 5 cases PASS + 全量既有测试零回归（特别是 event_log 既有测试）
- [ ] **T1.9** Commit: `feat(distillation): EventLogConfig BREAKING 迁移 + FileDistillationWriter V1 (T1)`

## Phase 2: CLI flag + 桥接

- [ ] **T2.1** Write failing test: `tests/test_event_log_capture_mode.cpp` ≥ 5 cases 占位
- [ ] **T2.2** Verify fail: 5 cases FAIL
- [ ] **T2.3** Modify: `examples/pdk_chat_demo/cli_args_parser`:
  - 新增 `--allow-training-capture` flag
  - **mock-mode guard**: `if (provider_mode == "mock" && flag_enabled) throw runtime_error + stderr warning`
- [ ] **T2.4** Implement: `src/modules/distillation/trajectory_bridge.cpp`:
  - `DistillationRecord from_trajectory_ir(const TrajectoryIR::CanonicalIR&)`
  - 复用 `TrajectoryIR::to_sft_data()` 输出作为 trajectory_jsonl
  - payload redact: 复用 `hash_prompt()` helper（不新造）
- [ ] **T2.5** Verify pass: 5 cases PASS
- [ ] **T2.6** Commit: `feat(distillation): CLI flag + TrajectoryIR bridge + payload redact (T2)`

## Phase 3: ADR 头部 + 文档同步 + ship

- [ ] **T3.1** Modify: `docs/adr/adr-0080-v1-2-amendment-d10-decouple.md` 头部追加:
  - `⏳ tracking: in-progress` 标注
  - 链接本 change: `see openspec/changes/capture-mode-and-distillation-writer-v1`
- [ ] **T3.2** Modify: `docs/adr/skill/adr-0061-13-distillation-output-format.md` 头部追加同样链接
- [ ] **T3.3** Modify: `docs/adr/adr-0068-event-emission-contract.md` 附录 A v1.7:
  - 新增主题 `event_log.capture_mode_downgrade`
- [ ] **T3.4** Modify: `docs/architecture/capability-application-map-2026-08.md`:
  - §七 changelog 追加 v2.5 条目（Distillation Data Plane V1 ship）
  - §一 +1（新能力 #31 Distillation Data Plane V1）
- [ ] **T3.5** Verify: `python3 tools/adr_lint.py` 通过
  - 关键验证: ADR-0080 v1.2 + ADR-0061-13 ADR-TRACKING-01 WARNING 应自动消失
- [ ] **T3.6** Verify: `python3 tools/docs_drift_audit.py` 0 NEW CRITICAL
- [ ] **T3.7** Verify: `openspec validate --changes --strict` PASS
- [ ] **T3.8** Verify: `ctest --output-on-failure` 全量 0 回归（动态基线）
- [ ] **T3.9** Verify 关键不变量（Oracle B3 类）:
  - `git diff HEAD -- include/agenticdsl/contract/` 0 行
  - `grep -rn "capture_prompt_bytes" src/ include/ examples/` 0 命中
- [ ] **T3.10** Commit: `docs(adr+cap-map): Distillation Data Plane V1 ship — ADR-TRACKING-01 WARNING 解除`
- [ ] **T3.11** `openspec archive capture-mode-and-distillation-writer-v1`

## 总估时

- Phase 0: 0.5 sprint（抽象层）
- Phase 1: 0.5 sprint（BREAKING 迁移 + 默认实现）
- Phase 2: 0.3 sprint（CLI flag + 桥接）
- Phase 3: 0.2 sprint（文档同步 + ship）
- **总估时: ~1.5 sprint**（与 ADR-0080 v1.2 自估 0.5 + ADR-0061-13 自估 1 合并节省 0.5 sprint）

## 关键不变量（强制遵守）

### Oracle B3 类（既有契约零修改）

```bash
# 必须 0 行的 git diff
git diff HEAD -- include/agenticdsl/contract/i_llm_provider_decorator.h
git diff HEAD -- include/agenticdsl/contract/iinteraction_bus.h
git diff HEAD -- include/agenticdsl/contract/itool_hook_registry.h
git diff HEAD -- include/agenticdsl/contract/iagent_hook_registry.h
git diff HEAD -- include/agenticdsl/contract/iagent_registry.h
git diff HEAD -- include/agenticdsl/contract/iagent_composition.h
git diff HEAD -- include/agenticdsl/contract/event_builder.h
git diff HEAD -- include/agenticdsl/contract/ievaluator.h
git diff HEAD -- include/agenticdsl/contract/imutation_governance.h
git diff HEAD -- include/agenticdsl/contract/idistillation_writer.h  # 本 change 新增, 仅新增
```

### BREAKING 迁移彻底

```bash
# 必须 0 命中
grep -rn "capture_prompt_bytes" src/ include/ examples/
```

### ADR-TRACKING-01 WARNING 自动消失

```bash
python3 tools/adr_lint.py 2>&1 | grep "adr-0080\|0061-13"
# 期望: 0 命中（因为本 change 目录名含 "0080" 和 "0061-13"）
```

## 测试要求汇总

- **≥ 3 cases** test_capture_mode
- **≥ 5 cases** test_distillation_writer
- **≥ 5 cases** test_event_log_capture_mode
- **总计: ≥ 13 cases**（超出 ≥10 门槛）

## 明确 out of scope (V2 deferred)

- Online/Training runtime 切换 API
- 分布式 capture
- cross-cluster distillation
- W3A EvaluationDataStore 流式接口
- 自适应 capture mode（基于 cost/budget 决策）
- NetworkDistillationWriter / StreamDistillationWriter
- Marketplace 第三方 Writer 注册

## 关键路径漂移修正（Oracle 决策 2 发现）

| ADR-0080 v1.2 §实施原文 | 实际位置 | 修正策略 |
|---|---|---|
| `src/core/event_log_config.h` | `src/core/types/event_log_config.h` | 本 change 实施时使用实际路径 |
| `--allow-training-capture` flag 在 `engine.cpp` | `examples/pdk_chat_demo/cli_args_parser` | 本 change 实施时使用实际位置 |
| `IDistillationWriter` 直接 hook engine | L1 契约层独立 | 本 change 按契约层纪律独立 |

ADR-0080 v1.2 amendment PR 修订：未来更新 ADR 时同步修正上述路径漂移。
