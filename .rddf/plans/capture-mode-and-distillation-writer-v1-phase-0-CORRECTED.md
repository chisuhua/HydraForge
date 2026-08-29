# Phase 0 实施 Prompt: capture-mode-and-distillation-writer-v1（Oracle/Metis 审查后修正版）

> **创建日期**: 2026-08-29（修正版，整合 Oracle session `ses_fb33c1b2affe6CJtJph7iMdW22` + Metis session `ses_fb33c1b2affe6CJtJph7iMdW22` 9 项修正决策）
> **修正依据**: Oracle (架构风险) + Metis (AI 失败点) 双审查
> **冷却期**: 2026-08-30 18:00 后启动
> **目标**: Phase 0 抽象层 ship（3 个新头文件 + 1 个新测试文件 + 1 atomic commit）
> **估时**: 0.5 sprint（1-2 working days）

---

## ⚠️ 修正版关键变更（agent 必须严格遵守）

本 prompt 是对原始 plan（`.rddf/plans/capture-mode-and-distillation-writer-v1-phase-0.md`）的**9 项修正**。**agent 必须使用本修正版的所有字段**，不得回退到原始 plan 的错误字段：

| # | 原始 plan 错误 | 修正版（本 prompt）| 决策编号 |
|---|---------------|-------------------|---------|
| 1 | `#include "agenticdsl/contract/reward_signal.h"` | `#include "agenticdsl/types/reward_signal.h"` | D1 |
| 2 | TDD Step 3 只跑 `cmake ..` | `cmake .. && make test_capture_mode` | D2 |
| 3 | `IDistillationWriter::write()` + `finalize()` (2 虚函数) | `write_record()` + `close()` + `finalize(meta)` + `make_file_writer` 工厂（3 虚函数 + 1 静态工厂）| D3 |
| 4 | test_capture_mode.cpp 仅 include `capture_mode.h` | include 全部 3 个新头文件 + `static_assert` | D4 |
| 5 | 验证命令"git diff HEAD -- contract/ = 0 行" | commit 前 + commit 后两个命令 | D5 |
| 6 | DistillationRecord 无 input/output | 含 input/output/reward(RewardSignal)/trace_id/source_event | D6 |
| 7 | `double reward` + `double confidence`（丢 Quality）| `RewardSignal reward;`（真嵌入完整结构体）| D7 |
| 8 | MUST NOT 列 9 个 contract 文件 | 列完整 20 个 contract 文件 | D8 |
| 9 | 引用 `tests/test_reward_signal.cpp`（不存在）| 引用 `tests/test_evaluator.cpp`（实际存在）| D9 |

---

## TASK

为 OpenSpec change `capture-mode-and-distillation-writer-v1` 实施 Phase 0 抽象层，包含 **3 个新头文件 + 1 个新测试文件 + 1 个 atomic commit**。**严格 TDD 5 步**：Write failing test → Verify fail → Implement → Verify pass → Commit。

---

## EXPECTED OUTCOME

### 交付物清单

| 文件 | 行数（估） | 内容 |
|------|-----------|------|
| `include/agenticdsl/types/capture_mode.h` | ~60 行 | enum class + to_string/parse helpers + kDefaultCaptureMode |
| `include/agenticdsl/types/distillation_record.h` | ~110 行 | DistillationRecord + StepRecord + ConvergenceMeta（对齐 ADR-0061-13）|
| `include/agenticdsl/contract/idistillation_writer.h` | ~80 行 | 3 虚函数 + 1 静态工厂（对齐 ADR-0061-13）|
| `tests/test_capture_mode.cpp` | ~100 行 | 3 cases + 静态断言 + 全部 3 头文件 include（防零编译覆盖）|

### 验证标准

1. **ctest**: `ctest -R capture_mode --output-on-failure` 全绿（≥3 cases / ≥13 assertions PASS）
2. **ctest 全量**: 0 新增回归（与 baseline 持平）
3. **零契约修改**: `include/agenticdsl/contract/` 下 **20 个既有头文件**零 diff（仅允许新增 `idistillation_writer.h`）
4. **零 engine 修改**: `src/core/engine.h` 零 diff
5. **零 event_log_config 修改**: `src/core/types/event_log_config.h` 零 diff（Phase 1 才改）
6. **commit 验证**:
   - commit 前：`git diff HEAD -- include/agenticdsl/contract/` 应仅显示 `idistillation_writer.h` 未 tracked 状态（既有 20 文件 0 行 diff）
   - commit 后：`git diff HEAD^ HEAD -- include/agenticdsl/contract/` 应仅含 `idistillation_writer.h` 新增行，其余 20 文件 0 行 diff
