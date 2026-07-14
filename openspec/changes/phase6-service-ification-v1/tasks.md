## 1. W1 RED Verdict Remediation (active, in-progress)

- [ ] 1.1 Change change定位: "ADR-0050 Candidate B v1" → "Phase 6 PDK Composition Spike (pre-strategic validation)"
- [ ] 1.2 Verify proposal/design/specs/tasks reflect Spike framing (no more ADR-0050 §决策修改)
- [ ] 1.3 Create `docs/adr/adr-0051-phase6-pdk-composition-spike.md` (🔍 Proposed status)
- [ ] 1.4 ADR-0051 §决策 records Spike scope; explicitly NOT Candidate B兑现
- [ ] 1.5 Replace all DECLARE_TOOL references with `IToolRegistry::register_tool_function()` pattern
- [ ] 1.6 Rename `knowledge_base.query` → `knowledge_base/query` (ADR-0043 slash-only)
- [ ] 1.7 G3 ToolCategory fixed to `Execute`; `allowed_layers` = `{Workflow}` only
- [ ] 1.8 Contract normalized to `unordered_map<string,string> args → nlohmann::json result` (remove "JSON-in/JSON-out" claim)
- [ ] 1.9 Delete `ToolRegistry::call_tool()` instrumentation task; use existing `tool.audit.*` events instead
- [ ] 1.10 Re-classify 5 escalation triggers: 2 runtime safety (ToolCoordinator RAII) + 2 plugin health (audit + G3 self-check) + 1 design review (manual)
- [ ] 1.11 Run `openspec validate phase6-service-ification-v1 --strict` and confirm exit 0
- [ ] 1.12 Run second Metis review and confirm 0 CRITICAL findings

## 2. G3 Knowledge Base Plugin

**BLOCKED until**: §1 12/12 complete AND Stage Gate 2026-07-18 passed AND Sprint 23 capacity confirmed

