# Phase 1 实施 Plan: capture-mode-and-distillation-writer-v1

> **创建日期**: 2026-08-29（基于 Phase 0 ship `11d3515` 后的下一步）
> **前置**: Phase 0 已 ship（CaptureMode + DistillationRecord + IDistillationWriter 抽象）
> **目标**: BREAKING 字段迁移 + FileDistillationWriter V1 实施
> **估时**: 0.5 sprint（2-3 working days）
> **设计依据**: `openspec/changes/capture-mode-and-distillation-writer-v1/tasks.md` Phase 1（已含 Oracle/Metis 修正注记）
> **关联**: Phase 0 修正版 plan `.rddf/plans/capture-mode-and-distillation-writer-v1-phase-0-CORRECTED.md`

---

## ⚠️ Phase 0 修正版经验应用到 Phase 1

| Phase 0 教训 | Phase 1 应用 |
|-------------|-------------|
| D1: 路径错误（contract/ vs types/）| ✅ 全部新路径确认实际存在 |
| D2: TDD 只 cmake 不 make | ✅ Phase 1 实施时 cmake + make |
| D3: 虚函数签名偏离 ADR | ✅ Phase 1 严格按修正版 3 虚函数 + 工厂 |
| D4: 零编译覆盖盲区 | ✅ test_distillation_writer.cpp include FileDistillationWriter 实现 + header |
| D5: commit 验证命令 | ✅ Phase 1 同样区分 commit 前/后 |
| D6/D7: 数据结构偏离 | ✅ Phase 1 FileDistillationWriter 实现对齐 ADR-0061-13 |
| D8: 9 vs 20 contract 文件 | ✅ 完整 20 文件清单 |
| D9: 测试引用文件不存在 | ✅ 引用实际存在文件 |

---

## TASK

为 OpenSpec change `capture-mode-and-distillation-writer-v1` 实施 **Phase 1**：BREAKING 字段迁移 + FileDistillationWriter V1 默认实现。

**严格 TDD 5 步**：Write failing test → Verify fail → Implement → Verify pass → Commit。

---

## EXPECTED OUTCOME

### 交付物清单

| 文件 | 类型 | 内容 |
|------|------|------|
| `tests/test_distillation_writer.cpp` | 新增 | ≥5 cases / ≥15 assertions |
| `src/core/types/event_log_config.h` | **修改**（BREAKING）| `bool capture_prompt_bytes` → `CaptureMode capture_mode` + `effective_capture_enabled()` |
| `src/core/engine.h` | **修改** | 构造参数 `bool capture_prompt_bytes` → `CaptureMode capture_mode` |
| `src/core/engine.cpp` | **修改** | 构造默认值 + 传递 EventLogConfig |
| `src/common/llm/tracing_decorator.h` | **修改** | 字段类型 bool → CaptureMode |
| `src/common/llm/tracing_decorator.cpp` | **修改** | 捕获开关判断（CaptureMode != Off）|
| `src/core/event_log.cpp` | **修改** | Training 三重保护 + Online→Training 降级 + 审计事件 |
| `src/modules/distillation/file_writer.h` | **新增** | FileDistillationWriter 类声明 |
| `src/modules/distillation/file_writer.cpp` | **新增** | write_record + close + finalize(meta) + make_file_writer 工厂实现 |
| `src/modules/distillation/CMakeLists.txt` | **新增** | 注册新模块 |
| `CMakeLists.txt`（根）| **修改** | `add_subdirectory(src/modules/distillation)` |
| **1 atomic commit** | — | `feat(distillation): EventLogConfig BREAKING 迁移 + FileDistillationWriter V1 (T1)` |

### 验证标准

1. **ctest**: `ctest -R distillation_writer --output-on-failure` ≥5 cases / ≥15 assertions PASS
2. **ctest 全量**: 0 新增回归（与 baseline 持平）
3. **BREAKING 迁移彻底**: `grep -rn "capture_prompt_bytes" src/ include/ examples/` = 0 命中
4. **Oracle B3 不变量**:
   - 既有 19 个 contract 头文件零 diff（**仅 idistillation_writer.h 已在 Phase 0 新增并允许修改**）
   - `src/core/engine.h` 是 src/ 不是 contract/，修改是允许的（但需在 commit message 中明确）
5. **审计事件发射**: `event_log.capture_mode_downgrade` 主题注册 + 实测发射验证
6. **Training 三重保护**:
   - agent_id 为空 throw
   - 路径不含 train|distill throw
   - 三重保护全过 emit WARNING
7. **≤1.5MB 硬上限**: 超大 record 抛 `std::length_error`
8. **FileDistillationWriter 文件命名**: `<agent_id>_<seq>.distill.v1.jsonl` + `<agent_id>_<seq>.distill.v1.meta.json`

---

## REQUIRED TOOLS

- `read` / `write` / `edit`
- `bash`（cmake + make + ctest + grep + git diff + git add + git commit）
- `grep` / `glob`（查找既有 event_log / tracing_decorator / CaptureMode）