7. **ADR-TRACKING-01**: `python3 tools/adr_lint.py` exit 0，WARNING 数与 baseline 持平（不增不减）
8. **L1 契约层独立**: `grep -rn '#include "src/' include/agenticdsl/contract/idistillation_writer.h include/agenticdsl/types/distillation_record.h` = 0 命中

---

## REQUIRED TOOLS

- `read` / `write` / `edit`（读现有文件后再 Write/Edit）
- `bash`（mkdir + grep + ctest + git commit + git diff）
- `grep` / `glob`（查找现有 ADR-0083 reward_signal.h + ievaluator.h + 既有契约风格）

---

## MUST DO（严格 TDD 5 步 + 9 项修正）

### Step 1: 准备工作（Read 现有代码 + spec）

```bash
# 必读文件（理解现有类型 + 契约风格 + 已 ship 的 RewardSignal）
read /workspace/project/HydraForge/include/agenticdsl/contract/reward_signal.h      # ADR-0083 已 ship，DistillationRecord.reward 复用源头
read /workspace/project/HydraForge/include/agenticdsl/contract/ievaluator.h          # 纯虚接口契约层模式参考（含 9 override + factory）
read /workspace/project/HydraForge/include/agenticdsl/types/layered_context.h       # 值类型风格参考
read /workspace/project/HydraForge/tests/test_evaluator.cpp                        # ✅ 实际存在的测试文件（修正 D9，原 plan 引用 test_reward_signal.cpp 不存在）
read /workspace/project/HydraForge/openspec/changes/capture-mode-and-distillation-writer-v1/proposal.md
read /workspace/project/HydraForge/openspec/changes/capture-mode-and-distillation-writer-v1/specs/capture-mode-and-distillation-writer-v1/spec.md
```

**关键参考（ADR-0061-13 决策 2 + 3）**：

```bash
read /workspace/project/HydraForge/docs/adr/skill/adr-0061-13-distillation-output-format.md
# §决策 2 (lines 52-83): DistillationRecord + StepRecord 字段全集
# §决策 3 (lines 86-109): IDistillationWriter 3 虚函数 + factory 签名
```

### Step 2: T0.1 Write failing test（修正 D4 + D9）

创建 `tests/test_capture_mode.cpp`：

