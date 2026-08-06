# cli-args-cxxopts Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use skill_use("execute") to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Replace `examples/pdk_chat_demo/main.cpp` hand-rolled argv scanning with vendored cxxopts and a centralized declarative flag table while preserving existing startup behavior.

**Architecture:** Vendor a pinned cxxopts single header under `external/cxxopts/` and expose it through a target-scoped interface target. Define project-owned `CliOptions`, `CliParseResult`, and one `CliFlagSpec` declaration table; `parse_cli_args()` constructs cxxopts options, handles help/errors at the CLI boundary, and returns only validated project types. Keep the `--skill-child` dispatch as the first branch in `main()`, then apply the parsed values at the existing config/provider/session boundaries without changing chat or persistence semantics.

**Tech Stack:** C++20, cxxopts v3.2.1 vendored single header, Catch2 v3 amalgamated, nlohmann_json, CMake target-scoped includes.

---

## File Structure

### Production Code

| File | Responsibility |
|---|---|
| `external/cxxopts/cxxopts.hpp` | Pinned vendored cxxopts single header |
| `external/cxxopts/LICENSE` | Upstream license text for the vendored header |
| `external/CMakeLists.txt` | Repository-owned `hydraforge_cxxopts` interface target |
| `examples/pdk_chat_demo/cli_options.h` | `CliOptions` and `CliParseResult` value types |
| `examples/pdk_chat_demo/cli_options.cpp` | Minimal translation unit for the value boundary |
| `examples/pdk_chat_demo/cli_args_parser.h` | Flag declaration types/table and `parse_cli_args()` API |
| `examples/pdk_chat_demo/cli_args_parser.cpp` | cxxopts construction, mapping, help, and errors |
| `examples/pdk_chat_demo/main.cpp` | Parser call and validated option wiring; child branch preserved |
| `examples/pdk_chat_demo/CMakeLists.txt` | Parser sources and target-scoped dependency wiring |
| `examples/pdk_chat_demo/tests/CMakeLists.txt` | CLI test target registration |

### Tests

| File | Responsibility |
|---|---|
| `examples/pdk_chat_demo/tests/test_cli_args_parser.cpp` | Defaults, each flag, combinations, declaration table, and parser errors |
| `examples/pdk_chat_demo/tests/test_pdk_chat_demo_cli_args.cpp` | Real executable help, invalid input, `--skill-child`, and `--mock` startup |
| `tests/test_cli_flags_hardcode_audit.cpp` | Static audit that the old argv scan is gone from `main.cpp` |

---

## Task 1: cxxopts vendoring and target integration

**Files:**
- Create: `external/cxxopts/cxxopts.hpp`
- Create: `external/cxxopts/LICENSE`
- Modify: `external/CMakeLists.txt`
- Modify: `examples/pdk_chat_demo/CMakeLists.txt`
- Modify: `examples/pdk_chat_demo/tests/CMakeLists.txt`
- Test: `examples/pdk_chat_demo/tests/test_cli_args_parser.cpp`

- [ ] **Step 1: Write the failing test**

Create the Catch2 test target and first test:

```cpp
#include <catch_amalgamated.hpp>
#include "cli_args_parser.h"

TEST_CASE("parser accepts mock mode", "[cli][stage3]") {
  char program[] = "pdk_chat_demo";
  char mock[] = "--mock";
  char* argv[] = {program, mock};
  const auto result = pdk_chat_demo::parse_cli_args(2, argv);
  REQUIRE(result.ok);
  CHECK(result.options.mock);
  CHECK_FALSE(result.options.print);
  CHECK_FALSE(result.options.offline);
}
```

Register `test_cli_args_parser` in `examples/pdk_chat_demo/tests/CMakeLists.txt` with `test_cli_args_parser.cpp`, `catch_amalgamated.cpp`, and `main_test_runner.cpp`, linking the same libraries as `test_chat_session`.

