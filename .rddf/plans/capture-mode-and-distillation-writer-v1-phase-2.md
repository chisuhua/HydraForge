# Phase 2 实施 Plan: capture-mode-and-distillation-writer-v1

> **创建日期**: 2026-08-29（Phase 0 ship `11d3515` + Phase 1 ship `9a781f8` 后）
> **目标**: CLI flag `--allow-training-capture` + TrajectoryIR bridge + payload redact
> **估时**: 0.3 sprint（1-2 working days）
> **设计依据**: `openspec/changes/capture-mode-and-distillation-writer-v1/tasks.md` Phase 2 + spec.md Requirements

---

## TASK

为 OpenSpec change `capture-mode-and-distillation-writer-v1` 实施 **Phase 2**：

1. **CLI flag** `--allow-training-capture`（pdk_chat_demo）
2. **TrajectoryIR bridge** `from_trajectory_ir(CanonicalIR)` → `DistillationRecord`
3. **payload redact** 复用 `hash_prompt()`（T21 已 ship）

**严格 TDD 5 步** + Phase 0/1 修正经验（D1-D9）。

---

## 已完成前置（Phase 0/1 ship）

| 组件 | 位置 | 状态 |
|------|------|------|
| CaptureMode 枚举 | `include/agenticdsl/types/capture_mode.h` | ✅ Phase 0 |
| DistillationRecord + StepRecord | `include/agenticdsl/types/distillation_record.h` | ✅ Phase 0 |
| IDistillationWriter + DistillationMetadata | `include/agenticdsl/contract/idistillation_writer.h` | ✅ Phase 0 |
| FileDistillationWriter | `src/modules/distillation/file_writer.{h,cpp}` | ✅ Phase 1 |
| EventLogConfig.capture_mode | `src/core/types/event_log_config.h` | ✅ Phase 1 |
| RewardSignal | `include/agenticdsl/contract/reward_signal.h` | ✅ ADR-0083 |
| hash_prompt() + estimate_tokens() | `include/agenticdsl/prompt/prompt_hash.h` | ✅ T21 |
| TrajectoryIR + CanonicalIR + to_sft_data() | `include/agenticdsl/ir/trajectory_ir.h` | ✅ T15 |

---

## EXPECTED OUTCOME

### 交付物

| 文件 | 类型 | 内容 |
|------|------|------|
| `examples/pdk_chat_demo/cli_options.h` | 修改 | `bool allow_training_capture = false;` |
| `examples/pdk_chat_demo/cli_args_parser.h` | 修改 | CliDestination 新增 `allow_training_capture` |
| `examples/pdk_chat_demo/cli_args_parser.cpp` | 修改 | flag 声明 + parse + mock-mode guard |
| `examples/pdk_chat_demo/main.cpp` | 修改 | 接线 CLI flag → config + mock guard 调用 |
| `src/modules/distillation/trajectory_bridge.h` | 新增 | `DistillationRecord from_trajectory_ir(...)` 声明 |
| `src/modules/distillation/trajectory_bridge.cpp` | 新增 | bridge 实现（复用 to_sft_data + hash_prompt）|
| `tests/test_trajectory_bridge.cpp` | 新增 | ≥5 cases / ≥15 assertions |
| `tests/test_event_log_capture_mode.cpp` | 新增 | ≥3 cases（mock guard 等）|
| `src/modules/distillation/CMakeLists.txt` | 修改 | +trajectory_bridge.cpp |
| **1 atomic commit** | — | `feat(distillation): CLI flag + TrajectoryIR bridge + payload redact (T2)` |

### 验证标准

1. `ctest -R "trajectory_bridge|event_log_capture_mode"` ≥8 cases PASS
2. 全量 ctest 0 新增回归
3. `grep -rn "capture_prompt_bytes" src/ include/ examples/` = 0 命中
4. Oracle B3: contract/ 20 既有文件零 diff
5. mock-mode guard: `--mock --allow-training-capture` → throw runtime_error（stderr 含 "requires real LLM provider"）
6. `--allow-training-capture` + provider=deepseek → 无 throw，配置生效
7. payload redact: JSONL 行 MUST NOT 含 `"prompt":` 字段（仅 hash）

