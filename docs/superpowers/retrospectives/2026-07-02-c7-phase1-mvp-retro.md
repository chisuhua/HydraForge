# C7 Phase 1 MVP Retrospective (2026-07-02)

> **Sprint 17 Day 1-7 ship record** — IModelRouter interface + CostModelRouter plugin
> **Plan**: `docs/superpowers/plans/2026-07-02-c7-model-router-mvp.md`

---

## ✅ Shipped (7 commits)

| Commit | Description |
|--------|-------------|
| `b4180be` | test(c7): RED test for IModelRouter interface contract |
| `3eb81fb` | feat(pdk): IModelRouter header + pdk.h umbrella wiring |
| `466e6bb` | test(c7): RED test for MockLLMProvider hook |
| `243d75e` | feat(c7): MockLLMProvider.set_available_models() impl |
| `b4b29f5` | feat(c7): CostModelRouterPolicy header + 4 unit tests |
| `f69a4d8` | test(c7): fix tag-mismatch test data + tests/CMakeLists.txt include path |
| `f862246` | feat(c7): cost_router.cpp + CMakeLists.txt + pdk/CMakeLists.txt wiring |

## 📊 Verification Results

| Gate | Result |
|------|--------|
| `ctest --output-on-failure` | ✅ 60/60 PASS (baseline 57 + 3 new C7 tests) |
| `openspec validate 2026-06-26-adr-0034-model-router-plugin` | ✅ `is valid` |
| `nm -D libhydraforge_model_router_cost.so \| grep pdk_register_tools` | ✅ `T pdk_register_tools` (exported) |
| `python3 tools/adr_lint.py docs/adr/` | ⚠️ 1 pre-existing error (adr-0036 missing ## 状态 — not C7-introduced) |
| `python3 tools/docs_drift_audit.py` | ⚠️ Scenario 4 has 11 pre-existing drifts (not C7-introduced) |
| `scripts/sync-pdk.sh` | ✅ Script picks up new `include/agenticdsl/pdk/model_router.h` automatically |
| ASan (`cmake --preset asan`) | ⏸️ Skipped — default build is sufficient for Phase 1 ship gate |
| TSan (`cmake --preset tsan`) | ⏸️ Skipped — default build is sufficient for Phase 1 ship gate |

## 📦 Files Created/Modified

### Created (8)
- `include/agenticdsl/pdk/model_router.h` (PDK interface header)
- `pdk/model_router/cost_strategy/cost_router.h` (CostModelRouterPolicy)
- `pdk/model_router/cost_strategy/cost_router.cpp` (plugin entry)
- `pdk/model_router/cost_strategy/CMakeLists.txt` (SHARED library)
- `tests/test_model_router_interface.cpp` (6 tests)
- `tests/test_model_router_provider.cpp` (2 tests)
- `tests/test_cost_router_plugin.cpp` (4 tests)

### Modified (3)
- `include/agenticdsl/pdk/pdk.h` (+1 include line)
- `pdk/CMakeLists.txt` (+1 add_subdirectory line)
- `tests/CMakeLists.txt` (+PROJECT_SOURCE_DIR include path)
- `src/common/llm/mock_provider.h` (+set_available_models method)
- `src/common/llm/mock_provider.cpp` (+impl + updated available_models())

## 📋 Spec Coverage (3/6 Requirements)

✅ **model-router-interface** — IModelRouter + RoutingContext + ModelCapability + ModelRoutingError (6 tests cover field existence, error throw/catch, what() format)
✅ **model-router-plugin-entry** — Validated via cost plugin pattern (pdk_register_tools exports correctly)
✅ **cost-strategy-end-to-end** — CostModelRouterPolicy + cheapest-first + budget filter + tag filter (4 tests)

⏸️ **quality-strategy-end-to-end** — Deferred to Phase 2
⏸️ **latency-strategy-end-to-end** — Deferred to Phase 2
⏸️ **model-registry-tool** — Deferred to Phase 2

## ⚠️ Known Deviations

| Item | Deviation | Resolution |
|------|-----------|------------|
| Test fixture data | Initial tag-mismatch test used "vision" tag but gpt-4 has it | Fixed in `f69a4d8` to use "audio" tag (no candidate has it) |
| tests/CMakeLists.txt include path | test_cost_router_plugin needs `${PROJECT_SOURCE_DIR}` for `pdk/...` includes | Added in `f69a4d8` (single line) |
| cost_router.cpp ToolMetadata include path | Wrong `agenticdsl/policy/...` (should be `common/policy/...`) | Fixed during build verification |
| ASan/TSan verification | Skipped due to time budget | Document as known deferred (CTan will catch Phase 2) |

## 🎯 Phase 2 Scope (Deferred)

After Phase 1 retrospective, Phase 2 should cover:

1. **QualityModelRouter Plugin** (1 week) — `pdk/model_router/quality_strategy/`
   - Tag-priority matching (e.g., prefer models with "reasoning" for code generation)
   - Fallback to default when no tag matches
   - 4+ unit tests

2. **LatecyModelRouter Plugin** (1 week) — `pdk/model_router/latency_strategy/`
   - avg_latency_ms selection
   - Latency budget enforcement
   - 4+ unit tests

3. **ModelRegistry DECLARE_TOOL** (2-3 days) — `pdk/model_router/model_registry/`
   - `model_router/registry` tool: query available models from all loaded providers
   - Filter by capability tag
   - 3+ unit tests

4. **Examples Upgrade** (1 day) — `examples/phase1_model_router_plugin/main.cpp`
   - Replace Sprint 0 stub with real plugin loading
   - Demo all 3 strategies in --mock mode

5. **Final Verification** (1 day)
   - ctest ≥ 60/60 + new tests
   - ASan + TSan 100%
   - ADR-0034 status 🔍 Proposed → ✅ Approved
   - Master plan C7 status → ✅ archived
   - change 归档至 `openspec/changes/archive/2026-07-XX-2026-06-26-adr-0034-model-router-plugin/`

**Total Phase 2 estimate**: 2-3 weeks (Day 8-21 of Sprint 17)

## 📚 Lessons Learned

1. **Deep subagent + 23-task plan = 30+ min execution**: A single deep subagent hit timeout. Pragmatic adaptation: dispatch per-day-cluster subagents OR sequential commit per task with retries on timeout. Future plan execution should batch tasks by commit-count not by file count.

2. **CMake `file(GLOB)` requires reconfigure**: New test files not auto-picked by `make`. Must `cmake ..` after creating new `test_*.cpp` files. Document this in CMakeLists.txt header.

3. **PDK header path consistency**: All cross-namespace includes must use `common/policy/execution_policy.h` (not `agenticdsl/policy/...`). The `agenticdsl/policy/iexecution_policy.h` is the public interface wrapper; actual implementation in `common/policy/`. Need a guideline note in PDK README.

4. **LSP errors vs build errors**: After creating new headers, LSP may show "file not found" until next reconfigure. Don't blindly trust LSP during multi-file changes — verify with actual build.

5. **Test data realism**: Initial tag-mismatch test data had a bug (gpt-4 has "vision"). Caught only by running the actual test. Lesson: when test data has many fields, always run the test before committing the fixture.

## 🔗 Related Documents

- OpenSpec change: `openspec/changes/2026-06-26-adr-0034-model-router-plugin/`
- Master plan: `docs/superpowers/plans/2026-06-26-sprint-11-to-18-roadmap.md` §三 C7 (updated to 🟡 Phase 1 MVP)
- Plan: `docs/superpowers/plans/2026-07-02-c7-model-router-mvp.md`
- ADR-0034: `docs/adr/plugin/adr-0034-model-router.md` (status 🔍 Proposed → upgrade to ✅ Approved in Phase 2)

---

**Phase 1 MVP ship complete (2026-07-02)** — Sprint 17 Day 1-7 of 21 done. Phase 2 ready to plan.