- [ ] **Step 2: Run test to verify it fails**

Run:

```bash
cmake -S . -B build -DAGENTICDSL_BUILD_EXAMPLES=ON -DPDK_CHAT_BUILD_TESTS=ON
cmake --build build --target test_cli_args_parser
```

Expected: configuration/build fails because `cli_args_parser.h`, `external/cxxopts/cxxopts.hpp`, and the parser implementation do not exist.

- [ ] **Step 3: Write minimal implementation**

Vendor the pinned cxxopts v3.2.1 single header and its upstream MIT license. Record `cxxopts v3.2.1` and its source revision in `external/CMakeLists.txt`; do not add a download cache. Define the repository-only target:

```cmake
add_library(hydraforge_cxxopts INTERFACE)
target_include_directories(hydraforge_cxxopts INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/cxxopts)
```

Add `external` to the top-level configured build only if required by the existing root CMake structure, and link `hydraforge_cxxopts` to `pdk_chat_demo_obj` with `PUBLIC` propagation to its test consumers. Add no `find_package(cxxopts)`, `FetchContent`, global `include_directories()`, or network step.

Create the project-owned boundary:

```cpp
#pragma once
#include <string>

namespace pdk_chat_demo {
struct CliOptions {
  bool mock = false;
  bool print = false;
  bool offline = false;
  std::string session_id;
  std::string provider;
};
struct CliParseResult {
  bool ok = false;
  bool show_help = false;
  CliOptions options;
  std::string help;
  std::string error;
};
}
```

`cli_args_parser.h` declares `CliParseResult parse_cli_args(int argc, char* argv[]);`; add the new source/header files to `SOURCES`/`HEADERS` and use target-scoped include directories.

- [ ] **Step 4: Run test to verify it passes**

Run:

```bash
cmake --build build --target test_cli_args_parser
ctest --test-dir build -R '^test_cli_args_parser$' --output-on-failure
```

Expected: the target compiles using only `${PROJECT_SOURCE_DIR}/external/cxxopts` and the initial test passes.

- [ ] **Step 5: Defer commit**

Check that `external/cxxopts/` contains the header and license, the pinned version is recorded, and CMake has no cxxopts package lookup or fetch path.

---

## Task 2: Declarative flag table and `parse_cli_args()` mapping

**Files:**
- Create: `examples/pdk_chat_demo/cli_options.cpp`
- Modify: `examples/pdk_chat_demo/cli_options.h`
- Create: `examples/pdk_chat_demo/cli_args_parser.cpp`
- Modify: `examples/pdk_chat_demo/cli_args_parser.h`
- Modify: `examples/pdk_chat_demo/CMakeLists.txt`
- Modify: `examples/pdk_chat_demo/tests/test_cli_args_parser.cpp`

- [ ] **Step 1: Write the failing test**

Extend `test_cli_args_parser.cpp` with a helper and complete declaration/value assertions:

```cpp
#include <initializer_list>
#include <vector>

namespace {
pdk_chat_demo::CliParseResult parse(std::initializer_list<const char*> values) {
  std::vector<char*> argv;
  for (const char* value : values) argv.push_back(const_cast<char*>(value));
  return pdk_chat_demo::parse_cli_args(static_cast<int>(argv.size()), argv.data());
}
}

TEST_CASE("declaration table contains exactly the five flags", "[cli][stage3]") {
  const auto& table = pdk_chat_demo::cli_flag_declarations();
  REQUIRE(table.size() == 5);
  CHECK(table[0].long_name == "mock");
  CHECK(table[1].long_name == "session");
  CHECK(table[2].long_name == "print");
  CHECK(table[2].short_name == "p");
  CHECK(table[3].long_name == "provider");
  CHECK(table[4].long_name == "offline");
}

TEST_CASE("all declarations map to CliOptions", "[cli][stage3]") {
  const auto result = parse({"pdk_chat_demo", "--mock", "--session", "demo-session", "-p", "--provider", "deepseek", "--offline"});
  REQUIRE(result.ok);
  CHECK(result.options.mock);
  CHECK(result.options.session_id == "demo-session");
  CHECK(result.options.print);
  CHECK(result.options.provider == "deepseek");
  CHECK(result.options.offline);
}

TEST_CASE("optional values have explicit defaults", "[cli][stage3]") {
  const auto result = parse({"pdk_chat_demo"});
  REQUIRE(result.ok);
  CHECK_FALSE(result.options.mock);
  CHECK_FALSE(result.options.print);
  CHECK_FALSE(result.options.offline);
  CHECK(result.options.session_id.empty());
  CHECK(result.options.provider.empty());
}
```

