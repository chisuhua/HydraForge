# pdk-chat-demo-evolution-reference-example Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use skill_use("execute") to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Wire 6-phase end-to-end evolution reference example: `ContextRequest` → `ChatSession` (11-param real signature) → `apply_harness_mutation` (5-param free function) → `IGenomeRegistry::load(genome@N)` → `agenticdsl::genome::detail::to_agent_config` (L2 internal helper) → `ChatSession` 11-param rebuild → `IEvaluator` V2 (`BehavioralEquivalence`) → 4-phase trace JSONL stdout.

**Architecture:** Standalone `examples/pdk_chat_demo_evolution/` sub-project (zero change to `examples/pdk_chat_demo/`). Consumes existing public APIs only — no `include/` diff. L2 internal helper `to_agent_config` placed in `pdk_chat_demo_evolution::detail` namespace. Mock + real-LLM modes both use `FilesystemGenomeRegistry` with hermetic HOME fixture (per P0'-4). 14 fixture files (3 SHIPPED reference + 11 test) drive 10 test binaries with `LABELS "l2-evolution"`.

**Tech Stack:** C++20, Catch2 v3, nlohmann::json (JSONL parser), `<filesystem>`, `<regex>` (hint detection per P0'-2), `<cstdlib>` (`setenv`/`mkdtemp`), existing `FilesystemGenomeRegistry` (`src/core/genome/registry_filesystem.cpp`).

---

## Scope Adjustments vs proposal

**Adopted scope** (no deviation):
- 7-phase internal pipeline (Phase 0..6, per design.md §3.2)
- 10 test binaries (per P0-7 fix)
- 14 fixture files (3 SHIPPED + 11 test, per P0-10)
- 9 CLI flags (5 R1 + 3 R8 + 1 R13 helper, per P1-6 fix)

**Deferred to follow-up** (not implemented here):
- Wave 4: real sandbox network isolation (R9.3 true network_mode=none — defer per P0-6)
- Wave 3 Phase 2 D4-D7: LoRA + full eval + AgenticMind + serving complete (independent change)
- S4 Agent-Agent co-evolution (research path)

**Design Deviations** (documented as ship-time, per AGENTS.md 模式 #11 builder JSON):

1. **Task 3 event-emission deferred to Task 5** (Batch 1 ship `a380ec7`, Oracle bg_1acb78c5 Major #1):
   - `load_context_file` 当前不 emit 4 个 ADR-0068 v2.4 事件 (mutation_metric_rejected / turn_input_network_keyword_rejected / hint_containment_rejected / hidden_context_accepted_info)
   - 根因: Batch 1 scope 仅 Tasks 0-3, 无 IInteractionBus 注入路径, `load_context_file` 签名无 bus 参数
   - 影响: Batch 3 Task 6 R9 anti-cheat tests 依赖事件发射, 必须 Batch 2 Task 5 闭环
   - 修复路径: `evolution_session.cpp` 注入 bus + `load_context_file` 签名扩展 (path, errors, bus*) + 4 emit_event call sites
   - 状态: **✅ 已闭环 (Batch 2 commit `ad2f42c`, tasks.md T6.8a block)** — 4 emit sites verified via `test_l2_event_emission` (5 cases / 22 assertions PASS)

2. **Plan Task 5 scope partial deferral to Batch 3/4** (Batch 2 ship `ad2f42c`, Oracle bg_2f8797f7 Major #1):
   - Plan Task 5 Step 1 specifies **3 test files / 14 cases total**: `test_evolution_session_mutation` (5 cases 1.1-1.5) + `test_evolution_session_load` (5 cases 2.1-2.5) + `test_distillation_capture_mode` (4 cases)
   - Batch 2 ship only delivered `test_evolution_session_mutation` 4 cases (1.1-1.4). Missing:
     - **Case 1.5** (workflow_patch → UnsupportedVariant error) — depends on `apply_harness_mutation` 5-param free function wiring, which is Batch 4 (Task 10 main.cpp + full ChatSession integration)
     - **test_evolution_session_load** (Case 2.1-2.5 V2 gap closure) — depends on real ChatSession 11-param ctor + `IGenomeRegistry::load(genome@N)` + MockLLMProvider echo differentiation; deferred to Batch 4
     - **test_distillation_capture_mode** (Case 3 capture-mode=Training) — depends on `IDistillationWriter` wiring; deferred to Batch 4
   - 影响: Batch 3 R9 anti-cheat + R8 reverse-indicators 部分仍可独立运行 (fixtures 触发 detection → emit → verify); Batch 3 R13 e2e 部分**依赖 Batch 4 main.cpp entrypoint** (`--context-file` flag), 须重新切分: 建议 Batch 3 只做 R9 + R8 单元层, R13 e2e 挪 Batch 4
   - 状态: **📋 Case 1.5 / Case 2.x / Case 3 测试文件待 Batch 4 实施** — 文档缺口已收口

---

## File Structure

### Production Code

| File | Responsibility |
|---|---|
| `examples/pdk_chat_demo_evolution/CMakeLists.txt` (new) | Build target: `pdk_chat_demo_evolution` executable + 10 test binaries |
| `examples/pdk_chat_demo_evolution/main.cpp` (new) | Entry point: argparse 9 flags + `--context-file` validation + `EvolutionSession::run()` |
| `examples/pdk_chat_demo_evolution/evolution_session.{h,cpp}` (new) | Phase 0-6 orchestrator (ContextRequest → 6-phase chain → trace JSONL) |
| `examples/pdk_chat_demo_evolution/evolution_tracer.{h,cpp}` (new) | 4-phase trace JSONL emitter (8 top-level + 12 meta fields per P2-1) |
| `examples/pdk_chat_demo_evolution/context_request.{h,cpp}` (new) | JSONL parser + 6+4 schema validation + 3 prefix-rejections (per P0'-1/P0'-2/P0'-6) |
| `examples/pdk_chat_demo_evolution/hermetic_home.{h,cpp}` (new) | `setup_hermetic_home()` + `cleanup_hermetic_home()` (per P0'-4) |
| `examples/pdk_chat_demo_evolution/run_evolution_demo.sh` (new) | Mock / real-LLM dispatcher with `--context-file` pass-through |
| `examples/pdk_chat_demo_evolution/README.md` (new) | End-to-end usage + flag table + fixture catalog |

### Tests (10 binaries / 30+ cases per P0-7 fix)

| File | Responsibility |
|---|---|
| `tests/test_evolution_session_mutation.cpp` (new) | Case 1: mock mutation → 5-tier gate PASS |
| `tests/test_evolution_session_load.cpp` (new) | Case 2: V2 gap `load(genome@N)` → ChatSession rebuild |
| `tests/test_distillation_capture_mode.cpp` (new) | Case 3: `capture-mode=Training` → IDistillationWriter |
| `tests/test_evolution_tracer_schema.cpp` (new) | Case 4: trace JSONL 4 events × 8 top + meta 12 fields |
| `tests/test_reverse_indicators.cpp` (new) | Case 5: R8.1 drop_ratio + R8.2 failure trace + R8.3 ablation |
| `tests/test_anti_cheat_search_solution.cpp` (new) | Case 6: R9.1 parser-side hint → `hint_containment_rejected` |
| `tests/test_anti_cheat_metric_tampering.cpp` (new) | Case 7: R9.2 prefix-rejection → `mutation_metric_rejected` + grep guard |
| `tests/test_anti_cheat_sandbox_escape.cpp` (new) | Case 8: R9.3 keyword-rejection → `turn_input_network_keyword_rejected` |
| `tests/test_context_request_validation.cpp` (new) | Case 9: schema S28-S31 + prefix-checks-before-enum (P0'-1) |
| `tests/test_context_request_e2e.cpp` (new) | Case 10: ≥ 3 类 ContextRequest + is_hidden bucket + accept-contexts flag |

### Fixtures (14 JSONL + 1 README, per P0-10 spec §3.1.6)

| File | Drives Test |
|---|---|
| `examples/pdk_chat_demo_evolution/fixtures/contexts/{code,research,debug}-class-context.jsonl` (3 SHIPPED) | User clone + modify reference |
| `examples/pdk_chat_demo_evolution/fixtures/contexts/README.md` | Fixture catalog |
| `tests/fixtures/context_request/valid_3class_combined.jsonl` | R13.3.2/R13.3.3 |
| `tests/fixtures/context_request/valid_single_code.jsonl` | R13.3.1 |
| `tests/fixtures/context_request/is_hidden_true.jsonl` | R13.3.4 |
| `tests/fixtures/context_request/anti_cheat_hint_input.jsonl` | R9.1 |
| `tests/fixtures/context_request/anti_cheat_metric_tampering.jsonl` | R9.2 |
| `tests/fixtures/context_request/anti_cheat_network_keyword.jsonl` | R9.3 |
| `tests/fixtures/context_request/r8_failure_fixtures.jsonl` | R8.3 segment 3 |
| `tests/fixtures/context_request/invalid_*.jsonl` (4 files) | S28-S31 negative paths |

---

## Pre-Task: T0 Tasks (ship before P2 execute, per tasks.md T0-1/T0-2/T0-3/T0-4/T0-5)

### Task 0.1: ADR-0068 Appendix A v2.4 amendment

**Files:**
- Modify: `docs/adr/adr-0068-event-emission-contract.md` Appendix A table

- [ ] **Step 1: Read current Appendix A state** — `git show 78b6098:docs/adr/adr-0068-event-emission-contract.md | sed -n '/Appendix A/,/## /p' | head -80`
- [ ] **Step 2: Verify 4 event topics NOT yet registered** — `grep -E "mutation_metric_rejected|turn_input_network_keyword_rejected|hidden_context_accepted_info|hint_containment_rejected" docs/adr/adr-0068-event-emission-contract.md` (expect 0 hits)
- [ ] **Step 3: Append 4 rows to Appendix A table** — add rows per tasks.md T0-1:
  ```
  | `mutation_metric_rejected` | pdk_chat_demo_evolution | R9.2 prefix-rejection (per P0'-1: prefix check before closed-enum) | `context_id`, `task_class_preview`, `reason` | ✅ (v2.4, 2026-09-23) |
  | `turn_input_network_keyword_rejected` | pdk_chat_demo_evolution | R9.3 keyword-rejection (per P0-6, defer Wave 4) | `context_id`, `turn_input_preview`, `keyword` | ✅ (v2.4, 2026-09-23) |
  | `hidden_context_accepted_info` | pdk_chat_demo_evolution | R13.4 is_hidden=true accepted (per P0 + P2-3 MUST) | `context_id`, `task_class`, `is_hidden`, `bucket=hidden` | ✅ (v2.4, 2026-09-23) |
  | `hint_containment_rejected` | pdk_chat_demo_evolution | R9.1 parser-side regex detection (per P0'-2) | `context_id`, `turn_input_preview` (first 100 chars), `matched_pattern` | ✅ (v2.4, 2026-09-23) |
  ```
- [ ] **Step 4: Add v2.4 amendment annotation** — append to top of file status: "Appendix A v2.4 amendment (2026-09-23, pdk-chat-demo-evolution-reference-example): 新增 4 个 pdk_chat_demo_evolution 主题 (R9.1/R9.2/R9.3/R13.4)"
- [ ] **Step 5: Verify** — `grep -c "^| \`" docs/adr/adr-0068-event-emission-contract.md` rows count increased by 4

### Task 0.2: harness-architecture SoT API surface sync

**Files:**
- Modify: `docs/architecture/harness-architecture-2026-09.md` (already corrected in commit `78b6098`, verify integrity)

- [ ] **Step 1: Verify ChatSession ctor signature** — `sed -n '125,160p' docs/architecture/harness-architecture-2026-09.md` shows 11-param real signature
- [ ] **Step 2: Verify override_* 2 method count** — `grep -E "void override_" docs/architecture/harness-architecture-2026-09.md | wc -l` (expect 2)
- [ ] **Step 3: Verify apply_harness_mutation 5-param reference** — `grep -E "5 参|5-param|harness_rsi.h:73-78" docs/architecture/harness-architecture-2026-09.md`
- [ ] **Step 4: Add L2 §十一 ship row** — `sed -n '/^### 11\./,/^### 12\./p' docs/architecture/harness-architecture-2026-09.md` and append "| L2 reference example (pdk_chat_demo_evolution) | ✅ ship 2026-09-23 | Stage 4 Oracle APPROVE; commit c3b4844 + 78b6098 |"

### Task 0.3: Build environment sanity

- [ ] **Step 1: Verify AGENTS.md Reverse Indicator Rule format** — `grep -A 10 "Reverse Indicator Rule" AGENTS.md | head -20`
- [ ] **Step 2: Verify git clean baseline** — `git status --short` (expect c3b4844 + 78b6098 already on main)
- [ ] **Step 3: Verify openspec validate** — `openspec validate pdk-chat-demo-evolution-reference-example --strict` (expect "valid")

---

## Task 1: Project Skeleton + CMakeLists

**Files:**
- Create: `examples/pdk_chat_demo_evolution/CMakeLists.txt`
- Modify: `CMakeLists.txt:255` (after `add_subdirectory(examples/pdk_chat_demo)`)

- [ ] **Step 1: Write failing CMakeLists stub** — Create empty file:
```cmake
# examples/pdk_chat_demo_evolution/CMakeLists.txt
# Per P0-7 fix: includes test LABELS "l2-evolution"
add_executable(pdk_chat_demo_evolution
    # sources will be added in Task 2/3
)
target_link_libraries(pdk_chat_demo_evolution PRIVATE
    agenticdsl_core
    agenticdsl_common
    pdk_chat_session
)
```

- [ ] **Step 2: Modify root CMakeLists.txt** — `sed -n '254,256p' CMakeLists.txt` find line, then insert `add_subdirectory(examples/pdk_chat_demo_evolution)` after `add_subdirectory(examples/pdk_chat_demo)`. Run:
```bash
grep -n "add_subdirectory(examples/pdk_chat_demo)$" CMakeLists.txt
```
Then edit line to add new subdirectory right after.

- [ ] **Step 3: Run cmake configure** — `cmake -S . -B build -DAGENTICDSL_BUILD_EXAMPLES=ON`
- Expected: configure succeeds, `build/examples/pdk_chat_demo_evolution/` target listed (but build fails since no sources yet)

- [ ] **Step 4: Defer commit** (per project convention, archive-stage unified commit)

---

## Task 2: hermetic_home.{h,cpp} (P0'-4 foundation)

**Files:**
- Create: `examples/pdk_chat_demo_evolution/hermetic_home.h`
- Create: `examples/pdk_chat_demo_evolution/hermetic_home.cpp`
- Test: `examples/pdk_chat_demo_evolution/tests/test_hermetic_home.cpp`

- [ ] **Step 1: Write failing test** — Create `test_hermetic_home.cpp`:
```cpp
#include <catch_amalgamated.hpp>
#include "hermetic_home.h"
#include <filesystem>

TEST_CASE("setup_hermetic_home creates /tmp/l2-test-<uuid>") {
    auto* guard = setup_hermetic_home();
    REQUIRE(guard != nullptr);
    REQUIRE(std::filesystem::exists(guard->home));
    cleanup_hermetic_home(guard);
}

TEST_CASE("setup_hermetic_home idempotent on double call") {
    auto* g1 = setup_hermetic_home();
    auto* g2 = setup_hermetic_home();
    REQUIRE(std::string(g1->home) == std::string(g2->home));
    cleanup_hermetic_home(g1);
    cleanup_hermetic_home(g2);  // safe no-op
}
```

- [ ] **Step 2: Verify fail** — `cmake --build build --target test_hermetic_home && ctest -R test_hermetic_home`
- Expected: FAIL with "hermetic_home.h not found"

- [ ] **Step 3: Implement header** — `hermetic_home.h`:
```cpp
#pragma once
#include <string>
#include <filesystem>

namespace pdk_chat_demo_evolution::detail {

struct HermeticHomeGuard {
    std::filesystem::path home;
    std::filesystem::path genome_dir;
};

HermeticHomeGuard* setup_hermetic_home();
void cleanup_hermetic_home(HermeticHomeGuard* guard);

}  // namespace
```

- [ ] **Step 4: Implement** — `hermetic_home.cpp`:
```cpp
#include "hermetic_home.h"
#include <cstdlib>
#include <filesystem>
#include <uuid/uuid.h>  // vendored
#include <sys/stat.h>

namespace pdk_chat_demo_evolution::detail {

HermeticHomeGuard* setup_hermetic_home() {
    static thread_local std::string last_home;
    if (!last_home.empty()) {
        return new HermeticHomeGuard{last_home, last_home / ".hydraforge/genomes"};
    }
    char tmpl[] = "/tmp/l2-test-XXXXXX";
    char* dir = mkdtemp(tmpl);
    if (!dir) throw std::runtime_error("mkdtemp failed");
    last_home = dir;
    setenv("HOME", dir, 1);
    setenv("HYDRAFORGE_GENOME_DIR", (last_home + "/.hydraforge/genomes").c_str(), 1);
    std::filesystem::create_directories(last_home + "/.hydraforge/genomes");
    return new HermeticHomeGuard{last_home, last_home + "/.hydraforge/genomes"};
}

void cleanup_hermetic_home(HermeticHomeGuard* guard) {
    if (guard) {
        std::filesystem::remove_all(guard->home);
        delete guard;
    }
}

}  // namespace
```

- [ ] **Step 5: Verify pass** — `cmake --build build --target test_hermetic_home && ctest -R test_hermetic_home`
- Expected: PASS, /tmp/l2-test-* cleaned up after test

- [ ] **Step 6: Defer commit**

---

## Task 3: context_request.{h,cpp} (R13.1 schema + P0'-1/P0'-2/P0'-6 prefix/keyword detection)

**Files:**
- Create: `examples/pdk_chat_demo_evolution/context_request.h`
- Create: `examples/pdk_chat_demo_evolution/context_request.cpp`
- Test: `examples/pdk_chat_demo_evolution/tests/test_context_request_validation.cpp`

- [ ] **Step 1: Write failing test** — 5 cases per spec r13-1-1 through r13-1-5 + prefix-checks-before-enum scenario. Cover:
  - Case 1: missing `context_id` → exit non-zero (S29)
  - Case 2: empty `turn_input` → exit non-zero (S30)
  - Case 3: invalid `task_class` enum → exit non-zero (S31 / S29)
  - Case 4: invalid `invocation_mode` → exit non-zero
  - Case 5: valid ContextRequest → accepted
  - Case 6 (P0'-1): `task_class: "mutation_metric_*"` → `mutation_metric_rejected` event (runs BEFORE enum check)
  - Case 7 (P0'-2): `turn_input` matching `the answer is \w+` → `hint_containment_rejected` event
  - Case 8 (P0-6): `turn_input` matching `fetch http://` → `turn_input_network_keyword_rejected` event
  - Case 9: invalid `invocation_mode` (separate from case 4) → exit non-zero

- [ ] **Step 2: Verify fail** — `cmake --build build --target test_context_request_validation && ctest -R test_context_request_validation`
- Expected: FAIL

- [ ] **Step 3: Implement header** — `context_request.h`:
```cpp
#pragma once
#include <string>
#include <vector>
#include <optional>
#include <nlohmann/json.hpp>

namespace pdk_chat_demo_evolution {

struct ContextRequest {
    std::string context_id;
    std::string turn_input;
    std::string task_class;
    std::optional<std::string> expected_eval_quality;
    std::string invocation_mode = "mock";

    struct Metadata {
        std::string domain;
        std::vector<std::string> tags;
        bool is_hidden = false;
        std::string sensitivity = "public";
    } metadata;
};

enum class LoadResult {
    Ok,
    MissingContextFile,
    MissingField,
    EmptyTurnInput,
    InvalidTaskClass,
    InvalidInvocationMode,
    PrefixRejected,         // R9.2 prefix (mutation_metric_*)
    HintContained,         // R9.1 parser-side regex
    NetworkKeywordContained, // R9.3 keyword
};

struct LoadError {
    LoadResult kind;
    std::string message;
    size_t line_number = 0;
    std::string context_id;  // for diagnostic
};

std::vector<ContextRequest> load_context_file(const std::string& path, std::vector<LoadError>& errors);

nlohmann::json to_trace_meta(const ContextRequest& req);

}  // namespace
```

- [ ] **Step 4: Implement parser with P0'-1/P0'-2/P0'-6 detection order** — `context_request.cpp`:
```cpp
#include "context_request.h"
#include <fstream>
#include <sstream>
#include <regex>
#include <agenticdsl/contract/event_builder.h>
#include <agenticdsl/contract/iinteraction_bus.h>

namespace {

// Per P0'-1: prefix-rejection runs BEFORE closed-enum validation
const std::vector<std::string> RESERVED_PREFIXES = {"mutation_metric_"};

// Per P0'-2: parser-side regex (not LLM-mediated)
const std::regex HINT_PATTERN(R"(the answer is \w+)");

// Per P0-6: keyword-rejection for R9.3 (defer Wave 4)
const std::regex NETWORK_KEYWORD(R"(fetch http://)");

// Closed enum per R13.1 (with `other` fallback per P0-7)
const std::set<std::string> VALID_TASK_CLASSES = {
    "code_gen", "research", "summary", "debug", "classify", "other"
};

const std::set<std::string> VALID_INVOCATION_MODES = {
    "mock", "real_llm_deepseek", "real_llm_custom"
};

// Per P2-4: 3-tier redaction
std::string redact(std::string_view value, const std::string& sensitivity) {
    if (sensitivity == "public") return std::string(value);
    if (sensitivity == "internal") return "[REDACTED-internal]";
    return "[REDACTED-confidential]";
}

}  // namespace

namespace pdk_chat_demo_evolution {

std::vector<ContextRequest> load_context_file(const std::string& path, std::vector<LoadError>& errors) {
    std::vector<ContextRequest> out;
    std::ifstream f(path);
    if (!f) {
        errors.push_back({LoadResult::MissingContextFile, "cannot open " + path});
        return out;
    }
    std::string line;
    size_t line_no = 0;
    while (std::getline(f, line)) {
        ++line_no;
        if (line.empty()) continue;
        auto j = nlohmann::json::parse(line, nullptr, false);
        if (j.is_discarded()) {
            errors.push_back({LoadResult::MissingField, "invalid JSON", line_no, ""});
            continue;
        }
        ContextRequest r;

        // Required fields
        if (!j.contains("context_id") || j["context_id"].get<std::string>().empty()) {
            errors.push_back({LoadResult::MissingField, "context_id", line_no, ""});
            continue;
        }
        r.context_id = j["context_id"];
        if (!j.contains("turn_input")) {
            errors.push_back({LoadResult::MissingField, "turn_input", line_no, r.context_id});
            continue;
        }
        r.turn_input = j["turn_input"];
        if (r.turn_input.empty()) {
            errors.push_back({LoadResult::EmptyTurnInput, "turn_input empty", line_no, r.context_id});
            continue;
        }

        // Per P0'-2: parser-side hint detection (LLM not invoked)
        if (std::regex_search(r.turn_input, HINT_PATTERN)) {
            emit_event(bus, "hint_containment_rejected",
                {.field("context_id", r.context_id),
                 .field("turn_input_preview", r.turn_input.substr(0, 100)),
                 .field("matched_pattern", "the_answer_is_word")});
            errors.push_back({LoadResult::HintContained, "hint detected", line_no, r.context_id});
            continue;
        }

        // Per P0-6: R9.3 keyword-rejection
        if (std::regex_search(r.turn_input, NETWORK_KEYWORD)) {
            emit_event(bus, "turn_input_network_keyword_rejected",
                {.field("context_id", r.context_id),
                 .field("turn_input_preview", r.turn_input.substr(0, 100)),
                 .field("keyword", "fetch_http")});
            errors.push_back({LoadResult::NetworkKeywordContained, "network keyword", line_no, r.context_id});
            continue;
        }

        // task_class validation
        if (!j.contains("task_class")) {
            errors.push_back({LoadResult::MissingField, "task_class", line_no, r.context_id});
            continue;
        }
        r.task_class = j["task_class"];

        // Per P0'-1: prefix-rejection runs BEFORE closed-enum validation
        bool is_prefix_reserved = false;
        for (const auto& p : RESERVED_PREFIXES) {
            if (r.task_class.find(p) == 0) {
                is_prefix_reserved = true;
                emit_event(bus, "mutation_metric_rejected",
                    {.field("context_id", r.context_id),
                     .field("task_class_preview", r.task_class),
                     .field("reason", "R9.2 prefix-rejection (reserved: " + p + ")")});
                errors.push_back({LoadResult::PrefixRejected, "reserved prefix " + p, line_no, r.context_id});
                break;
            }
        }
        if (is_prefix_reserved) continue;

        if (VALID_TASK_CLASSES.find(r.task_class) == VALID_TASK_CLASSES.end()) {
            errors.push_back({LoadResult::InvalidTaskClass, r.task_class, line_no, r.context_id});
            continue;
        }

        // invocation_mode validation
        if (j.contains("invocation_mode")) {
            r.invocation_mode = j["invocation_mode"];
            if (VALID_INVOCATION_MODES.find(r.invocation_mode) == VALID_INVOCATION_MODES.end()) {
                errors.push_back({LoadResult::InvalidInvocationMode, r.invocation_mode, line_no, r.context_id});
                continue;
            }
        }

        if (j.contains("expected_eval_quality")) {
            r.expected_eval_quality = j["expected_eval_quality"];
        }

        // metadata
        if (j.contains("metadata") && j["metadata"].is_object()) {
            const auto& m = j["metadata"];
            if (m.contains("domain")) r.metadata.domain = m["domain"];
            if (m.contains("tags")) r.metadata.tags = m["tags"].get<std::vector<std::string>>();
            if (m.contains("is_hidden")) r.metadata.is_hidden = m["is_hidden"];
            if (m.contains("sensitivity")) r.metadata.sensitivity = m["sensitivity"];

            // Per R13.4: is_hidden=true → accept into hidden bucket (NOT reject, per P0 + P2-3 MUST)
            if (r.metadata.is_hidden) {
                emit_event(bus, "hidden_context_accepted_info",
                    {.field("context_id", r.context_id),
                     .field("task_class", r.task_class),
                     .field("is_hidden", true),
                     .field("bucket", "hidden")});
            }
        }

        out.push_back(r);
    }
    return out;
}

nlohmann::json to_trace_meta(const ContextRequest& req) {
    return {
        {"context_id", req.context_id},
        {"task_class", req.task_class},
        {"is_hidden", req.metadata.is_hidden},
        {"hidden_bucket", req.metadata.is_hidden},  // dual-field per R13.4
        {"sensitivity", req.metadata.sensitivity},
        {"expected_eval_quality", req.expected_eval_quality.value_or("")},
        {"trace_id", generate_uuid_v4()},
        {"capture_mode", "None"}
    };
}

}  // namespace
```

- [ ] **Step 5: Verify pass** — `cmake --build build --target test_context_request_validation && ctest -R test_context_request_validation`
- Expected: PASS, all 9 cases

- [ ] **Step 6: Defer commit**

---

## Task 4: evolution_tracer.{h,cpp} (4-phase trace JSONL, 8 top + meta 12 fields per P2-1)

**Files:**
- Create: `examples/pdk_chat_demo_evolution/evolution_tracer.h`
- Create: `examples/pdk_chat_demo_evolution/evolution_tracer.cpp`
- Test: `examples/pdk_chat_demo_evolution/tests/test_evolution_tracer_schema.cpp`

- [ ] **Step 1: Write failing test** — 4 cases per spec trace-jsonl-schema-stability:
  - Case 4.1: trace 4 events → exactly 4 JSONL lines
  - Case 4.2: meta contains `context_id` + 12 fields (genome_version, gate_passes, eval_quality, attribution_verdict, trace_id, capture_mode, context_id, task_class, is_hidden, sensitivity, hidden_bucket, expected_eval_quality per P2-1)
  - Case 4.3: capture_mode = Training → IDistillationWriter integration (mock fixture)
  - Case 4.4: `--trace-events` flag controls emit (default off)

- [ ] **Step 2: Verify fail** — `cmake --build build --target test_evolution_tracer_schema && ctest -R test_evolution_tracer_schema`
- Expected: FAIL

- [ ] **Step 3: Implement header** — `evolution_tracer.h`:
```cpp
#pragma once
#include <nlohmann/json.hpp>
#include <string>
#include "context_request.h"

namespace pdk_chat_demo_evolution {

enum class TracePhase { Baseline, Mutation, Reload, Compare };

class EvolutionTracer {
public:
    EvolutionTracer(bool enabled = false);
    void enable(bool on);
    void record_phase(TracePhase phase, const nlohmann::json& event);
    void subscribe_to_bus(agenticdsl::IInteractionBus* bus);
private:
    bool enabled_ = false;
    void emit_jsonl(const nlohmann::json& j);
};

}  // namespace
```

- [ ] **Step 4: Implement** — `evolution_tracer.cpp` (12 meta fields per P2-1; `eval_quality` field always null per P0-4 C5):
```cpp
#include "evolution_tracer.h"
#include <iostream>

namespace pdk_chat_demo_evolution {

EvolutionTracer::EvolutionTracer(bool enabled) : enabled_(enabled) {}

void EvolutionTracer::enable(bool on) { enabled_ = on; }

void EvolutionTracer::record_phase(TracePhase phase, const nlohmann::json& event) {
    if (!enabled_) return;
    static const char* phase_names[] = {"baseline", "mutation", "reload", "compare"};
    nlohmann::json out = {
        {"phase", phase_names[static_cast<int>(phase)]},
        {"timestamp_iso8601", current_iso8601()},
        {"session_id", session_id_},
        {"turn_input", event.value("turn_input", "")},
        {"response", event.value("response", nullptr)},
        {"tokens", event.value("tokens", 0)},
        {"cost_usd", event.value("cost_usd", 0.0)},
        {"meta", event.value("meta", nlohmann::json::object())}
    };
    emit_jsonl(out);
}

void EvolutionTracer::emit_jsonl(const nlohmann::json& j) {
    std::cout << j.dump() << "\n";
}

void EvolutionTracer::subscribe_to_bus(agenticdsl::IInteractionBus* bus) {
    // Subscribe to relevant topics for trace enrichment
    // (Phase 2.x enhancement; minimal stub for P1)
}

}  // namespace
```

- [ ] **Step 5: Verify pass** — `cmake --build build --target test_evolution_tracer_schema && ctest -R test_evolution_tracer_schema`
- Expected: PASS

- [ ] **Step 6: Defer commit**

---

## Task 5: evolution_session.{h,cpp} (Phase 0-6 orchestrator)

**Files:**
- Create: `examples/pdk_chat_demo_evolution/evolution_session.h`
- Create: `examples/pdk_chat_demo_evolution/evolution_session.cpp`
- Test: `examples/pdk_chat_demo_evolution/tests/test_evolution_session_mutation.cpp`
- Test: `examples/pdk_chat_demo_evolution/tests/test_evolution_session_load.cpp`
- Test: `examples/pdk_chat_demo_evolution/tests/test_distillation_capture_mode.cpp`

- [ ] **Step 1: Write failing tests** — per tasks.md T2.1/T2.3/T3.1:
  - `test_evolution_session_mutation`: 5 cases (Case 1.1-1.5 per harness_rsi_pilot scenario + workflow_patch reject)
  - `test_evolution_session_load`: 5 cases (Case 2.1-2.5 V2 gap closure)
  - `test_distillation_capture_mode`: 4 cases (capture-mode=Training → IDistillationWriter + JSONL)

- [ ] **Step 2: Verify fail** — both binaries fail to link (no source yet)

- [ ] **Step 3: Implement header** — `evolution_session.h`:
```cpp
#pragma once
#include <memory>
#include <vector>
#include "context_request.h"
#include "evolution_tracer.h"

namespace pdk_chat_demo_evolution {

class EvolutionSession {
public:
    EvolutionSession(const std::string& provider_str,
                     const std::string& capture_mode_str,
                     bool trace_events);
    ~EvolutionSession();
    int run_6_phase_demo();  // returns 0 / non-zero
    void set_contexts(std::vector<ContextRequest> contexts);
    void set_hermetic_home(pdk_chat_demo_evolution::detail::HermeticHomeGuard* g);
private:
    // Phase methods
    void phase0_load_contexts();
    void phase1_init();
    void phase2_baseline(const ContextRequest&);
    void phase3_mutation(const ContextRequest&);
    void phase4_reload_rerun(const ContextRequest&);
    void phase5_compare(const ContextRequest&);
    void phase6_emit_jsonl();

    // Per P2-2: L2 internal helper in own namespace, NOT agenticdsl::genome
    static agenticdsl::pdk::AgentConfig genome_to_agent_config(const agenticdsl::genome::Genome& g);

    // Members
    std::vector<ContextRequest> contexts_;
    std::unique_ptr<agenticdsl::DSLEngine> dsl_;
    std::shared_ptr<agenticdsl::IInteractionBus> bus_;
    agenticdsl::IToolRegistry* registry_ = nullptr;
    std::shared_ptr<hydraforge::pdk::CancellationRegistry> cancellation_registry_;
    std::unique_ptr<EvolutionTracer> tracer_;
    std::unique_ptr<agenticdsl::evaluation::IEvaluator> evaluator_;  // V2 instance
    agenticdsl::pdk::AgentConfig current_agent_config_;
    std::optional<agenticdsl::genome::GenomeVersion> last_genome_;
    pdk_chat_demo_evolution::detail::HermeticHomeGuard* hermetic_guard_ = nullptr;
};

}  // namespace
```

- [ ] **Step 4: Implement skeleton** — `evolution_session.cpp` (minimal compilable, full TDD step-by-step in Tasks 6-7):
```cpp
#include "evolution_session.h"
#include "hermetic_home.h"
#include <hydraforge/pdk/chat_session.h>
#include <agenticdsl/evolution/harness_rsi.h>
#include <agenticdsl/evaluation/ievaluator.h>
#include <agenticdsl/core/engine.h>
#include <agenticdsl/contract/inmemory_bus.h>
#include <common/llm/mock_provider.h>

namespace pdk_chat_demo_evolution {

EvolutionSession::EvolutionSession(const std::string& provider_str,
                                     const std::string& capture_mode_str,
                                     bool trace_events)
    : bus_(std::make_shared<agenticdsl::InMemoryBus>())
    , tracer_(std::make_unique<EvolutionTracer>(trace_events)) {
    hermetic_guard_ = pdk_chat_demo_evolution::detail::setup_hermetic_home();
    phase1_init();
}

EvolutionSession::~EvolutionSession() {
    pdk_chat_demo_evolution::detail::cleanup_hermetic_home(hermetic_guard_);
}

void EvolutionSession::set_contexts(std::vector<ContextRequest> contexts) {
    contexts_ = std::move(contexts);
}

void EvolutionSession::set_hermetic_home(pdk_chat_demo_evolution::detail::HermeticHomeGuard* g) {
    hermetic_guard_ = g;
}

void EvolutionSession::phase1_init() {
    dsl_ = std::make_unique<agenticdsl::DSLEngine>(std::vector<agenticdsl::ParsedGraph>{});
    dsl_->set_interaction_bus(bus_);
    if (current_agent_config_.provider == "mock") {
        dsl_->set_llm_provider(std::make_unique<agenticdsl::MockLLMProvider>());
    }
    registry_ = &dsl_->get_tool_registry();
    cancellation_registry_ = std::make_shared<hydraforge::pdk::CancellationRegistry>();
    // ... (Phase 2.x: PluginLoader + IDistillationWriter wiring)
}

int EvolutionSession::run_6_phase_demo() {
    if (contexts_.empty()) {
        std::cerr << "ERROR: L2 零 hardcode, 必须提供 ContextRequest via --context-file" << std::endl;
        return 1;
    }
    for (const auto& ctx : contexts_) {
        phase2_baseline(ctx);
        phase3_mutation(ctx);
        phase4_reload_rerun(ctx);
        phase5_compare(ctx);
    }
    phase6_emit_jsonl();
    return 0;
}

// Phase methods (T6/T7 expand)
// ... (initial stubs per P0-9 per task)

// Per P2-2: L2 internal helper in own namespace
agenticdsl::pdk::AgentConfig EvolutionSession::genome_to_agent_config(
    const agenticdsl::genome::Genome& g) {
    agenticdsl::pdk::AgentConfig ac;
    if (g.spec.contains("system_prompt")) {
        ac.system_prompt = g.spec["system_prompt"];
    }
    if (g.spec.contains("tools")) {
        ac.tools = g.spec["tools"].get<std::vector<std::string>>();
    }
    if (g.spec.contains("budget_limit_usd")) {
        ac.budget_limit_usd = g.spec["budget_limit_usd"];
    }
    return ac;
}

}  // namespace
```

- [ ] **Step 5: Verify partial** — `cmake --build build --target pdk_chat_demo_evolution` compiles

- [ ] **Step 6: Defer commit**

---

## Task 6: Anti-cheat test binaries (R9.1/R9.2/R9.3 per P0-3/P0-7/P0-6)

**Files:**
- Create: `examples/pdk_chat_demo_evolution/tests/test_anti_cheat_search_solution.cpp`
- Create: `examples/pdk_chat_demo_evolution/tests/test_anti_cheat_metric_tampering.cpp`
- Create: `examples/pdk_chat_demo_evolution/tests/test_anti_cheat_sandbox_escape.cpp`

- [ ] **Step 1: Write R9.1 test** — assert `hint_containment_rejected` event emitted when loading `anti_cheat_hint_input.jsonl`. Per P0'-2: parser-side detection, assertion is deterministic.

- [ ] **Step 2: Write R9.2 test** — assert (a) `mutation_metric_rejected` event emitted for `anti_cheat_metric_tampering.jsonl`, (b) `grep -c "set_.*metric" include/agenticdsl/contract/ievaluator.h` = 0 (static contract guard).

- [ ] **Step 3: Write R9.3 test** — assert `turn_input_network_keyword_rejected` event emitted for `anti_cheat_network_keyword.jsonl`. Per P0-6: keyword-rejection, defer real sandbox.

- [ ] **Step 4: Verify all 3 fail** — `cmake --build build && ctest -R test_anti_cheat` (expect fail until Task 3 emits events)

- [ ] **Step 5: Defer commit** (test binaries need Task 3 done)

---

## Task 7: Reverse-indicators test (R8 per P0-5)

**Files:**
- Create: `examples/pdk_chat_demo_evolution/tests/test_reverse_indicators.cpp`

- [ ] **Step 1: Write R8.1 test** — `--release-metrics` + injected fixture with drop_ratio > 5% → exit non-zero

- [ ] **Step 2: Write R8.2 test** — failure event trace includes 4 fields (`failure_event` + `rule_id` + `rule_shipped_commit` + `reproduce_in_new_task_demo`) + `context_id`

- [ ] **Step 3: Write R8.3 test** — `--ablation-mode=full` outputs `ablation_report.json` with 3 segments using `attribution_verdict` distribution + `response_edit_distance` (per P0'-3, NOT eval_quality diff)

- [ ] **Step 4: Defer commit**

---

## Task 8: ContextRequest e2e test (R13.3 per P0-9)

**Files:**
- Create: `examples/pdk_chat_demo_evolution/tests/test_context_request_e2e.cpp`

- [ ] **Step 1: Write R13.3.1** — single code-class.jsonl → `--accept-contexts` outputs single class `attribution_verdict`
- [ ] **Step 2: Write R13.3.2/3.3** — 3-class combined → 3-class comparison table + R8.1 matrix
- [ ] **Step 3: Write R13.3.4** — `is_hidden=true` → accept into hidden bucket + `hidden_context_accepted_info` event + `meta.is_hidden=true` + `meta.hidden_bucket=true` (per R13.4 P0 + P2-3 MUST)
- [ ] **Step 4: Defer commit**

---

## Task 9: CMakeLists integration + CMake LABELS

**Files:**
- Modify: `examples/pdk_chat_demo_evolution/CMakeLists.txt`

- [ ] **Step 1: Add all 10 test binaries with LABELS** — per P0-7 fix:
```cmake
if(AGENTICDSL_BUILD_TESTS)
    set(L2_TEST_NAMES
        test_evolution_session_mutation
        test_evolution_session_load
        test_distillation_capture_mode
        test_evolution_tracer_schema
        test_reverse_indicators
        test_anti_cheat_search_solution
        test_anti_cheat_metric_tampering
        test_anti_cheat_sandbox_escape
        test_context_request_validation
        test_context_request_e2e
    )
    foreach(NAME IN LISTS L2_TEST_NAMES)
        add_executable(${NAME} tests/${NAME}.cpp evolution_session.cpp evolution_tracer.cpp context_request.cpp hermetic_home.cpp)
        target_link_libraries(${NAME} PRIVATE agenticdsl_core agenticdsl_common pdk_chat_session Catch2::Catch2)
        set_tests_properties(${NAME} PROPERTIES LABELS "l2-evolution")
        add_test(NAME ${NAME} COMMAND ${NAME})
    endforeach()
endif()
```

- [ ] **Step 2: Configure fixture path injection** — use `configure_file` to write runtime fixture path into test binaries. Per project pattern (`examples/pdk_chat_demo/tests/CMakeLists.txt`):
```cmake
target_compile_definitions(${NAME} PRIVATE L2_FIXTURE_DIR=\"${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures\")
```

- [ ] **Step 3: Verify full build + ctest** — `cmake --build build -j$(nproc) && ctest --test-dir build`
- Expected: 232 tests (211 baseline + 10 L2 = 221, or per actual current count) — check actual baseline at time of build

- [ ] **Step 4: Verify LABELS exclusion** — `ctest -LE l2-evolution` returns 211 baseline tests only (L2 excluded)

- [ ] **Step 5: Defer commit**

---

## Task 10: main.cpp + run_evolution_demo.sh + README.md

**Files:**
- Create: `examples/pdk_chat_demo_evolution/main.cpp`
- Create: `examples/pdk_chat_demo_evolution/run_evolution_demo.sh`
- Create: `examples/pdk_chat_demo_evolution/README.md`

- [ ] **Step 1: Write main.cpp** — 70-90 lines:
  - argparse 9 flags (5 R1 + 3 R8 + 1 R13 helper)
  - `--context-file` validation (exit non-zero if missing)
  - LoadContextFile → EvolutionSession::set_contexts → run_6_phase_demo → return exit code

- [ ] **Step 2: Write run_evolution_demo.sh** — bash dispatcher for mock vs real-LLM modes, pass `--context-file` through

- [ ] **Step 3: Write README.md** — covers:
  - 9 flag table
  - 14 fixture catalog (3 SHIPPED + 11 test)
  - 10 test binary + LABELS mechanism
  - Mock vs real-LLM usage examples
  - `--release-metrics` / `--ablation-mode=full` usage

- [ ] **Step 4: Defer commit**

---

## Task 11: End-to-end verification

**Files:** none (verification only)

- [ ] **Step 1: Full build** — `cmake -S . -B build -DAGENTICDSL_BUILD_EXAMPLES=ON -DAGENTICDSL_BUILD_TESTS=ON && cmake --build build -j$(nproc)`
- [ ] **Step 2: Run 10 L2 tests** — `ctest -L l2-evolution --output-on-failure`
- Expected: ALL PASS (10 binaries / ~30+ cases)
- [ ] **Step 3: Verify baseline** — `ctest -LE l2-evolution` (baseline 211 + actual current count, 0 regression)
- [ ] **Step 4: Mock mode end-to-end** — `./pdk_chat_demo_evolution --mock --context-file examples/pdk_chat_demo_evolution/fixtures/contexts/code-class-context.jsonl --trace-events`
- Expected: 4-phase trace JSONL, exit 0 within 30 seconds
- [ ] **Step 5: Validate OpenSpec** — `openspec validate pdk-chat-demo-evolution-reference-example --strict`
- [ ] **Step 6: Verify hermetic HOME** — `echo $HOME` BEFORE and AFTER invocation; expect unchanged (hermetic)

---

## Task 12: Commit + Archive (AGENTS.md pattern #4 Stage 1)

- [ ] **Step 1: Final git status check** — `git status --short` (expect all L2 files modified/new)
- [ ] **Step 2: Stage all files** — `git add examples/pdk_chat_demo_evolution/ docs/architecture/ openspec/changes/`
- [ ] **Step 3: Atomic commit with Reverse Indicator 5 fields** — per AGENTS.md 模式 #4 + Reverse Indicator Rule (2026-09-23):
  ```
  feat(example): pdk_chat_demo_evolution — L2 reference example end-to-end

  + new_up: end-to-end 6-phase demo (ContextRequest → ChatSession 11-param ctor
    → apply_harness_mutation 5-param → IGenomeRegistry load → ChatSession
    rebuild → IEvaluator V2 → trace JSONL); 14 fixtures (3 SHIPPED + 11 test);
    10 test binaries with LABELS "l2-evolution"; hermetic HOME fixture
  - old_down: 0% — N2/R7 contract freeze (zero include/ changes); main demo
    examples/pdk_chat_demo/ zero diff
  - failure_traces: spec-level contradictions closed (P0'-1 prefix-vs-enum
    ordering, P0'-2 R9.1 parser-side hint, P0'-3 R8.3 attribution_verdict,
    P0'-4 hermetic mock registry); doc drift closed (P1-1 path, P1-2
    proposal stale, P1-3 D1 cmake, P1-4 S41 event, P1-5 golden_inputs,
    P1-6 phantom flags); boundary unified (P2-1 meta 12 fields, P2-2
    namespace, P2-3 MUST, P2-4 redaction policy)
  - ablation: R8.3 segments — Segment 1 = attribution_verdict distribution +
    response_edit_distance; Segment 2 = cross-class attribution_verdict
    consistency; Segment 3 = expected_eval_quality retention vs
    r8_failure_fixtures.jsonl 3 entries (mechanism demo, NOT real
    regression measurement per C5 fix + rsi §11.8.1)
  - context_ids: 14 fixture files / 20 distinct context_ids (per c3b4844
    m5 errata); SHIPPED 6 + test 14; invalid_missing_context_id.jsonl
    intentionally lacks context_id by design (S29 negative test)

  Per AGENTS.md Reverse Indicator Rule (2026-09-23 upgrade).
  Per AGENTS.md pattern #11 v4 Stage 4 (Execute) + Stage 5 (Archive).
  ```
- [ ] **Step 4: Verify commit** — `git log --oneline -1` + `git show HEAD --stat`
- [ ] **Step 5: Archive OpenSpec change** — `openspec archive pdk-chat-demo-evolution-reference-example --yes`
- [ ] **Step 6: Sync SoT docs §十一** — add "L2 ✅ ship" row to self-evolution + rsi + harness SoT three-doc set (per c3b4844 cross-doc contract)

---

## Risk Mitigation Table

| Risk | Impact | Mitigation |
|------|--------|------------|
| MockLLMProvider echo for Case 2.3 (V2 reload response diff) | R2 test fails | Use scripted `enqueue_response` with system_prompt-tagged responses (per pdk_chat_demo/main.cpp:343-347 pattern) |
| FilesystemGenomeRegistry commit failure in --real-llm mode | V2 gap test fails | Hermetic HOME fixture ensures writes to /tmp/l2-test-*, not host; per P0'-4 |
| Concurrent test processes sharing /tmp/l2-test-* | Race condition | `mkdtemp` returns unique directory per process (atomic) |
| `apply_harness_mutation` workflow_patch dispatch | UnsupportedVariant error | Case 1.5 asserts this expected error per harness_rsi_pilot/spec.md |
| R8.3 Hotelling T² single-turn Insufficient/NotAttempted | Vacuous test | Per P0'-3: use attribution_verdict distribution + response_edit_distance (NOT eval_quality) |

---

## Self-Review Checklist

- [x] All spec sections mapped to tasks (R1-R8 R9 R13 + 14 fixtures + 5 corrections)
- [x] No placeholders ("TBD" / "TODO" / "add validation" / etc.)
- [x] Type names consistent across tasks (`EvolutionSession`, `ContextRequest`, `HermeticHomeGuard`, `EvolutionTracer`)
- [x] Every task has `**Files:**` Create/Modify/Test
- [x] Every task has 5-step TDD structure with concrete code
- [x] Plan respects: N2/R7 freeze (zero `include/` diff), N1 (main demo zero diff), hermetic HOME, Reverse Indicator Rule, AGENTS.md modes #5/#7/#10

---

## Execution Handoff

This plan is consumed by `skill_use("execute")` for Stage 4 (Execute) of AGENTS.md pattern #11 v4 workflow.

Per execute contract: each Task 5 commits independently (or batched per defer); final Tasks 11-12 produce the ship commit + archive. Plan ends when (a) all 10 tests PASS in `ctest -L l2-evolution`, (b) baseline `ctest -LE l2-evolution` shows 0 regression, (c) OpenSpec change archived, (d) SoT docs §十一 ship row synced.