## 1. CLI flag registry integration

- [ ] 1.1 Confirm the shipped `cli-args-cxxopts` registry API and add declarative entries for `--fork <node_id>` and `--name <session_name>` with defaults, value requirements, and help text.
- [ ] 1.2 Add parser tests for missing values, repeated flags, `--fork node_42`, `--name my-debug-session`, and combined `--session sess_abc --fork node_42`.
- [ ] 1.3 Verify generated `--help` output contains both flags and their session-tree descriptions.
- [ ] 1.4 Commit: `feat(cli): declare session tree startup flags through cxxopts registry`

## 2. Chat demo startup wiring

- [ ] 2.1 Replace the local `--session` argument scan in `examples/pdk_chat_demo/main.cpp` with the shared declarative parse result, preserving `--mock` and existing defaults.
- [ ] 2.2 Implement startup sequencing after `SessionManager` creation: resolve the session id, load the JSONL session, apply `--fork` through `SessionManager::fork`, and switch to the new branch before registering or entering the chat loop.
- [ ] 2.3 Apply `--name` only when creating a new session, persisting the name in session metadata without silently renaming an existing `--session` target.
- [ ] 2.4 Verify the normal startup path and the existing slash command registry still work when neither new flag is supplied.
- [ ] 2.5 Commit: `feat(chat-demo): wire startup fork and new-session name flags`

## 3. Session metadata and error contract

- [ ] 3.1 Reuse an existing SessionManager rename or metadata API if available; otherwise add the smallest `rename_session` API needed for new-session creation and persistence, without changing fork semantics.
- [ ] 3.2 Add validation for empty or malformed `node_id`, missing session ids, missing nodes, and invalid flag combinations, returning a non-zero process status before the chat loop.
- [ ] 3.3 Standardize diagnostics to include the offending flag/value and a `--help` hint, while avoiding fallback to a different session after an explicit `--session` or `--fork` request.
- [ ] 3.4 Commit: `fix(session): make startup tree flag errors explicit and non-destructive`

## 4. Tests and validation

- [ ] 4.1 Add unit coverage for the declarative flag table and startup operation ordering, including load before fork and name persistence scope.
- [ ] 4.2 Add `tests/test_pdk_chat_demo_session_tree_cli_flags.cpp` covering `--fork`, `--name`, combined `--session --fork`, mock mode, help output, and non-existent session/node failures.
- [ ] 4.3 Add a persistence regression that reopens the JSONL session and verifies the forked branch and new-session name are present.
- [ ] 4.4 Build the pdk chat demo and run the focused tests, then run `ctest --output-on-failure` for the full suite.
- [ ] 4.5 Commit: `test(chat-demo): cover session tree startup flags and persistence errors`

## 5. Change gate

- [ ] 5.1 Verify no hand-written argv loop remains for the new flags and `--help` is generated from the shared registry.
- [ ] 5.2 Run `openspec validate session-tree-tui --json` and confirm the result reports `passed: true` and `failed: 0`.
- [ ] 5.3 Commit: `chore(openspec): validate session tree cli flag artifacts`
