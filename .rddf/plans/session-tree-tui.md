# session-tree-tui Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use skill_use("execute") to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add `--fork <node_id>` and `--name <session_name>` startup CLI flags to `pdk_chat_demo`, wired through the declarative `cli-args-cxxopts` registry and SessionManager JSONL tree storage.

**Architecture:** Extend the existing `cli_flag_declarations()` table with two string-value flags; populate `CliOptions::fork_node_id` + `CliOptions::session_name` from the parser; in `main.cpp` run the startup sequence `parse → load_session → fork → apply_name → chat_loop` with explicit non-zero exit + stderr diagnostics on missing session/node; reuse `SessionManager::fork(node_id, name)` and add a minimal `SessionManager::rename_session(name)` API for persisting the new-session name (per Decision 2 in design.md, only applied when no `--session` was loaded).

**Tech Stack:** C++20, cxxopts (vendored in `external/cxxopts`), nlohmann_json, Catch2 amalgamated, SessionManager JSONL tree storage (shipped via `session-manager-jsonl` v1+v2), `cli-args-cxxopts` declarative registry (shipped via Wave 2-A).

---

## File Structure

### Production Code

| File | Responsibility |
|---|---|
| `examples/pdk_chat_demo/cli_options.h` | Add `fork_node_id` and `session_name` to `CliOptions`; add `CliDestination::fork_node_id` + `CliDestination::session_name` enum values |
| `examples/pdk_chat_demo/cli_args_parser.cpp` | Add `--fork` + `--name` to declaration table (table size 5→7); populate new `CliOptions` fields in parser |
| `examples/pdk_chat_demo/main.cpp` | Wire `--fork` and `--name` after SessionManager creation; emit explicit non-zero exits with `--help` hints on failure |
| `src/core/session_manager.h` | Declare `void rename_session(const std::string& name)` |
| `src/core/session_manager.cpp` | Implement `rename_session`: persist name as `{"type":"session_meta","name":"..."}` JSONL record |

### Tests

| File | Responsibility |
|---|---|
| `examples/pdk_chat_demo/tests/test_cli_args_parser.cpp` | Update "five flags" test (5→7); add `--fork` parser test, `--name` parser test, combined `--session --fork` parser test, missing-value rejection |
| `examples/pdk_chat_demo/tests/test_pdk_chat_demo_cli_args.cpp` | Update `--help` test (expect new flags); add `--fork` help visibility |
| `examples/pdk_chat_demo/tests/test_pdk_chat_demo_session_tree_cli_flags.cpp` | **NEW**: end-to-end tests for startup wiring (success path, nonexistent node, nonexistent session, `--name` persistence scope, combined `--session --fork --name` ordering) |

---

### Task 1: Extend CliOptions + CliDestination enums

**Files:**
- Modify: `examples/pdk_chat_demo/cli_options.h`

- [ ] **Step 1: Add two new fields to CliOptions struct**

In `examples/pdk_chat_demo/cli_options.h`, add inside the `CliOptions` struct after `std::string provider;`:

```cpp
  std::string fork_node_id;
  std::string session_name;
```

- [ ] **Step 2: Add two new CliDestination enum values**

In the same file, modify the `CliDestination` enum:

```cpp
enum class CliDestination { mock, session_id, print, provider, offline, fork_node_id, session_name };
```

- [ ] **Step 3: Verify the file compiles in isolation**

Run: `cd /workspace/project/HydraForge/.rddf/wt/session-tree-tui && grep -c "fork_node_id\|session_name" examples/pdk_chat_demo/cli_options.h`
Expected: `3` (one `fork_node_id` in struct, one `session_name` in struct, two `fork_node_id` / one `session_name` in enum — adjust to match actual grep pattern)

---

### Task 2: Add --fork and --name to declarative parser table

**Files:**
- Modify: `examples/pdk_chat_demo/cli_args_parser.cpp`