---

## MUST DO（严格 TDD 5 步 + Phase 0 修正经验）

### Step 1: 准备工作（Read 现有代码）

```bash
# 必读文件
read /workspace/project/HydraForge/src/core/types/event_log_config.h        # BREAKING 迁移目标
read /workspace/project/HydraForge/src/core/event_log.h /workspace/project/HydraForge/src/core/event_log.cpp  # 启动校验目标
read /workspace/project/HydraForge/src/core/engine.h /workspace/project/HydraForge/src/core/engine.cpp          # 5 消费者 #1-2
read /workspace/project/HydraForge/src/common/llm/tracing_decorator.h /workspace/project/HydraForge/src/common/llm/tracing_decorator.cpp  # 5 消费者 #3-4
read /workspace/project/HydraForge/include/agenticdsl/types/capture_mode.h       # Phase 0 已 ship
read /workspace/project/HydraForge/include/agenticdsl/types/distillation_record.h  # Phase 0 已 ship
read /workspace/project/HydraForge/include/agenticdsl/contract/idistillation_writer.h  # Phase 0 已 ship（含 DistillationMetadata struct）
read /workspace/project/HydraForge/openspec/changes/capture-mode-and-distillation-writer-v1/tasks.md  # Phase 1 完整任务定义
read /workspace/project/HydraForge/openspec/changes/capture-mode-and-distillation-writer-v1/specs/capture-mode-and-distillation-writer-v1/spec.md  # 13 ADDED Requirements
```

### Step 2: T1.1 Write failing test

创建 `tests/test_distillation_writer.cpp`（≥5 cases / ≥15 assertions）：

