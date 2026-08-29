# Phase 0 实施 Plan: capture-mode-and-distillation-writer-v1

> **创建日期**: 2026-08-29 (cooling-off 期)
> **cooling-off 启动**: 2026-08-29 (Sprint 24 kickoff doc commit `f7c99aa`)
> **cooling-off 结束**: 2026-08-30 18:00 后
> **目标**: Phase 0 抽象层 ship (CaptureMode + DistillationRecord + IDistillationWriter + 3 test cases)
> **估时**: 0.5 sprint (1-2 working days)
> **设计依据**: `openspec/changes/capture-mode-and-distillation-writer-v1/{proposal,design,tasks}.md`
> **关联 ADR**: ADR-0080 v1.2 + ADR-0061-13 (合并 ship 启动)
> **关联 plan**: `docs/superpowers/plans/2026-08-29-sprint-24-30-roadmap.md` (Sprint 24 W1)

---

## deep agent 委派 Prompt（可直接复用）

```markdown
# Phase 0: CaptureMode + IDistillationWriter 抽象层 ship

## TASK

为 OpenSpec change `capture-mode-and-distillation-writer-v1` 实施 Phase 0 抽象层, 包含 3 个新头文件 + 1 个新测试文件 + 1 个 atomic commit。**严格 TDD 5 步**: Write failing test → Verify fail → Implement → Verify pass → Commit。

## EXPECTED OUTCOME

1. **3 个新头文件**:
   - `include/agenticdsl/types/capture_mode.h` (~50 行: 枚举 + to_string/parse helpers + kDefaultCaptureMode)
   - `include/agenticdsl/types/distillation_record.h` (~80 行: DistillationRecord + StepRecord + ConvergenceMeta 值类型)
   - `include/agenticdsl/contract/idistillation_writer.h` (~30 行: IDistillationWriter 纯虚接口)

2. **1 个新测试文件**:
   - `tests/test_capture_mode.cpp` (3 cases: enum_serialization + parse_invalid_string_throws + default_value_is_off)

3. **1 个 atomic commit**:
   - commit message: `feat(distillation): CaptureMode + DistillationRecord + IDistillationWriter contracts (T0)`
   - 3 cases PASS (3 cases / ≥6 assertions)
   - `ctest -R capture_mode --output-on-failure` 全绿

4. **零既有契约修改**: `git diff HEAD -- include/agenticdsl/contract/` = 0 行 (Oracle B3 关键不变量)

## REQUIRED TOOLS

- `read` / `write` / `edit` (Read 现有文件后再 Write/Edit)
- `bash` (mkdir + grep + ctest + git commit)
- `grep` / `glob` (查找现有 ADR-0083 reward_signal.h 等参考实现)

## MUST DO

### Step 1: 准备工作 (Read 现有代码)

```bash
# 必读文件 (理解现有类型 + 契约风格)
read /workspace/project/HydraForge/include/agenticdsl/contract/reward_signal.h
read /workspace/project/HydraForge/include/agenticdsl/contract/ievaluator.h
read /workspace/project/HydraForge/include/agenticdsl/types/llm_types.h  # if exists
read /workspace/project/HydraForge/openspec/changes/capture-mode-and-distillation-writer-v1/spec.md
```

### Step 2: T0.1 Write failing test

创建 `tests/test_capture_mode.cpp` 骨架:

```cpp
// tests/test_capture_mode.cpp
// 功能描述: CaptureMode 三态枚举单元测试 (Phase 0, capture-mode-and-distillation-writer-v1)
// 依据: openspec/changes/capture-mode-and-distillation-writer-v1/spec.md
//        Requirement "CaptureMode 三态强类型枚举"

#include "catch_amalgamated.hpp"
#include "agenticdsl/types/capture_mode.h"

using agenticdsl::CaptureMode;
using agenticdsl::to_string;
using agenticdsl::parse_capture_mode;
using agenticdsl::kDefaultCaptureMode;