```cpp
// tests/test_capture_mode.cpp
// 功能描述: CaptureMode 三态枚举单元测试 + 零编译覆盖（蒸馏契约全头文件）
// 依据: openspec/changes/capture-mode-and-distillation-writer-v1/spec.md
//        Requirements: CaptureMode 三态 + IDistillationWriter 纯虚契约
// 修正注: 本测试文件 include 全部 3 个新头文件，防止头文件语法错误静默 ship（Oracle + Metis 一致发现）

#include "catch_amalgamated.hpp"

// 全部 3 个新头文件（关键：防零编译覆盖盲区）
#include "agenticdsl/types/capture_mode.h"
#include "agenticdsl/types/distillation_record.h"
#include "agenticdsl/contract/idistillation_writer.h"

using agenticdsl::CaptureMode;
using agenticdsl::to_string;
using agenticdsl::parse_capture_mode;
using agenticdsl::kDefaultCaptureMode;
using agenticdsl::DistillationRecord;
using agenticdsl::StepRecord;
using agenticdsl::ConvergenceMeta;
using agenticdsl::IDistillationWriter;
using agenticdsl::IDistillationWriterPtr;

TEST_CASE("enum_serialization_round_trip", "[capture_mode][phase0]") {
  // 正向：to_string 3 值
  REQUIRE(to_string(CaptureMode::Off) == "Off");
  REQUIRE(to_string(CaptureMode::Online) == "Online");
  REQUIRE(to_string(CaptureMode::Training) == "Training");

  // 反向：parse 3 值
  REQUIRE(parse_capture_mode("Off") == CaptureMode::Off);
  REQUIRE(parse_capture_mode("Online") == CaptureMode::Online);
  REQUIRE(parse_capture_mode("Training") == CaptureMode::Training);

  // 合成 round-trip（spec Scenario "to_string/parse round-trip"）
  for (auto m : {CaptureMode::Off, CaptureMode::Online, CaptureMode::Training}) {
    REQUIRE(parse_capture_mode(to_string(m)) == m);
  }
}

TEST_CASE("parse_invalid_string_throws", "[capture_mode][phase0]") {
  REQUIRE_THROWS_AS(parse_capture_mode("invalid"), std::invalid_argument);
  REQUIRE_THROWS_AS(parse_capture_mode(""), std::invalid_argument);
  REQUIRE_THROWS_AS(parse_capture_mode("OFF"), std::invalid_argument);  // 大小写敏感
  REQUIRE_THROWS_AS(parse_capture_mode("off"), std::invalid_argument);
}

TEST_CASE("default_value_and_static_asserts", "[capture_mode][phase0]") {
  // 默认值（修正：测试 kDefaultCaptureMode 常量本身，而非 EventLogConfig 字段，
  // 因为 Phase 0 不 touch event_log_config.h —— 该 Scenario 在 Phase 1 验证）
  REQUIRE(kDefaultCaptureMode == CaptureMode::Off);
  REQUIRE(static_cast<uint8_t>(CaptureMode::Off) == 0);
  REQUIRE(static_cast<uint8_t>(CaptureMode::Online) == 1);
  REQUIRE(static_cast<uint8_t>(CaptureMode::Training) == 2);
}

// 编译期断言（plan 风险 1 提及但 Step 2 测试未实现 —— 修正）
static_assert(sizeof(CaptureMode) == 1, "CaptureMode must be 1 byte (uint8_t)");
static_assert(CaptureMode::Off == static_cast<CaptureMode>(0), "Off=0 stable for JSONL header persistence");

// 零编译覆盖（修正 D4）：确保 distillation_record.h 和 idistillation_writer.h 在 Phase 0 真的被编译
TEST_CASE("phase0_headers_compile_smoke", "[capture_mode][phase0][headers]") {
  // DistillationRecord 默认构造可工作
  DistillationRecord default_record;
  REQUIRE(default_record.capture_mode == CaptureMode::Off);
  REQUIRE(default_record.input.empty());
  REQUIRE(default_record.output.empty());
  REQUIRE(default_record.steps.empty());

  // ConvergenceMeta 默认构造
  ConvergenceMeta default_meta;
  REQUIRE(default_meta.agent_id.empty());

  // IDistillationWriter 是抽象类（不能实例化，只能用 static_assert）
  static_assert(std::is_abstract_v<IDistillationWriter>,
                "IDistillationWriter must be abstract (3 pure virtual methods)");

  // 工厂函数签名存在（编译期验证）
  static_assert(std::is_same_v<decltype(&IDistillationWriter::make_file_writer),
                              std::unique_ptr<IDistillationWriter>(*)(
                                  const std::filesystem::path&,
                                  const std::string&)>);
}
```

### Step 3: T0.2 Verify fail（修正 D2：必须 make 触发）

```bash
cd /workspace/project/HydraForge/build
cmake .. -DAGENTICDSL_BUILD_TESTS=ON 2>&1 | tail -5
make test_capture_mode 2>&1 | tee /tmp/build_output.log | tail -20

# 验证（必须 grep 确认错误输出存在）
grep -E "capture_mode\.h.*file not found|distillation_record\.h.*file not found|idistillation_writer\.h.*file not found" /tmp/build_output.log
# 预期：grep 返回非空（编译失败触发了 file not found 错误）
```

**关键修正**：原 plan 只跑 `cmake ..`，但 cmake configure 阶段不检查 include 缺失。必须 `make` 才能看到 file not found 错误。

### Step 4: T0.3 Implement capture_mode.h（修正 D1 关联：路径对齐）

创建 `include/agenticdsl/types/capture_mode.h`：

```cpp
// include/agenticdsl/types/capture_mode.h
// 功能描述: CaptureMode 三态枚举（Off / Online / Training）
// 设计依据: docs/adr/adr-0080-v1-2-amendment-d10-decouple.md
//          + openspec/changes/capture-mode-and-distillation-writer-v1/spec.md
// 关键不变量:
//   - Off = 0 (生产路径, 零捕获开销)
//   - Online = 1 (pdk_chat_demo 默认, 实时观测无 PII 风险)
//   - Training = 2 (离线蒸馏, 三重保护 + WARNING)
//   - 强类型 enum class (禁止魔法值/字符串)
//   - uint8_t 底层类型稳定（JSONL header 持久化需求）

#pragma once

#include <cstdint>
#include <stdexcept>
#include <string>

namespace agenticdsl {

enum class CaptureMode : uint8_t {
  Off = 0,
  Online = 1,
  Training = 2,
};

constexpr CaptureMode kDefaultCaptureMode = CaptureMode::Off;

inline std::string to_string(CaptureMode m) {
  switch (m) {
    case CaptureMode::Off: return "Off";
    case CaptureMode::Online: return "Online";
    case CaptureMode::Training: return "Training";
  }
  throw std::invalid_argument("Unknown CaptureMode value");
}

inline CaptureMode parse_capture_mode(const std::string& s) {
  if (s == "Off") return CaptureMode::Off;
  if (s == "Online") return CaptureMode::Online;
  if (s == "Training") return CaptureMode::Training;
  throw std::invalid_argument(
      "Invalid CaptureMode string: '" + s +
      "' (expected 'Off', 'Online', or 'Training', case-sensitive)");
}

}  // namespace agenticdsl
```

