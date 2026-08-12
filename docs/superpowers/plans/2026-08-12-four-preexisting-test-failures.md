# Four Pre-existing Test Failures Repair Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Make the four named pre-existing tests reliable by fixing fenced-YAML detection and startup option ordering, removing obsolete Wave 1 test registration, and making live minimax execution explicitly opt-in.

**Architecture:** Keep the production parser and startup flow focused. Reuse the newline handling already present in `MarkdownParser::parse_yaml_fenced_block()`. Validate session CLI options before expensive/plugin-dependent DSL startup, while retaining engine-before-plugin unload ordering. Treat Phase C `test_model_switching` as the current `/model` contract and gate only the external live-LLM test path.

**Tech Stack:** C++20, CMake/CTest, Catch2 v3, nlohmann JSON, existing AgenticDSL parser/session/plugin APIs.

---

## File map and constraints

- Modify `examples/pdk_chat_demo/dsl_validator.cpp:86-114` — skip LF and CRLF after the opening YAML fence before matching the begin marker.
- Test `examples/pdk_chat_demo/tests/test_dsl_validator.cpp` or the existing validator test file — add a real fenced YAML fixture and assert `ValidationResult::valid`.
- Modify `examples/pdk_chat_demo/main.cpp` only around startup/session setup — ensure `--fork`/`--name` diagnostics happen before DSL validation, without changing the plugin cleanup order.
- Test `examples/pdk_chat_demo/tests/test_pdk_chat_demo_session_tree_cli_flags.cpp` — retain subprocess assertions; use them as the regression proof for early diagnostics.
- Modify `examples/pdk_chat_demo/tests/CMakeLists.txt:272-335` — add live-test opt-in environment handling and remove only the obsolete `test_pdk_chat_model_command` target block.
- Modify `examples/pdk_chat_demo/tests/test_e2e_real_llm.cpp:70-74,126-131` — skip unless `HYDRAFORGE_RUN_REAL_LLM=1`, then retain strict API/response assertions.
- Do not modify `commands/model_command.cpp`, `chat_session.cpp`, `test_model_switching.cpp`, or plugin whitelist production code.
- Do not commit; the user did not request commits.

### Task 1: Lock the fenced-YAML regression

**Files:**
- Test: `examples/pdk_chat_demo/tests/test_dsl_validator.cpp` (use the existing validator-test file if the target has a different current name)
- Modify: `examples/pdk_chat_demo/tests/CMakeLists.txt` only if the test target is not already registered

- [ ] **Step 1: Write the failing test**

Add a Catch2 case using a literal with the production layout:

```cpp
TEST_CASE("validates fenced AgenticDSL YAML after opening-fence newline",
          "[dsl-validator][yaml]") {
  const std::string content =
      "### AgenticDSL `/main`\n"
      "```yaml\n"
      "# --- BEGIN AgenticDSL ---\n"
      "name: test-agent\n"
      "version: 1\n"
      "agent_loop: react\n"
      "nodes:\n"
      "  - id: start\n"
      "    type: start\n"
      "  - id: end\n"
      "    type: end\n"
      "```\n";

  const auto result = pdk_chat_demo::DslValidator{}.validate(content);

  REQUIRE(result.valid);
}
```

Use the repository's actual required-field value syntax if the existing validator fixture establishes a stricter scalar format; the important regression is the ` ```yaml\n# --- BEGIN...` boundary and a valid `nodes:` list.

- [ ] **Step 2: Run the test to verify it fails**

Run:

```bash
cmake --build build --target <validator-test-target> -j2
build/examples/pdk_chat_demo/tests/<validator-test-binary> "[dsl-validator][yaml]"
```

Expected before the fix: the case fails because the validator treats the fenced block as absent and reports missing frontmatter/nodes fields.

- [ ] **Step 3: Implement the minimal parser fix**

In `extract_yaml_fenced_block`, replace the current first-line calculation:

```cpp
size_t after_fence = fence_pos + fence_open.size();
size_t line_end = content.find('\n', after_fence);
```