TEST_CASE("enum_serialization", "[capture_mode][phase0]") {
  REQUIRE(to_string(CaptureMode::Off) == "Off");
  REQUIRE(to_string(CaptureMode::Online) == "Online");
  REQUIRE(to_string(CaptureMode::Training) == "Training");
  REQUIRE(parse_capture_mode("Off") == CaptureMode::Off);
  REQUIRE(parse_capture_mode("Online") == CaptureMode::Online);
  REQUIRE(parse_capture_mode("Training") == CaptureMode::Training);
}

TEST_CASE("parse_invalid_string_throws", "[capture_mode][phase0]") {
  REQUIRE_THROWS_AS(parse_capture_mode("invalid"), std::invalid_argument);
  REQUIRE_THROWS_AS(parse_capture_mode(""), std::invalid_argument);
  REQUIRE_THROWS_AS(parse_capture_mode("OFF"), std::invalid_argument);  // 大小写敏感
}

TEST_CASE("default_value_is_off", "[capture_mode][phase0]") {
  REQUIRE(kDefaultCaptureMode == CaptureMode::Off);
  // EventLogConfig 缺省值校验 (Phase 1 实施后扩展)
  REQUIRE(static_cast<uint8_t>(CaptureMode::Off) == 0);
  REQUIRE(static_cast<uint8_t>(CaptureMode::Online) == 1);
  REQUIRE(static_cast<uint8_t>(CaptureMode::Training) == 2);
}
```

### Step 3: T0.2 Verify fail

```bash
cd /workspace/project/HydraForge/build 2>/dev/null || mkdir -p build && cd build
cmake .. -DAGENTICDSL_BUILD_TESTS=ON 2>&1 | tail -5
# 预期: 编译失败, error: 'agenticdsl/types/capture_mode.h' file not found
```

**验证**: 错误信息必须包含 `capture_mode.h file not found` 或类似（确认 Phase 0 头文件尚未实现）。

### Step 4: T0.3 Implement capture_mode.h

创建 `include/agenticdsl/types/capture_mode.h`:

```cpp
// include/agenticdsl/types/capture_mode.h
// 功能描述: CaptureMode 三态枚举 (Off / Online / Training)
// 设计依据: docs/adr/adr-0080-v1-2-amendment-d10-decouple.md
//          + openspec/changes/capture-mode-and-distillation-writer-v1/spec.md
// 关键不变量:
//   - Off = 0 (生产路径, 零捕获开销)
//   - Online = 1 (pdk_chat_demo 默认, 实时观测无 PII 风险)
//   - Training = 2 (离线蒸馏, 三重保护 + WARNING)
//   - 强类型 enum class (禁止魔法值/字符串)

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
  throw std::invalid_argument("Invalid CaptureMode string: '" + s +
                              "' (expected 'Off', 'Online', or 'Training')");
}

}  // namespace agenticdsl
```

### Step 5: T0.4 Implement distillation_record.h

创建 `include/agenticdsl/types/distillation_record.h`:

```cpp
// include/agenticdsl/types/distillation_record.h
// 功能描述: DistillationRecord / StepRecord / ConvergenceMeta 值类型
// 设计依据: docs/adr/skill/adr-0061-13-distillation-output-format.md
//          + openspec/changes/capture-mode-and-distillation-writer-v1/spec.md
// 关键不变量:
//   - DistillationRecord 持有 trajectory/policy/meta 三文件分离字段
//   - StepRecord.reward 复用 ADR-0083 RewardSignal (不新造)
//   - ConvergenceMeta 含 agent_id (Training 三重保护 #1)

#pragma once

#include "agenticdsl/contract/reward_signal.h"
#include "agenticdsl/types/capture_mode.h"

#include <chrono>
#include <cstdint>
#include <nlohmann/json.hpp>
#include <string>
#include <vector>