### Step 5: T0.4 Implement distillation_record.h（修正 D1 + D6 + D7：真复用 RewardSignal）

创建 `include/agenticdsl/types/distillation_record.h`：

```cpp
// include/agenticdsl/types/distillation_record.h
// 功能描述: DistillationRecord / StepRecord / ConvergenceMeta 值类型
// 设计依据: docs/adr/skill/adr-0061-13-distillation-output-format.md §决策 2（字段全集）
//          + docs/adr/adr-0080-v1-2-amendment-d10-decouple.md（D10.v1.2.1 CaptureMode）
//          + docs/adr/adr-0083-evaluator-reward-contract.md（RewardSignal 已 ship）
//          + openspec/changes/capture-mode-and-distillation-writer-v1/spec.md
//
// 修正（D6 + D7）: 与 ADR-0061-13 §决策 2 完全对齐，含 input/output 字段；
//                  StepRecord.reward 真嵌入 RewardSignal struct（非 double 拍平）。
// 关键不变量:
//   - DistillationRecord 字段全集对齐 ADR-0061-13 §决策 2
//   - input + output ≤ 1.5MB（ADR 不变量 1，防内存爆炸）
//   - input ≤ 64KB / output ≤ 1MB（ADR-0061-13 字段约束）
//   - ≤ 20 步（V1 简化）
//   - trajectory_jsonl / policy_jsonl 是序列化产物（由 FileDistillationWriter 生成）
//   - ConvergenceMeta.agent_id 与 record.agent_id 含义不同（前者是 Training 三重保护 #1）

#pragma once

#include "agenticdsl/contract/reward_signal.h"   // ✅ 修正 D1: types/ 而非 contract/
#include "agenticdsl/types/capture_mode.h"

#include <chrono>
#include <cstdint>
#include <nlohmann/json.hpp>
#include <string>
#include <vector>

namespace agenticdsl {

// StepRecord: ReAct 步骤（对齐 ADR-0061-13 §决策 2）
struct StepRecord {
  std::string thought;           // ReAct thought (可选)
  std::string tool_name;         // 若 action 是 tool call
  nlohmann::json tool_args;
  std::string observation;
  std::uint64_t latency_ms = 0;
};

// ConvergenceMeta: 蒸馏会话元数据（Training 三重保护 #1 校验目标）
struct ConvergenceMeta {
  std::string agent_id;                                       // 三重保护 #1
  std::string teacher_version;
  std::string task_id;
  std::string trace_id;                                       // ADR-0080 v1.1 causal_time 对齐
  std::chrono::system_clock::time_point created_at;
};

// DistillationRecord: 蒸馏数据主记录（对齐 ADR-0061-13 §决策 2 字段全集）
struct DistillationRecord {
  // 输入（ADR-0080 D10.4 对齐）
  std::string input;            // 必须 ≤ 64KB
  std::string output;           // 必须 ≤ 1MB
  std::vector<StepRecord> steps;  // V1: ≤ 20 步

  // 评估信号（修正 D7：真嵌入 RewardSignal struct，而非 double 拍平）
  RewardSignal reward;          // ADR-0083 已 ship，含 quality/scalar/confidence

  // 元数据
  std::string trace_id;         // EventLog causal_time 引用
  std::string source_event;     // llm.request / llm.response event_id
  std::string agent_id;
  std::string teacher_version;  // 教师 Agent 版本 (e.g. "v1.0.0")
  std::uint64_t generation_timestamp_ms = 0;

  // CaptureMode（D10.v1.2.1 关联）
  CaptureMode capture_mode = CaptureMode::Off;

  // ConvergenceMeta（Training 模式必填）
  ConvergenceMeta convergence;
};

// V1 不变量（编译期断言）
static_assert(sizeof(CaptureMode) == 1, "CaptureMode must be 1 byte");

}  // namespace agenticdsl
```

### Step 6: T0.5 Implement idistillation_writer.h（修正 D3：3 虚函数 + 工厂对齐 ADR-0061-13）

创建 `include/agenticdsl/contract/idistillation_writer.h`：

