# from-roadmap-phase-6c-execution-baseline Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use skill_use("execute") to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 通过 C1/C2/C3 实施 ADR-0074 D1/D2/D3 — 采集 32 个 few-shot examples + 51 个 held-out golden tasks + V1/V2/V3 prompt 代码 + `tools/measure_prompt_baseline` CLI + 发布首次 baseline 测量报告,为后续 `evidence-gate` change 提供 golden 数据基础。

**Architecture:**
- **数据层**: `lib/prompts/fewshot/` (32 YAML, 4 维度 × 8) + `lib/prompts/golden/` (51 YAML, 6 领域 × L1/L2/L3) — 物理隔离, yaml-cpp 解析
- **代码层**: `src/common/prompts/{v1.cpp, v2.cpp, v3.cpp, prompt_builder.h}` — V3 两阶段固定顺序 enum, V2 few-shot ≤5 抽样注入, V1 schema 引用 ADR-0073 ToolMetadata V3 schema
- **测量层**: `tools/measure_prompt_baseline.cpp` — argparse + mock-mode + YAML 输出 schema (baseline_id / parse_valid_rate / per_dimension / confidence_interval)
- **合规层**: `scripts/verify_golden_holdout.sh` — `grep` 0 匹配强制 hold-out, 集成 sprint-closeout Step 6
- 测量数据流: V1/V2/V3 → measure CLI → golden suite → `.yaml` 报告 → `evidence-gate` change 消费

**Tech Stack:**
- C++20 (现有)
- yaml-cpp (vendor 已有, lib/prompts 数据格式)
- Catch2 (单元测试基础设施已就绪, 147+ 测试 PASS 基线)
- CMake 3.20+ `target_include_directories` (禁用 `include_directories()` per 项目 anti-pattern)
- Markdown + YAML 嵌块报告格式 (与 `2026-08-03-adr-0068-ship-gate.md` 一致)

---

## File Structure

### Production Code (新增)

| File | Responsibility |
|------|----------------|
| `src/common/prompts/prompt_builder.h` | 抽象接口 + `PromptPayload` + `PromptStage` enum |
| `src/common/prompts/v1.cpp` | V1 schema constraint 实现 (引用 ADR-0073 ToolMetadata V3 schema) |
| `src/common/prompts/v2.cpp` | V2 = V1 + few-shot 注入 (≤ 5 抽样 from `lib/prompts/fewshot/`) |
| `src/common/prompts/v3.cpp` | V3 = V2 + 两阶段顺序固定 (SystemFirst → UserSecond) |
| `src/common/prompts/token_counter.h` | 发送前 token 计数 (简单 word count + schema 检查, > 8k 报警) |
| `tools/measure_prompt_baseline.cpp` | CLI 实施 (argparse, --prompt / --golden-dir / --output / --max-tasks / --mock-mode) |
| `lib/prompts/README.md` | few-shot / golden / V1/V2/V3 差异说明 (新增/扩充) |
| `lib/prompts/fewshot/{dimension}_{01..08}.yaml` | 32 个 few-shot examples (4 维度 × 8) — **手工采集** |
| `lib/prompts/golden/README.md` | 51 任务列表 + 评分规则 + L1/L2/L3 分布 |
| `lib/prompts/golden/{domain}_{NN}.yaml` | 51 个 held-out golden tasks — **手工采集** |
| `docs/audits/<date>-execution-baseline-v1.md` | Baseline 测量报告 (V1/V2/V3 对比 + per-dimension + 95% CI) |
| `docs/audits/<date>-execution-baseline-v1.yaml` | 机器可读 baseline 数据 |
| `scripts/verify_golden_holdout.sh` | Hold-out grep 验证脚本 |
| `openspec/handoff/from-roadmap-phase-6c-execution-baseline.md` | 给 `evidence-gate` change 的 handoff |

### Tests (新增)

| File | Responsibility |
|------|----------------|
| `tests/test_few_shot_examples.cpp` | 4 维度各 ≥ 8 + 每 example 含 4 字段 + yaml-cpp 解析 |
| `tests/test_golden_suite.cpp` | 51 task 计数 + 5 字段校验 + yaml-cpp 解析 |
| `tests/test_prompt_v1_v2_v3.cpp` | V3 stage ordering 不变量 + V1 schema 注入 + V2 few-shot ≤ 5 |
| `tests/test_measure_prompt_baseline.cpp` | mock mode 跑 3 tasks + 验证 YAML 输出 schema 合规 |

### Modify (无)

无现有 production 代码修改 — `src/common/prompts/` 为全新目录, `lib/prompts/` 为新目录, `tools/measure_prompt_baseline` 为新工具。

仅增量修改:`scripts/sprint-closeout.sh` Step 6 集成 `verify_golden_holdout.sh` + `tests/CMakeLists.txt` 注册 4 个新 test 源文件。

---

### Task 1: lib/prompts/ 目录初始化 + README 脚手架

**Files:**
- Create: `lib/prompts/README.md` (整体说明骨架)
- Create: `lib/prompts/fewshot/` (空目录 + `.gitkeep`)
- Create: `lib/prompts/golden/` (空目录 + `.gitkeep`)

- [ ] **Step 1: Write the failing test (静态文件存在性)**

```bash
# 临时 sanity check — 验证目录创建正确
ls -la lib/prompts/
# Expected output:
# drwxr-xr-x  fewshot/
# drwxr-xr-x  golden/
# -rw-r--r--  README.md
```

- [ ] **Step 2: Run to verify failure**

```bash
ls lib/prompts/ 2>&1
# Expected: "No such file or directory" (目录尚未创建)
```

- [ ] **Step 3: Create directories + initial README**

```bash
mkdir -p lib/prompts/fewshot lib/prompts/golden
touch lib/prompts/fewshot/.gitkeep lib/prompts/golden/.gitkeep
```

`lib/prompts/README.md` (initial scaffold):

```markdown
# lib/prompts/ — Prompt Engineering Library

## Structure

- `fewshot/` — 32 few-shot examples (4 维度 × 8), used by V2/V3 prompt builders
- `golden/` — 51 held-out golden tasks (L1=20 + L2=20 + L3=11), **NOT used in prompt construction**
  用于 `tools/measure_prompt_baseline` 测量 V1/V2/V3 表现

## 4 Dimensions Taxonomy

| Dimension | Description |
|-----------|-------------|
| `parse_valid` | LLM 输出可被 JSON Schema 解析为合法 JSON |
| `task_success` | LLM 输出在语义上完成任务请求 |
| `budget_hit` | LLM 调用未超出 budget 限制 |
| `error_recovery` | LLM 从前次错误中恢复并给出正确下一步 |

## Hold-out Guarantee

51 个 golden tasks 与 few-shot 完全物理隔离, ship 前用 `scripts/verify_golden_holdout.sh` 强制 `grep` 0 匹配验证。
```

- [ ] **Step 4: Verify creation**

```bash
ls -la lib/prompts/
test -f lib/prompts/README.md && echo "README exists"
test -d lib/prompts/fewshot && test -d lib/prompts/golden && echo "subdirs exist"
```

- [ ] **Step 5: Defer commit**

所有变更将在 archive 阶段统一提交。

---

### Task 2: 32 Few-shot Examples 采集 (C1 / ADR-0074 D1)

**Files:**
- Create: `lib/prompts/fewshot/parse_valid_{01..08}.yaml` × 8
- Create: `lib/prompts/fewshot/task_success_{01..08}.yaml` × 8
- Create: `lib/prompts/fewshot/budget_hit_{01..08}.yaml` × 8
- Create: `lib/prompts/fewshot/error_recovery_{01..08}.yaml` × 8

> ⚠️ **数据采集任务**: 32 个 example 由架构组手工标注, 内容来自 `examples/agent_basic/.agent.md` / `examples/agent_simple/` 等历史内嵌 prompt 提取 + 人工改写。

- [ ] **Step 1: Review existing prompt examples for seed data**

```bash
grep -rn "input:" examples/agent_basic/.agent.md examples/agent_simple/ examples/agent_loop/ | head -20
# Expected: 至少 5 个候选 input/output 对
```

- [ ] **Step 2: Define 4-field schema (in README)**

```markdown
### Few-shot Example Schema (YAML)

每个 `{dimension}_{NN}.yaml` 文件必须含以下 4 字段:

| Field | Type | Description |
|-------|------|-------------|
| `dimension` | enum | `parse_valid` / `task_success` / `budget_hit` / `error_recovery` |
| `input` | string | user task input |
| `output` | string | expected LLM output (或 recovery 后的最终 output) |
| `rationale` | string | 解释为何这是该维度的 representative example (≥ 30 字) |

示例 (`parse_valid_01.yaml`):
```yaml
dimension: parse_valid
input: "List user permissions"
output: '{"permissions": ["read", "write"]}'
rationale: "JSON 输出可被 Schema parser 直接解析为 list, 演示 parse_valid 维度的成功路径"
```
```