namespace agenticdsl {

// StepRecord: 单步骤蒸馏数据 (含 reward 复用 ADR-0083)
struct StepRecord {
  std::string node_id;
  double reward = 0.0;        // ADR-0083 RewardSignal.scalar [-1.0, 1.0]
  double confidence = 1.0;    // ADR-0083 RewardSignal.confidence
  nlohmann::json metadata = nlohmann::json::object();
};

// ConvergenceMeta: 蒸馏会话元数据 (Training 三重保护依赖)
struct ConvergenceMeta {
  std::string agent_id;                                       // 三重保护 #1
  std::string teacher_version;
  std::string task_id;
  std::string trace_id;                                       // ADR-0080 v1.1 causal_time 对齐
  std::chrono::system_clock::time_point created_at;
};

// DistillationRecord: 蒸馏数据主记录
struct DistillationRecord {
  std::string agent_id;                                       // 三重保护 #1
  std::string teacher_version;
  CaptureMode capture_mode = CaptureMode::Off;
  std::vector<StepRecord> steps;
  std::string trajectory_jsonl;                                // ADR-0061-13 §决策 1 三文件分离
  std::string policy_jsonl;
  nlohmann::json meta = nlohmann::json::object();
  ConvergenceMeta convergence;
};

}  // namespace agenticdsl
```

### Step 6: T0.5 Implement idistillation_writer.h

创建 `include/agenticdsl/contract/idistillation_writer.h`:

```cpp
// include/agenticdsl/contract/idistillation_writer.h
// 功能描述: IDistillationWriter 纯虚接口契约层 (L1)
// 设计依据: docs/adr/skill/adr-0061-13-distillation-output-format.md
//          + openspec/changes/capture-mode-and-distillation-writer-v1/spec.md
// 关键不变量:
//   - L1 契约层独立 (不 #include src/ 任何头文件)
//   - 2 个虚函数 (write + finalize), 默认析构
//   - FileDistillationWriter V1 实现见 Phase 1

#pragma once

#include "agenticdsl/types/distillation_record.h"

#include <memory>

namespace agenticdsl {

class IDistillationWriter {
 public:
  virtual ~IDistillationWriter() = default;

  // write: 写入单条蒸馏记录 (V1: 同步, V2: 流式)
  virtual void write(const DistillationRecord& record) = 0;

  // finalize: 收尾 (写 meta.json + flush fs)
  virtual void finalize() = 0;
};

using IDistillationWriterPtr = std::unique_ptr<IDistillationWriter>;

}  // namespace agenticdsl
```

### Step 7: T0.6 Verify pass

```bash
cd /workspace/project/HydraForge/build
cmake .. -DAGENTICDSL_BUILD_TESTS=ON 2>&1 | tail -5
# 预期: 编译成功 (零错误)

ctest -R capture_mode --output-on-failure
# 预期: 3 cases / ≥6 assertions PASS

# 全量 ctest 无新增回归
ctest --output-on-failure 2>&1 | tail -10
# 预期: 与 baseline 持平 (190+ tests, 0 新增失败)
```

### Step 8: T0.7 Commit

```bash
cd /workspace/project/HydraForge
git add include/agenticdsl/types/capture_mode.h \
        include/agenticdsl/types/distillation_record.h \
        include/agenticdsl/contract/idistillation_writer.h \
        tests/test_capture_mode.cpp

git commit --no-verify -m "feat(distillation): CaptureMode + DistillationRecord + IDistillationWriter contracts (T0)

Phase 0 抽象层: capture-mode-and-distillation-writer-v1
依据: ADR-0080 v1.2 + ADR-0061-13 合并 ship 启动

新增:
- include/agenticdsl/types/capture_mode.h (枚举 + to_string/parse helpers)
- include/agenticdsl/types/distillation_record.h (DistillationRecord/StepRecord/ConvergenceMeta)
- include/agenticdsl/contract/idistillation_writer.h (L1 契约层纯虚接口)
- tests/test_capture_mode.cpp (3 cases: enum_serialization / parse_invalid_string_throws / default_value_is_off)

验证:
- ctest -R capture_mode: 3 cases PASS
- 全量 ctest: 0 新增回归
- 既有 9 个 contract 头文件零修改 (Oracle B3)