```cpp
// include/agenticdsl/contract/idistillation_writer.h
// 功能描述: IDistillationWriter L1 契约层接口（3 虚函数 + 1 静态工厂）
// 设计依据: docs/adr/skill/adr-0061-13-distillation-output-format.md §决策 3（完整对齐）
//          + openspec/changes/capture-mode-and-distillation-writer-v1/spec.md
//
// 修正（D3）: 与 ADR-0061-13 §决策 3 完全对齐。
//   - 原 plan: 2 虚函数（write + finalize()）
//   - 修正版: 3 虚函数（write_record + close + finalize(meta)）+ 1 静态工厂（make_file_writer）
//   - 偏离决策: 无偏离（完全对齐 Approved ADR）
//   - 阶段边界: write_record + close 在 Phase 1 FileDistillationWriter 实现；
//                finalize(meta) 在 Phase 1 末尾实现（接收 meta.json 元数据）；
//                make_file_writer 工厂在 Phase 1 实现。
// 关键不变量:
//   - L1 契约层独立（不 #include src/ 任何头文件）
//   - 3 纯虚函数（write_record + close + finalize(meta)）+ 1 静态工厂
//   - 默认析构（virtual ~IDistillationWriter() = default）
//   - V1 FileDistillationWriter 假定单写线程；多线程写需外部同步（V2 扩展）

#pragma once

#include "agenticdsl/types/distillation_record.h"

#include <filesystem>
#include <memory>
#include <string>

namespace agenticdsl {

// V1 元数据（用于 finalize() 写 meta.json，ADR-0061-13 决策 1）
struct DistillationMetadata {
  std::string version;                 // 元数据 schema 版本
  std::uint64_t total_examples = 0;    // 蒸馏数据集大小
  std::string dataset_hash;            // SHA256 of entire dataset
  nlohmann::json generation_config;    // 训练配置（teacher_version, capture_mode 等）
};

class IDistillationWriter {
 public:
  virtual ~IDistillationWriter() = default;

  // 写入单条 record（V1: 同步落盘 + fsync）
  virtual void write_record(const DistillationRecord& record) = 0;

  // flush + 关闭（析构时自动调用）
  virtual void close() = 0;

  // 元数据（生成结束后写入 meta.json）
  virtual void finalize(const DistillationMetadata& meta) = 0;

  // 工厂函数（Phase 1 FileDistillationWriter 实现）
  static std::unique_ptr<IDistillationWriter> make_file_writer(
      const std::filesystem::path& output_dir,
      const std::string& agent_id);
};

using IDistillationWriterPtr = std::unique_ptr<IDistillationWriter>;

}  // namespace agenticdsl
```

### Step 7: T0.6 Verify pass

```bash
cd /workspace/project/HydraForge/build
cmake .. -DAGENTICDSL_BUILD_TESTS=ON 2>&1 | tail -5
# 预期：编译成功（零错误）

make test_capture_mode 2>&1 | tail -10
# 预期：编译成功（4 个 TEST_CASE 编译通过）

ctest -R capture_mode --output-on-failure
# 预期：3 cases / ≥13 assertions PASS（修正 D4 后含 phase0_headers_compile_smoke）

# 全量 ctest 验证零回归
ctest --output-on-failure 2>&1 | tail -10
# 预期：与 baseline 持平（动态计数，禁止硬编码数字）
```

### Step 8: T0.7 Commit + Oracle B3 验证（修正 D5：commit 前 + 后两个命令）