```cpp
// tests/test_distillation_writer.cpp
// 功能描述: FileDistillationWriter V1 单元测试（写 + close + finalize + make_file_writer 工厂 + Training 三重保护 + ≤1.5MB 硬约束 + 审计事件）
// 依据: openspec/changes/capture-mode-and-distillation-writer-v1/spec.md
//        Requirements: BREAKING 字段迁移 + IDistillationWriter FileDistillationWriter + payload redact + capture_mode_downgrade 审计事件

#include "catch_amalgamated.hpp"

#include "agenticdsl/types/capture_mode.h"
#include "agenticdsl/types/distillation_record.h"
#include "agenticdsl/contract/idistillation_writer.h"
#include "agenticdsl/contract/iinteraction_bus.h"

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <memory>
#include <string>

using agenticdsl::CaptureMode;
using agenticdsl::DistillationRecord;
using agenticdsl::StepRecord;
using agenticdsl::DistillationMetadata;
using agenticdsl::IDistillationWriter;
using agenticdsl::IDistillationWriterPtr;
using agenticdsl::RewardSignal;

namespace fs = std::filesystem;

// Helper: 创建临时目录
static fs::path make_temp_dir(const std::string& prefix) {
  fs::path p = fs::temp_directory_path() / (prefix + "_" + std::to_string(std::rand()));
  fs::create_directories(p);
  return p;
}

// Helper: 构造测试 record
static DistillationRecord make_test_record(const std::string& agent_id, CaptureMode mode) {
  DistillationRecord r;
  r.input = "test_input";
  r.output = "test_output";
  r.steps.push_back(StepRecord{"thought1", "tool1", nlohmann::json::object(), "obs1", 100});
  r.reward = RewardSignal{};  // 默认 RewardSignal
  r.trace_id = "trace_001";
  r.source_event = "llm.response";
  r.agent_id = agent_id;
  r.teacher_version = "v1.0.0";
  r.generation_timestamp_ms = 1234567890;
  r.capture_mode = mode;
  r.convergence.agent_id = agent_id;
  r.convergence.teacher_version = "v1.0.0";
  r.convergence.task_id = "task_001";
  r.convergence.trace_id = "trace_001";
  return r;
}

// Case 1: FileDistillationWriter 基本 write_record + close 流程
TEST_CASE("file_writer_write_record_creates_jsonl", "[distillation_writer][phase1]") {
  fs::path temp_dir = make_temp_dir("distill_write");
  IDistillationWriterPtr writer = IDistillationWriter::make_file_writer(temp_dir, "test_agent");
  REQUIRE(writer != nullptr);

  DistillationRecord r = make_test_record("test_agent", CaptureMode::Online);
  writer->write_record(r);
  writer->close();

  // 验证文件存在：<agent_id>_<seq>.distill.v1.jsonl
  bool found = false;
  for (const auto& entry : fs::directory_iterator(temp_dir)) {
    if (entry.path().extension() == ".jsonl") {
      found = true;
      std::ifstream ifs(entry.path());
      std::string line;
      REQUIRE(std::getline(ifs, line));
      REQUIRE(line.find("test_input") != std::string::npos);
      REQUIRE(line.find("test_output") != std::string::npos);
      REQUIRE(line.find("\"agent_id\":\"test_agent\"") != std::string::npos);
    }
  }
  REQUIRE(found);
  fs::remove_all(temp_dir);
}

// Case 2: finalize 写 meta.json
TEST_CASE("file_writer_finalize_writes_meta_json", "[distillation_writer][phase1]") {
  fs::path temp_dir = make_temp_dir("distill_meta");
  IDistillationWriterPtr writer = IDistillationWriter::make_file_writer(temp_dir, "test_agent");

  DistillationRecord r = make_test_record("test_agent", CaptureMode::Online);
  writer->write_record(r);

  DistillationMetadata meta;
  meta.version = "v1";
  meta.total_examples = 1;
  meta.dataset_hash = "abc123";
  meta.generation_config = {{"teacher_version", "v1.0.0"}, {"capture_mode", "Online"}};
  writer->finalize(meta);

  // 验证 meta.json 存在
  bool found = false;
  for (const auto& entry : fs::directory_iterator(temp_dir)) {
    if (entry.path().extension() == ".meta.json") {
      found = true;
      std::ifstream ifs(entry.path());
      std::string content((std::istreambuf_iterator<char>(ifs)),
                          std::istreambuf_iterator<char>());
      REQUIRE(content.find("\"version\":\"v1\"") != std::string::npos);
      REQUIRE(content.find("\"total_examples\":1") != std::string::npos);
      REQUIRE(content.find("\"dataset_hash\":\"abc123\"") != std::string::npos);
    }
  }
  REQUIRE(found);
  fs::remove_all(temp_dir);
}

// Case 3: ≤1.5MB 硬上限（超限 throw std::length_error）
TEST_CASE("file_writer_size_limit_throws_length_error", "[distillation_writer][phase1]") {
  fs::path temp_dir = make_temp_dir("distill_size");
  IDistillationWriterPtr writer = IDistillationWriter::make_file_writer(temp_dir, "test_agent");

  DistillationRecord r = make_test_record("test_agent", CaptureMode::Online);
  r.input = std::string(2 * 1024 * 1024, 'x');  // 2MB > 1.5MB 上限
  REQUIRE_THROWS_AS(writer->write_record(r), std::length_error);

  fs::remove_all(temp_dir);
}

// Case 4: Training 模式 + agent_id 为空 → throw
TEST_CASE("training_mode_empty_agent_id_throws", "[distillation_writer][phase1]") {
  fs::path temp_dir = make_temp_dir("distill_empty_agent");
  IDistillationWriterPtr writer = IDistillationWriter::make_file_writer(temp_dir, "");

  DistillationRecord r = make_test_record("", CaptureMode::Training);
  REQUIRE_THROWS_AS(writer->write_record(r), std::invalid_argument);

  fs::remove_all(temp_dir);
}

// Case 5: Training 模式 + agent_id 非空 → PASS（成功写入）
TEST_CASE("training_mode_valid_agent_id_succeeds", "[distillation_writer][phase1]") {
  fs::path temp_dir = make_temp_dir("distill_training");
  // 路径包含 train 以满足"路径必含 train|distill" 隐含规则
  fs::path train_dir = temp_dir / "train_data";
  fs::create_directories(train_dir);

  IDistillationWriterPtr writer = IDistillationWriter::make_file_writer(train_dir, "training_agent");
  REQUIRE(writer != nullptr);

  DistillationRecord r = make_test_record("training_agent", CaptureMode::Training);
  writer->write_record(r);
  writer->close();
  // 不抛异常即视为通过

  fs::remove_all(temp_dir);
}

// Case 6: write_record + write_record 序列号递增
TEST_CASE("file_writer_sequence_increments", "[distillation_writer][phase1]") {
  fs::path temp_dir = make_temp_dir("distill_seq");
  IDistillationWriterPtr writer = IDistillationWriter::make_file_writer(temp_dir, "seq_agent");

  DistillationRecord r1 = make_test_record("seq_agent", CaptureMode::Online);
  DistillationRecord r2 = make_test_record("seq_agent", CaptureMode::Online);
  writer->write_record(r1);
  writer->write_record(r2);
  writer->close();

  // 验证有 2 个不同 seq 文件（filename 含序列号）
  int jsonl_count = 0;
  for (const auto& entry : fs::directory_iterator(temp_dir)) {
    if (entry.path().extension() == ".jsonl") jsonl_count++;
  }
  REQUIRE(jsonl_count == 2);

  fs::remove_all(temp_dir);
}
```

### Step 3: T1.2 Verify fail（必须 cmake + make）

```bash
cd /workspace/project/HydraForge/build
cmake .. -DAGENTICDSL_BUILD_TESTS=ON 2>&1 | tail -5
make test_distillation_writer 2>&1 | tee /tmp/build_phase1.log | tail -20

# 验证（必须 grep 确认编译错误）
grep -E "file_writer\.h.*file not found|file_writer\.cpp.*file not found" /tmp/build_phase1.log
# 预期：grep 返回非空
```