- [ ] **Step 2: Run test to verify it fails**

Run `cmake --build build --target test_cli_args_parser && ctest --test-dir build -R '^test_cli_args_parser$' --output-on-failure`.

Expected: compilation fails because `CliFlagSpec`, `cli_flag_declarations()`, and the cxxopts mapping are not implemented.

- [ ] **Step 3: Write minimal implementation**

Define the table in `cli_args_parser.h`:

```cpp
#include "cli_options.h"
#include <string>
#include <vector>
namespace pdk_chat_demo {
enum class CliValueKind { flag, string };
enum class CliDestination { mock, session_id, print, provider, offline };
struct CliFlagSpec {
  std::string long_name;
  std::string short_name;
  CliValueKind value_kind;
  std::string value_name;
  std::string description;
  CliDestination destination;
};
const std::vector<CliFlagSpec>& cli_flag_declarations();
CliParseResult parse_cli_args(int argc, char* argv[]);
}
```

Implement the one declaration source and parser boundary in `cli_args_parser.cpp`:

```cpp
#include "cli_args_parser.h"
#include <cxxopts.hpp>
#include <exception>
#include <iostream>

namespace pdk_chat_demo {
const std::vector<CliFlagSpec>& cli_flag_declarations() {
  static const std::vector<CliFlagSpec> table = {
    {"mock", "", CliValueKind::flag, "", "Use MockLLMProvider without network requests", CliDestination::mock},
    {"session", "", CliValueKind::string, "ID", "Load the selected persisted session", CliDestination::session_id},
    {"print", "p", CliValueKind::flag, "", "Enable print mode intent", CliDestination::print},
    {"provider", "", CliValueKind::string, "NAME", "Override the configured provider for this run", CliDestination::provider},
    {"offline", "", CliValueKind::flag, "", "Enable offline startup intent independently of mock", CliDestination::offline},
  };
  return table;
}

CliParseResult parse_cli_args(int argc, char* argv[]) {
  CliParseResult result;
  cxxopts::Options options("pdk_chat_demo", "HydraForge PDK chat demo");
  for (const auto& spec : cli_flag_declarations()) {
    const std::string spelling = spec.short_name.empty() ? spec.long_name : spec.short_name + "," + spec.long_name;
    if (spec.value_kind == CliValueKind::string)
      options.add_options()(spelling, spec.description, cxxopts::value<std::string>(), spec.value_name);
    else
      options.add_options()(spelling, spec.description);
  }
  options.add_options()("help", "Show generated usage");
  try {
    const auto parsed = options.parse(argc, argv);
    result.help = options.help();
    result.show_help = parsed.count("help") != 0;
    if (!result.show_help) {
      result.options.mock = parsed["mock"].as<bool>();
      result.options.print = parsed["print"].as<bool>();
      result.options.offline = parsed["offline"].as<bool>();
      if (parsed.count("session")) result.options.session_id = parsed["session"].as<std::string>();
      if (parsed.count("provider")) result.options.provider = parsed["provider"].as<std::string>();
    }
    result.ok = true;
  } catch (const std::exception& error) {
    result.error = std::string(error.what()) + ". Use --help for usage.";
    result.help = options.help();
  }
  return result;
}
}
```