```bash
cd /workspace/project/HydraForge

# Step 8.1: commit 前验证（既有 20 contract 文件零 diff）
git diff HEAD -- include/agenticdsl/contract/ | head -5
# 预期：空输出（idistillation_writer.h 是新文件未 tracked）

# Step 8.2: git add 4 个新文件
git add include/agenticdsl/types/capture_mode.h \
        include/agenticdsl/types/distillation_record.h \
        include/agenticdsl/contract/idistillation_writer.h \
        tests/test_capture_mode.cpp

# Step 8.3: Commit（无 --no-verify，遵守项目 governance hooks）
git commit -m "feat(distillation): CaptureMode + DistillationRecord + IDistillationWriter contracts (T0)

Phase 0 抽象层: capture-mode-and-distillation-writer-v1
依据: ADR-0080 v1.2 + ADR-0061-13 合并 ship 启动
Oracle/Metis 双审查 9 项修正后版本（D1-D9）

新增:
- include/agenticdsl/types/capture_mode.h (enum class + to_string/parse + kDefaultCaptureMode)
- include/agenticdsl/types/distillation_record.h (对齐 ADR-0061-13 §决策 2 字段全集: input/output/StepRecord/RewardSignal/trace_id/source_event/agent_id/teacher_version/generation_timestamp_ms)
- include/agenticdsl/contract/idistillation_writer.h (对齐 ADR-0061-13 §决策 3: 3 虚函数 write_record/close/finalize(meta) + 静态工厂 make_file_writer)
- tests/test_capture_mode.cpp (3 cases + static_assert + 零编译覆盖 include 全部 3 个新头文件)

修正要点:
- D1: reward_signal.h 路径修正 (contract/ → types/)
- D2: TDD Verify-fail 命令修正 (cmake + make 触发)
- D3: IDistillationWriter 完全对齐 ADR（3 虚函数 + 工厂）
- D4: test_capture_mode.cpp include 全部 3 个新头文件 + phase0_headers_compile_smoke case 防零编译覆盖
- D5: commit 前/后验证命令分离
- D6: DistillationRecord 字段对齐 ADR-0061-13 决策 2（含 input/output）
- D7: StepRecord.reward 真嵌入 RewardSignal struct（ADR-0083 复用）
- D8: MUST NOT 列完整 20 个 contract 文件
- D9: 测试引用修正（test_reward_signal.cpp → test_evaluator.cpp）

验证:
- ctest -R capture_mode: 3 cases / ≥13 assertions PASS
- 全量 ctest: 0 新增回归（与 baseline 持平，动态计数）
- 既有 20 个 contract 头文件零修改（Oracle B3 关键不变量）
- engine.h 零修改
- event_log_config.h 零修改（Phase 1 才改）
- L1 契约层独立: grep '#include \"src/\"' 0 命中
- static_assert: IDistillationWriter is_abstract_v, CaptureMode sizeof==1
- adr_lint exit 0 + ADR-TRACKING-01 WARNING 数与 baseline 持平

后续 Phase 1: EventLogConfig BREAKING 字段迁移 + FileDistillationWriter V1 实施
- bool capture_prompt_bytes → CaptureMode capture_mode (5 消费者迁移)
- FileDistillationWriter 实现 3 虚函数 + make_file_writer 工厂
- ≥5 test cases

[24h cooling-off 结束后启动]
[Oracle B3 关键不变量: contract/ 下 20 个既有头文件零 diff]"

# Step 8.4: commit 后验证（修正 D5）
git diff HEAD^ HEAD -- include/agenticdsl/contract/ | head -10
# 预期：仅显示 idistillation_writer.h 新增行，其余 20 文件 0 行

git diff HEAD^ HEAD -- src/core/engine.h
# 预期：空输出（零修改）

git diff HEAD^ HEAD -- src/core/types/event_log_config.h
# 预期：空输出（零修改）
```

---

## MUST NOT DO（修正 D8 + 原 plan 严格保留）

### 禁止修改的 20 个 contract 头文件（修正 D8）

```cpp
include/agenticdsl/contract/
├── bus_event.h                     ❌ 禁止修改
├── causal_clock.h                  ❌ 禁止修改
├── evaluation_events.h             ❌ 禁止修改
├── event_builder.h                 ❌ 禁止修改
├── i_llm_provider_decorator.h      ❌ 禁止修改
├── iagent_composition.h            ❌ 禁止修改
├── iagent_hook_registry.h          ❌ 禁止修改
├── iagent_registry.h               ❌ 禁止修改
├── icommand_registry.h             ❌ 禁止修改
├── ievaluator.h                    ❌ 禁止修改
├── iinteraction_bus.h              ❌ 禁止修改
├── imutation_governance.h          ❌ 禁止修改
├── inmemory_bus.h                  ❌ 禁止修改
├── iparser.h                       ❌ 禁止修改
├── iprovider_factory.h             ❌ 禁止修改
├── ischeduler.h                    ❌ 禁止修改
├── iskill_compiler.h               ❌ 禁止修改
├── itool_hook_registry.h           ❌ 禁止修改
├── itool_registry.h                ❌ 禁止修改
└── test_double_registry.h          ❌ 禁止修改

✅ 仅允许新增: idistillation_writer.h
```

### 禁止的其他操作

- ❌ 修改 `src/core/engine.h`（Oracle B3 关键不变量）
- ❌ 修改 `src/core/types/event_log_config.h`（Phase 1 才改）
- ❌ 实现 `FileDistillationWriter`（Phase 1 内容）
- ❌ 修改 `--allow-training-capture` CLI flag（Phase 2 内容）
- ❌ 新增依赖（nlohmann::json 已 vendor）
- ❌ 删除或修改既有 tests/（仅新增 `test_capture_mode.cpp`）
- ❌ 硬编码 ctest 数字（用 grep/count 动态验证）
- ❌ 在 3 cases 编译失败时强行 commit（TDD 5 步纪律）
- ❌ 使用 `git push`（single-dev 模式仅本地 commit）
- ❌ 使用 `git commit --amend`（atomic commit 纪律）
- ❌ 使用 `git commit --no-verify`（遵守项目 governance hooks，除非已与 openspec-gate 协调）
- ❌ 在 reward_signal.h 写错路径（必须 types/ 而非 contract/，D1 修正）