---

## MUST DO（TDD 5 步 + D1-D9 修正经验）

### Step 1: Read 现有代码

```bash
# CLI 相关
read /workspace/project/HydraForge/examples/pdk_chat_demo/cli_options.h
read /workspace/project/HydraForge/examples/pdk_chat_demo/cli_args_parser.h
read /workspace/project/HydraForge/examples/pdk_chat_demo/cli_args_parser.cpp
read /workspace/project/HydraForge/examples/pdk_chat_demo/main.cpp

# Bridge 相关
read /workspace/project/HydraForge/include/agenticdsl/ir/trajectory_ir.h
read /workspace/project/HydraForge/include/agenticdsl/prompt/prompt_hash.h
read /workspace/project/HydraForge/include/agenticdsl/types/distillation_record.h
read /workspace/project/HydraForge/include/agenticdsl/contract/reward_signal.h

# Spec
read /workspace/project/HydraForge/openspec/changes/capture-mode-and-distillation-writer-v1/specs/capture-mode-and-distillation-writer-v1/spec.md
```

### Step 2: T2.1 Write failing test

#### 2a. `tests/test_trajectory_bridge.cpp`（≥5 cases）

```cpp
// tests/test_trajectory_bridge.cpp
// 功能描述: TrajectoryIR → DistillationRecord bridge 单元测试
#include "catch_amalgamated.hpp"
#include "agenticdsl/ir/trajectory_ir.h"
#include "agenticdsl/types/distillation_record.h"
#include "agenticdsl/types/capture_mode.h"
#include "modules/distillation/trajectory_bridge.h"  // include 真实 header（D4 防零编译覆盖）

using namespace agenticdsl;

// Case 1: CanonicalIR → DistillationRecord 基本字段映射
TEST_CASE("bridge_canonical_to_record_basic", "[trajectory_bridge][phase2]") {
  ir::TrajectoryIR::CanonicalIR canonical;
  canonical.schema_version = "1.0";
  canonical.metadata = {{"agent_id", "teacher_v1"}, {"teacher_version", "v1.0.0"}};
  canonical.canonical_steps.push_back({"step1", {{"thought", "think"}}});

  DistillationRecord record = distillation::from_trajectory_ir(canonical);
  REQUIRE(record.teacher_version == "v1.0.0");
  REQUIRE(record.capture_mode == CaptureMode::Off);  // 默认 Off
  // steps 映射（bridge 应把 canonical_steps → record.steps）
}

// Case 2: agent_id 映射
// Case 3: to_sft_data 复用（trajectory_jsonl 含 sft 数据）
// Case 4: payload redact — 复用 hash_prompt()，JSONL 不含原始 prompt
// Case 5: 空 CanonicalIR 安全（零 steps）
```

#### 2b. `tests/test_event_log_capture_mode.cpp`（≥3 cases）

```cpp
// Case 1: mock + allow-training-capture → throw runtime_error
// Case 2: real provider + allow-training-capture → 无 throw
// Case 3: EventLogConfig effective_capture_enabled() 语义
```

### Step 3: T2.2 Verify fail

```bash
cd /workspace/project/HydraForge/build
cmake .. -DAGENTICDSL_BUILD_TESTS=ON 2>&1 | tail -5
make test_trajectory_bridge test_event_log_capture_mode 2>&1 | tee /tmp/build_phase2.log | tail -20
grep -E "trajectory_bridge\.h.*file not found|trajectory_bridge\.cpp.*file not found" /tmp/build_phase2.log
```

### Step 4: T2.3 CLI flag（修改 cli_options.h + cli_args_parser.{h,cpp}）

**cli_options.h**:
```cpp
struct CliOptions {
  bool mock = false;
  bool print = false;
  bool offline = false;
  bool allow_training_capture = false;   // ✅ 新增
  // ... 其余字段不变
};
```

**cli_args_parser.h**:
```cpp
enum class CliDestination { mock, session_id, print, provider, offline, fork_node_id, session_name, system_prompt, append_system_prompt, allow_training_capture };
```