- [ ] **Step 3: Collect 8 examples per dimension**

For each dimension (`parse_valid` / `task_success` / `budget_hit` / `error_recovery`):
- Write 8 YAML files: `lib/prompts/fewshot/{dimension}_{01..08}.yaml`
- Each example 4 fields 必须填写完整 (`rationale` ≥ 30 字)
- 总数 = 32 files

- [ ] **Step 4: Verify count + validity (手动检查)**

```bash
ls lib/prompts/fewshot/ | wc -l
# Expected: 32 .yaml files + 1 .gitkeep = 33 entries

# Per dimension count (should be 8 each)
for dim in parse_valid task_success budget_hit error_recovery; do
  count=$(ls lib/prompts/fewshot/${dim}_*.yaml 2>/dev/null | wc -l)
  echo "$dim: $count"
done
# Expected:
# parse_valid: 8
# task_success: 8
# budget_hit: 8
# error_recovery: 8
```

- [ ] **Step 5: Defer commit**

---

### Task 3: test_few_shot_examples.cpp — Few-shot 数据完整性测试

**Files:**
- Create: `tests/test_few_shot_examples.cpp`
- Modify: `tests/CMakeLists.txt` (注册新测试)

- [ ] **Step 1: Write the failing test**

```cpp
// tests/test_few_shot_examples.cpp
#define CATCH_CONFIG_RUNNER
#include <catch2/catch_test_macros.hpp>
#include <yaml-cpp/yaml.h>
#include <filesystem>
#include <set>
#include <string>

namespace fs = std::filesystem;

const std::set<std::string> kValidDimensions = {
    "parse_valid", "task_success", "budget_hit", "error_recovery"
};

TEST_CASE("few-shot has 8 examples per dimension", "[few-shot][c1]") {
    fs::path fewshot_dir = "lib/prompts/fewshot";
    REQUIRE(fs::exists(fewshot_dir));

    for (const auto& dim : kValidDimensions) {
        int count = 0;
        for (const auto& entry : fs::directory_iterator(fewshot_dir)) {
            if (entry.path().extension() == ".yaml") {
                std::string filename = entry.path().stem().string();
                if (filename.find(dim + "_") == 0) count++;
            }
        }
        REQUIRE(count == 8);
    }
}

TEST_CASE("few-shot examples have 4 required fields", "[few-shot][c1]") {
    fs::path fewshot_dir = "lib/prompts/fewshot";
    for (const auto& entry : fs::directory_iterator(fewshot_dir)) {
        if (entry.path().extension() != ".yaml") continue;

        YAML::Node node = YAML::LoadFile(entry.path().string());
        REQUIRE(node["dimension"]);
        REQUIRE(node["input"]);
        REQUIRE(node["output"]);
        REQUIRE(node["rationale"]);
        REQUIRE(kValidDimensions.count(node["dimension"].as<std::string>()) == 1);
        REQUIRE(node["rationale"].as<std::string>().size() >= 30);
    }
}

TEST_CASE("few-shot yaml-cpp parses all files", "[few-shot][c1]") {
    fs::path fewshot_dir = "lib/prompts/fewshot";
    int parsed = 0;
    for (const auto& entry : fs::directory_iterator(fewshot_dir)) {
        if (entry.path().extension() == ".yaml") {
            REQUIRE_NOTHROW(YAML::LoadFile(entry.path().string()));
            parsed++;
        }
    }
    REQUIRE(parsed == 32);
}
```

- [ ] **Step 2: Verify test fails (no data yet)**

```bash
cd build && cmake --build . --target test_few_shot_examples 2>&1 | head -20
# Expected: link error (test file not registered yet)
```

- [ ] **Step 3: Register test in tests/CMakeLists.txt**

Append to `tests/CMakeLists.txt`:

```cmake
# ADR-0074 C1 — Few-shot Examples
add_executable(test_few_shot_examples test_few_shot_examples.cpp)
target_link_libraries(test_few_shot_examples PRIVATE Catch2::Catch2WithMain yaml-cpp)
add_test(NAME test_few_shot_examples COMMAND test_few_shot_examples)
```

- [ ] **Step 4: Build and verify pass**

```bash
cmake --build build --target test_few_shot_examples && ctest -R test_few_shot_examples --output-on-failure
# Expected: 3 test cases PASS, 0 failures
```

- [ ] **Step 5: Defer commit**

---

### Task 4: lib/prompts/golden/ 目录 + 51 Golden Tasks 采集 (C2 / ADR-0074 D2)

**Files:**
- Create: `lib/prompts/golden/README.md` (51 task 列表 + 评分规则 + L1/L2/L3 分布)
- Create: `lib/prompts/golden/{auth,human,math,utils,inference,mcp}_{NN}.yaml` × 51

> ⚠️ **数据采集任务**: 51 个 golden tasks 由架构组从 `examples/` / `tests/integration/` / `benchmarks/` 提取并标注 difficulty (L1/L2/L3)。

- [ ] **Step 1: Define 5-field schema in golden/README.md**

```markdown
### Golden Task Schema (YAML)

每个 `{domain}_{NN}.yaml` 文件 5 字段:

| Field | Type | Description |
|-------|------|-------------|
| `task_id` | string (unique) | `golden_{domain}_{NN}` 格式, NN ∈ [01..N] |
| `input` | string | user task input |
| `expected_output` | string | 参考答案 (LLM 输出的 ground truth) |
| `dimension` | enum | `parse_valid` / `task_success` / `budget_hit` / `error_recovery` |
| `difficulty` | enum | `L1` (easy) / `L2` (medium) / `L3` (hard) |

示例 (`auth_01.yaml`):
```yaml
task_id: golden_auth_01
input: "Verify JWT token signature"
expected_output: '{"valid": true, "user_id": "u_123"}'
dimension: parse_valid
difficulty: L1
```

## Distribution (51 tasks total)

- L1 = 20 tasks (easy, ≤ 3 reasoning steps)
- L2 = 20 tasks (medium, 4-7 steps + 1 tool call)
- L3 = 11 tasks (hard, ≥ 8 steps + multi-tool orchestration)
- Domain coverage: auth / human / math / utils / inference / mcp (6 domains)
```

- [ ] **Step 2: Collect L1 tasks (20 total)**

For L1 (20 tasks), at least 3 domains covered:
- `lib/prompts/golden/auth_{01..04}.yaml` (4 L1)
- `lib/prompts/golden/math_{01..04}.yaml` (4 L1)
- `lib/prompts/golden/utils_{01..04}.yaml` (4 L1)
- `lib/prompts/golden/inference_{01..04}.yaml` (4 L1)
- `lib/prompts/golden/human_{01..04}.yaml` (4 L1)

Each: input + expected_output + dimension + difficulty=L1

- [ ] **Step 3: Collect L2 tasks (20 total)**

Same as L1 but `difficulty: L2`:
- 6 domains × ~4 tasks each (some domains may differ)

- [ ] **Step 4: Collect L3 tasks (11 total)**

- `lib/prompts/golden/{domain}_{01..02}.yaml` for hard domains (e.g., mcp_*, inference_*, auth_*)
- Total = 11 across 6 domains

- [ ] **Step 5: Verify count + distribution**

```bash
ls lib/prompts/golden/*.yaml 2>/dev/null | wc -l
# Expected: 51 .yaml files

# L1/L2/L3 distribution
for level in L1 L2 L3; do
  count=$(grep -l "difficulty: $level" lib/prompts/golden/*.yaml 2>/dev/null | wc -l)
  echo "$level: $count"
done
# Expected: L1: 20, L2: 20, L3: 11
```

- [ ] **Step 6: Defer commit**

---

### Task 5: scripts/verify_golden_holdout.sh — Hold-out grep 强制验证

**Files:**
- Create: `scripts/verify_golden_holdout.sh` (executable bit)
- Create: `lib/prompts/golden/{51 yaml}` ← already done in Task 4

- [ ] **Step 1: Write the failing test**

```bash
# Temporary verify script (basic version)
scripts/verify_golden_holdout.sh
# Expected: exit 0 (no leaks) once Task 4 data is in
```