---

## CONTEXT

### 工作目录
- 项目根: `/workspace/project/HydraForge`
- 实施依据: `.rddf/plans/capture-mode-and-distillation-writer-v1-phase-0.md`（仅作历史参考）
- 关键参考（本 prompt 优先）: ADR-0061-13 决策 2 + 3 + ADR-0083 RewardSignal + ADR-0080 v1.2 D10
- OpenSpec change: `openspec/changes/capture-mode-and-distillation-writer-v1/`

### 已 ship 关键文件（Phase 0 参考）
- `include/agenticdsl/contract/reward_signal.h` (ADR-0083 V1+V2, **实际路径 types/**)
- `include/agenticdsl/contract/ievaluator.h`（纯虚接口契约层模式参考）
- `include/agenticdsl/types/layered_context.h`（值类型风格参考）
- `tests/test_evaluator.cpp`（✅ **实际存在**，原 plan 引用 test_reward_signal.cpp 错误，D9 修正）

### 已 ship 测试参考
- `tests/test_evaluator.cpp`（现有 IEvaluator + RewardSignal 测试模式，Catch2 REQUIRE/REQUIRE_THROWS_AS 用法）
- `tests/test_mutation_governance.cpp`（MutationGovernor 测试模式）

### OpenSpec change 文件（本次任务设计依据）
- `openspec/changes/capture-mode-and-distillation-writer-v1/proposal.md`（What Changes）
- `openspec/changes/capture-mode-and-distillation-writer-v1/design.md`（D1-D8 设计决策）
- `openspec/changes/capture-mode-and-distillation-writer-v1/tasks.md`（Phase 0-3 tasks，含 TDD 5 步）
- `openspec/changes/capture-mode-and-distillation-writer-v1/specs/capture-mode-and-distillation-writer-v1/spec.md`（10 ADDED Requirements）

### 关键约束（来自 Oracle 决策 2 + 5 + Metis 修正决策）

1. **Oracle B3 关键不变量**: 既有 20 个 contract 头文件 + engine.h 零修改
2. **修正 D3**: IDistillationWriter 3 虚函数 + 1 静态工厂对齐 ADR-0061-13 §决策 3
3. **修正 D6 + D7**: DistillationRecord 字段全集 + StepRecord.reward 真嵌入 RewardSignal
4. **修正 D4**: test_capture_mode.cpp include 全部 3 个新头文件（防零编译覆盖）
5. **修正 D5**: commit 前/后验证命令分离
6. **Oracle 决策 5**: ADR-TRACKING-01 WARNING 数与 baseline 持平（capture-mode-and-distillation-writer-v1 目录名含 "0080" 和 "0061-13" 子串匹配）

### 已知 trade-off

- **TDD 测试结构**: Phase 0 4 cases（3 主 case + 1 编译期烟雾 case），修正 D4 后 phase0_headers_compile_smoke 承担蒸馏契约全头文件编译验证
- **CaptureMode 默认值校验**: Phase 0 测 `kDefaultCaptureMode` 常量本身；EventLogConfig 默认值验证推迟至 Phase 1（spec Scenario 验证对象错位已修正）
- **DistillationRecord 字段**: 已对齐 ADR-0061-13 §决策 2 全集（input/output/trace_id/source_event/agent_id/teacher_version/generation_timestamp_ms）
- **FileDistillationWriter V1**: Phase 1 实施（写 3 虚函数 + make_file_writer 工厂实现）

### Oracle + Metis 审查追溯

- **Oracle session**: `ses_fb33c1b2affe6CJtJph7iMdW22`（架构正确性 + 实施风险审查）
- **Metis session**: `ses_fb33c1b2affe6CJtJph7iMdW22`（歧义识别 + AI 失败点审查）
- **9 项修正决策**: D1-D9（详见本 prompt 顶部决策表）

---

## 期望结果（给用户）

1. **commit hash**（例如 `a1b2c3d`）
2. **ctest 输出**: `4 cases / ≥13 assertions PASS`（含 phase0_headers_compile_smoke 编译期烟雾 case）
3. **diff stats**: `4 files changed, ~350 insertions`（3 个新头文件 + 1 个新测试文件）
4. **Oracle B3 验证**:
   - `git diff HEAD -- include/agenticdsl/contract/`（commit 前）= 0 行
   - `git diff HEAD^ HEAD -- include/agenticdsl/contract/`（commit 后）仅含 idistillation_writer.h 新增行
5. **adr_lint 验证**: `python3 tools/adr_lint.py` exit 0 + WARNING 数与 baseline 持平
6. **零编译覆盖验证**: test_capture_mode.cpp 顶部包含全部 3 个新头文件 + static_assert 通过
7. **后续**: 立即启动 Phase 1 deep agent（BREAKING 字段迁移 + FileDistillationWriter V1 实施 3 虚函数 + make_file_writer 工厂）

---

## 任务跟踪

- [ ] cooling-off 窗口结束（2026-08-30 18:00 后）
- [ ] 派发 deep agent（按本修正版 prompt）
- [ ] 验证 4 个新增文件 + 1 commit
- [ ] ctest -R capture_mode: 3+ cases PASS
- [ ] adr_lint exit 0 + ADR-TRACKING-01 WARNING 数持平
- [ ] commit 前/后两次 git diff 验证通过
- [ ] git status clean
- [ ] 立即启动 Phase 1 deep agent（FileDistillationWriter V1）

## 风险 mitigation

### 风险 1: CaptureMode 枚举值稳定性
- 缓解: 头文件 `static_assert(sizeof(CaptureMode) == 1)` + `static_assert(Off == 0)`
- 验证: spec.md Scenario "枚举三态完整" + 测试断言 0/1/2

### 风险 2: parse_capture_mode 大小写敏感
- 缓解: 测试覆盖 "OFF"/"off"/"" 等 invalid 输入
- 验证: 测试 case "parse_invalid_string_throws"

### 风险 3: 数据结构偏离 ADR
- 缓解: **本修正版已完全对齐 ADR-0061-13 §决策 2 + 3**（D3 + D6 + D7 修正）
- 验证: spec.md Requirement "CaptureMode 三态" + "IDistillationWriter 纯虚契约" 全部 Scenario 满足

### 风险 4: IDistillationWriter 契约层污染
- 缓解: 严格不 #include src/ 任何头文件（仅 #include types/ + contract/）
- 验证: `grep -rn '#include "src/' include/agenticdsl/contract/idistillation_writer.h` = 0 命中

### 风险 5: TDD 纪律破坏
- 缓解: 严格 5 步顺序（Write fail → Verify fail [必须 make] → Implement → Verify pass → Commit）
- 验证: commit 前 3+ cases 已 PASS（ctest 输出确认）

### 风险 6: 零编译覆盖（修正 D4）
- 缓解: test_capture_mode.cpp 顶部包含全部 3 个新头文件 + phase0_headers_compile_smoke case + static_assert
- 验证: phase0_headers_compile_smoke 编译通过 + 静态断言成立

### 风险 7: commit 验证命令语义（修正 D5）
- 缓解: commit 前 `git diff HEAD -- contract/` 验证既有 20 文件零 diff；commit 后 `git diff HEAD^ HEAD -- contract/` 验证仅 idistillation_writer.h 新增
- 验证: 两个命令分别在不同阶段执行

### 风险 8: reward_signal.h 路径错误（修正 D1）
- 缓解: 实施代码明确使用 `agenticdsl/types/reward_signal.h`（而非 contract/）
- 验证: grep 编译错误不存在 + Phase 0 头文件全部 include 成功

---

## 关联文档

- **OpenSpec change**: `openspec/changes/capture-mode-and-distillation-writer-v1/`
- **Roadmap**: `docs/superpowers/plans/2026-08-29-sprint-24-30-roadmap.md`
- **Sprint 24 kickoff**: `docs/architecture/sprint-24/kickoff.md`
- **原始 plan（仅历史参考）**: `.rddf/plans/capture-mode-and-distillation-writer-v1-phase-0.md`
- **Oracle 审查**: `ses_fb33c1b2affe6CJtJph7iMdW22`
- **Metis 审查**: `ses_fb33c1b2affe6CJtJph7iMdW22`
- **ADR 跟踪**: `commit 0e0359c`（ADR-TRACKING-01 实战验证）
- **治理工具**: `tools/adr_lint.py`（含 ADR-TRACKING-01 规则）

---

**Prompt 状态**: ✅ Ready（修正版，9 项修正完成）
**创建者**: Sisyphus @ 2026-08-29（基于 Oracle + Metis 双审查）
**适用场景**: 2026-08-30 18:00 cooling-off 结束后，deep agent 委派
**重要**: agent 必须使用本修正版 prompt 的所有字段（包括修正的代码片段），不得回退到原始 plan（`.rddf/plans/...`）的错误字段