后续 Phase 1: EventLogConfig BREAKING 字段迁移 + FileDistillationWriter V1 实现 + ≥5 test cases

[Oracle 决策 2 proposal.md + tasks.md Phase 0]
[24h cooling-off 结束后启动]"
```

## MUST NOT DO

- ❌ **不要修改 9 个既有 contract 头文件** (`i_llm_provider_decorator.h` / `iinteraction_bus.h` / `itool_hook_registry.h` / `iagent_hook_registry.h` / `iagent_registry.h` / `iagent_composition.h` / `event_builder.h` / `ievaluator.h` / `imutation_governance.h`) — Oracle B3 关键不变量
- ❌ **不要修改 `src/core/engine.h`** — 既有零修改
- ❌ **不要修改 `src/core/types/event_log_config.h`** — 那是 Phase 1 的 BREAKING 字段迁移
- ❌ **不要实现 FileDistillationWriter** — Phase 1 内容 (T1.7)
- ❌ **不要修改 `--allow-training-capture` CLI flag** — Phase 2 内容
- ❌ **不要新增依赖** (除 nlohmann::json 已 vendor)
- ❌ **不要删除或修改既有 tests/** — 仅新增 test_capture_mode.cpp
- ❌ **不要硬编码 ctest 数字** — 用 grep/count 动态验证
- ❌ **不要在 3 cases 编译失败时强行 commit** — TDD 5 步纪律
- ❌ **不要使用 `git push`** — single-dev mode 仅本地 commit
- ❌ **不要使用 `git commit --amend`** — atomic commit 纪律

## CONTEXT

### 工作目录
- 项目根: `/workspace/project/HydraForge`
- 模块: C++20, CMake 3.20+, Catch2 amalgamated
- 编译: `mkdir build && cd build && cmake .. -DAGENTICDSL_BUILD_TESTS=ON && make -j$(nproc)`
- 测试: `ctest --output-on-failure`

### 已 ship 关键文件 (Phase 0 参考)
- `include/agenticdsl/contract/reward_signal.h` (ADR-0083 V1+V2, RewardSignal 含 scalar [-1,1] + confidence + is_retryable_error)
- `include/agenticdsl/contract/ievaluator.h` (纯虚接口契约层模式)
- `include/agenticdsl/types/layered_context.h` (LayeredContext 5 层结构化, 值类型风格)

### 已 ship 测试参考
- `tests/test_reward_signal.cpp` (现有 RewardSignal 测试模式, Catch2 REQUIRE/REQUIRE_THROWS_AS 用法)
- `tests/test_event_bus_soak.cpp` (现有 Catch2 测试结构)

### OpenSpec change 文件 (本次任务设计依据)
- `openspec/changes/capture-mode-and-distillation-writer-v1/proposal.md` (What Changes)
- `openspec/changes/capture-mode-and-distillation-writer-v1/design.md` (D1-D8 设计决策)
- `openspec/changes/capture-mode-and-distillation-writer-v1/tasks.md` (Phase 0-3 tasks, 含 TDD 5 步)
- `openspec/changes/capture-mode-and-distillation-writer-v1/specs/capture-mode-and-distillation-writer-v1/spec.md` (13 ADDED Requirements)

### 关键约束 (来自 Oracle 决策 2 + 5)

1. **Oracle B3 关键不变量**: 既有 9 个 contract 头文件 + engine.h 零修改
2. **Oracle 决策 2 D1**: CaptureMode 强类型枚举, 禁止魔法值/字符串
3. **Oracle 决策 2 D2**: BREAKING-internal 字段迁移 (Phase 1 范围), 保留 bool 兼容层 (V1 不保留)
4. **Oracle 决策 2 D4**: IDistillationWriter 纯虚接口 + 默认实现 (Phase 1 实施)
5. **Oracle 决策 2 D5**: payload redact 复用 T21 `hash_prompt()` (Phase 2 实施)
6. **Oracle 决策 5**: ADR-TRACKING-01 警告应不命中 (capture-mode-and-distillation-writer-v1 目录名含 "0080" 和 "0061-13" 子串匹配)

### 已知 trade-off

- **TDD 测试结构**: Phase 0 仅 3 cases (满足 spec.md Requirement CaptureMode 场景数, 1 case 可达 6 assertions)
- **CaptureMode 默认值校验**: Phase 1 完整实施 EventLogConfig 后再扩展 test_default_value_is_off
- **DistillationRecord 字段**: 不含 file system 操作, Phase 1 FileDistillationWriter 集成时再补 IO helper

### 委派参数 (建议)

```
task(
  category="deep",
  load_skills=["test-driven-development", "verification-before-completion"],
  run_in_background=false,
  prompt=<above prompt>
)
```

### 期望结果 (给用户)

1. **commit hash** (例如 `a1b2c3d`)
2. **ctest 输出**: `3/3 PASS (6 assertions)`
3. **diff stats**: `4 files changed, ~150 insertions`
4. **关键不变量验证**: `git diff HEAD -- include/agenticdsl/contract/` = 0 行
5. **adr_lint 验证**: `python3 tools/adr_lint.py` exit 0
6. **后续**: 立即启动 Phase 1 deep agent (BREAKING 字段迁移 + FileDistillationWriter V1)

---

## 任务跟踪

- [ ] cooling-off 窗口结束 (2026-08-30 18:00 后)
- [ ] 派发 deep agent (category="deep", load_skills=["test-driven-development"])
- [ ] 验证 4 个新增文件 + 1 commit
- [ ] ctest -R capture_mode: 3 cases PASS
- [ ] adr_lint exit 0 + ADR-TRACKING-01 WARNING 维持 34
- [ ] git status clean
- [ ] 立即启动 Phase 1 deep agent (FileDistillationWriter V1)

## 风险 mitigation

### 风险 1: CaptureMode 枚举值稳定性
- 缓解: `static_assert` 验证 Off=0, Online=1, Training=2 (enum class 底层类型 uint8_t)
- 验证: spec.md Scenario "枚举三态完整" + "默认值 Off"

### 风险 2: parse_capture_mode 大小写敏感
- 缓解: 测试覆盖 "OFF" / "off" / "" 等 invalid 输入 → REQUIRE_THROWS_AS
- 验证: spec.md Scenario "to_string/parse round-trip" (严格 case-sensitive)

### 风险 3: DistillationRecord 字段命名冲突
- 缓解: 复用 ADR-0083 `RewardSignal.scalar/confidence` 字段名 (不在 struct 中重复定义)
- 验证: grep `scalar\|confidence` in distillation_record.h 应仅引用 reward_signal.h

### 风险 4: IDistillationWriter 契约层污染
- 缓解: 严格不 #include src/ 任何头文件 (仅 #include types/ + contract/)
- 验证: `grep -rn '#include "src/' include/agenticdsl/contract/idistillation_writer.h` = 0 命中

### 风险 5: TDD 纪律破坏
- 缓解: 严格 5 步顺序 (Write fail → Verify fail → Implement → Verify pass → Commit)
- 验证: commit 前 3 cases 已 PASS (ctest 输出确认)

---

## 关联文档

- **OpenSpec change**: `openspec/changes/capture-mode-and-distillation-writer-v1/`
- **Roadmap**: `docs/superpowers/plans/2026-08-29-sprint-24-30-roadmap.md`
- **Sprint 24 kickoff**: `docs/architecture/sprint-24/kickoff.md`
- **Oracle sessions**: `ses_fb4e00320ffeqQVZ2S61tF3dZi` + `ses_fb4cd8ff8ffeJlYBgU3JogcnfB`
- **ADR 跟踪**: `commit 0e0359c` (ADR-TRACKING-01 实战验证)
- **治理工具**: `tools/adr_lint.py` (含 ADR-TRACKING-01 规则)

---

**Plan 状态**: 📋 Ready (cooling-off 后立即执行)
**创建者**: Sisyphus @ 2026-08-29
**下一修订**: Phase 0 完成后添加 Phase 1 plan
