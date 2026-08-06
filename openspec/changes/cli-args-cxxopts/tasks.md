## 1. cxxopts vendoring and target integration

- [ ] 1.1 Add the pinned cxxopts single header under `external/cxxopts/` and retain the upstream license or NOTICE text beside it.
- [ ] 1.2 Record the exact cxxopts version and source revision in the external dependency note so upgrades are explicit and reviewable.
- [ ] 1.3 Update the `pdk_chat_demo` CMake target with target-scoped include configuration; do not add global `include_directories()`, `find_package()`, FetchContent, or system package requirements.
- [ ] 1.4 Configure the example build with cxxopts available in offline mode and verify the target does not attempt a network fetch.
- [ ] 1.5 Add a build check that the vendored header and license are present before compiling the demo.
- [ ] 1.6 Commit: `build(cli): vendor pinned cxxopts for offline chat demo parsing`

## 2. Declarative CLI options and parse wiring

- [ ] 2.1 Define the `CliOptions` value structure at the demo boundary for mock, session id, print, provider, and offline values, with explicit defaults.
- [ ] 2.2 Define the centralized flag declaration table containing long name, short name where applicable, argument type, default behavior, description, and destination mapping.
- [ ] 2.3 Implement `parse_cli_args(argc, argv)` with cxxopts and map validated values into `CliOptions`; keep cxxopts types out of downstream provider and session APIs.
- [ ] 2.4 Replace the hand-rolled `main.cpp` scan at lines 76-83 with the parser call while preserving the pre-parser `--skill-child` early branch.
- [ ] 2.5 Wire `--mock` and `--session <id>` to their existing initialization paths without changing provider or session semantics.
- [ ] 2.6 Wire `-p/--print`, `--provider`, and `--offline` to the existing startup configuration boundary without implementing RPC or persistent config mutation.
- [ ] 2.7 Commit: `refactor(chat-demo): replace hand-rolled argv scan with declarative cxxopts parser`

## 3. Existing flag behavior equivalence

- [ ] 3.1 Add parser unit coverage for `--mock`, `--session demo-session`, and the combined invocation, asserting exact `CliOptions` values.
- [ ] 3.2 Add regression coverage for the no-flag default path and verify the default provider/session behavior remains unchanged.
- [ ] 3.3 Add regression coverage for `--skill-child` so the child entry point is selected before normal option parsing.
- [ ] 3.4 Add an end-to-end mock invocation fixture that starts `pdk_chat_demo --mock` and verifies the demo reaches the same mock startup path as before.
- [ ] 3.5 Test repeated or malformed session arguments and document the selected cxxopts behavior instead of silently accepting an ambiguous value.
- [ ] 3.6 Commit: `test(cli): preserve mock and session flag behavior across parser migration`

## 4. Help generation and invalid input contract

- [ ] 4.1 Add `--help` coverage that captures generated output and asserts every current flag appears with its declaration and description.
- [ ] 4.2 Verify help exits successfully without initializing DSLEngine, loading plugins, opening sessions, or entering the input loop.
- [ ] 4.3 Add unknown flag coverage that asserts a diagnostic mentions the invalid option and `--help`, with a non-zero exit code.
- [ ] 4.4 Add missing-value coverage for `--session` and `--provider`, asserting a non-zero exit code and no partial startup.
- [ ] 4.5 Add short and long spelling coverage for `-p` and `--print`, asserting identical parse results.
- [ ] 4.6 Commit: `test(cli): cover generated help and non-zero invalid-argument exits`

## 5. Full validation and downstream handoff

- [ ] 5.1 Add `tests/test_cli_args_parser.cpp` for declaration-table and per-flag parse assertions.
- [ ] 5.2 Add `tests/test_pdk_chat_demo_cli_args.cpp` for help, unknown flag, missing value, and mock E2E process behavior.
- [ ] 5.3 Verify existing `test_chat_session` and `test_e2e_mock` fixtures remain green without changing their session or LLM assertions.
- [ ] 5.4 Run `cmake --build build --target pdk_chat_demo` and `ctest --output-on-failure -j$(nproc)`; record any known pre-existing failure separately.
- [ ] 5.5 Run `openspec validate cli-args-cxxopts --json` and require `summary.totals.failed = 0` and `passed = true`.
- [ ] 5.6 Confirm Wave 2 consumers can add declarations without editing the old imperative scan, then hand off to `chat-streaming-slash-tui` and `session-tree-tui`.
- [ ] 5.7 Commit: `test(cli): validate cxxopts migration and downstream flag extension point`