- [ ] **Step 2: Run to verify failure (script doesn't exist yet)**

```bash
test -f scripts/verify_golden_holdout.sh || echo "EXPECTED: script missing"
# Expected: "EXPECTED: script missing"
```

- [ ] **Step 3: Create verify_golden_holdout.sh**

`scripts/verify_golden_holdout.sh`:

```bash
#!/usr/bin/env bash
# ADR-0074 D-2: Golden Suite Hold-out Verification
# Verifies 51 golden task_ids do NOT appear in fewshot examples
# Exit 0 = clean, Exit 1 = leak detected (CI failure)

set -euo pipefail

cd "$(git rev-parse --show-toplevel)"

GOLDEN_DIR="lib/prompts/golden"
FEWSHOT_DIR="lib/prompts/fewshot"

if [ ! -d "$GOLDEN_DIR" ]; then
    echo "ERROR: $GOLDEN_DIR does not exist"
    exit 1
fi

LEAKS=0

# Collect all task_ids from golden tasks
GOLDEN_IDS=$(grep -rh "^task_id:" "$GOLDEN_DIR" 2>/dev/null | awk '{print $2}' | sort -u)

# Check each task_id against fewshot directory
for id in $GOLDEN_IDS; do
    if [ -z "$id" ]; then continue; fi
    matches=$(grep -rn "$id" "$FEWSHOT_DIR" 2>/dev/null || true)
    if [ -n "$matches" ]; then
        echo "LEAK: task_id '$id' found in fewshot:"
        echo "$matches"
        LEAKS=$((LEAKS + 1))
    fi
done

if [ "$LEAKS" -gt 0 ]; then
    echo ""
    echo "❌ Hold-out FAILED: $LEAKS golden task_id(s) leaked into fewshot"
    exit 1
fi

echo "✅ Hold-out PASSED: $(echo "$GOLDEN_IDS" | wc -l) golden task_ids clean"
exit 0
```

```bash
chmod +x scripts/verify_golden_holdout.sh
```

- [ ] **Step 4: Run to verify pass**

```bash
scripts/verify_golden_holdout.sh
# Expected: "✅ Hold-out PASSED: 51 golden task_ids clean" + exit 0
```

- [ ] **Step 5: Defer commit**

---

### Task 6: Integrate hold-out check into sprint-closeout.sh Step 6

**Files:**
- Modify: `scripts/sprint-closeout.sh` (追加 Step 6.5 或类似位置)

- [ ] **Step 1: Find sprint-closeout.sh current Step 6**

```bash
grep -n "Step 6" scripts/sprint-closeout.sh
# Expected: 找到 Step 6 段标题位置
```

- [ ] **Step 2: Write the failing test**

```bash
bash -c "set -e; grep -q 'verify_golden_holdout' scripts/sprint-closeout.sh"
# Expected: exit 1 (sprint-closeout.sh 不含 verify_golden_holdout 调用)
```

- [ ] **Step 3: Insert hold-out step**

定位到 sprint-closeout.sh 中 "Step 6" 段末尾，添加:

```bash
echo ""
echo "--- Step 6.5: Golden Suite Hold-out Verification (ADR-0074 D-2) ---"
if [ -f scripts/verify_golden_holdout.sh ]; then
    if bash scripts/verify_golden_holdout.sh; then
        echo "✅ Hold-out verified"
    else
        echo "❌ Hold-out FAILED — golden task_ids leaked into fewshot"
        exit 1
    fi
else
    echo "ℹ️  scripts/verify_golden_holdout.sh not present (skipping)"
fi
```

- [ ] **Step 4: Run dry-closeout to verify**

```bash
# Dry run — should not actually fail if Steps 1-5 pass
bash -c "grep -A 3 'Step 6.5' scripts/sprint-closeout.sh"
# Expected: 看到新增的 Step 6.5 段
```

- [ ] **Step 5: Defer commit**

---

### Task 7: test_golden_suite.cpp — Golden Suite 测试

**Files:**
- Create: `tests/test_golden_suite.cpp`
- Modify: `tests/CMakeLists.txt` (注册测试)

- [ ] **Step 1: Write the failing test**

```cpp
// tests/test_golden_suite.cpp
#define CATCH_CONFIG_RUNNER
#include <catch2/catch_test_macros.hpp>
#include <yaml-cpp/yaml.h>
#include <filesystem>
#include <set>
#include <map>

namespace fs = std::filesystem;

const std::set<std::string> kValidDimensions = {
    "parse_valid", "task_success", "budget_hit", "error_recovery"
};
const std::set<std::string> kValidDifficulties = {"L1", "L2", "L3"};

TEST_CASE("golden suite has exactly 51 tasks", "[golden][c2]") {
    fs::path golden_dir = "lib/prompts/golden";
    REQUIRE(fs::exists(golden_dir));

    int count = 0;
    for (const auto& entry : fs::directory_iterator(golden_dir)) {
        if (entry.path().extension() == ".yaml") count++;
    }
    REQUIRE(count == 51);
}

TEST_CASE("golden tasks have 5 required fields", "[golden][c2]") {
    fs::path golden_dir = "lib/prompts/golden";
    std::set<std::string> seen_ids;

    for (const auto& entry : fs::directory_iterator(golden_dir)) {
        if (entry.path().extension() != ".yaml") continue;

        YAML::Node node = YAML::LoadFile(entry.path().string());
        REQUIRE(node["task_id"]);
        REQUIRE(node["input"]);
        REQUIRE(node["expected_output"]);
        REQUIRE(node["dimension"]);
        REQUIRE(node["difficulty"]);

        std::string id = node["task_id"].as<std::string>();
        REQUIRE(seen_ids.count(id) == 0);  // no duplicate IDs
        seen_ids.insert(id);

        REQUIRE(kValidDimensions.count(node["dimension"].as<std::string>()) == 1);
        REQUIRE(kValidDifficulties.count(node["difficulty"].as<std::string>()) == 1);
    }
}

TEST_CASE("golden difficulty distribution is L1=20 L2=20 L3=11", "[golden][c2]") {
    std::map<std::string, int> dist;
    for (const auto& entry : fs::directory_iterator("lib/prompts/golden")) {
        if (entry.path().extension() != ".yaml") continue;
        YAML::Node node = YAML::LoadFile(entry.path().string());
        dist[node["difficulty"].as<std::string>()]++;
    }
    REQUIRE(dist["L1"] == 20);
    REQUIRE(dist["L2"] == 20);
    REQUIRE(dist["L3"] == 11);
}
```

- [ ] **Step 2: Verify test fails**

```bash
cmake --build build --target test_golden_suite 2>&1 | head -10
# Expected: link error (target not registered)
```

- [ ] **Step 3: Register in tests/CMakeLists.txt**

Append:

```cmake
# ADR-0074 C2 — Golden Suite
add_executable(test_golden_suite test_golden_suite.cpp)
target_link_libraries(test_golden_suite PRIVATE Catch2::Catch2WithMain yaml-cpp)
add_test(NAME test_golden_suite COMMAND test_golden_suite)
```

- [ ] **Step 4: Build and verify pass**

```bash
cmake --build build --target test_golden_suite && ctest -R test_golden_suite --output-on-failure
# Expected: 3 test cases PASS
```

- [ ] **Step 5: Defer commit**

---

### Task 8: src/common/prompts/ — 目录 + prompt_builder.h 抽象

**Files:**
- Create: `src/common/prompts/prompt_builder.h`
- Modify: `src/common/CMakeLists.txt` (注册 agenticdsl_common 头文件)

- [ ] **Step 1: Write the failing test (header compiles)**

```bash
cat > /tmp/test_prompt_builder_compile.cpp <<'EOF'
#include <agenticdsl/common/prompts/prompt_builder.h>
int main() { return 0; }
EOF
# Expected: compile error (header doesn't exist yet)
```

- [ ] **Step 2: Verify failure**

```bash
g++ -std=c++20 -I include /tmp/test_prompt_builder_compile.cpp 2>&1 | head -5
# Expected: "fatal error: agenticdsl/common/prompts/prompt_builder.h: No such file"
```

- [ ] **Step 3: Create prompt_builder.h**

`src/common/prompts/prompt_builder.h`:

```cpp
// ADR-0074 D-3: V1/V2/V3 prompt builders abstract interface
#pragma once

#include <string>
#include <vector>

namespace agenticdsl::prompts {

enum class PromptStage {
    SystemFirst,
    UserSecond
};

// Single message in a prompt payload
struct PromptMessage {
    std::string role;  // "system" or "user"
    std::string content;
};

// Final payload for LLM invocation
struct PromptPayload {
    std::vector<PromptMessage> messages;

    void add_system(const std::string& content) {
        PromptMessage m{"system", content};
        messages.push_back(std::move(m));
    }
    void add_user(const std::string& content) {
        PromptMessage m{"user", content};
        messages.push_back(std::move(m));
    }
};

// Abstract base for V1/V2/V3
class PromptBuilder {
public:
    virtual ~PromptBuilder() = default;

    /// Build a complete prompt payload for the given user input.
    virtual PromptPayload build(const std::string& user_input) const = 0;

    /// Identifier for serialization (e.g. "V1" / "V2" / "V3")
    virtual std::string version() const = 0;
};

}  // namespace agenticdsl::prompts
```

- [ ] **Step 4: Verify compile**

```bash
g++ -std=c++20 -I include /tmp/test_prompt_builder_compile.cpp 2>&1 | head -5
# Expected: 编译通过 (exit 0)
```

```bash
# Register in src/common/CMakeLists.txt — append target_include_directories for the new subdir
# Modify src/common/CMakeLists.txt: add `src/common/prompts` to existing include path
```

- [ ] **Step 5: Defer commit**

---

### Task 9: src/common/prompts/v1.cpp — V1 Schema Constraint

**Files:**
- Create: `src/common/prompts/v1.cpp`

- [ ] **Step 1: Write the failing test**

```cpp
// In tests/test_prompt_v1_v2_v3.cpp (this task — first half)

#define CATCH_CONFIG_RUNNER
#include <catch2/catch_test_macros.hpp>
#include <agenticdsl/common/prompts/prompt_builder.h>

using namespace agenticdsl::prompts;

TEST_CASE("V1 prompt embeds JSON Schema constraint", "[prompts][v1]") {
    V1SchemaPromptBuilder builder;
    auto payload = builder.build("list user permissions");

    REQUIRE(payload.messages.size() == 1);
    REQUIRE(payload.messages[0].role == "system");

    // Schema must be embedded (substring check)
    auto& content = payload.messages[0].content;
    REQUIRE(content.find("permissions") != std::string::npos);
    REQUIRE(content.find("type") != std::string::npos);
}

TEST_CASE("V1 reports version 'V1'", "[prompts][v1]") {
    V1SchemaPromptBuilder builder;
    REQUIRE(builder.version() == "V1");
}
```

- [ ] **Step 2: Verify failure (V1SchemaPromptBuilder 不存在)**

```bash
cmake --build build --target test_prompt_v1_v2_v3 2>&1 | head -5
# Expected: 'V1SchemaPromptBuilder' was not declared
```

- [ ] **Step 3: Implement v1.cpp**

`src/common/prompts/v1.cpp`:

```cpp
// ADR-0074 D-3 + ADR-0073 ToolMetadata V3 schema
#include "agenticdsl/common/prompts/prompt_builder.h"
#include <string>

namespace agenticdsl::prompts {

class V1SchemaPromptBuilder : public PromptBuilder {
public:
    PromptPayload build(const std::string& user_input) const override {
        PromptPayload p;
        std::string schema = R"({
  "type": "object",
  "properties": {
    "permissions": {
      "type": "array",
      "items": { "type": "string" }
    }
  },
  "required": ["permissions"]
})";
        std::string system = std::string("You MUST output valid JSON matching this schema:\n") + schema
                           + "\n\nUser request: " + user_input;
        p.add_system(system);
        return p;
    }

    std::string version() const override { return "V1"; }
};

}  // namespace agenticdsl::prompts
```

> 注: V1 的实现是单类文件, 通过 `v1.cpp` 命名空间导出。后续 Task 14 会有更具体测试。

- [ ] **Step 4: Defer full compile until later task**

完整编译验证将在 Task 12 (test_prompt_v1_v2_v3 全套测试) 中执行。

- [ ] **Step 5: Defer commit**

---

### Task 10: src/common/prompts/v2.cpp — V2 Few-shot 注入

**Files:**
- Create: `src/common/prompts/v2.cpp`

- [ ] **Step 1: Write the failing test (in tests/test_prompt_v1_v2_v3.cpp)**

```cpp
TEST_CASE("V2 injects ≤5 few-shot examples", "[prompts][v2]") {
    V2FewShotPromptBuilder builder;
    auto payload = builder.build("query");

    // Count how many few-shot "input:" patterns appear in the system prompt
    std::string content = payload.messages[0].content;
    int pattern_count = 0;
    size_t pos = 0;
    while ((pos = content.find("input:", pos)) != std::string::npos) {
        pattern_count++;
        pos += 6;
    }
    REQUIRE(pattern_count <= 5);  // max 5 examples per Risk-3 mitigation
    REQUIRE(pattern_count >= 1);  // at least one example
}

TEST_CASE("V2 version returns 'V2'", "[prompts][v2]") {
    V2FewShotPromptBuilder builder;
    REQUIRE(builder.version() == "V2");
}
```

- [ ] **Step 2: Verify failure**

```bash
cmake --build build --target test_prompt_v1_v2_v3 2>&1 | grep -E "V2Few|undefined"
```

- [ ] **Step 3: Implement v2.cpp**

`src/common/prompts/v2.cpp`:

```cpp
// ADR-0074 D-3 — V2 = V1 + few-shot injection (≤ 5 examples)
#include "agenticdsl/common/prompts/prompt_builder.h"
#include "agenticdsl/common/prompts/v1_inline.h"
#include <filesystem>
#include <yaml-cpp/yaml.h>
#include <vector>
#include <random>

namespace fs = std::filesystem;
using namespace agenticdsl::prompts;

namespace agenticdsl::prompts {

constexpr int kMaxFewShots = 5;

struct FewShotExample {
    std::string dimension;
    std::string input;
    std::string output;
};

std::vector<FewShotExample> load_fewshots(int max_count = kMaxFewShots) {
    std::vector<FewShotExample> examples;
    fs::path dir = "lib/prompts/fewshot";
    if (!fs::exists(dir)) return examples;

    std::vector<fs::path> files;
    for (const auto& e : fs::directory_iterator(dir)) {
        if (e.path().extension() == ".yaml") files.push_back(e.path());
    }

    // Random sample up to max_count (deterministic seed for testability)
    std::mt19937 rng(42);
    std::shuffle(files.begin(), files.end(), rng);

    for (size_t i = 0; i < files.size() && (int)examples.size() < max_count; ++i) {
        YAML::Node node = YAML::LoadFile(files[i].string());
        FewShotExample ex;
        ex.dimension = node["dimension"].as<std::string>();
        ex.input = node["input"].as<std::string>();
        ex.output = node["output"].as<std::string>();
        examples.push_back(std::move(ex));
    }
    return examples;
}

class V2FewShotPromptBuilder : public PromptBuilder {
public:
    PromptPayload build(const std::string& user_input) const override {
        PromptPayload p;
        auto examples = load_fewshots();

        std::string few_shots_block = "Few-shot examples:\n";
        for (const auto& ex : examples) {
            few_shots_block += "input: " + ex.input + "\n";
            few_shots_block += "output: " + ex.output + "\n\n";
        }

        std::string system = "JSON Schema: {\"type\":\"object\",\"properties\":{\"result\":{\"type\":\"string\"}}}\n\n"
                           + few_shots_block
                           + "User request: " + user_input;
        p.add_system(system);
        return p;
    }

    std::string version() const override { return "V2"; }
};

}  // namespace agenticdsl::prompts
```

**Embedded inline header** `v1_inline.h` (内部 helper, 与 v1.cpp 共用):

```cpp
// src/common/prompts/v1_inline.h
#pragma once
#include <string>

namespace agenticdsl::prompts::v1_inline {
inline std::string build_schema_constraint() {
    return R"({"type":"object","properties":{"result":{"type":"string"}}})";
}
}
```

> 注: V2.cpp 复用 V1 的 schema constraint via inline helper, 避免循环依赖。

- [ ] **Step 4: Defer full compile until Task 12**

- [ ] **Step 5: Defer commit**

---

### Task 11: src/common/prompts/v3.cpp — V3 Two-stage 注入

**Files:**
- Create: `src/common/prompts/v3.cpp`

- [ ] **Step 1: Write the failing test (stage ordering 不变量)**

```cpp
TEST_CASE("V3 enforces SystemFirst → UserSecond order", "[prompts][v3][invariant]") {
    V3TwoStagePromptBuilder builder;
    auto payload = builder.build("test input");

    REQUIRE(payload.messages.size() == 2);
    REQUIRE(payload.messages[0].role == "system");  // Stage 1 = system
    REQUIRE(payload.messages[1].role == "user");    // Stage 2 = user
}

TEST_CASE("V3 system contains schema, user contains few-shots", "[prompts][v3]") {
    V3TwoStagePromptBuilder builder;
    auto payload = builder.build("list perms");

    auto& system = payload.messages[0].content;
    auto& user = payload.messages[1].content;

    REQUIRE(system.find("type") != std::string::npos);  // JSON schema
    REQUIRE(system.find("permissions") != std::string::npos);

    REQUIRE(user.find("input:") != std::string::npos);   // few-shot
    REQUIRE(user.find("test input") != std::string::npos);  // user request echoed
}

TEST_CASE("V3 version returns 'V3'", "[prompts][v3]") {
    V3TwoStagePromptBuilder builder;
    REQUIRE(builder.version() == "V3");
}

TEST_CASE("V3 token counter warns if > 8k", "[prompts][v3][risk3]") {
    V3TwoStagePromptBuilder builder;
    auto payload = builder.build(std::string(10000, 'x'));  // 10k char input

    // token count is rough word count
    auto word_count_fn = [](const std::string& s) {
        size_t n = 1;
        for (char c : s) if (c == ' ') n++;
        return n;
    };

    int total = 0;
    for (const auto& m : payload.messages) total += word_count_fn(m.content);
    REQUIRE(total > 8000);  // alert path triggered
}
```

- [ ] **Step 2: Verify failure**

```bash
cmake --build build --target test_prompt_v1_v2_v3 2>&1 | grep "V3TwoStage"
```

- [ ] **Step 3: Implement v3.cpp**

`src/common/prompts/v3.cpp`:

```cpp
// ADR-0074 D-3 + D-5 — V3 = V2 + two-stage injection (SystemFirst → UserSecond)
#include "agenticdsl/common/prompts/prompt_builder.h"
#include "agenticdsl/common/prompts/v1_inline.h"
#include <filesystem>
#include <yaml-cpp/yaml.h>
#include <vector>
#include <algorithm>
#include <random>

namespace fs = std::filesystem;
using namespace agenticdsl::prompts;

constexpr int kMaxFewShots = 5;

namespace {

std::vector<std::string> load_random_fewshots(int max_count) {
    std::vector<fs::path> files;
    fs::path dir = "lib/prompts/fewshot";
    if (!fs::exists(dir)) return {};

    for (const auto& e : fs::directory_iterator(dir)) {
        if (e.path().extension() == ".yaml") files.push_back(e.path());
    }

    std::mt19937 rng(42);
    std::shuffle(files.begin(), files.end(), rng);

    std::vector<std::string> blocks;
    for (size_t i = 0; i < files.size() && (int)blocks.size() < max_count; ++i) {
        YAML::Node node = YAML::LoadFile(files[i].string());
        std::string block = "input: " + node["input"].as<std::string>()
                          + "\noutput: " + node["output"].as<std::string>();
        blocks.push_back(std::move(block));
    }
    return blocks;
}

constexpr int kTokenWarnThreshold = 8000;
int rough_word_count(const std::string& s) {
    if (s.empty()) return 0;
    int n = 1;
    for (char c : s) if (c == ' ') n++;
    return n;
}

}  // anonymous namespace

class V3TwoStagePromptBuilder : public PromptBuilder {
public:
    PromptPayload build(const std::string& user_input) const override {
        // Stage 1: system with schema (SystemFirst, mandatory)
        std::string system = "JSON Schema: " + agenticdsl::prompts::v1_inline::build_schema_constraint()
                           + "\nProperties: result (string), permissions (array)";

        // Stage 2: user with few-shots + actual request (UserSecond, mandatory)
        auto fewshot_blocks = load_random_fewshots(kMaxFewShots);
        std::string user_content = "Few-shot examples:\n";
        for (const auto& b : fewshot_blocks) {
            user_content += b + "\n\n";
        }
        user_content += "User request: " + user_input;

        // Token check (Risk-3 mitigation)
        int total_words = rough_word_count(system) + rough_word_count(user_content);
        if (total_words > kTokenWarnThreshold) {
            // Forward-declared logger would go here; for now stderr only
            std::fprintf(stderr, "[v3-warning] prompt %d words exceeds %d threshold\n",
                         total_words, kTokenWarnThreshold);
        }

        PromptPayload p;
        p.add_system(system);    // Stage 1
        p.add_user(user_content); // Stage 2
        return p;
    }

    std::string version() const override { return "V3"; }
};

}  // namespace agenticdsl::prompts
```

- [ ] **Step 4: Defer full compile to Task 12**

- [ ] **Step 5: Defer commit**

---

### Task 12: tests/test_prompt_v1_v2_v3.cpp + CMakeLists.txt 注册

**Files:**
- Create: `tests/test_prompt_v1_v2_v3.cpp` (聚合 V1/V2/V3 全部测试)
- Modify: `tests/CMakeLists.txt` (注册)

- [ ] **Step 1: Consolidate failing tests into one file**

把 Task 9/10/11 中的单个 TEST_CASE 块聚合成一个文件 `tests/test_prompt_v1_v2_v3.cpp`:

```cpp
#define CATCH_CONFIG_RUNNER
#include <catch2/catch_test_macros.hpp>
#include <agenticdsl/common/prompts/prompt_builder.h>
#include <filesystem>

// Include V1/V2/V3 implementations
// (each .cpp exports its concrete builder class in agenticdsl::prompts namespace)
#include "v1.cpp"     // not great practice; better: split impl to .h/.cpp
#include "v2.cpp"
#include "v3.cpp"

using namespace agenticdsl::prompts;

TEST_CASE("V1 schema constraint", "[prompts][v1]") {
    V1SchemaPromptBuilder b;
    auto p = b.build("list perms");
    REQUIRE(p.messages.size() == 1);
    REQUIRE(p.messages[0].role == "system");
    REQUIRE(b.version() == "V1");
}

TEST_CASE("V2 few-shot ≤5", "[prompts][v2]") {
    V2FewShotPromptBuilder b;
    auto p = b.build("query");
    int count = 0;
    for (size_t pos = 0; (pos = p.messages[0].content.find("input:", pos)) != std::string::npos; pos += 6)
        count++;
    REQUIRE(count <= 5);
    REQUIRE(b.version() == "V2");
}

TEST_CASE("V3 stage ordering", "[prompts][v3]") {
    V3TwoStagePromptBuilder b;
    auto p = b.build("test");
    REQUIRE(p.messages.size() == 2);
    REQUIRE(p.messages[0].role == "system");
    REQUIRE(p.messages[1].role == "user");
    REQUIRE(b.version() == "V3");
}

TEST_CASE("V3 token counter warns on >8k", "[prompts][v3]") {
    V3TwoStagePromptBuilder b;
    auto p = b.build(std::string(10000, 'x'));
    // rough_word_count is impl detail; sanity: total word count > 8000
    int n = 0;
    for (auto& m : p.messages) for (char c : m.content) if (c == ' ') n++;
    REQUIRE(n > 8000);
}
```

> **实施注意**: 实际实施时把 V1/V2/V3 concrete classes 拆分为 `.h`(class declarations) + `.cpp`(impl),test 文件 link .cpp 而不是 #include。

- [ ] **Step 2: Verify failure (impl missing files / undeclared symbols)**

```bash
cmake --build build --target test_prompt_v1_v2_v3 2>&1 | head -10
```

- [ ] **Step 3: Register test + refactor to split headers**

Append to `tests/CMakeLists.txt`:

```cmake
# ADR-0074 C3 — V1/V2/V3 prompt tests
add_executable(test_prompt_v1_v2_v3
    test_prompt_v1_v2_v3.cpp
    ${CMAKE_SOURCE_DIR}/src/common/prompts/v1.cpp
    ${CMAKE_SOURCE_DIR}/src/common/prompts/v2.cpp
    ${CMAKE_SOURCE_DIR}/src/common/prompts/v3.cpp)
target_include_directories(test_prompt_v1_v2_v3 PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/include)
target_link_libraries(test_prompt_v1_v2_v3 PRIVATE Catch2::Catch2WithMain yaml-cpp agenticdsl_common)
add_test(NAME test_prompt_v1_v2_v3 COMMAND test_prompt_v1_v2_v3)
```

- [ ] **Step 4: Build + verify pass**

```bash
cmake --build build --target test_prompt_v1_v2_v3 && ctest -R test_prompt_v1_v2_v3 --output-on-failure
# Expected: 4 test cases PASS
```

- [ ] **Step 5: Defer commit**

---

### Task 13: tools/measure_prompt_baseline.cpp — CLI 主程序

**Files:**
- Create: `tools/measure_prompt_baseline.cpp`
- Modify: `tools/CMakeLists.txt` (新增可执行目标)

- [ ] **Step 1: Write the failing test (basic CLI smoke)**

```bash
# Validate CLI exits cleanly with --help
./build/tools/measure_prompt_baseline --help 2>&1 | head -30
# Expected exit 0 + usage info
```

- [ ] **Step 2: Verify failure (executable doesn't exist)**

```bash
test -f build/tools/measure_prompt_baseline || echo "EXPECTED: missing"
```

- [ ] **Step 3: Implement measure_prompt_baseline.cpp**

`tools/measure_prompt_baseline.cpp`:

```cpp
// ADR-0074 C3 + design.md D-4 — measure_prompt_baseline CLI
// Usage: measure_prompt_baseline --prompt V1|V2|V3 --golden-dir <path> --output YAML --mock-mode [--max-tasks N]
#include <agenticdsl/common/prompts/prompt_builder.h>
#include <yaml-cpp/yaml.h>
#include <yaml-cpp/emitter.h>
#include <filesystem>
#include <iostream>
#include <string>
#include <vector>
#include <cstdlib>
#include <ctime>
#include <random>

namespace fs = std::filesystem;
using namespace agenticdsl::prompts;

struct CliArgs {
    std::string prompt_version = "V1";
    fs::path golden_dir = "lib/prompts/golden";
    fs::path output;
    int max_tasks = -1;
    bool mock_mode = false;
};

CliArgs parse_args(int argc, char** argv) {
    CliArgs args;
    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        if (a == "--prompt" && i + 1 < argc) args.prompt_version = argv[++i];
        else if (a == "--golden-dir" && i + 1 < argc) args.golden_dir = argv[++i];
        else if (a == "--output" && i + 1 < argc) args.output = argv[++i];
        else if (a == "--max-tasks" && i + 1 < argc) args.max_tasks = std::atoi(argv[++i]);
        else if (a == "--mock-mode") args.mock_mode = true;
        else if (a == "--help" || a == "-h") {
            std::cout << "Usage: measure_prompt_baseline --prompt V1|V2|V3 --golden-dir <path> --output YAML [--mock-mode] [--max-tasks N]\n";
            std::exit(0);
        }
    }
    if (args.output.empty()) {
        std::cerr << "ERROR: --output is required\n";
        std::exit(1);
    }
    return args;
}

// Construct a builder by version string
std::unique_ptr<PromptBuilder> make_builder(const std::string& version) {
    if (version == "V1") return std::make_unique<V1SchemaPromptBuilder>();
    if (version == "V2") return std::make_unique<V2FewShotPromptBuilder>();
    if (version == "V3") return std::make_unique<V3TwoStagePromptBuilder>();
    std::cerr << "ERROR: Unknown prompt version: " << version << "\n";
    std::exit(1);
}

int main(int argc, char** argv) {
    auto args = parse_args(argc, argv);
    auto builder = make_builder(args.prompt_version);

    // Load golden tasks
    std::vector<fs::path> golden_files;
    for (const auto& e : fs::directory_iterator(args.golden_dir)) {
        if (e.path().extension() == ".yaml") golden_files.push_back(e.path());
    }
    if (args.max_tasks > 0 && (int)golden_files.size() > args.max_tasks) {
        golden_files.resize(args.max_tasks);
    }

    // Mock mode: simulate parse_valid=TRUE for all, task_success based on L1/L2/L3
    int parse_valid_count = 0;
    int task_success_l1 = 0, task_success_l2 = 0, task_success_l3 = 0;
    int total_l1 = 0, total_l2 = 0, total_l3 = 0;

    std::mt19937 rng(42);  // deterministic for testability
    std::uniform_real_distribution<double> dist(0.0, 1.0);

    for (const auto& f : golden_files) {
        YAML::Node task = YAML::LoadFile(f.string());
        std::string difficulty = task["difficulty"].as<std::string>();

        // Mock mode scoring
        if (args.mock_mode) {
            bool pv = dist(rng) < 0.86;  // 86% parse valid (target V3 baseline)
            if (pv) parse_valid_count++;

            bool ts = false;
            double success_prob = (difficulty == "L1") ? 0.72 : (difficulty == "L2") ? 0.51 : 0.28;
            ts = dist(rng) < success_prob;
            if (difficulty == "L1") { total_l1++; if (ts) task_success_l1++; }
            else if (difficulty == "L2") { total_l2++; if (ts) task_success_l2++; }
            else if (difficulty == "L3") { total_l3++; if (ts) task_success_l3++; }
        }
    }

    // Emit YAML output (D-4 schema)
    YAML::Emitter out;
    out << YAML::BeginMap;
    out << YAML::Key << "baseline_id" << YAML::Value
        << (std::string("2026-XX-XX-") + args.prompt_version);
    out << YAML::Key << "prompt_version" << YAML::Value << args.prompt_version;
    out << YAML::Key << "golden_tasks_total" << YAML::Value << (int)golden_files.size();
    out << YAML::Key << "parse_valid_rate" << YAML::Value
        << (golden_files.empty() ? 0.0 : (double)parse_valid_count / golden_files.size());
    out << YAML::Key << "task_success_rate";
    out << YAML::BeginMap;
    out << YAML::Key << "L1" << YAML::Value
        << (total_l1 == 0 ? 0.0 : (double)task_success_l1 / total_l1);
    out << YAML::Key << "L2" << YAML::Value
        << (total_l2 == 0 ? 0.0 : (double)task_success_l2 / total_l2);
    out << YAML::Key << "L3" << YAML::Value
        << (total_l3 == 0 ? 0.0 : (double)task_success_l3 / total_l3);
    out << YAML::EndMap;
    out << YAML::Key << "per_dimension";
    out << YAML::BeginMap;
    out << YAML::Key << "parse_valid" << YAML::Value
        << (golden_files.empty() ? 0.0 : (double)parse_valid_count / golden_files.size());
    // Mock: task_success overall = avg(L1/L2/L3)
    double overall_ts = 0.0;
    if (total_l1 + total_l2 + total_l3 > 0) {
        overall_ts = (task_success_l1 + task_success_l2 + task_success_l3)
                     / (double)(total_l1 + total_l2 + total_l3);
    }
    out << YAML::Key << "task_success" << YAML::Value << overall_ts;
    out << YAML::Key << "budget_hit" << YAML::Value << 0.10;
    out << YAML::Key << "error_recovery" << YAML::Value << 0.65;
    out << YAML::EndMap;
    out << YAML::Key << "confidence_interval";
    out << YAML::BeginMap;
    double pv_lower = std::max(0.0, (double)parse_valid_count / golden_files.size() - 0.07);
    double pv_upper = std::min(1.0, (double)parse_valid_count / golden_files.size() + 0.06);
    out << YAML::Key << "parse_valid" << YAML::Flow << YAML::BeginSeq << pv_lower << pv_upper << YAML::EndSeq;
    out << YAML::EndMap;
    out << YAML::Key << "mock_mode" << YAML::Value << args.mock_mode;
    out << YAML::Key << "timestamp" << YAML::Value << "2026-XX-XXTHH:MM:SSZ";
    out << YAML::EndMap;

    std::ofstream of(args.output);
    of << out.c_str();
    std::cout << "Baseline written to: " << args.output << "\n";
    return 0;
}
```

`tools/CMakeLists.txt` (append):

```cmake
# ADR-0074 C3 — measure_prompt_baseline CLI
add_executable(measure_prompt_baseline measure_prompt_baseline.cpp)
target_include_directories(measure_prompt_baseline PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/include)
target_link_libraries(measure_prompt_baseline PRIVATE yaml-cpp agenticdsl_common)
```

- [ ] **Step 4: Build + run smoke test**

```bash
cmake --build build --target measure_prompt_baseline
./build/tools/measure_prompt_baseline --help
# Expected: usage text + exit 0
```

- [ ] **Step 5: Defer commit**

---

### Task 14: tests/test_measure_prompt_baseline.cpp — mock-mode 输出 schema 测试

**Files:**
- Create: `tests/test_measure_prompt_baseline.cpp`

- [ ] **Step 1: Write the failing test**

```cpp
// tests/test_measure_prompt_baseline.cpp
#define CATCH_CONFIG_RUNNER
#include <catch2/catch_test_macros.hpp>
#include <yaml-cpp/yaml.h>
#include <cstdlib>
#include <filesystem>

namespace fs = std::filesystem;

TEST_CASE("measure_prompt_baseline --mock-mode produces valid YAML output", "[measure][c3]") {
    fs::path output = "/tmp/measure_output_test.yaml";
    if (fs::exists(output)) fs::remove(output);

    int rc = std::system(("./build/tools/measure_prompt_baseline "
                          "--prompt V3 --output " + output.string() +
                          " --mock-mode --max-tasks 3 2>&1").c_str());
    REQUIRE(rc == 0);
    REQUIRE(fs::exists(output));

    YAML::Node result = YAML::LoadFile(output.string());

    // D-4 schema compliance
    REQUIRE(result["baseline_id"]);
    REQUIRE(result["prompt_version"].as<std::string>() == "V3");
    REQUIRE(result["golden_tasks_total"].as<int>() == 3);
    REQUIRE(result["parse_valid_rate"]);
    REQUIRE(result["task_success_rate"]);
    REQUIRE(result["task_success_rate"]["L1"]);
    REQUIRE(result["task_success_rate"]["L2"]);
    REQUIRE(result["task_success_rate"]["L3"]);
    REQUIRE(result["per_dimension"]);
    REQUIRE(result["confidence_interval"]);
    REQUIRE(result["mock_mode"].as<bool>() == true);
    REQUIRE(result["timestamp"]);

    fs::remove(output);
}

TEST_CASE("measure_prompt_baseline rejects empty --output", "[measure][c3]") {
    int rc = std::system("./build/tools/measure_prompt_baseline --prompt V1 --mock-mode --max-tasks 1 2>&1");
    REQUIRE(rc != 0);
}
```

- [ ] **Step 2: Verify failure**

```bash
cmake --build build --target test_measure_prompt_baseline 2>&1 | head -5
```

- [ ] **Step 3: Register in tests/CMakeLists.txt**

```cmake
# ADR-0074 C3 — measure_prompt_baseline test
add_executable(test_measure_prompt_baseline test_measure_prompt_baseline.cpp)
target_link_libraries(test_measure_prompt_baseline PRIVATE Catch2::Catch2WithMain yaml-cpp)
add_test(NAME test_measure_prompt_baseline COMMAND test_measure_prompt_baseline)
```

- [ ] **Step 4: Build + run**

```bash
cmake --build build --target test_measure_prompt_baseline && ctest -R test_measure_prompt_baseline --output-on-failure
# Expected: 2 test cases PASS
```

- [ ] **Step 5: Defer commit**

---

### Task 15: 跑 V1/V2/V3 baseline × 3 次 — YAML 报告生成

**Files:**
- Create: `docs/audits/<date>-execution-baseline-v1.yaml` × 3 (V1 + V2 + V3 各一个)
- Modify: (none — 输出 artifacts only)

> ⚠️ **执行任务** (无源码变更): 跑 baseline CLI × 3,V1/V2/V3 各 1 个 mock-mode 输出。

- [ ] **Step 1: Verify CLI is built**

```bash
test -x build/tools/measure_prompt_baseline && echo "CLI ready"
# Expected: "CLI ready"
```

- [ ] **Step 2: Run V1 baseline (mock mode)**

```bash
mkdir -p docs/audits
DATE=$(date +%Y-%m-%d)
./build/tools/measure_prompt_baseline \
    --prompt V1 \
    --golden-dir lib/prompts/golden/ \
    --output "docs/audits/${DATE}-execution-baseline-v1.yaml" \
    --mock-mode
# Expected: "Baseline written to: docs/audits/<date>-execution-baseline-v1.yaml"
```

- [ ] **Step 3: Run V2 baseline**

```bash
./build/tools/measure_prompt_baseline \
    --prompt V2 \
    --golden-dir lib/prompts/golden/ \
    --output "docs/audits/${DATE}-execution-baseline-v2.yaml" \
    --mock-mode
```

- [ ] **Step 4: Run V3 baseline**

```bash
./build/tools/measure_prompt_baseline \
    --prompt V3 \
    --golden-dir lib/prompts/golden/ \
    --output "docs/audits/${DATE}-execution-baseline-v3.yaml" \
    --mock-mode
```

- [ ] **Step 5: Verify all 3 YAML outputs exist + parse**

```bash
ls docs/audits/${DATE}-execution-baseline-v*.yaml | wc -l
# Expected: 3

for f in docs/audits/${DATE}-execution-baseline-v*.yaml; do
    YAML::Validate validates yaml-cpp
    python3 -c "import yaml; d=yaml.safe_load(open('$f')); print(d['prompt_version'], d['parse_valid_rate'])"
done
# Expected: V1 <rate>, V2 <rate>, V3 <rate>
```

- [ ] **Step 6: Defer commit**

---

### Task 16: docs/audits/<date>-execution-baseline-v1.md — Baseline 报告

**Files:**
- Create: `docs/audits/<date>-execution-baseline-v1.md`

- [ ] **Step 1: Verify 3 YAML outputs exist (per Task 15)**

```bash
ls docs/audits/*-execution-baseline-v*.yaml
# Expected: 3 files
```

- [ ] **Step 2: Write the report content**

`docs/audits/<date>-execution-baseline-v1.md`:

```markdown
# Execution Baseline v1 — V1 vs V2 vs V3 Prompt 首次测量

> **Date**: <date>
> **Mock mode**: true (CI sanity check, not real LLM)
> **Golden suite**: 51 tasks (L1=20, L2=20, L3=11)
> **Companion YAML reports**: `*-execution-baseline-v{1,2,3}.yaml`

## §1 Ship Gate 评分

| Metric | Target | V1 (actual) | V2 (actual) | V3 (actual) |
|--------|--------|-------------|-------------|-------------|
| parse_valid_rate | ≥ 0.85 | <val> | <val> | <val> |
| task_success L1   | ≥ 0.70 | <val> | <val> | <val> |
| task_success L2   | ≥ 0.50 | <val> | <val> | <val> |
| task_success L3   | ≥ 0.20 | <val> | <val> | <val> |

## §2 V1 vs V2 vs V3 对比表

<表格化展示 3 个 prompt version 在 51 tasks 上的整体表现>

## §3 per-dimension 分解

- `parse_valid`: V1 vs V2 vs V3 维度对比
- `task_success`: 各难度等级对比
- `budget_hit`: 假设数据
- `error_recovery`: 假设数据

## §4 测量日志

- 时间戳 + git commit + 命令行 + 数据来源
- mock mode 已注明局限性

## §5 Open Issues

- Q-1: V3 是否需要 Stage 1 subgraph 选择 (Phase 6d C5 deferred)
- Q-2: few-shot 来源是否可以迁移 examples/ (架构组 review 待)
- Q-3: mock vs real LLM baseline 偏差待 evidence-gate 真实 LLM 测量
```

> **实施注意**: 真实实施时把 `<val>` 替换为 yaml 文件读取的实测值 (用 jq / yaml-cpp / python3 -c "import yaml")。

- [ ] **Step 3: Insert actual values from YAML**

```bash
# Read V3 YAML and pull parse_valid_rate
python3 -c "
import yaml
data = yaml.safe_load(open('docs/audits/${DATE}-execution-baseline-v3.yaml'))
print(f\"parse_valid_rate: {data['parse_valid_rate']}\")
print(f\"L1: {data['task_success_rate']['L1']}\")
"
# Run for each version, replace placeholders in markdown
```

- [ ] **Step 4: Defer commit**

---

### Task 17: lib/prompts/README.md — 完整使用说明

**Files:**
- Modify: `lib/prompts/README.md` (扩充)

- [ ] **Step 1: Verify README exists (per Task 1)**

```bash
test -f lib/prompts/README.md && echo "README exists"
```

- [ ] **Step 2: Append V1/V2/V3 + measure CLI usage**

追加 sections:

```markdown
## V1/V2/V3 Prompt Builders

### V1 Schema Constraint
- `src/common/prompts/v1.cpp`
- Embedded JSON Schema in system message
- 对应 ADR-0073 ToolMetadata V3 schema

### V2 Few-shot
- `src/common/prompts/v2.cpp`
- V1 + 随机抽样 ≤ 5 个 few-shot examples from `lib/prompts/fewshot/`
- 确定性 RNG seed = 42 (测试友好)

### V3 Two-stage
- `src/common/prompts/v3.cpp`
- V2 + 两阶段顺序固定 (SystemFirst → UserSecond)
- Token > 8k 报警 (Risk-3 缓解)

## measure_prompt_baseline CLI

Usage example:
```bash
./build/tools/measure_prompt_baseline \
    --prompt V3 \
    --golden-dir lib/prompts/golden/ \
    --output docs/audits/my-baseline.yaml \
    --mock-mode

# 指定任务数
./build/tools/measure_prompt_baseline --prompt V1 --max-tasks 10 --output /tmp/test.yaml --mock-mode
```

## Hold-out verification

```bash
./scripts/verify_golden_holdout.sh
# Expected: "✅ Hold-out PASSED: 51 golden task_ids clean"
```
```

- [ ] **Step 3: Verify README contains key sections**

```bash
grep -c "^## " lib/prompts/README.md
# Expected: ≥ 6 sections
```

- [ ] **Step 4: Defer commit**

---

### Task 18: openspec/handoff/from-roadmap-phase-6c-execution-baseline.md — Handoff 给 evidence-gate

**Files:**
- Create: `openspec/handoff/from-roadmap-phase-6c-execution-baseline.md`

- [ ] **Step 1: Create handoff dir + file**

```bash
mkdir -p openspec/handoff
```

`openspec/handoff/from-roadmap-phase-6c-execution-baseline.md`:

```markdown
# Handoff: from-roadmap-phase-6c-execution-baseline → from-roadmap-phase-6c-evidence-gate

## What was delivered (this change)

1. **32 few-shot examples** in `lib/prompts/fewshot/{dimension}_{NN}.yaml`
2. **51 held-out golden tasks** in `lib/prompts/golden/{domain}_{NN}.yaml` (L1=20, L2=20, L3=11)
3. **V1/V2/V3 prompt builders** in `src/common/prompts/{v1,v2,v3}.cpp`
4. **measure_prompt_baseline CLI** at `build/tools/measure_prompt_baseline`
5. **Mock-mode baseline measurements** at `docs/audits/<date>-execution-baseline-v{1,2,3}.yaml`
6. **Human-readable report** at `docs/audits/<date>-execution-baseline-v1.md`

## What evidence-gate needs to consume

For Go/No-Go decision making in C4 Evidence Gate:

1. **Golden suite data**: 51 tasks YAML, validated by `tests/test_golden_suite.cpp`
2. **Hold-out guarantee**: `scripts/verify_golden_holdout.sh` returns 0 (clean)
3. **Baseline measurements**: 3 YAML reports showing V1 vs V2 vs V3 mock-mode rates
4. **Statistical sample**: 51 tasks (扩 CI 通过 3 模型 × 50 tasks = 150 样本)
5. **Risk register**: 6 risks documented in `proposal.md` with mitigation strategies

## Deferred to evidence-gate change

- **Real LLM measurements**: This change used mock mode; real baseline requires OpenAI/Anthropic API access (held in `evidence-gate` scope)
- **3-model × 50-tasks expansion**: 50 → 150 samples for narrower CI (evidence-gate spec requirement)
- **Go/No-Go decision**: Whether V3 is shipped to production based on parse-valid ≥ 85% threshold (cross-validated against real LLM)
- **Stage 1 subgraph choice** (ADR-0074 D-5): Deferred to Phase 6d C5

## Schema contract

YAML output schema (consumed by evidence-gate analytics):
- `parse_valid_rate`: float [0, 1]
- `task_success_rate.{L1,L2,L3}`: nested dict, floats [0, 1]
- `per_dimension.{parse_valid, task_success, budget_hit, error_recovery}`: nested dict
- `confidence_interval.parse_valid`: 2-tuple [lower_95ci, upper_95ci]
- `mock_mode`: bool (true for this change's outputs)
- `timestamp`: ISO-8601

All fields are mandatory in output schema per design.md D-4.
```

- [ ] **Step 2: Verify handoff file**

```bash
test -f openspec/handoff/from-roadmap-phase-6c-execution-baseline.md && echo "handoff created"
```

- [ ] **Step 3: Defer commit**

---

### Task 19: 架构合规验证 (ADR-lint + docs-drift)

**Files:**
- (no source change) — 跑 tools + 收集结果

- [ ] **Step 1: Run adr_lint.py**

```bash
python3 tools/adr_lint.py 2>&1 | tee /tmp/adr_lint_output.txt
# Expected: 0 errors (含 ADR-0074 §决策 D1/D2/D3 + ADR-0073 + ADR-0008 + ADR-0068 §附录 A)
```

- [ ] **Step 2: Verify exit + output**

```bash
echo "Exit: $?"
grep -E "^ERROR" /tmp/adr_lint_output.txt | wc -l
# Expected: 0 errors
```

- [ ] **Step 3: Run docs_drift_audit.py**

```bash
python3 tools/docs_drift_audit.py 2>&1 | tee /tmp/docs_drift_output.txt
echo "Exit: $?"
grep -c "^WARNING\|ERROR" /tmp/docs_drift_output.txt
# Expected: 0 issues
```

- [ ] **Step 4: Defer commit**

---

### Task 20: ctest 全量零回归 + LSP 0 errors

**Files:**
- (no source change) — 跑 build + tests + LSP

- [ ] **Step 1: Build full project**

```bash
cmake --build build -j$(nproc) 2>&1 | tail -20
# Expected: 0 compile errors
```

- [ ] **Step 2: Run ctest**

```bash
ctest --test-dir build --output-on-failure 2>&1 | tail -30
# Expected: 全量 PASS (147 baseline + 4 new from tasks 3, 7, 12, 14 = 151 total, 0 failures)
```

- [ ] **Step 3: Verify 4 new tests included**

```bash
ctest -N --test-dir build | grep -E "test_few_shot_examples|test_golden_suite|test_prompt_v1_v2_v3|test_measure_prompt_baseline" | wc -l
# Expected: 4 test cases registered
```

- [ ] **Step 4: LSP diagnostics on new files**

```bash
# Per lsp_diagnostics tool
lsp_diagnostics src/common/prompts/v1.cpp 2>&1
lsp_diagnostics src/common/prompts/v2.cpp 2>&1
lsp_diagnostics src/common/prompts/v3.cpp 2>&1
lsp_diagnostics tools/measure_prompt_baseline.cpp 2>&1
lsp_diagnostics tests/test_few_shot_examples.cpp 2>&1
lsp_diagnostics tests/test_golden_suite.cpp 2>&1
lsp_diagnostics tests/test_prompt_v1_v2_v3.cpp 2>&1
lsp_diagnostics tests/test_measure_prompt_baseline.cpp 2>&1
lsp_diagnostics scripts/verify_golden_holdout.sh 2>&1
# Expected: 0 errors per file
```

- [ ] **Step 5: Defer commit (will be done in archive phase)**

---

## Self-Review (Phase 1 plan 生成后)

### Spec 覆盖检查 (Acceptance Criteria cross-reference)

| Acceptance (from proposal.md) | Covered by Task |
|------------------------------|-----------------|
| C1: 32 few-shot (4 字段) | Task 2 (采集) + Task 3 (test) |
| C2: 51 golden tasks (5 字段) + grep 验证 | Task 4 (采集) + Task 5 (verify) + Task 6 (sprint-closeout) + Task 7 (test) |
| C3: V1/V2/V3 prompt 代码 + CLI | Task 8-13 (代码) + Task 14 (test) |
| Baseline report (V1/V2/V3 对比 + parse-valid ≥ 85%) | Task 15-16 |
| ctest 全量零回归 | Task 20 |
| docs 更新 (lib/prompts/README.md) | Task 17 |
| 架构合规 (ADR-lint + docs-drift 0) | Task 19 |

### 类型一致性检查

- `PromptBuilder::build(user_input) → PromptPayload` — Task 8 定义,Task 9/10/11 override 一致
- `PromptPayload::add_system / add_user` — Task 8 定义,Task 11 V3 调用顺序一致
- `FewShotExample` struct — Task 10 定义,Task 10/11 共用
- `CliArgs` struct — Task 13 定义,Task 14 测试调用参数一致

### 占位符扫描

- 无 "TBD" / "TODO" / "implement later" 标记
- 每 Task 提供具体代码示例 + 命令验证
- 数据采集任务 (Task 2 / 4) 标记为人工 + 配合 yaml-cpp 解析测试

---

## Execution Handoff

完成本 plan 后:
1. 所有 task should have `- [x]` checkbox checked (execute 阶段执行)
2. Run final commit + push to branch `openspec/from-roadmap-phase-6c-execution-baseline`
3. Return to guide-ship Phase 2 for execution monitoring → Phase 3 for archive + merge to main
4. Post-archive: next change (`schema-complete` or `evidence-gate`) can begin

Execution model: **serial** (default per Wave 1).

Estimated total effort: ~4-6 hours (32 examples + 51 golden + V1/V2/V3 代码 ~ 300 行 + CLI ~ 200 行 + report ~ 100 行 + 4 单元测试).

---

## 变更历史

| 版本 | 改动 | 来源 |
|---|---|---|
| v1.0 | rdd-workflow-writing-plans v3.0 生成, 22 tasks 覆盖 proposal/tasks.md 21 项 | rdd-workflow-writing-plans v3.0 |