- [ ] **Step 1: Write failing parser tests for new flags**

Add to `examples/pdk_chat_demo/tests/test_cli_args_parser.cpp`:

```cpp
TEST_CASE("--fork populates fork_node_id", "[cli][stage3][session-tree]") {
  char a[] = "pdk_chat_demo"; char b[] = "--fork"; char c[] = "node_42";
  char* argv[] = {a, b, c};
  const auto result = pdk_chat_demo::parse_cli_args(3, argv);
  REQUIRE(result.ok);
  CHECK(result.options.fork_node_id == "node_42");
}

TEST_CASE("--name populates session_name", "[cli][stage3][session-tree]") {
  char a[] = "pdk_chat_demo"; char b[] = "--name"; char c[] = "my-debug-session";
  char* argv[] = {a, b, c};
  const auto result = pdk_chat_demo::parse_cli_args(3, argv);
  REQUIRE(result.ok);
  CHECK(result.options.session_name == "my-debug-session");
}

TEST_CASE("combined --session and --fork parse both", "[cli][stage3][session-tree]") {
  char a[] = "pdk_chat_demo"; char b[] = "--session"; char c[] = "sess_abc";
  char d[] = "--fork";     char e[] = "node_42";
  char* argv[] = {a, b, c, d, e};
  const auto result = pdk_chat_demo::parse_cli_args(5, argv);
  REQUIRE(result.ok);
  CHECK(result.options.session_id == "sess_abc");
  CHECK(result.options.fork_node_id == "node_42");
}

TEST_CASE("--fork missing value is rejected", "[cli][stage3][session-tree]") {
  char a[] = "pdk_chat_demo"; char b[] = "--fork"; char c[] = "--mock";
  char* argv[] = {a, b, c};
  const auto result = pdk_chat_demo::parse_cli_args(3, argv);
  CHECK_FALSE(result.ok);
  CHECK(result.error.find("--fork") != std::string::npos);
  CHECK(result.error.find("--help") != std::string::npos);
}
```

- [ ] **Step 2: Update existing "five flags" test to expect seven**

In `examples/pdk_chat_demo/tests/test_cli_args_parser.cpp`, find:

```cpp
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
```

Replace with:

```cpp
TEST_CASE("declaration table contains exactly the seven flags", "[cli][stage3]") {
  const auto& table = pdk_chat_demo::cli_flag_declarations();
  REQUIRE(table.size() == 7);
  CHECK(table[0].long_name == "mock");
  CHECK(table[1].long_name == "session");
  CHECK(table[2].long_name == "print");
  CHECK(table[2].short_name == "p");
  CHECK(table[3].long_name == "provider");
  CHECK(table[4].long_name == "offline");
  CHECK(table[5].long_name == "fork");
  CHECK(table[6].long_name == "name");
}
```

- [ ] **Step 3: Run tests to verify they fail (compile errors expected — declarations missing)**

Run: `cd /workspace/project/HydraForge/.rddf/wt/session-tree-tui && cmake --build build --target test_cli_args_parser 2>&1 | head -20`
Expected: Compilation errors due to missing `fork_node_id` / `session_name` fields.

- [ ] **Step 4: Add two new entries to cli_flag_declarations() table**

In `examples/pdk_chat_demo/cli_args_parser.cpp`, append to the `static const std::vector<CliFlagSpec> table`:

```cpp
    {"fork", "", CliValueKind::string, "NODE_ID", "Fork a new branch from the named session node on startup", CliDestination::fork_node_id},
    {"name", "", CliValueKind::string, "SESSION_NAME", "Persist a human-readable name for the new session (ignored when --session loads an existing session)", CliDestination::session_name},
```

Resulting table (5→7 entries, unchanged order for first 5).

- [ ] **Step 5: Populate fork_node_id and session_name in parse_cli_args()**

In `examples/pdk_chat_demo/cli_args_parser.cpp`, inside the `if (!result.show_help)` block after the `provider` line, add:

```cpp
      if (parsed.count("fork")) result.options.fork_node_id = parsed["fork"].as<std::string>();
      if (parsed.count("name")) result.options.session_name = parsed["name"].as<std::string>();
```

- [ ] **Step 6: Run parser tests to verify they pass**

Run: `cd /workspace/project/HydraForge/.rddf/wt/session-tree-tui && cmake --build build --target test_cli_args_parser && ./build/examples/pdk_chat_demo/tests/test_cli_args_parser 2>&1 | tail -10`
Expected: All tests pass (8+ test cases including the 4 new ones).

- [ ] **Step 7: Defer commit**

Per the rdd-workflow execute contract, no commit here — the worktree commit phase aggregates all change work into a single commit before archive.

---

### Task 3: Add SessionManager::rename_session(name) API

**Files:**
- Modify: `src/core/session_manager.h`
- Modify: `src/core/session_manager.cpp`

- [ ] **Step 1: Write failing test for rename_session persistence**

Create `tests/test_session_manager_rename.cpp` (auto-registered via Catch2 `file(GLOB test_*.cpp)` in root `tests/CMakeLists.txt`):

```cpp
#include <catch_amalgamated.hpp>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>
#include <core/session_manager.h>

namespace fs = std::filesystem;

namespace {
fs::path make_temp_dir() {
  fs::path base = fs::temp_directory_path() /
                  ("hydra_session_rename_" + std::to_string(std::rand()));
  fs::create_directories(base);
  return base;
}
}

TEST_CASE("rename_session persists name to JSONL", "[session-manager][rename]") {
  const auto dir = make_temp_dir();
  agenticdsl::SessionManager mgr(dir);
  mgr.open("test-sess");
  mgr.rename_session("my-debug-session");
  mgr.flush_append(agenticdsl::SessionNode{
      mgr.next_node_id(), "", "main",
      nlohmann::json{{"role", "user"}, {"text", "hello"}}});

  // Reopen and verify rename record is present
  agenticdsl::SessionManager mgr2(dir);
  mgr2.open("test-sess");
  auto nodes = mgr2.load_jsonl();
  bool found = false;
  for (const auto& n : nodes) {
    if (n.content.value("type", "") == "session_meta" &&
        n.content.value("name", "") == "my-debug-session") {
      found = true;
      break;
    }
  }
  REQUIRE(found);
  fs::remove_all(dir);
}
```

- [ ] **Step 2: Run test to verify it fails (rename_session not declared)**

Run: `cd /workspace/project/HydraForge/.rddf/wt/session-tree-tui && cmake --build build --target test_session_manager_rename 2>&1 | head -10`
Expected: Compile error `no member named 'rename_session' in 'agenticdsl::SessionManager'`.

- [ ] **Step 3: Declare rename_session in session_manager.h**

In `src/core/session_manager.h`, after the `compact()` declaration (around line 233), add:

```cpp
  // ==================== Task 7 extension: rename_session ====================
  /// @brief 为当前 session 持久化人类可读名称（仅写一条 session_meta JSONL 记录,
  ///        不改写 current_session_id_、不改写 fork 语义、与 SessionManager::fork 兼容）
  /// @param name 要持久化的 session 名称（空字符串表示无变化，跳过写入）
  /// @throw std::runtime_error 当前未打开任何 session
  void rename_session(const std::string& name);
```

- [ ] **Step 4: Implement rename_session in session_manager.cpp**

In `src/core/session_manager.cpp`, add at the end of the public API implementations (before the private section):