Add both sources to `pdk_chat_demo_obj`; do not expose cxxopts or `ParseResult` in `CliOptions`, `main.cpp`, provider code, or session code.

- [ ] **Step 4: Run test to verify it passes**

Run `cmake --build build --target test_cli_args_parser && ctest --test-dir build -R '^test_cli_args_parser$' --output-on-failure`.

Expected: all five declaration entries, all mappings, and explicit defaults pass.

- [ ] **Step 5: Defer commit**

Audit signatures and includes: only `cli_args_parser.cpp` may depend on `<cxxopts.hpp>`; downstream code receives `CliOptions`/`CliParseResult` only.

---

## Task 3: Replace `main.cpp` scan and preserve existing behavior

**Files:**
- Modify: `examples/pdk_chat_demo/main.cpp:83-119,231-268,371-385`
- Modify: `examples/pdk_chat_demo/CMakeLists.txt`
- Modify: `examples/pdk_chat_demo/tests/CMakeLists.txt`
- Modify: `examples/pdk_chat_demo/tests/test_cli_args_parser.cpp`
- Create: `examples/pdk_chat_demo/tests/test_pdk_chat_demo_cli_args.cpp`

- [ ] **Step 1: Write the failing test**

Add parser regression tests:

```cpp
TEST_CASE("mock and session remain independent", "[cli][stage3]") {
  char a[] = "pdk_chat_demo"; char b[] = "--mock"; char c[] = "--session"; char d[] = "demo-session";
  char* argv[] = {a, b, c, d};
  const auto result = pdk_chat_demo::parse_cli_args(4, argv);
  REQUIRE(result.ok);
  CHECK(result.options.mock);
  CHECK(result.options.session_id == "demo-session");
}

TEST_CASE("offline is not mock", "[cli][stage3]") {
  char a[] = "pdk_chat_demo"; char b[] = "--offline"; char* argv[] = {a, b};
  const auto result = pdk_chat_demo::parse_cli_args(2, argv);
  REQUIRE(result.ok);
  CHECK(result.options.offline);
  CHECK_FALSE(result.options.mock);
}
```

Create `test_pdk_chat_demo_cli_args.cpp` using `popen()` and a CMake-defined `PDK_CHAT_DEMO_PATH`:

```cpp
#include <catch_amalgamated.hpp>
#include <cstdio>
#include <string>
#ifndef PDK_CHAT_DEMO_PATH
#define PDK_CHAT_DEMO_PATH "pdk_chat_demo"
#endif
TEST_CASE("--mock reaches the existing mock startup path", "[cli][stage3][e2e]") {
  const std::string command = "printf '' | \"" PDK_CHAT_DEMO_PATH "\" --mock 2>&1";
  FILE* pipe = popen(command.c_str(), "r");
  REQUIRE(pipe != nullptr);
  std::string output; char buffer[256];
  while (fgets(buffer, sizeof(buffer), pipe)) output += buffer;
  const int status = pclose(pipe);
  CHECK(status == 0);
  CHECK(output.find("Mock mode: provider=mock, model=test") != std::string::npos);
  CHECK(output.find("Using MockLLMProvider") != std::string::npos);
}
```

Also add a child-branch process case using the existing valid `--skill-child` test protocol fixture; assert normal `Mock mode`, `Live mode`, `Session started`, and `User>` markers are absent.

- [ ] **Step 2: Run test to verify it fails**

Run `cmake --build build --target pdk_chat_demo test_cli_args_parser test_pdk_chat_demo_cli_args && ctest --test-dir build -R '^(test_cli_args_parser|test_pdk_chat_demo_cli_args)$' --output-on-failure`.

Expected: parser tests pass, but the process regression fails until `main.cpp` consumes `parse_cli_args()` and the new test target is registered.

- [ ] **Step 3: Write minimal implementation**