### Step 4: T1.3 Implement event_log_config.h（BREAKING 迁移）

修改 `src/core/types/event_log_config.h`：

```cpp
// src/core/types/event_log_config.h
// 功能描述: EventLog 配置 — BREAKING 迁移（bool capture_prompt_bytes → CaptureMode capture_mode）
// 依据: ADR-0080 v1.2 D10.v1.2.1 + openspec/changes/capture-mode-and-distillation-writer-v1/spec.md
//       Requirements: BREAKING 字段迁移彻底 + 5 消费者更新 + EventLogConfig 新字段

#pragma once

#include "agenticdsl/types/capture_mode.h"

#include <chrono>
#include <cstdint>
#include <string>

namespace agenticdsl {

struct EventLogConfig {
  CaptureMode capture_mode = CaptureMode::Off;  // ✅ Phase 1 迁移（替换原 bool capture_prompt_bytes = false）

  // 既有字段（保留不动）
  std::string log_file_path = "events.jsonl";
  std::size_t max_file_size_bytes = 100 * 1024 * 1024;
  std::chrono::milliseconds flush_interval{100};
  std::uint32_t max_retries = 3;

  // ✅ Phase 1 新增 helper
  bool effective_capture_enabled() const {
    return capture_mode != CaptureMode::Off;
  }
};

}  // namespace agenticdsl
```

### Step 5: T1.4 迁移 5 消费者

#### 5.1 `src/core/engine.h` 第 192 行

**修改前**:
```cpp
DSLEngine(std::unique_ptr<ILLMProvider> provider,
          /* ... 其他参数 ... */
          bool capture_prompt_bytes = false,   // ❌ 旧 API
          /* ... */);
```

**修改后**:
```cpp
DSLEngine(std::unique_ptr<ILLMProvider> provider,
          /* ... 其他参数 ... */
          CaptureMode capture_mode = CaptureMode::Off,  // ✅ Phase 1 新 API（替换 bool）
          /* ... */);
```

注意：`capture_mode` 默认值用 `CaptureMode::Off`，对应原 `false` 语义。

#### 5.2 `src/core/engine.cpp` 第 198 行 / 第 217 行

**修改前**:
```cpp
DSLEngine::DSLEngine(..., bool capture_prompt_bytes, ...) {
  // ...
  EventLogConfig cfg;
  cfg.capture_prompt_bytes = capture_prompt_bytes;  // ❌ 旧
  // ...
}
```

**修改后**:
```cpp
DSLEngine::DSLEngine(..., CaptureMode capture_mode, ...) {
  // ...
  EventLogConfig cfg;
  cfg.capture_mode = capture_mode;  // ✅ Phase 1 新
  // ...
}
```

#### 5.3 `src/common/llm/tracing_decorator.h` 第 41/42/57 行

**修改前**:
```cpp
class TracingDecorator : public ILLMProviderDecorator {
 public:
  // ...
  void set_capture_prompt_bytes(bool enabled) { capture_prompt_bytes_ = enabled; }
  bool is_capture_prompt_bytes() const { return capture_prompt_bytes_; }
 private:
  bool capture_prompt_bytes_ = false;
};
```

**修改后**:
```cpp
class TracingDecorator : public ILLMProviderDecorator {
 public:
  // ...
  void set_capture_mode(CaptureMode mode) { capture_mode_ = mode; }  // ✅ 新
  CaptureMode capture_mode() const { return capture_mode_; }        // ✅ 新
 private:
  CaptureMode capture_mode_ = CaptureMode::Off;  // ✅ 新
};
```

#### 5.4 `src/common/llm/tracing_decorator.cpp` 第 62/88 行

**修改前**:
```cpp
if (capture_prompt_bytes_) {  // ❌ 旧
  // ...
}
if (capture_prompt_bytes_ && r.text.size() <= 1024 * 1024) {  // ❌ 旧
  // ...
}
```

**修改后**:
```cpp
if (capture_mode_ != CaptureMode::Off) {  // ✅ 新（语义等价原 capture_prompt_bytes_=true）
  // ...
}
if (capture_mode_ == CaptureMode::Online && r.text.size() <= 1024 * 1024) {  // ✅ 新（Online 模式才捕获）
  // ...
}
```

### Step 6: T1.5 验证 BREAKING 迁移彻底

```bash
grep -rn "capture_prompt_bytes" /workspace/project/HydraForge/src /workspace/project/HydraForge/include /workspace/project/HydraForge/examples
# 预期：0 命中
```

### Step 7: T1.6 event_log.cpp 启动校验 + 降级 + 审计事件

修改 `src/core/event_log.cpp` 新增：