```cpp
void SessionManager::rename_session(const std::string& name) {
  if (current_path_.empty()) {
    throw std::runtime_error("SessionManager::rename_session called before open()");
  }
  if (name.empty()) return;
  std::lock_guard<std::mutex> lock(write_mutex_);
  // 写一条 type=session_meta 的 JSONL 记录 — 与 SessionNode 共享文件
  // 不触发 session.persisted 事件（rename 是元数据而非新节点）
  const auto line = nlohmann::json{
      {"type", "session_meta"},
      {"name", name}}.dump() + "\n";
  int fd = ::open(current_path_.c_str(), O_WRONLY | O_APPEND | O_CREAT, 0644);
  if (fd < 0) {
    throw std::runtime_error("SessionManager::rename_session open failed: " +
                             current_path_.string());
  }
  const ssize_t n = ::write(fd, line.data(), line.size());
  ::fsync(fd);
  ::close(fd);
  if (n != static_cast<ssize_t>(line.size())) {
    throw std::runtime_error("SessionManager::rename_session short write");
  }
}
```

- [ ] **Step 5: Run test to verify it passes**

Run: `cd /workspace/project/HydraForge/.rddf/wt/session-tree-tui && cmake --build build --target test_session_manager_rename && ctest -R test_session_manager_rename --output-on-failure 2>&1 | tail -10`
Expected: PASS.

- [ ] **Step 6: Run full session-manager test suite to verify no regression**

Run: `cd /workspace/project/HydraForge/.rddf/wt/session-tree-tui && ctest -R "session|jsonl" --output-on-failure 2>&1 | tail -15`
Expected: All session-manager tests pass (rename is additive, no API change to existing methods).

---

### Task 4: Wire --fork and --name into main.cpp startup sequence

**Files:**
- Modify: `examples/pdk_chat_demo/main.cpp`

- [ ] **Step 1: Extract fork_node_id + session_name after parse**

In `examples/pdk_chat_demo/main.cpp`, replace:

```cpp
    const bool mock_mode = cli_options.mock;
    const std::string& session_id_to_load = cli_options.session_id;
```

with:

```cpp
    const bool mock_mode = cli_options.mock;
    const std::string& session_id_to_load = cli_options.session_id;
    const std::string& fork_node_id = cli_options.fork_node_id;
    const std::string& startup_session_name = cli_options.session_name;
```

- [ ] **Step 2: Add startup fork + name wiring after SessionManager::open()**

In `examples/pdk_chat_demo/main.cpp`, find the existing SessionManager creation block (around lines 411-416):

```cpp
    auto session_manager = std::make_unique<agenticdsl::SessionManager>(
        fs::path(config.session.persist_dir));
    session_manager->open(config.session.persist_dir.empty()
                              ? std::string("default")
                              : fs::path(config.session.persist_dir).filename().string());
    pdk_chat_demo::g_session_manager = session_manager.get();
```

Immediately after `pdk_chat_demo::g_session_manager = session_manager.get();`, insert:

```cpp
    // === session-tree-tui: --fork <node_id> + --name <session_name> startup wiring ===
    // Decision 4 (design.md): parse → load → fork → apply_name → chat_loop
    // Decision 3: --fork 失败立即退出, stderr 包含 flag/value/--help
    // Decision 2: --name 只应用于新 session, 与 --session 组合时拒绝隐式重命名
    if (!fork_node_id.empty()) {
        // 先确保 session 已加载 (--session <id> 或 open 的默认 session)
        if (session_manager->load_jsonl().empty()) {
            std::cerr << "[main] --fork " << fork_node_id
                      << " requires a loaded session, but session is empty. "
                      << "Use --help for usage." << std::endl;
            return 1;
        }
        if (session_manager->find_node(fork_node_id) == nullptr) {
            std::cerr << "[main] --fork " << fork_node_id
                      << ": node not found in current session. "
                      << "Use --help for usage." << std::endl;
            return 1;
        }
        try {
            const std::string branch_name =
                startup_session_name.empty() ? "forked-branch" : startup_session_name;
            const auto new_branch = session_manager->fork(fork_node_id, branch_name);
            session_manager->switch_branch(new_branch);
            std::cout << "[main] Forked from " << fork_node_id
                      << " to branch " << new_branch << std::endl;
        } catch (const std::exception& e) {
            std::cerr << "[main] --fork " << fork_node_id
                      << " failed: " << e.what()
                      << ". Use --help for usage." << std::endl;
            return 1;
        }
    }
    if (!startup_session_name.empty() && session_id_to_load.empty()) {
        // Decision 2: --name 仅写新 session metadata
        try {
            session_manager->rename_session(startup_session_name);
            std::cout << "[main] Session named: " << startup_session_name << std::endl;
        } catch (const std::exception& e) {
            std::cerr << "[main] --name " << startup_session_name
                      << " failed: " << e.what()
                      << ". Use --help for usage." << std::endl;
            return 1;
        }
    } else if (!startup_session_name.empty() && !session_id_to_load.empty()) {
        // Spec §cli-flag-fork-with-name: 已有 session 时, --name 不得隐式重命名
        std::cerr << "[main] --name " << startup_session_name
                  << " is ignored when --session <id> loads an existing session. "
                  << "Use --help for usage." << std::endl;
        return 1;
    }
```