Add `#include "cli_args_parser.h"`. Preserve this as the first executable branch, before parser invocation and all initialization:

```cpp
if (argc > 1 && std::string(argv[1]) == "--skill-child") {
  return agenticdsl::skill_child_main(argc, argv);
}
const auto cli = pdk_chat_demo::parse_cli_args(argc, argv);
if (!cli.ok) { std::cerr << "[main] " << cli.error << std::endl; return 2; }
if (cli.show_help) { std::cout << cli.help << std::endl; return 0; }
const auto& cli_options = cli.options;
const bool mock_mode = cli_options.mock;
const std::string& session_id_to_load = cli_options.session_id;
```

Delete the `std::vector<std::string>` scan. Before `config.validate()`, apply the explicit provider override without persisting it:

```cpp
if (!cli_options.provider.empty()) config.agent.provider = cli_options.provider;
config.validate();
```

Keep the existing mock override/provider initialization and session loading blocks unchanged except for their new local values. Keep `offline` independent from `mock`; retain `print` and `offline` in the validated `CliOptions` startup boundary without implementing RPC, output protocol, backend substitution, or config-file mutation. Add the process test target with `PDK_CHAT_DEMO_PATH="${PROJECT_BINARY_DIR}/examples/pdk_chat_demo/pdk_chat_demo"`, `WORKING_DIRECTORY` set where generated `config.json` is available, and the existing plugin environment if needed.

- [ ] **Step 4: Run test to verify it passes**

Run `cmake --build build --target pdk_chat_demo test_cli_args_parser test_pdk_chat_demo_cli_args && ctest --test-dir build -R '^(test_cli_args_parser|test_pdk_chat_demo_cli_args)$' --output-on-failure`.

Expected: combined flags preserve exact values, `--mock` follows the previous startup path, and `--skill-child` bypasses normal parsing/initialization.

- [ ] **Step 5: Defer commit**

Verify the child conditional precedes `parse_cli_args()` textually and no `DSLEngine`, `ChatConfig`, plugin, or session object is initialized before successful parse/help handling.

---

## Task 4: Generated help and invalid-input exit contract

**Files:**
- Modify: `examples/pdk_chat_demo/cli_args_parser.cpp`
- Modify: `examples/pdk_chat_demo/main.cpp`
- Modify: `examples/pdk_chat_demo/tests/test_cli_args_parser.cpp`
- Modify: `examples/pdk_chat_demo/tests/test_pdk_chat_demo_cli_args.cpp`
- Create: `tests/test_cli_flags_hardcode_audit.cpp`
- Modify: `tests/CMakeLists.txt`

- [ ] **Step 1: Write the failing test**

Add direct parser assertions:

```cpp
TEST_CASE("invalid input returns a diagnostic and nonzero parse status", "[cli][stage3]") {
  char a[] = "pdk_chat_demo"; char b[] = "--not-a-real-flag"; char* argv[] = {a, b};
  const auto unknown = pdk_chat_demo::parse_cli_args(2, argv);
  CHECK_FALSE(unknown.ok); CHECK(unknown.error.find("not-a-real-flag") != std::string::npos);
  CHECK(unknown.error.find("--help") != std::string::npos);
  char c[] = "--session"; char* missing[] = {a, c};
  const auto no_value = pdk_chat_demo::parse_cli_args(2, missing);
  CHECK_FALSE(no_value.ok); CHECK(no_value.error.find("session") != std::string::npos);
  CHECK(no_value.error.find("--help") != std::string::npos);
}

TEST_CASE("short and long print spellings are equivalent", "[cli][stage3]") {
  char a[] = "pdk_chat_demo"; char b[] = "-p"; char c[] = "--print";
  char* short_argv[] = {a, b}; char* long_argv[] = {a, c};
  const auto short_result = pdk_chat_demo::parse_cli_args(2, short_argv);
  const auto long_result = pdk_chat_demo::parse_cli_args(2, long_argv);
  REQUIRE(short_result.ok); REQUIRE(long_result.ok);
  CHECK(short_result.options.print); CHECK(long_result.options.print);
}
```