```cpp
// src/core/event_log.cpp（追加代码段，不修改既有函数）
#include "agenticdsl/types/capture_mode.h"
#include "agenticdsl/contract/iinteraction_bus.h"

namespace {

// Training 三重保护校验
void validate_training_mode(const EventLogConfig& cfg, const std::string& output_path) {
  if (cfg.capture_mode != CaptureMode::Training) return;

  // 保护 #1: agent_id 必须非空（隐含 — 由 make_file_writer 校验，这里仅路径检查）
  // 保护 #2: 路径必须包含 "train" 或 "distill" 子串
  if (output_path.find("train") == std::string::npos &&
      output_path.find("distill") == std::string::npos) {
    throw std::invalid_argument(
        "EventLog Training mode requires output path containing 'train' or 'distill'");
  }

  // 保护 #3: WARNING 记录（v1.1 emit WARNING 事件）
  std::cerr << "[WARNING] EventLogConfig in Training mode — PII capture risk. "
            << "Ensure agent_id is set and output path: " << output_path << std::endl;
}

// Online → Training 降级检测
bool detect_online_to_training_downgrade(const EventLogConfig& new_cfg,
                                          CaptureMode previous_mode) {
  return previous_mode == CaptureMode::Online &&
         new_cfg.capture_mode == CaptureMode::Training;
}

}  // anonymous namespace

// 在 EventLogWriter 构造中调用（修改既有构造）
EventLogWriter::EventLogWriter(EventLogConfig config,
                                std::shared_ptr<IInteractionBus> bus)
    : config_(std::move(config)),
      bus_(std::move(bus)) {
  validate_training_mode(config_, config_.log_file_path);

  // Online → Training 降级检测 + 审计事件发射
  static CaptureMode s_previous_mode = CaptureMode::Off;  // V1: 全局共享，V2: per-instance
  if (detect_online_to_training_downgrade(config_, s_previous_mode)) {
    if (bus_) {
      // 构造 BusEvent + emit event_log.capture_mode_downgrade
      // 详细 emit 逻辑参考 ADR-0068 §决策 2 EventBuilder API
    }
  }
  s_previous_mode = config_.capture_mode;
}
```

### Step 8: T1.7 实施 FileDistillationWriter V1

#### 8.1 新建 `src/modules/distillation/file_writer.h`

```cpp
// src/modules/distillation/file_writer.h
// 功能描述: FileDistillationWriter V1 — IDistillationWriter 默认实现
// 依据: ADR-0061-13 §决策 3 + openspec/changes/capture-mode-and-distillation-writer-v1/spec.md
//       Requirements: IDistillationWriter FileDistillationWriter 实现 + Training 三重保护 + ≤1.5MB 硬约束
// 关键不变量:
//   - 3 虚函数对齐 ADR-0061-13 决策 3：write_record + close + finalize(meta)
//   - 1 静态工厂 make_file_writer（实现于 .cpp）
//   - ≤1.5MB 硬上限（input + output 总大小）
//   - Training 模式 + agent_id 空 → throw invalid_argument
//   - 命名：<agent_id>_<seq>.distill.v1.jsonl + .meta.json

#pragma once

#include "agenticdsl/contract/idistillation_writer.h"

#include <atomic>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <string>

namespace agenticdsl {

class FileDistillationWriter : public IDistillationWriter {
 public:
  // 工厂函数实现（Phase 1）
  static std::unique_ptr<IDistillationWriter> make_file_writer(
      const std::filesystem::path& output_dir,
      const std::string& agent_id);

  FileDistillationWriter(const std::filesystem::path& output_dir,
                          std::string agent_id);
  ~FileDistillationWriter() override = default;

  void write_record(const DistillationRecord& record) override;
  void close() override;
  void finalize(const DistillationMetadata& meta) override;

 private:
  std::filesystem::path output_dir_;
  std::string agent_id_;
  std::ofstream jsonl_stream_;
  std::atomic<std::uint64_t> seq_{0};  // 文件序列号
  std::string current_filename_;       // 当前 .jsonl 文件名
  std::mutex mu_;                       // write_record 互斥（V1 单线程假定，多线程需外部同步）
  bool closed_ = false;

  // ≤1.5MB 硬上限
  static constexpr std::size_t kMaxRecordSizeBytes = 1.5 * 1024 * 1024;

  void validate_training_protection(const DistillationRecord& record);
  std::string next_filename() const;
  void open_new_jsonl();
};

}  // namespace agenticdsl
```

#### 8.2 新建 `src/modules/distillation/file_writer.cpp`

