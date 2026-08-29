# Design: capture-mode-and-distillation-writer-v1

## Context

**不变量链断裂**：
```
ADR-0080 v1.2 (CaptureMode 三态) [Approved 2026-08-25, 零代码]
       ↓ 解锁
ADR-0061-13 (IDistillationWriter) [Approved 2026-08-25, 幻影 change]
       ↓ 解锁
T19 GEPA / T20 AFlow [已 ship 2026-08-28, 但无下游消费通道]
```

**Oracle 二次审查 (2026-08-29)**: 蒸馏数据面闭环实际阻塞点从"ADR 层"移至"代码层"。Training 模式 capture 路径仍不可用，0061-13 即使 ship 也只能写到手动目录，无合规 capture 来源。

**所有前置已 ship**:
- ADR-0083 `RewardSignal` (V1+V2, `include/agenticdsl/contract/reward_signal.h`)
- T15 TrajectoryIR (V1, `include/agenticdsl/ir/trajectory_ir.h`)
- ADR-0068 附录 A v1.6 (27+ 主题注册)
- T21 payload redact (hash-only PII 范式)
- `--allow-training-capture` mock-mode guard 模式（pdk_chat_demo model_switching 已有先例）
- nlohmann::json vendor

## Scope Boundaries

### 范围 IN

- 4 新头文件（CaptureMode + DistillationRecord + IDistillationWriter + convergence meta）
- 2 新 .cpp（FileDistillationWriter + TrajectoryIR bridge）
- 2 新测试文件（≥12 cases）
- EventLogConfig 字段迁移（BREAKING-internal 5 消费者）
- `--allow-training-capture` CLI flag
- ADR-0068 附录 A v1.7 新增 1 主题
- ADR-0080 v1.2 + ADR-0061-13 头部加 `⏳ tracking: in-progress` 注记
- adr_lint ADR-TRACKING-01 WARNING 自动消失（实际验证）

### 范围 OUT（V2 deferred）

- Online/Training runtime API 切换（V1 仅启动校验）
- 分布式 capture（V2）
- cross-cluster distillation（V2）
- W3A EvaluationDataStore 流式接口（V2）
- 自适应 capture mode（基于 cost/budget 决策）

## Design Decisions

### D1 — CaptureMode 三态强类型枚举

**禁止魔法值/字符串**：
```cpp
enum class CaptureMode : uint8_t { Off = 0, Online = 1, Training = 2 };
```
- `Online`: 实时观测模式（pdk_chat_demo 默认，无 PII 风险）
- `Training`: 离线蒸馏模式（三重保护 + WARNING）
- `Off`: 完全关闭（生产路径，零开销）

### D2 — BREAKING-internal 字段迁移（保留语义）

**禁止 bool 兼容层**（Oracle 决策 2）：
```cpp
// BEFORE (v1.1):
struct EventLogConfig {
  bool capture_prompt_bytes = false;
  // ...
};

// AFTER (v1.2):
struct EventLogConfig {
  CaptureMode capture_mode = CaptureMode::Off;
  bool effective_capture_enabled() const { return capture_mode != CaptureMode::Off; }
  // ...
};
```
- 5 消费者（engine.h, engine.cpp, tracing_decorator.h, tracing_decorator.cpp, event_log_config.h）同步迁移
- `grep -rn "capture_prompt_bytes" src/ include/ examples/` 必须 0 命中（迁移彻底验证）

### D3 — Training 三重保护（路径漂移修正）

**禁止依赖 `engine.cpp` 做校验**（ADR-0080 v1.2 文档漂移）：
- CLI flag 解析在 `examples/pdk_chat_demo/cli_args_parser`（不是 engine.cpp）
- EventLogConfig 实际路径 `src/core/types/event_log_config.h`（不是 `src/core/event_log_config.h`）

**三重保护**（任一失败 → throw）：
1. `agent_id` 非空（ConvergenceMeta.agent_id）
2. 输出路径含 `train|distill` 子串
3. stderr WARNING（明确告知 PII 风险）

### D4 — IDistillationWriter 纯虚接口 + 默认实现

**L1 契约层纪律**：
```cpp
class IDistillationWriter {
public:
  virtual ~IDistillationWriter() = default;
  virtual void write(const DistillationRecord& record) = 0;
  virtual void finalize() = 0;
};
```
- V1 默认实现: `FileDistillationWriter`（同步写 + fsync + finalize meta.json）
- 扩展点: V2 StreamDistillationWriter / NetworkDistillationWriter（Marketplace 准备）

### D5 — payload redact 复用 T21 范式

**禁止新造 hash helper**：
- 复用 `include/agenticdsl/prompt/prompt_hash.h::hash_prompt()`
- 复用 `prompt_hash.h::estimate_tokens()`
- `DistillationRecord.trajectory` JSONL 含 `"prompt_hash": "<hash>"` 而非完整 prompt

### D6 — Mock-mode hard rejection（参考 model_switching）

```cpp
if (provider_mode == "mock" && capture_mode == CaptureMode::Training) {
  throw std::runtime_error("Training mode requires real LLM provider (--provider deepseek/openai)");
}
```
- 与 `chat-async-io-model-switching` mock-mode guard 同模式
- 防止 mock 生成的低质数据污染训练集

### D7 — 命名空间卫生

- 新代码全部 `agenticdsl::` 命名空间（既有惯例）
- 类型别名 `using CaptureMode = agenticdsl::CaptureMode`（如需要）
- 无裸 `IDistillationWriter` / `DistillationRecord`

### D8 — 文件命名 + 大小硬约束