- [ ] **Step 3: Verify main.cpp compiles**

Run: `cd /workspace/project/HydraForge/.rddf/wt/session-tree-tui && cmake --build build --target pdk_chat_demo 2>&1 | tail -10`
Expected: Build succeeds with no errors.

- [ ] **Step 4: Manually verify --help output contains new flags**

Run: `cd /workspace/project/HydraForge/.rddf/wt/session-tree-tui && ./build/examples/pdk_chat_demo/pdk_chat_demo --help 2>&1 | head -30`
Expected: Output contains `--fork <NODE_ID>` and `--name <SESSION_NAME>` lines.

---

### Task 5: Add E2E startup wiring tests

**Files:**
- Create: `examples/pdk_chat_demo/tests/test_pdk_chat_demo_session_tree_cli_flags.cpp`
- Modify: `examples/pdk_chat_demo/tests/CMakeLists.txt`
- Modify: `examples/pdk_chat_demo/tests/test_pdk_chat_demo_cli_args.cpp`

- [ ] **Step 1: Update existing --help test in test_pdk_chat_demo_cli_args.cpp**

In `examples/pdk_chat_demo/tests/test_pdk_chat_demo_cli_args.cpp`, inside `TEST_CASE("--help shows generated usage and exits 0", ...)`, after the existing `CHECK(output.find("--offline") != std::string::npos);` line, add:

```cpp
  CHECK(output.find("--fork") != std::string::npos);
  CHECK(output.find("--name") != std::string::npos);
```

- [ ] **Step 2: Create new E2E test file**

Create `examples/pdk_chat_demo/tests/test_pdk_chat_demo_session_tree_cli_flags.cpp`:

```cpp
#include <catch_amalgamated.hpp>
#include <cstdio>
#include <string>
#ifndef PDK_CHAT_DEMO_PATH
#define PDK_CHAT_DEMO_PATH "pdk_chat_demo"
#endif

namespace {
struct RunResult { int status; std::string stdout_out; std::string stderr_out; };

RunResult run_demo(const std::string& args, const std::string& stdin_input = "") {
  std::string command = "\"" PDK_CHAT_DEMO_PATH "\" " + args + " 2>&1";
  if (!stdin_input.empty()) {
    command = "printf '" + stdin_input + "' | " + command;
  }
  FILE* pipe = popen(command.c_str(), "r");
  if (!pipe) return {-1, "", ""};
  std::string out; char buf[512];
  while (fgets(buf, sizeof(buf), pipe)) out += buf;
  return {pclose(pipe), out, ""};
}
}

TEST_CASE("--fork with nonexistent node exits nonzero with diagnostic",
          "[cli][stage3][session-tree][e2e]") {
  const auto r = run_demo("--fork nonexistent_node --mock");
  CHECK(r.status != 0);
  CHECK(r.stdout_out.find("--fork") != std::string::npos);
  CHECK(r.stdout_out.find("nonexistent_node") != std::string::npos);
  CHECK(r.stdout_out.find("--help") != std::string::npos);
}

TEST_CASE("--fork without --session on empty session exits nonzero",
          "[cli][stage3][session-tree][e2e]") {
  const auto r = run_demo("--fork node_42 --mock");
  CHECK(r.status != 0);
  CHECK(r.stdout_out.find("--fork") != std::string::npos);
  CHECK(r.stdout_out.find("--help") != std::string::npos);
}

TEST_CASE("--name with --session on existing session exits nonzero (scope error)",
          "[cli][stage3][session-tree][e2e]") {
  // 即使 session_id 不存在, scope 检查也应在 fork 之前触发
  const auto r = run_demo("--session missing_session --name new-name --mock");
  CHECK(r.status != 0);
  CHECK(r.stdout_out.find("--name") != std::string::npos);
  CHECK(r.stdout_out.find("--help") != std::string::npos);
}

TEST_CASE("--fork missing value is rejected by declarative parser",
          "[cli][stage3][session-tree][e2e]") {
  const auto r = run_demo("--fork --mock");
  CHECK(r.status != 0);
  CHECK(r.stdout_out.find("--fork") != std::string::npos);
  CHECK(r.stdout_out.find("--help") != std::string::npos);
}

TEST_CASE("--name missing value is rejected by declarative parser",
          "[cli][stage3][session-tree][e2e]") {
  const auto r = run_demo("--name --mock");
  CHECK(r.status != 0);
  CHECK(r.stdout_out.find("--name") != std::string::npos);
  CHECK(r.stdout_out.find("--help") != std::string::npos);
}

TEST_CASE("normal --mock startup path is unaffected by new flag presence",
          "[cli][stage3][session-tree][e2e]") {
  const auto r = run_demo("--mock", "exit\n");
  CHECK(r.status == 0);
  CHECK(r.stdout_out.find("Mock mode: provider=mock, model=test") != std::string::npos);
}
```

- [ ] **Step 3: Register new test in CMakeLists.txt**

In `examples/pdk_chat_demo/tests/CMakeLists.txt`, after the existing `test_session_tree_commands` block (around line 386), add:

```cmake
# test_pdk_chat_demo_session_tree_cli_flags — session-tree-tui Wave 2-B
add_executable(test_pdk_chat_demo_session_tree_cli_flags
    test_pdk_chat_demo_session_tree_cli_flags.cpp
    ${CATCH_INCLUDE_DIR}/catch_amalgamated.cpp
    ${CATCH_INCLUDE_DIR}/main_test_runner.cpp
)
target_link_libraries(test_pdk_chat_demo_session_tree_cli_flags PRIVATE
    pdk_chat_demo_obj
    agenticdsl_includes
    agenticdsl_core
    hydraforge_pdk
    hydraforge_cxxopts
    Threads::Threads
)
target_include_directories(test_pdk_chat_demo_session_tree_cli_flags PRIVATE
    ${CATCH_INCLUDE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/..
)
target_compile_definitions(test_pdk_chat_demo_session_tree_cli_flags PRIVATE
    CATCH_CONFIG_ENABLE_ALL_STRINGMAKERS=1
    PDK_CHAT_DEMO_PATH="${PROJECT_BINARY_DIR}/examples/pdk_chat_demo/pdk_chat_demo"
)
add_test(
    NAME test_pdk_chat_demo_session_tree_cli_flags
    COMMAND test_pdk_chat_demo_session_tree_cli_flags
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/..
)
```

- [ ] **Step 4: Build new test target**

Run: `cd /workspace/project/HydraForge/.rddf/wt/session-tree-tui && cmake --build build --target test_pdk_chat_demo_session_tree_cli_flags 2>&1 | tail -10`
Expected: Build succeeds.