- [ ] 2.1 Create directory structure `pdk/g3_knowledge_base/` matching `pdk/llama_engine/` pattern (CMakeLists.txt + plugin.cpp + plugin.h)
- [ ] 2.2 Implement G3 plugin entry point with `pdk_register_tools(IToolRegistry&)` using `registry.register_tool_function("knowledge_base/query", meta, lambda)` (NOT DECLARE_TOOL)
- [ ] 2.3 Implement tool handler with hardcoded 3-5 document snippets (in-memory `std::vector<std::string>`)
- [ ] 2.4 Implement internal session store keyed by `session_id` (`std::unordered_map<string, SessionState>`)
- [ ] 2.5 Implement MockLLMProvider call in tool handler (max 30 lines per spec)
- [ ] 2.6 Implement mandatory error schema `{success: bool, error: string?, payload: object?}` for all return paths
- [ ] 2.7 Verify G3 tool handler does NOT call any approval-requiring tool internally (defect #5 prevention)
- [ ] 2.8 Add `pdk/g3_knowledge_base/` to root `CMakeLists.txt` (PDK plugin subdirectory pattern)
- [ ] 2.9 Write `tests/test_g3_knowledge_base.cpp`: single-shot call test (new session_id)
- [ ] 2.10 Write `tests/test_g3_knowledge_base.cpp`: multi-turn test (same session_id, prior context preserved)
- [ ] 2.11 Write `tests/test_g3_knowledge_base.cpp`: session isolation test (different session_ids, independent contexts)
- [ ] 2.12 Write `tests/test_g3_knowledge_base.cpp`: error schema test (mandatory fields present)
- [ ] 2.13 Write `tests/test_g3_knowledge_base.cpp`: tool handler line count test (≤30 lines)
- [ ] 2.14 Run `ctest -R test_g3_knowledge_base --output-on-failure` and confirm 5/5 PASS
- [ ] 2.15 Run `cmake --build build --target agenticdsl_pdk` and confirm zero compile errors

## 3. G1 Coding Assistant Plugin

**BLOCKED until**: §2 complete + §1 12/12 complete

- [ ] 3.1 Create directory structure `pdk/g1_coding_assistant/` matching `pdk/llama_engine/` pattern
- [ ] 3.2 Implement G1 plugin entry point with `DEFINE_AGENT(React, ...)` per Sprint 20 macro
- [ ] 3.3 Register exactly 1 tool manifest entry referencing `knowledge_base/query` (discover via `IToolRegistry::has_tool()`)
- [ ] 3.4 Implement 2-step ReAct loop: step 1 invokes G3 tool, step 2 synthesizes final review comment
- [ ] 3.5 Implement mock code input handler (treat code as opaque string, no parsing)
- [ ] 3.6 Implement MockLLMProvider wiring (Sprint 19 mock pattern)
- [ ] 3.7 Verify G1 source uses `DEFINE_AGENT(React, ...)` syntax (no new agent loop macro)
- [ ] 3.8 Add `pdk/g1_coding_assistant/` to root `CMakeLists.txt`
- [ ] 3.9 Write `tests/test_g1_coding_assistant.cpp`: 2-step ReAct loop execution test
- [ ] 3.10 Write `tests/test_g1_coding_assistant.cpp`: tool manifest size assertion (exactly 1 entry)
- [ ] 3.11 Write `tests/test_g1_coding_assistant.cpp`: MockLLMProvider wiring test (no real LLM)
- [ ] 3.12 Run `ctest -R test_g1_coding_assistant --output-on-failure` and confirm 3/3 PASS

## 4. End-to-End Integration

**BLOCKED until**: §3 complete + §1 12/12 complete

- [ ] 4.1 Implement end-to-end scenario: G1 invokes G3 via existing `IToolRegistry::call_tool()`
- [ ] 4.2 Verify G3's internal MockLLMProvider receives prior context on 2nd G1→G3 call (multi-turn through composition)
- [ ] 4.3 Verify session isolation works through composition: G1 session A vs session B invoke G3 with different `session_id`s
- [ ] 4.4 Verify error propagation: G3 returns `{success: false, error: "..."}` → G1 ReAct receives and surfaces error
- [ ] 4.5 Write `tests/test_service_v1.cpp`: full E2E test (G1-calls-G3 end-to-end with multi-turn)
- [ ] 4.6 Write `tests/test_service_v1.cpp`: session isolation through composition test
- [ ] 4.7 Write `tests/test_service_v1.cpp`: error propagation through composition test
- [ ] 4.8 Run full ctest suite and confirm 72+N/72+N PASS (zero regression)
- [ ] 4.9 Run ASan suite and confirm 72+N/72+N PASS (zero regression)
- [ ] 4.10 Run `wc -l` on G3 tool handler and confirm ≤30 lines

## 5. Awkward Pattern Detection Method

**BLOCKED until**: §2-§3 complete

- [ ] 5.1 Create Layer 1 static code review checklist at `docs/service-composition/layer1-checklist.md` (5 categories)
- [ ] 5.2 Configure G3 plugin to emit Layer 2 instrumentation fields via existing `tool.audit.{invoked,completed,denied}` event payloads (NOT modifying `ToolRegistry::call_tool()`)
- [ ] 5.3 Verify G3's audit events contain: `caller_session_id`, `callee_tool_name`, `args_keys_only`, `return_latency_ms`, `callee_internally_invoked_llm` (self-reported in G3 audit metadata)
- [ ] 5.4 Create Layer 3 memo template at `docs/service-composition/layer3-memo-template.md` (5 fixed sections)
- [ ] 5.5 Run Layer 1 review on G1+G3 source code (primary engineer) — record findings
- [ ] 5.6 Run Layer 1 review on G1+G3 source code (reviewer engineer) — independent findings
- [ ] 5.7 Run Layer 3 memo (primary engineer) — 1-page "what felt wrong" capturing tacit patterns
- [ ] 5.8 Run Layer 3 memo (reviewer engineer) — independent 1-page memo
- [ ] 5.9 Compare Layer 3 dual memos: identify divergence (signals orthogonal findings)

## 6. Escalation Trigger Monitoring

**BLOCKED until**: §2-§3 complete + ToolCoordinator RAII implementation complete

- [ ] 6.1 Implement ToolCoordinator RAII guard: nesting depth > 2 → HARD KILL (ToolCoordinator .h/.cpp modification allowed per Oracle Q4)
- [ ] 6.2 Implement ToolCoordinator cycle detection (same tool on stack twice) → IMMEDIATE HARD KILL
- [ ] 6.3 G3 plugin self-reports session store size via audit event; trigger if > 1K
- [ ] 6.4 G3 plugin self-reports error-as-success ratio via audit; trigger if > 10%
- [ ] 6.5 ADR-0051 review process: 2+ awkward pattern categories from Layer 1 + Layer 3 dual memos → formalization trigger
- [ ] 6.6 Add unit test for each escalation trigger (5 tests total)
- [ ] 6.7 Wire escalation triggers to `tests/test_service_v1.cpp` E2E tests

## 7. ADR-0051 Finalization

**BLOCKED until**: §2-§6 complete + Layer 3 dual memos produced

- [ ] 7.1 Update ADR-0051 §决策 with finalized v1 contract (`register_tool_function`-based, `unordered_map<string,string> → nlohmann::json`)
- [ ] 7.2 Update ADR-0051 §不变量 with explicit logical-not-physical isolation warning
- [ ] 7.3 Update ADR-0051 §观察 with Layer 3 dual memo findings (primary + reviewer)
- [ ] 7.4 Update ADR-0051 §触发条件 with reclassified escalation trigger thresholds (2 runtime + 2 plugin health + 1 design review)
- [ ] 7.5 Update ADR-0051 §后续 with non-normative onboarding seed section

## 8. Onboarding Documentation

**BLOCKED until**: §7 complete

- [ ] 8.1 Create `docs/service-composition/spike-onboarding.md` (2-3 pages, 15 min readable)
- [ ] 8.2 Section 1: "What Spike IS" — in-process / `register_tool_function`-based / `unordered_map<string,string> → nlohmann::json` / no new macros
- [ ] 8.3 Section 2: "What Spike IS NOT" — not networked / not async / not streaming / not multi-tenant / not Candidate B v1
- [ ] 8.4 Section 3: "Spike Contract" normative spec (~1 page: tool name format / args schema / return schema / error schema)
- [ ] 8.5 Section 4: "Does your Agent fit Spike?" decision tree (4 questions: stateless / session / streaming / cross-agent)
- [ ] 8.6 Section 5: Trigger thresholds for DECLARE_SERVICE push (cross-reference escalation triggers)
- [ ] 8.7 Section 6: Reference implementation links to G1+G3 plugins

## 9. Complete Test Coverage

**BLOCKED until**: §2-§8 complete

- [ ] 9.1 Expand `tests/test_service_v1.cpp` to cover all 3 spec files' requirements
- [ ] 9.2 Test pdk-service-composition contract: in-process discovery, transport-agnostic signatures, logical-not-physical declaration
- [ ] 9.3 Test coding-assistant-agent: 2-step ReAct, single tool, mock code, DEFINE_AGENT(React) usage
- [ ] 9.4 Test knowledge-base-agent: hardcoded retrieval, multi-turn session, session isolation, error schema, ≤30 line handler
- [ ] 9.5 Run full ctest suite: confirm 72+N/72+N PASS

## 10. Ship Gate Hard Block Verification

**BLOCKED until**: §2-§9 complete + Stage Gate 2026-07-18 passed + Sprint 23 capacity confirmed

- [ ] 10.1 Verify Stage Gate 2026-07-18 re-evaluation passed and Spike ship gate conditions met (NOT ADR-0050 amendment — removed per Oracle Q6 Spike framing)
- [ ] 10.2 Verify Sprint 23 commitment: 1.5 eng × 2 weeks committed (Risk V1-R2 closed)
- [ ] 10.3 Verify ctest zero regression (72+N/72+N PASS)
- [ ] 10.4 Verify ASan zero regression (72+N/72+N PASS)
- [ ] 10.5 Verify NO DECLARE_SERVICE macro introduced (grep `include/agenticdsl/` for `DECLARE_SERVICE` returns 0)
- [ ] 10.6 Verify NO new namespace introduced (grep for `agenticdsl::service` returns 0)
- [ ] 10.7 Verify NO existing ADR amended (Tier 1/2/3 fallback protocol satisfied if any defect)
- [ ] 10.8 Verify BOTH Layer 3 dual memos committed to `docs/service-composition/observations/`
- [ ] 10.9 Verify all 5 escalation triggers wired and tested (5 unit tests PASS)
- [ ] 10.10 Run `openspec validate phase6-service-ification-v1 --strict` and confirm exit 0
- [ ] 10.11 ToolCoordinator RAII guard implementation present (nesting depth + cycle detection, per Oracle Q4)

## 11. ADR Status Flip and Archive

**BLOCKED until**: §10 11/11 complete

- [ ] 11.1 Change ADR-0051 status from 🔍 Proposed → ✅ Approved (experimental) in `docs/adr/adr-0051-phase6-pdk-composition-spike.md` §状态
- [ ] 11.2 Commit ADR-0051 status flip change (separate commit per project convention)
- [ ] 11.3 Run `scripts/sprint-closeout.sh` and confirm 7/7 steps green
- [ ] 11.4 Run `tools/adr_lint.py` and confirm 0 errors
- [ ] 11.5 Run `tools/docs_drift_audit.py` and confirm 0 DRIFT items
- [ ] 11.6 Mark all tasks in `tasks.md` complete (every `- [ ]` → `- [x]`)
- [ ] 11.7 Run `openspec archive phase6-service-ification-v1 --yes` and confirm archive to `openspec/changes/archive/`
- [ ] 11.8 Update master plan `docs/superpowers/plans/2026-07-10-phase5-remainder-adr-sync.md` §十/§十一/§十二 with C19 (phase6-service-ification-v1 Spike) ship record
- [ ] 11.9 Update `docs/active-status.md` §一 Approved count (+1 for ADR-0051 experimental) and §二 Closed changes table
- [ ] 11.10 Update `AGENTS.md` Recent Changes section with C19 (phase6-service-ification-v1 Spike) ship record (2026-07-XX)
- [ ] 11.11 Schedule C20 placeholder: G2/G4/G5 team kickoff date set for Sprint 24 (after Spike ship)

## 12. Post-Ship Follow-Up

- [ ] 12.1 Run sprint-closeout-7-days-later validation (re-run ctest + ASan after 7 days, verify no regression introduced)
- [ ] 12.2 Schedule C20 kickoff date (single unified kickoff for G2/G4/G5 teams using spike-onboarding.md as material)
- [ ] 12.3 Begin ADR-0052+ drafts only if 2+ different-category awkward patterns triggered during demo (per spec §Awkward Pattern Detection Methodology)
- [ ] 12.4 Monitor escalation triggers in production for 2 weeks (per Stage Gate 2-week stability rule)

## 13. Spike → Candidate B Promotion Criteria (W4+, post-Spike)

- [ ] 13.1 Evidence threshold: ≥3 awkward patterns from ≥2 different Layer 1 categories observed
- [ ] 13.2 Layer 1 reviewer agreement: ≥2 reviewers independently identify the patterns
- [ ] 13.3 Layer 3 dual memos convergence: primary + reviewer memos agree on ≥1 major awkward pattern
- [ ] 13.4 ADR-0050 §启动条件 #5 re-evaluation: Oracle round 4 confirms internal Spike evidence supports "外部 agent/tool" demand
- [ ] 13.5 If all 13.1-13.4 met: propose ADR-0052 "Phase 6 Candidate B v1" (decides whether to launch Phase 6 with `phase6-service-ification-v2`)
- [ ] 13.6 If not met: ADR-0051 stays ✅ Approved (experimental); strategic direction re-evaluation required