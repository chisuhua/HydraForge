# Four Pre-existing HydraForge Test Failures — Repair Design

**Date:** 2026-08-12
**Scope:** `test_pdk_chat_demo_cli_args`, `test_pdk_chat_demo_session_tree_cli_flags`, `test_pdk_chat_model_command`, and `test_e2e_real_llm`

## Goal

Restore the four named tests without deleting coverage, weakening production behavior, or changing unrelated modules. The default local CTest run must not require a live minimax service; an explicitly enabled live-LLM run must retain strict assertions.

## Root causes

1. `examples/pdk_chat_demo/dsl_validator.cpp::extract_yaml_fenced_block()` reads the newline immediately following `````yaml`` as an empty first line. It therefore fails to recognize the repository's `# --- BEGIN AgenticDSL ---` fenced YAML files. `MarkdownParser::parse_yaml_fenced_block()` already contains the correct newline handling.
2. `pdk_chat_demo/main.cpp` does not process parsed `--fork`/`--name` options before DSL validation. `SessionManager` is initialized too late, so invalid startup options never produce their required diagnostics.
3. `test_pdk_chat_model_command.cpp` is a Wave 1 test. Phase C replaced the command implementation and added `test_model_switching.cpp`, but the old executable remains registered and asserts removed strings without injecting `g_command_session`.
4. `test_e2e_real_llm` is credential-gated but not explicitly execution-gated. A machine with a residual key or an unavailable endpoint can enter the live path during ordinary CTest and fail on model-dependent greeting text.

## Design decisions

### YAML and startup repair

- Add a regression test using the real fenced YAML layout before changing production code.
- Make `DslValidator::extract_yaml_fenced_block()` skip LF and CRLF after the opening fence, then inspect the next line for the begin marker. Preserve support for the existing inline/legacy behavior.
- Reuse the existing validator behavior and error types; do not rewrite the parser or schema.
- Move only the necessary session option validation/initialization ahead of DSL validation. Invalid `--fork` and `--name` combinations must fail with diagnostics containing the relevant flag/value and `--help`.
- Preserve the existing safe shutdown ordering: engine-owned registries must be destroyed before plugin shared objects are unloaded.

### Model command test repair

Remove the obsolete Wave 1 test target and its CMake registration. Keep `test_model_switching`, which tests the Phase C API used by the current `/model` command. No production `/model` output or behavior changes are needed.

### Real-LLM gate

- Require `HYDRAFORGE_RUN_REAL_LLM=1` to enter live minimax test cases.
- If the opt-in variable is absent, use Catch2 `SKIP()` with an explicit reason. This is distinct from a configured live run and does not turn live failures into passes.
- Keep `MINIMAX_API_KEY` validation and existing hard failures after opt-in. Keep the response non-empty assertion; do not make production or test success depend on hardcoded greeting prose.
- Ensure the test's plugin whitelist environment is set in the opt-in CTest registration, preserving the existing `HYDRAFORGE_PLUGIN_PATH` mechanism.

## Verification contract

1. Add/run a focused validator test that fails before the fenced-block fix and passes after it.
2. Run the two CLI subprocess targets and verify their expected diagnostics and zero SIGSEGV.
3. Run `test_model_switching`; confirm the obsolete target is no longer registered.
4. Run `test_e2e_real_llm` without opt-in and verify Catch2 reports a skip with CTest success.
5. Run it with opt-in only when the environment is configured; failures must remain visible.
6. Run the full CTest suite and report any unrelated pre-existing failures separately.

## Non-goals

- No general Markdown parser rewrite.
- No changes to plugin security policy.
- No hardcoded LLM responses.
- No deletion of active Phase C tests.
- No commits or history rewriting as part of this task.