with the same LF/CRLF handling used by `MarkdownParser::parse_yaml_fenced_block`:

```cpp
size_t line_start = fence_pos + fence_open.size();
if (line_start < content.size() && content[line_start] == '\n') {
  ++line_start;
} else if (line_start + 1 < content.size() &&
           content[line_start] == '\r' && content[line_start + 1] == '\n') {
  line_start += 2;
}
const size_t line_end = content.find('\n', line_start);
if (line_end == std::string::npos) return "";
const std::string first_line = content.substr(line_start, line_end - line_start);
```

Keep the existing trim, marker comparison, and closing-fence extraction unchanged.

- [ ] **Step 4: Run the test to verify it passes**

Run the same focused test. Expected: PASS. Then run the existing validator/parser tests:

```bash
ctest --test-dir build --output-on-failure -R "(dsl|yaml|parser)"
```

Expected: no new failures.

### Task 2: Repair startup session-option ordering and preserve cleanup safety

**Files:**
- Modify: `examples/pdk_chat_demo/main.cpp`
- Test: `examples/pdk_chat_demo/tests/test_pdk_chat_demo_session_tree_cli_flags.cpp`
- Related existing API: `src/core/session_manager.{h,cpp}`

- [ ] **Step 1: Confirm the failing subprocess contract**

Run:

```bash
ctest --test-dir build --output-on-failure -R "test_pdk_chat_demo_session_tree_cli_flags|test_pdk_chat_demo_cli_args"
```

Expected before the fix: the subprocess exits nonzero, but valid `--fork`/`--name` diagnostics are absent and `--mock` can terminate with SIGSEGV after DSL validation.

- [ ] **Step 2: Add/strengthen one regression assertion if needed**

Keep the existing assertions that require the relevant flag, supplied value, and `--help` in the diagnostic. Do not replace them with only `status != 0`; the output contract is the regression.

- [ ] **Step 3: Implement early session option handling**

After CLI parsing/help handling and before plugin loading or DSL validation:

1. Construct/open the `SessionManager` using the configured persistence directory.
2. Resolve the requested `--session` if present.
3. Reject `--fork` without a usable session/node source, with a diagnostic containing `--fork`, the supplied node id when available, and `--help`.
4. Reject `--name` combined with `--session` when that combination is prohibited by the current CLI contract, with `--name` and `--help`.
5. Apply valid fork/name operations through the existing `SessionManager` API.

Do not create a second session manager later. Keep the existing `StartupCleanupGuard` lifetime and its safe order (`engine.reset()` before plugin unloading). Do not add a new global or bypass existing session APIs.

- [ ] **Step 4: Build and run the two subprocess targets**

```bash
cmake --build build --target pdk_chat_demo test_pdk_chat_demo_cli_args test_pdk_chat_demo_session_tree_cli_flags -j2
ctest --test-dir build --output-on-failure -R "test_pdk_chat_demo_cli_args|test_pdk_chat_demo_session_tree_cli_flags"
```

Expected: both targets PASS; no SIGSEGV; invalid startup options report their own diagnostics.

### Task 3: Remove the obsolete Wave 1 `/model` test target

**Files:**
- Modify: `examples/pdk_chat_demo/tests/CMakeLists.txt:308-335`
- Delete: `examples/pdk_chat_demo/tests/test_pdk_chat_model_command.cpp` only if it is not referenced elsewhere
- Preserve: `examples/pdk_chat_demo/tests/test_model_switching.cpp:587-611` target and tests

- [ ] **Step 1: Verify the stale test fails for the obsolete contract**

```bash
ctest --test-dir build --output-on-failure -R "test_pdk_chat_model_command|test_model_switching"
```

Expected before the change: the Wave 1 target fails on `Wave 1 stub` and lowercase `usage: /model`; the Phase C target remains the current contract test.

- [ ] **Step 2: Remove only obsolete registration and source**