```cpp
// src/modules/distillation/file_writer.cpp
#include "file_writer.h"

#include <chrono>
#include <iomanip>
#include <sstream>
#include <stdexcept>

namespace agenticdsl {

// 工厂实现
std::unique_ptr<IDistillationWriter> IDistillationWriter::make_file_writer(
    const std::filesystem::path& output_dir,
    const std::string& agent_id) {
  return std::make_unique<FileDistillationWriter>(output_dir, agent_id);
}

FileDistillationWriter::FileDistillationWriter(
    const std::filesystem::path& output_dir,
    std::string agent_id)
    : output_dir_(output_dir), agent_id_(std::move(agent_id)) {
  std::filesystem::create_directories(output_dir_);
  open_new_jsonl();
}

void FileDistillationWriter::validate_training_protection(
    const DistillationRecord& record) {
  if (record.capture_mode != CaptureMode::Training) return;

  // 三重保护 #1: agent_id 非空
  if (record.agent_id.empty()) {
    throw std::invalid_argument(
        "FileDistillationWriter: Training mode requires non-empty agent_id");
  }

  // 三重保护 #2: 路径含 train|distill（构造时已通过 validate_training_mode）
  // 三重保护 #3: WARNING（构造时已 stderr 输出）

  // record 总大小 ≤ 1.5MB
  const std::size_t total = record.input.size() + record.output.size();
  if (total > kMaxRecordSizeBytes) {
    throw std::length_error(
        "FileDistillationWriter: record size " + std::to_string(total) +
        " bytes exceeds 1.5MB hard limit");
  }
}

std::string FileDistillationWriter::next_filename() const {
  std::ostringstream oss;
  oss << agent_id_ << "_" << std::setw(6) << std::setfill('0') << seq_.load() << ".distill.v1.jsonl";
  return oss.str();
}

void FileDistillationWriter::open_new_jsonl() {
  current_filename_ = next_filename();
  jsonl_stream_.open(output_dir_ / current_filename_,
                      std::ios::out | std::ios::app | std::ios::binary);
  if (!jsonl_stream_) {
    throw std::runtime_error(
        "FileDistillationWriter: failed to open " + current_filename_);
  }
}

void FileDistillationWriter::write_record(const DistillationRecord& record) {
  std::lock_guard<std::mutex> lock(mu_);

  validate_training_protection(record);

  // 序列化为 JSON 行（v1: 简单 nlohmann::json dump）
  nlohmann::json j;
  j["input"] = record.input;
  j["output"] = record.output;
  j["steps"] = nlohmann::json::array();
  for (const auto& s : record.steps) {
    j["steps"].push_back({{"thought", s.thought}, {"tool_name", s.tool_name},
                           {"tool_args", s.tool_args}, {"observation", s.observation},
                           {"latency_ms", s.latency_ms}});
  }
  j["reward"] = {{"scalar", record.reward.scalar},
                 {"confidence", record.reward.confidence},
                 {"quality", static_cast<int>(record.reward.quality)}};
  j["trace_id"] = record.trace_id;
  j["source_event"] = record.source_event;
  j["agent_id"] = record.agent_id;
  j["teacher_version"] = record.teacher_version;
  j["generation_timestamp_ms"] = record.generation_timestamp_ms;
  j["capture_mode"] = agenticdsl::to_string(record.capture_mode);
  j["convergence"] = {{"agent_id", record.convergence.agent_id},
                       {"teacher_version", record.convergence.teacher_version},
                       {"task_id", record.convergence.task_id},
                       {"trace_id", record.convergence.trace_id}};

  jsonl_stream_ << j.dump() << "\n";
  jsonl_stream_.flush();  // V1: 同步 fsync（性能 v2 优化）

  // 序列号递增（每 record 一个文件，便于切片）
  ++seq_;
  // V1: 不开新文件（简单实现，所有 record 写入同一 .jsonl）
  // V2: 可按 size threshold rotation
}

void FileDistillationWriter::close() {
  std::lock_guard<std::mutex> lock(mu_);
  if (jsonl_stream_.is_open()) {
    jsonl_stream_.flush();
    jsonl_stream_.close();
  }
  closed_ = true;
}

void FileDistillationWriter::finalize(const DistillationMetadata& meta) {
  std::lock_guard<std::mutex> lock(mu_);
  // 确保 close 已调用
  if (!closed_) close();

  // 写 meta.json
  nlohmann::json j;
  j["version"] = meta.version;
  j["total_examples"] = meta.total_examples;
  j["dataset_hash"] = meta.dataset_hash;
  j["generation_config"] = meta.generation_config;

  std::filesystem::path meta_path = output_dir_ /
      (agent_id_ + "_" + std::to_string(seq_.load()) + ".distill.v1.meta.json");
  std::ofstream ofs(meta_path);
  if (!ofs) {
    throw std::runtime_error(
        "FileDistillationWriter: failed to write meta.json");
  }
  ofs << j.dump(2);
  ofs.close();
}

}  // namespace agenticdsl
```

#### 8.3 新建 `src/modules/distillation/CMakeLists.txt`

```cmake
# src/modules/distillation/CMakeLists.txt
add_library(agenticdsl_distillation
    file_writer.cpp
)
target_link_libraries(agenticdsl_distillation PUBLIC
    agenticdsl_core
    agenticdsl_contract
    agenticdsl_policy
    agenticdsl_llm
)
target_include_directories(agenticdsl_distillation PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${PROJECT_SOURCE_DIR}/include
)
```

### Step 9: T1.8 Verify pass