**cli_args_parser.cpp**:
```cpp
const std::vector<CliFlagSpec>& cli_flag_declarations() {
  static const std::vector<CliFlagSpec> table = {
    // ... 既有 9 项
    {"allow-training-capture", "", CliValueKind::flag, "",
     "Enable Training-mode distillation capture (requires real LLM provider, rejected in mock mode)",
     CliDestination::allow_training_capture},
  };
  return table;
}

// parse_cli_args 内:
result.options.allow_training_capture = parsed["allow-training-capture"].as<bool>();
```

### Step 5: T2.4 main.cpp 接线 + mock guard

**main.cpp**（找到 mock_mode 计算处，约 line 116）：

```cpp
const bool mock_mode = cli_options.mock;
// ...
// Mock-mode hard rejection（spec Requirement: mock + training 拒绝）
if (cli_options.allow_training_capture && mock_mode) {
  throw std::runtime_error(
      "--allow-training-capture requires a real LLM provider (not mock). "
      "Mock-generated data would pollute the distillation training set.");
}
```

**注意**：throw 需在 main() 早期（mock_mode 计算后），且需捕获显示到 stderr。

### Step 6: T2.4 TrajectoryIR bridge

**`src/modules/distillation/trajectory_bridge.h`**:
```cpp
#pragma once
#include "agenticdsl/ir/trajectory_ir.h"
#include "agenticdsl/types/distillation_record.h"

namespace agenticdsl::distillation {

// TrajectoryIR CanonicalIR → DistillationRecord
// V1: 复用 TrajectoryIR::to_sft_data() 输出作为 trajectory_jsonl 来源
//     payload redact: 复用 hash_prompt()（T21，不新造）
DistillationRecord from_trajectory_ir(
    const ir::TrajectoryIR::CanonicalIR& canonical,
    const std::string& trace_id = "",
    const std::string& source_event = "");

}  // namespace agenticdsl::distillation
```

**`src/modules/distillation/trajectory_bridge.cpp`**:
```cpp
#include "trajectory_bridge.h"
#include "agenticdsl/prompt/prompt_hash.h"
#include <nlohmann/json.hpp>

namespace agenticdsl::distillation {

DistillationRecord from_trajectory_ir(
    const ir::TrajectoryIR::CanonicalIR& canonical,
    const std::string& trace_id,
    const std::string& source_event) {
  DistillationRecord record;

  // agent_id / teacher_version from metadata
  if (canonical.metadata.contains("agent_id"))
    record.agent_id = canonical.metadata["agent_id"].get<std::string>();
  if (canonical.metadata.contains("teacher_version"))
    record.teacher_version = canonical.metadata["teacher_version"].get<std::string>();

  record.trace_id = trace_id;
  record.source_event = source_event;

  // steps 映射: canonical_steps → StepRecord
  for (const auto& cs : canonical.canonical_steps) {
    StepRecord sr;
    sr.thought = cs.metadata.contains("thought")
        ? cs.metadata["thought"].get<std::string>() : "";
    sr.tool_name = cs.metadata.contains("tool_name")
        ? cs.metadata["tool_name"].get<std::string>() : "";
    sr.observation = cs.metadata.contains("observation")
        ? cs.metadata["observation"].get<std::string>() : "";
    sr.latency_ms = cs.metadata.contains("latency_ms")
        ? cs.metadata["latency_ms"].get<std::uint64_t>() : 0;
    record.steps.push_back(std::move(sr));
  }

  // trajectory_jsonl: 复用 TrajectoryIR::to_sft_data() 输出
  // payload redact: JSONL 中 prompt 字段替换为 hash_prompt()（不保留原文）
  nlohmann::json sft = ir::TrajectoryIR::to_sft_data(canonical);
  // 对含 prompt 的字段做 hash 脱敏（V1: 简单遍历顶层字段）
  if (sft.contains("prompt")) {
    const std::string raw = sft["prompt"].get<std::string>();
    sft["prompt"] = hash_prompt(raw);  // 不可逆 hash，16 hex chars
  }
  record.trajectory_jsonl = sft.dump();

  return record;
}

}  // namespace agenticdsl::distillation
```

### Step 7: T2.5 Verify pass