Delete the complete `test_pdk_chat_model_command` executable/add_test block. Delete the stale source only after confirming no other CMake target or source includes it. Do not modify the current command implementation or delete Phase C tests.

- [ ] **Step 3: Reconfigure and verify target registration**

```bash
cmake -S . -B build
cmake --build build --target test_model_switching -j2
ctest --test-dir build --output-on-failure -R "test_pdk_chat_model_command|test_model_switching"
```

Expected: `test_model_switching` PASS and no registered `test_pdk_chat_model_command` failure.

### Task 4: Make real minimax execution explicitly opt-in

**Files:**
- Modify: `examples/pdk_chat_demo/tests/test_e2e_real_llm.cpp:70-74,126-131`
- Modify: `examples/pdk_chat_demo/tests/CMakeLists.txt:272-306`

- [ ] **Step 1: Verify the current live test is environment-sensitive**

```bash
env -u HYDRAFORGE_RUN_REAL_LLM ctest --test-dir build --output-on-failure -R test_e2e_real_llm
```

Expected before the change: the test may run when `MINIMAX_API_KEY` is inherited, and can fail due to unavailable service/plugin setup or model-dependent greeting content.

- [ ] **Step 2: Add the explicit opt-in gate**

At the beginning of each live test case (or in one shared helper used by both cases), require exactly `HYDRAFORGE_RUN_REAL_LLM=1`. If absent, call:

```cpp
SKIP("HYDRAFORGE_RUN_REAL_LLM=1 is required for live minimax tests");
```

Keep the existing `MINIMAX_API_KEY` gate after this opt-in check. Do not convert failures after opt-in into skips. Keep the non-empty response and provider error assertions.

- [ ] **Step 3: Keep the plugin whitelist in the test environment**

Retain `HYDRAFORGE_PLUGIN_PATH=${PROJECT_BINARY_DIR}/pdk` on the CTest property. If CMake supports it in the current minimum version, compose the live opt-in variable there only for an explicitly requested live invocation; otherwise document that `HYDRAFORGE_RUN_REAL_LLM=1` must be exported by the caller. Do not silently enable live traffic in default CTest.

- [ ] **Step 4: Verify default skip and explicit strict mode**

Default:

```bash
env -u HYDRAFORGE_RUN_REAL_LLM ctest --test-dir build --output-on-failure -R test_e2e_real_llm
```

Expected: PASS with Catch2 SKIP output.

Explicit mode (only where credentials/network are intentionally configured):

```bash
HYDRAFORGE_RUN_REAL_LLM=1 ctest --test-dir build --output-on-failure -R test_e2e_real_llm
```

Expected: the test executes; real failures remain failures.

### Task 5: Full verification and post-write review

**Files:** all files changed by Tasks 1–4.

- [ ] **Step 1: Run diagnostics/build checks**

```bash
cmake --build build -j2
ctest --test-dir build --output-on-failure
```

Expected: all failures caused by this work are resolved. Record unrelated failures with their existing baseline explanation rather than changing them.

- [ ] **Step 2: Run static diagnostics on changed C++ files**

Run the configured clangd/LSP diagnostics for:

```text
examples/pdk_chat_demo/dsl_validator.cpp
examples/pdk_chat_demo/main.cpp
examples/pdk_chat_demo/tests/test_e2e_real_llm.cpp
examples/pdk_chat_demo/tests/CMakeLists.txt
```

Expected: no new errors or warnings attributable to the changes.

- [ ] **Step 3: Perform the post-write review**

Check that the diff contains no type suppression, no empty catch, no deleted active tests, no unrelated formatting/refactor, no production behavior change to `/model`, and no live-network request in default CTest. Confirm each changed behavior is covered by a test that would fail if reverted.

- [ ] **Step 4: Inspect final working-tree diff**

```bash
GIT_MASTER=1 git status --short
GIT_MASTER=1 git diff --stat
GIT_MASTER=1 git diff --check
```

Expected: only intended design/plan/source/test files are changed; do not commit.