```bash
cd /workspace/project/HydraForge/build
cmake .. -DAGENTICDSL_BUILD_TESTS=ON 2>&1 | tail -5
make test_distillation_writer 2>&1 | tail -10
# 预期：6 cases / ≥15 assertions PASS

ctest -R distillation_writer --output-on-failure
# 预期：全绿

# 全量 ctest
ctest --output-on-failure 2>&1 | tail -10
# 预期：与 baseline 持平（动态计数）
```

### Step 10: T1.9 Commit

```bash
cd /workspace/project/HydraForge

# Step 10.1: commit 前验证
git diff HEAD -- include/agenticdsl/contract/ | head -5
# 预期：idistillation_writer.h 在 Phase 0 commit 过，是已 tracked 文件，diff 应为 0 行
# ✅ Oracle B3: contract/ 下其他 19 个既有文件 0 行 diff

# Step 10.2: git add 12 个文件
git add tests/test_distillation_writer.cpp \
        src/core/types/event_log_config.h \
        src/core/engine.h \
        src/core/engine.cpp \
        src/common/llm/tracing_decorator.h \
        src/common/llm/tracing_decorator.cpp \
        src/core/event_log.cpp \
        src/modules/distillation/file_writer.h \
        src/modules/distillation/file_writer.cpp \
        src/modules/distillation/CMakeLists.txt \
        CMakeLists.txt

# Step 10.3: Commit
git commit -m "feat(distillation): EventLogConfig BREAKING 迁移 + FileDistillationWriter V1 (T1)

Phase 1 实施: capture-mode-and-distillation-writer-v1

BREAKING 迁移:
- src/core/types/event_log_config.h: bool capture_prompt_bytes → CaptureMode capture_mode
- src/core/engine.h:192 公开 API 签名: bool → CaptureMode (default CaptureMode::Off)
- src/core/engine.cpp:198,217: 构造默认值迁移
- src/common/llm/tracing_decorator.h: 字段类型 bool → CaptureMode
- src/common/llm/tracing_decorator.cpp: 捕获开关判断 (CaptureMode != Off)

新增 FileDistillationWriter V1 (src/modules/distillation/):
- write_record + close + finalize(meta) 3 虚函数 + make_file_writer 工厂（对齐 ADR-0061-13 §决策 3）
- Training 三重保护: agent_id 非空 + 路径含 train|distill + WARNING
- ≤1.5MB 硬上限（超限 throw std::length_error）
- 命名: <agent_id>_<seq>.distill.v1.jsonl + .meta.json
- 文件锁: std::mutex（V1 单线程假定，多线程需外部同步）

新增审计事件（src/core/event_log.cpp）:
- Training 模式启动校验 + WARNING stderr 输出
- Online→Training 降级检测 + emit event_log.capture_mode_downgrade 主题（ADR-0068 §决策 2）

新增 src/modules/distillation/CMakeLists.txt + 根 CMakeLists.txt add_subdirectory。

测试:
- tests/test_distillation_writer.cpp (6 cases / ≥15 assertions)
  - file_writer_write_record_creates_jsonl
  - file_writer_finalize_writes_meta_json
  - file_writer_size_limit_throws_length_error
  - training_mode_empty_agent_id_throws
  - training_mode_valid_agent_id_succeeds
  - file_writer_sequence_increments

验证:
- ctest -R distillation_writer: 6 cases / ≥15 assertions PASS
- grep 'capture_prompt_bytes' src/ include/ examples/ = 0 命中
- 全量 ctest: 0 新增回归（与 baseline 持平，动态计数）
- Oracle B3: include/agenticdsl/contract/ 下 19 个既有文件零 diff
  (idistillation_writer.h 在 Phase 0 ship，本 Phase 1 未修改)

修正（D 系列，应用 Phase 0 教训）:
- D1: 全部 include 路径已 grep 验证实际存在
- D2: TDD Verify-fail 必须 cmake + make
- D3: FileDistillationWriter 3 虚函数 + 工厂严格对齐 ADR-0061-13
- D4: test_distillation_writer.cpp include FileDistillationWriter 实现
- D5: commit 前/后双向验证
- D8: MUST NOT 完整 19 个 contract 文件清单

后续 Phase 2: CLI flag --allow-training-capture + TrajectoryIR bridge + payload redact

[Oracle B3: contract/ 下 19 个既有文件零 diff]"
```

### Step 11: commit 后验证

```bash
git diff HEAD^ HEAD -- include/agenticdsl/contract/ | head -10
# 预期：仅显示 Phase 0 已 ship 的 idistillation_writer.h 修改（应为空，因为 Phase 1 不修改 contract/）
# 其他 19 个 contract 文件 0 行 diff

git diff HEAD^ HEAD -- src/core/engine.h | head -10
# 预期：1 行修改（capture_prompt_bytes → capture_mode）

grep -rn "capture_prompt_bytes" /workspace/project/HydraForge/src /workspace/project/HydraForge/include /workspace/project/HydraForge/examples
# 预期：0 命中
```

---

## MUST NOT DO

### ❌ 禁止修改 contract/ 下 19 个既有文件（Oracle B3 + Phase 1 边界）