```bash
cd /workspace/project/HydraForge/build
cmake .. -DAGENTICDSL_BUILD_TESTS=ON 2>&1 | tail -5
make test_trajectory_bridge test_event_log_capture_mode 2>&1 | tail -10
ctest -R "trajectory_bridge|event_log_capture_mode" --output-on-failure
ctest --output-on-failure 2>&1 | tail -10  # 全量 0 回归
```

### Step 8: T2.6 Commit

```bash
git add examples/pdk_chat_demo/cli_options.h \
        examples/pdk_chat_demo/cli_args_parser.h \
        examples/pdk_chat_demo/cli_args_parser.cpp \
        examples/pdk_chat_demo/main.cpp \
        src/modules/distillation/trajectory_bridge.h \
        src/modules/distillation/trajectory_bridge.cpp \
        src/modules/distillation/CMakeLists.txt \
        tests/test_trajectory_bridge.cpp \
        tests/test_event_log_capture_mode.cpp

git commit -m "feat(distillation): CLI flag + TrajectoryIR bridge + payload redact (T2)

Phase 2 实施: capture-mode-and-distillation-writer-v1

CLI flag:
- cli_options.h: bool allow_training_capture = false
- cli_args_parser.{h,cpp}: --allow-training-capture flag + parse
- main.cpp: mock-mode hard rejection (throw runtime_error, stderr 含 'requires real LLM provider')

TrajectoryIR bridge:
- trajectory_bridge.h/cpp: from_trajectory_ir(CanonicalIR) → DistillationRecord
- 复用 TrajectoryIR::to_sft_data() 输出作为 trajectory_jsonl 来源
- payload redact: 复用 hash_prompt()（T21, 不新造）, JSONL 不含原始 prompt

测试:
- test_trajectory_bridge.cpp: ≥5 cases
- test_event_log_capture_mode.cpp: ≥3 cases (mock guard + effective_capture_enabled)

验证:
- ctest -R 'trajectory_bridge|event_log_capture_mode' ≥8 cases PASS
- 全量 ctest 0 新增回归
- grep capture_prompt_bytes = 0 命中
- Oracle B3: contract/ 20 既有文件零 diff

[Phase 2, D1-D9 修正经验应用]"
```

---

## MUST NOT DO

- ❌ 修改 contract/ 下 20 个既有头文件（Oracle B3）
- ❌ 修改 capture_mode.h / distillation_record.h / idistillation_writer.h（Phase 0 ship 后冻结）
- ❌ 修改 event_log_config.h（Phase 1 已迁移完成，不再动）
- ❌ 新造 hash 函数（必须复用 hash_prompt()）
- ❌ 在 mock guard 中使用 `provider_mode == "mock"` 硬编码判断（应从 cli_options.mock 读取）
- ❌ 实现 Phase 3 内容（ADR 头部 + 文档同步 + archive）
- ❌ 使用 `git commit --no-verify`
- ❌ 硬编码 ctest 数字
- ❌ 跳过 make 直接 commit（D2）

---

## CONTEXT

### 已 ship 关键文件（Phase 0/1 + T15/T21 复用）
- `include/agenticdsl/types/capture_mode.h`（Phase 0）
- `include/agenticdsl/types/distillation_record.h`（Phase 0）
- `include/agenticdsl/contract/idistillation_writer.h`（Phase 0）
- `src/modules/distillation/file_writer.{h,cpp}`（Phase 1）
- `include/agenticdsl/ir/trajectory_ir.h`（T15, 含 CanonicalIR + to_sft_data）
- `include/agenticdsl/prompt/prompt_hash.h`（T21, 含 hash_prompt + estimate_tokens）
- `examples/pdk_chat_demo/cli_args_parser.{h,cpp}` + `cli_options.h`（Phase 2 修改目标）

### Phase 0/1 修正经验（D1-D9 应用）
- D1: include 路径 grep 验证实际存在
- D2: TDD Verify-fail 必须 cmake + make
- D4: 测试 include 真实实现 header（防零编译覆盖）
- D5: commit 前/后双向验证 contract/ 零 diff
- D8: MUST NOT 完整 20 contract 文件清单

---

**Plan 状态**: ✅ Ready
**创建者**: Sisyphus @ 2026-08-29
**适用场景**: Phase 1 ship 后，deep agent 委派实施 Phase 2