- 命名: `<agent_id>_<seq>.distill.v1.jsonl`（seq 自增，避免冲突）
- ≤1.5MB/record（不变量 1，超出 throw `std::length_error`）
- finalize 后写 `<agent_id>_<seq>.distill.v1.meta.json`（ConvergenceMeta 摘要）

### D9 — Phase 0 ship 时对齐 ADR-0061-13 §决策 2 + 3（反向证据, 2026-08-29）

**历史背景**: Phase 0 plan 初稿（`capture-mode-and-distillation-writer-v1-phase-0.md`）的 T0.4 + T0.5 代码片段与 ADR-0061-13 §决策 2 + 3 存在 4 项偏离:
1. DistillationRecord 无 `input`/`output`/`trace_id`/`source_event`/`generation_timestamp_ms` 字段
2. StepRecord 字段集为通用步骤（`node_id`/`reward`/`confidence`）而非 ADR ReAct 步骤（`thought`/`tool_name`/`tool_args`/`observation`/`latency_ms`）
3. StepRecord.reward 拍平为 `double` + `double confidence` 而非真嵌入 `RewardSignal` struct（丢 Quality 三值枚举）
4. IDistillationWriter 仅 2 虚函数（`write` + `finalize()`）而非 ADR §决策 3 的 3 虚函数 + 工厂（`write_record` + `close` + `finalize(meta)` + `make_file_writer`）

**Oracle + Metis 双审查**（sessions `ses_fb33c1b2affe6CJtJph7iMdW22` + `ses_fb33c1973ffem31zWGz6f2XTlt`）识别上述偏离, 产出 9 项修正决策 (D1-D9), 由修正版 prompt `capture-mode-and-distillation-writer-v1-phase-0-CORRECTED.md` (655 行) 强制实施.

**Phase 0 ship 实际对齐结果** (commit `11d3515`):
- ✅ D1: `reward_signal.h` 路径修正为 `types/`
- ✅ D3: IDistillationWriter 3 虚函数 + 1 静态工厂完全对齐 ADR §决策 3
- ✅ D6 + D7: DistillationRecord 字段全集对齐 ADR §决策 2（含 input/output/trace_id/source_event/agent_id/teacher_version/generation_timestamp_ms/CaptureMode/ConvergenceMeta）+ StepRecord 真嵌入 `RewardSignal reward` 字段
- ✅ D8: MUST NOT 列完整 20 个 contract 既有头文件清单

**结论**: Phase 0 ship 与 ADR-0061-13 §决策 2 + 3 **完全对齐, 零偏离**. 此 D9 决策作为反向证据, 防 Phase 1 实施时误改回归.

## Risks

| 风险 | 缓解 |
|---|---|
| BREAKING 字段迁移遗漏消费者 | `grep -rn "capture_prompt_bytes"` 0 命中验证 |
| Training 模式 PII 泄漏 | 三重保护 + payload redact + WARNING |
| Mock 模式污染训练集 | hard rejection（参考 model_switching） |
| Path 路径漂移再次发生 | Oracle 决策 2 注记 + ADR-0080 v1.2 amendment PR 修正 |
| ADR-TRACKING-01 WARNING 不消失 | ship 时实际验证 adr_lint 输出 |
| 5 消费者中某个测试失败 | Phase 1 单独 commit + 迁移后立即 `ctest -R event_log` |

## Verification Gates

- ✅ `grep -rn "capture_prompt_bytes"` 0 命中
- ✅ ≥ 12 cases PASS (3 capture_mode + 5 distillation_writer + 5 event_log_capture_mode - 1 重复 = 12)
- ✅ `git diff HEAD -- include/agenticdsl/contract/` 0 行（契约层零修改）
- ✅ adr_lint ADR-TRACKING-01 WARNING 自动消失（ADR-0080 v1.2 + ADR-0061-13 现在有 tracking change）
- ✅ docs_drift_audit 0 NEW CRITICAL
- ✅ openspec validate --strict PASS
- ✅ ctest 全量 0 回归（动态基线）
- ✅ ADR-0068 附录 A v1.7 含 `event_log.capture_mode_downgrade`
- ✅ cap-map §一 +1（新能力 #31 Distillation Data Plane V1）

## Dependencies

### 满足

- ✅ ADR-0083 RewardSignal 已 ship
- ✅ T15 TrajectoryIR 已 ship
- ✅ T21 payload redact 范式已 ship
- ✅ EventLogConfig v1.1 (迁移源)
- ✅ nlohmann::json vendor
- ✅ `--allow-training-capture` mock-mode guard 模式

### 不依赖

- T19 GEPA Phase 3（T19 V1/V2 已 ship，但不直接依赖本 change）
- T20 AFlow MCTS Phase 2（同上）

## Success Criteria

- ADR-0080 v1.2 + ADR-0061-13 头部 `⏳ tracking: in-progress` + change 名链接
- 4 新头文件 + 2 新 .cpp ship
- ≥ 12 cases PASS
- ctest 全量 0 回归
- BREAKING 字段迁移彻底（5 消费者 + grep 验证）
- ADR-TRACKING-01 WARNING 自动消失
- OpenSpec archive 完成
- ADR-0068 附录 A v1.7 ship
- cap-map §一 +1 新能力 #31

## Out of Scope (V2 deferred)

- Online/Training runtime 切换 API（V1 仅启动校验）
- 分布式 capture（V2）
- cross-cluster distillation（V2）
- W3A EvaluationDataStore 流式接口（V2）
- 自适应 capture mode（基于 cost/budget 决策）
- NetworkDistillationWriter（V2，仅 FileDistillationWriter V1）
- StreamDistillationWriter（V2）
- Marketplace 第三方 Writer 注册（V2）