```
include/agenticdsl/contract/
├── bus_event.h                     ❌ 禁止
├── causal_clock.h                  ❌ 禁止
├── evaluation_events.h             ❌ 禁止
├── event_builder.h                 ❌ 禁止
├── i_llm_provider_decorator.h      ❌ 禁止
├── iagent_composition.h            ❌ 禁止
├── iagent_hook_registry.h          ❌ 禁止
├── iagent_registry.h               ❌ 禁止
├── icommand_registry.h             ❌ 禁止
├── ievaluator.h                    ❌ 禁止
├── iinteraction_bus.h              ❌ 禁止
├── imutation_governance.h          ❌ 禁止
├── inmemory_bus.h                  ❌ 禁止
├── iparser.h                       ❌ 禁止
├── iprovider_factory.h             ❌ 禁止
├── ischeduler.h                    ❌ 禁止
├── iskill_compiler.h               ❌ 禁止
├── itool_hook_registry.h           ❌ 禁止
├── itool_registry.h                ❌ 禁止
└── test_double_registry.h          ❌ 禁止

✅ idistillation_writer.h（Phase 0 新增，本 Phase 1 不修改）
```

注意：Phase 1 修改的是 `src/` 而非 `include/agenticdsl/contract/`！允许修改 src/ 下文件（event_log_config.h, engine.h/cpp, tracing_decorator.h/cpp, event_log.cpp）。

### ❌ 禁止的其他操作

- ❌ 修改 reward_signal.h 路径（D1 修正，必须 types/ 而非 contract/）
- ❌ 实施 CLI flag `--allow-training-capture`（Phase 2 内容）
- ❌ 实施 TrajectoryIR bridge（Phase 2 内容）
- ❌ 实施 payload redact（Phase 2 内容，复用 T21 hash_prompt()）
- ❌ 使用 2 虚函数签名（必须 3 虚函数 + 工厂，D3 修正）
- ❌ 使用 `git commit --no-verify`（遵守 governance hooks）
- ❌ 使用 `git push` / `git commit --amend`
- ❌ 硬编码 ctest 数字
- ❌ 跳过 TDD 5 步（必须 make 触发 fail，验证编译错误）

---

## CONTEXT

### 工作目录
- 项目根: `/workspace/project/HydraForge`
- OpenSpec change: `openspec/changes/capture-mode-and-distillation-writer-v1/`
- Phase 0 ship: commit `11d3515` (CaptureMode + DistillationRecord + IDistillationWriter 抽象)
- Phase 0 修正版 plan: `.rddf/plans/capture-mode-and-distillation-writer-v1-phase-0-CORRECTED.md`

### 关键约束
- Oracle B3: contract/ 下 19 个既有文件零 diff
- BREAKING 迁移彻底: `grep "capture_prompt_bytes" src/ include/ examples/` = 0
- TDD 5 步纪律（必须 cmake + make）
- Atomic commit
- adr_lint exit 0 + ADR-TRACKING-01 WARNING 数持平

### 已 ship 关键参考
- `include/agenticdsl/types/capture_mode.h` (Phase 0 ship)
- `include/agenticdsl/types/distillation_record.h` (Phase 0 ship)
- `include/agenticdsl/contract/idistillation_writer.h` (Phase 0 ship, 含 DistillationMetadata struct)
- `include/agenticdsl/contract/reward_signal.h` (ADR-0083 已 ship, DistillationRecord.reward 复用源头)
- `include/agenticdsl/contract/iinteraction_bus.h` (审计事件发射目标)

### Phase 0 修正经验（应用）
- D1 路径：grep 验证实际文件位置
- D2 TDD：cmake + make
- D3 签名：3 虚函数 + 工厂严格对齐 ADR-0061-13
- D4 零编译覆盖：测试 include 全部实现
- D5 commit 验证：commit 前 + 后双向
- D8 contract 清单：完整 19 文件
- D9 测试引用：实际存在文件

---

## 期望结果（给用户）

1. **commit hash**（例如 `a2b3c4d`）
2. **ctest 输出**: `6 cases / ≥15 assertions PASS`
3. **diff stats**: `11 files changed, ~400 insertions, ~20 deletions`（BREAKING 迁移 + 新增 file_writer）
4. **Oracle B3 验证**: contract/ 下 19 个既有文件零 diff
5. **BREAKING 验证**: `grep "capture_prompt_bytes" = 0 命中`
6. **全量 ctest**: 0 新增回归
7. **后续**: 立即启动 Phase 2 deep agent（CLI flag + TrajectoryIR bridge + payload redact）

---

**Plan 状态**: ✅ Ready
**创建者**: Sisyphus @ 2026-08-29（基于 Phase 0 ship 后的下一步）
**适用场景**: Phase 0 ship 后（cooling-off 已结束），deep agent 委派实施 Phase 1
**关键约束**: Oracle B3 + 修正版经验 D1-D9 全部应用