Add process tests that run `--help`, `--not-a-real-flag`, `--session`, and `--provider` without credentials. Assert help exit 0 and contains `--mock`, `--session`, `-p`/`--print`, `--provider`, and `--offline`; assert invalid cases return nonzero, mention the invalid/missing option and `--help`, and do not contain startup markers. The static audit must compile as a Catch2 test and use `std::system("grep -nE 'vector<string>|argv\\[|argc > 1.*--mock|args\\[i\\].*--session' examples/pdk_chat_demo/main.cpp")`, requiring a nonzero result.

- [ ] **Step 2: Run test to verify it fails**

Run `cmake --build build --target test_cli_args_parser test_pdk_chat_demo_cli_args test_cli_flags_hardcode_audit && ctest --test-dir build -R '^(test_cli_args_parser|test_pdk_chat_demo_cli_args|test_cli_flags_hardcode_audit)$' --output-on-failure`.

Expected: the invalid-input tests fail if errors are swallowed, process tests fail if `main()` does not return `0/2` before startup, and the audit fails while the old scan remains.

- [ ] **Step 3: Write minimal implementation**

Ensure `parse_cli_args()` sets `ok=true` only after successful parse or help, and in the catch block sets `ok=false`, preserves a concise cxxopts diagnostic, appends `Use --help for usage.`, and does not map partial values. Ensure `options.help()` is generated from the same five table entries plus the built-in help option. In `main()`, print `cli.help` and return 0 for help; print `cli.error` and return 2 for parse failures before constructing config/engine. Configure the three tests in their respective CMake files with the correct executable path and `add_test()` entries; use only target-scoped includes. Register the audit in root `tests/CMakeLists.txt` through the existing `file(GLOB test_*.cpp)` mechanism.

- [ ] **Step 4: Run test to verify it passes**

Run the targeted build/CTest command from Step 2, then execute:

```bash
build/examples/pdk_chat_demo/pdk_chat_demo --help
build/examples/pdk_chat_demo/pdk_chat_demo --not-a-real-flag >/tmp/cli.out 2>&1; test $? -ne 0
```

Expected: help exits 0 before startup and lists all five flags; unknown/missing values exit nonzero with a help hint; the audit finds no old scan.

- [ ] **Step 5: Defer commit**

Review every help assertion against the five table entries and confirm no hand-written help string duplicates the declaration descriptions.

---

## Task 5: Full validation and downstream handoff

**Files:**
- Modify: `examples/pdk_chat_demo/tests/test_cli_args_parser.cpp`
- Modify: `examples/pdk_chat_demo/tests/test_pdk_chat_demo_cli_args.cpp`
- Modify: `tests/test_cli_flags_hardcode_audit.cpp`
- Modify: `openspec/changes/cli-args-cxxopts/tasks.md`

- [ ] **Step 1: Write the failing test**

Complete the test matrix with explicit provider and print/offline boundary assertions:

```cpp
TEST_CASE("new options expose validated values without coupling downstream", "[cli][stage3]") {
  char a[] = "pdk_chat_demo"; char b[] = "--provider"; char c[] = "deepseek";
  char d[] = "--offline"; char e[] = "-p"; char* argv[] = {a, b, c, d, e};
  const auto result = pdk_chat_demo::parse_cli_args(5, argv);
  REQUIRE(result.ok);
  CHECK(result.options.provider == "deepseek");
  CHECK(result.options.offline);
  CHECK(result.options.print);
  CHECK_FALSE(result.options.mock);
}
```

Add a hardcode-audit assertion that adding a future flag requires changing the declaration table rather than an argv loop, and retain existing `test_chat_session`/`test_e2e_mock` targets unchanged.

- [ ] **Step 2: Run test to verify it fails**

Run:

```bash
cmake --build build --target pdk_chat_demo
ctest --test-dir build -R '^(test_cli_args_parser|test_pdk_chat_demo_cli_args|test_cli_flags_hardcode_audit|test_chat_session|test_e2e_mock)$' --output-on-failure
```

Expected: the complete CLI matrix exposes any missing CMake registration, option mapping, child bypass, or downstream startup regression before the full suite is attempted.

- [ ] **Step 3: Write minimal implementation**

Finish only the missing wiring exposed by the focused tests: ensure all three new options are represented in `CliOptions`, the single table, generated help, and the existing startup configuration boundary; keep provider override ephemeral, keep offline distinct from mock, and do not implement RPC/JSON mode, session enumeration, persistence mutation, or print protocol. Update `tasks.md` checkboxes only for tasks actually verified; do not modify unrelated OpenSpec artifacts.

- [ ] **Step 4: Run test to verify it passes**

Run the complete gate:

```bash
cmake --build build --target pdk_chat_demo
ctest --test-dir build --output-on-failure -j$(nproc)
openspec validate cli-args-cxxopts --json
```

Expected: the CLI targets and existing `test_chat_session`/`test_e2e_mock` pass, the full CTest suite has zero new failures, and validation reports `summary.totals.failed = 0` with `passed = true`. Record any unrelated pre-existing failure separately rather than weakening this change's tests.

- [ ] **Step 5: Defer commit**

Perform the handoff review: confirm a future string option can be added by one `CliFlagSpec` row plus its destination mapping, confirm no downstream signature uses cxxopts types, and report the exact validation commands/results to `chat-streaming-slash-tui` and `session-tree-tui` consumers. Do not commit unless explicitly requested.

---

## Critical Constraints (MUST satisfy)

1. Vendor pinned cxxopts at `external/cxxopts/cxxopts.hpp` with license metadata; never use FetchContent, `find_package`, a system package, or network access.
2. Use target-scoped CMake includes; never add global `include_directories()`.
3. Preserve `--skill-child` before normal parsing and pass its original `argc/argv` unchanged.
4. `CliOptions` is the only downstream value type; never pass `cxxopts::ParseResult` beyond `cli_args_parser.cpp`.
5. Cover all five ADDED requirements and every scenario in `specs/cli-flags/spec.md`.
6. Every task follows failing test, verify fail, minimal implementation, verify pass, defer commit.
7. Include compilable code in implementation/test steps; do not leave placeholders.
8. Acceptance is full CTest with zero new regressions and `openspec validate cli-args-cxxopts --json` passed.

## Banned Patterns

- Do not write `TBD`, `TODO`, `implement later`, or `fill in details` in the implementation plan.
- Do not say “add appropriate error handling” or “write tests for the above” without code and commands.
- Do not refer to an undefined type or function.
- Do not reintroduce imperative argv scanning, global include paths, system cxxopts discovery, RPC semantics, or config persistence.

## Self-Review

- `grep -c '^## Task ' .rddf/plans/cli-args-cxxopts.md` must return `5`.
- `grep -c '^- \[ \]' .rddf/plans/cli-args-cxxopts.md` must return `25`.
- Spec coverage: `cli-flags-vendored` → Tasks 1/5; `cli-flags-declarative` → Task 2/5; behavior equivalence and child bypass → Task 3; generated help and invalid exits → Task 4; new options → Tasks 2/5. Every listed scenario is explicitly tested.
- Placeholder scan must find none of the banned terms.
- Type consistency is `CliOptions`, `CliParseResult`, `CliFlagSpec`, `CliValueKind`, and `CliDestination` throughout.

## Final Step

After writing this file, copy it byte-for-byte to `.rddf/wt/cli-args-cxxopts/.rddf/plans/cli-args-cxxopts.md`, compare both files with `cmp`, and report both paths, `5` tasks, `25` TDD steps, and zero uncovered spec scenarios.