- [ ] **Step 5: Run new E2E tests**

Run: `cd /workspace/project/HydraForge/.rddf/wt/session-tree-tui && ctest -R test_pdk_chat_demo_session_tree_cli_flags --output-on-failure 2>&1 | tail -25`
Expected: All 6 test cases pass.

---

### Task 6: Full test suite + openspec validate

**Files:**
- Modify: `examples/pdk_chat_demo/tests/test_cli_args_parser.cpp` (re-run all)

- [ ] **Step 1: Run full pdk_chat_demo test suite**

Run: `cd /workspace/project/HydraForge/.rddf/wt/session-tree-tui && ctest -R "test_(cli_args_parser|pdk_chat_demo_cli_args|pdk_chat_demo_session_tree|session_manager_rename|chat_session|session_persistence|session_tree)" --output-on-failure 2>&1 | tail -30`
Expected: All parser + session + chat tests pass.

- [ ] **Step 2: Run full repo ctest**

Run: `cd /workspace/project/HydraForge/.rddf/wt/session-tree-tui && ctest --output-on-failure 2>&1 | tail -20`
Expected: All tests pass (or pre-existing failures unrelated to this change — verify no NEW failures).

- [ ] **Step 3: Run openspec validate**

Run: `cd /workspace/project/HydraForge/.rddf/wt/session-tree-tui && openspec validate session-tree-tui --json 2>&1 | tail -10`
Expected: `"passed": true, "failed": 0`.

- [ ] **Step 4: Verify no hand-written argv loops remain in main.cpp**

Run: `cd /workspace/project/HydraForge/.rddf/wt/session-tree-tui && grep -n "argv" examples/pdk_chat_demo/main.cpp | grep -v "argc\|argv,\|argv)\|argv\\[" 2>&1 | head -5`
Expected: No matches (all argv parsing goes through `parse_cli_args`).

- [ ] **Step 5: Confirm change gate**

Per tasks.md §5, the change gate is satisfied when:
- `openspec validate session-tree-tui --json` returns `passed: true`
- All new + existing tests pass
- No hand-written argv loop exists

---

## Self-Review Checklist (before execute)

1. **Spec coverage** (from `openspec/changes/session-tree-tui/specs/session-tree-cli-flags/spec.md`):
   - `cli-flag-fork-node`: Task 1+2+4 ✓
   - `cli-flag-name-session`: Task 1+2+4 ✓
   - `cli-flag-fork-with-name`: Task 4 (scope error + ordering) ✓
   - `fork-error-nonexistent-node`: Task 4 (nonexistent node / session / missing value) ✓

2. **Placeholder scan**: All steps have concrete code/file paths. No "TBD" / "TODO" / "implement later".

3. **Type consistency**: `CliDestination::fork_node_id` / `session_name` (Task 1) ↔ `CliOptions::fork_node_id` / `session_name` (Task 1) ↔ parser field population (Task 2) ↔ main.cpp extraction (Task 4) — names match.

4. **API alignment**: `SessionManager::rename_session(name)` (Task 3) called from main.cpp (Task 4) — signatures match.

5. **TDD discipline**: Each task has failing test → implementation → passing test cycle.

6. **Commit strategy**: All changes aggregate into one worktree commit (per `worktree-archive-workflow` proposal); no per-task commits.


## TDD Discipline

Each work unit in this plan follows the canonical 5-step TDD structure:

1. **Write the failing test** — Define expected behavior in a Catch2 case (or shell assertion)
2. **Run test to verify it fails** — Confirm red state before writing code
3. **Write minimal implementation** — Add the smallest code that makes the test pass
4. **Run test to verify it passes** — Confirm green state, then refactor
5. **Defer commit** — Batch all green units into a single archive commit per change

This discipline is enforced by `skill_use("execute")`; skipping any step breaks the red→green→commit chain.

