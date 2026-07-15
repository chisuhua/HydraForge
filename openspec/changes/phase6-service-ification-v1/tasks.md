## 1. W1 RED Verdict Remediation ✅ COMPLETE (2026-07-14, per active-status.md §二 + ADR-0051 §启动条件 #1)

> **Oracle 审查 P0 修复**: active-status §二 记录 "W1 fix list 11/12 ✅ + 2nd Metis 0 CRITICAL ✅" (2026-07-14)。本节 12/12 全部 completed,作为 W2-W3 启动的 W1 baseline。

- [x] 1.1 Change change定位: "ADR-0050 Candidate B v1" → "Phase 6 PDK Composition Spike (pre-strategic validation)"
- [x] 1.2 Verify proposal/design/specs/tasks reflect Spike framing (no more ADR-0050 §决策修改)
- [x] 1.3 Create `docs/adr/adr-0051-phase6-pdk-composition-spike.md` (🔍 Proposed status)
- [x] 1.4 ADR-0051 §决策 records Spike scope; explicitly NOT Candidate B兑现
- [x] 1.5 Replace all DECLARE_TOOL references with `IToolRegistry::register_tool_function()` pattern
- [x] 1.6 Rename `knowledge_base.query` → `knowledge_base/query` (ADR-0043 slash-only)
- [x] 1.7 G3 ToolCategory fixed to `Execute`; `allowed_layers` = `{Workflow}` only
- [x] 1.8 Contract normalized to `unordered_map<string,string> args → nlohmann::json result` (remove "JSON-in/JSON-out" claim)
- [x] 1.9 Delete `ToolRegistry::call_tool()` instrumentation task; use existing `tool.audit.*` events instead
- [x] 1.10 Re-classify 5 escalation triggers: 2 runtime safety (ToolCoordinator RAII) + 2 plugin health (audit + G3 self-check) + 1 design review (manual)
- [x] 1.11 Run `openspec validate phase6-service-ification-v1 --strict` and confirm exit 0
- [x] 1.12 Run second Metis review and confirm 0 CRITICAL findings

## 2. G3 Knowledge Base Plugin

**BLOCKED until**: §1 12/12 complete AND Stage Gate 2026-07-18 passed AND Sprint 23 capacity confirmed

- [ ] 2.1 Create directory structure `pdk/g3_knowledge_base/` matching `pdk/llama_engine/` pattern (CMakeLists.txt + plugin.cpp + plugin.h)
- [ ] 2.2 Implement G3 plugin entry point with `pdk_register_tools(IToolRegistry&)` using `registry.register_tool_function("knowledge_base/query", meta, lambda)` (NOT DECLARE_TOOL)
- [ ] 2.2.1 **Construct ToolMetadata per ADR-0004 V2 4-param format**: `ToolCategory::Execute` + `allowed_layers={LayerProfile::Workflow}` + `approval_policy=make_approval("agent")` (plan+agent enabled, force_approval_always=false, yolo=false) + `cost_estimate=0.0` (mock LLM = free); reference `pdk/llama_engine/` registration pattern (Sprint 4 PDK skeleton + Sprint 6 C6 upgrade)
- [ ] 2.3 Implement tool handler with hardcoded 3-5 document snippets (in-memory `std::vector<std::string>`)
- [ ] 2.4 Implement internal session store keyed by `session_id` (`std::unordered_map<string, SessionState>`)
- [ ] 2.4.1 **Protect session store with `std::shared_mutex`** (R2 risk mitigation per Oracle 审查): read lock (`std::shared_lock`) on `get()`/`has()` operations, write lock (`std::unique_lock`) on `insert()`/`update()`; ensures ctest parallel execution safety per ADR-0020 logical-not-physical isolation warning
- [ ] 2.5 Implement MockLLMProvider call in tool handler (max 30 lines per spec)
- [ ] 2.6 Implement mandatory error schema `{success: bool, answer: string?, error: string?}` for all return paths (unified per Metis review A1: `answer` NOT `payload` — G1 ReAct loop expects `answer` field)
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
- [ ] 3.2 Implement G1 plugin entry point with `DEFINE_AGENT(CodingAssistant, AgentLoopType::React)` (2-parameter macro, per `include/agenticdsl/pdk/agent_macros.h` actual definition from Sprint 20); construct `DSLEngine` instance with `engine->set_llm_provider(std::make_unique<MockLLMProvider>())` (or equivalent injection pipeline)
- [ ] 3.3 Register exactly 1 tool manifest entry referencing `knowledge_base/query` (discover via `IToolRegistry::has_tool()`)
- [ ] 3.4 Implement 2-step ReAct loop: step 1 invokes G3 tool, step 2 synthesizes final review comment
- [ ] 3.5 Implement mock code input handler (treat code as opaque string, no parsing)
- [ ] 3.6 Implement MockLLMProvider wiring (Sprint 19 mock pattern): G1's DSLEngine MUST receive `MockLLMProvider` via `engine->set_llm_provider()` or constructor injection; G3 MUST use per-test-instance `MockLLMProvider` (NOT shared static instance) to avoid data race (Metis F2/H5: `mock_provider.h:33` declares single-threaded, `generate()` operates lock-free on `history_`/`response_queue_`)
- [ ] 3.7 Verify G1 source uses `DEFINE_AGENT(CodingAssistant, AgentLoopType::React)` syntax (2-parameter, matching actual macro; no new agent loop macro; NO `agent_id=`, `tool_manifest=`, `llm_provider=` pseudo-arguments — those are fabricated in original spec, per Metis F2 code verification)
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

- [ ] 6.1 Implement ToolCoordinator RAII guard: nesting depth > 2 → HARD KILL (in `src/common/tools/tool_coordinator.h/.cpp`, modification allowed per Oracle Q4 + ADR-0051 §决策 5)
- [ ] 6.1.1 Use `thread_local int nesting_depth_` + `thread_local std::vector<std::string> active_call_stack_` for RAII scope tracking; depth++/--/pop in RAII ctor/dtor; cycle detection on push (check `active_call_stack_` for duplicate tool name). **Known limitation (Metis F4)**: thread_local variables are per-jthread-worker (DomainWorkerPool, Sprint 3); cycle detection is limited to same-thread invocations only. Cross-thread cycle (e.g., G1 on Worker A → G3 on Worker B → G1 on Worker A) is NOT detectable by thread_local mechanism. This is an accepted v1 limitation documented in ADR-0051 §不变量.
- [ ] 6.1.2 Emit `cycle_detected_log` audit event payload (call stack trace + caller/callee names + thread_local snapshot) before HARD KILL for forensic analysis
- [ ] 6.2 Implement ToolCoordinator cycle detection (same tool on stack twice) → IMMEDIATE HARD KILL
- [ ] 6.3 G3 plugin self-reports session store size via audit event; trigger if > 1K
- [ ] 6.4 G3 plugin self-reports error-as-success ratio via audit; trigger if > 10%
- [ ] 6.5 ADR-0051 review process: 2+ awkward pattern categories from Layer 1 + Layer 3 dual memos → formalization trigger (event-trigger; lower bar than §13.1 strategic promotion threshold — see §13.7)
- [ ] 6.6 Add unit test for each escalation trigger (5 tests total: depth>2 / cycle / session>1K / error>10% / design review)
- [ ] 6.7 Wire escalation triggers to `tests/test_service_v1.cpp` E2E tests
- [ ] 6.8 Add normal 2-level nesting regression test (R4 false-positive mitigation per Oracle 审查): G1→G3 composition (depth=2, no cycle, no escalation trigger fired) → assert successful return; verifies RAII guard does NOT误杀 legitimate nested calls

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

**BLOCKED until**: §4 complete (per Oracle D1 议程建议 #D-6, 2026-07-16; §9.1-§9.4 与 §2/§3/§4 重叠已在 capacity doc §9 0.5 人天分配中合并入 §2/§3/§4, §9.5 跑 ctest 只需 §4 E2E 通过后即可启动, 无需等 §5-§8)

- [ ] 9.1 Cross-reference: verify G3 test coverage from §2.9-§2.13 is complete (5 tests PASS) — no new test writing needed
- [ ] 9.2 Cross-reference: verify G1 test coverage from §3.9-§3.11 is complete (3 tests PASS) — no new test writing needed
- [ ] 9.3 Cross-reference: verify E2E test coverage from §4.5-§4.7 is complete (3 tests PASS) — no new test writing needed
- [ ] 9.4 Cross-reference: verify contract spec coverage in §4.5-§4.7 covers pdk-service-composition requirements — no new test writing needed
- [ ] 9.5 Run full ctest suite: confirm 72+N/72+N PASS (aggregate regression gate; distinct from §4.8 per-module testing)
- [ ] 9.6 Cross-reference: verify spike-onboarding.md red banner (§8.1 note per Metis H2: Spike code is tension-maximizing MVP, NOT production reference for G2/G4/G5)

## 10. Ship Gate Hard Block Verification

**BLOCKED until**: §2-§9 complete + Stage Gate 2026-07-18 passed + Sprint 23 capacity confirmed

- [ ] 10.1 Verify Stage Gate 2026-07-18 re-evaluation passed and Spike ship gate conditions met (NOT ADR-0050 amendment — removed per Oracle Q6 Spike framing)
- [ ] 10.2 Verify Sprint 23 commitment: 1.5 eng × 2 weeks committed (Risk V1-R2 closed)
- [ ] 10.3 Verify ctest zero regression (72+N/72+N PASS)
- [ ] 10.4 Verify ASan zero regression (72+N/72+N PASS)
- [ ] 10.5 Verify NO DECLARE_SERVICE macro introduced (grep `include/agenticdsl/` for `DECLARE_SERVICE` returns 0) AND NO `call_tool_json` overload implemented (grep `src/` `pdk/` for `call_tool_json` returns 0 — per Metis F3: 'v2+ may introduce' in spec MUST NOT be implemented in Spike)
- [ ] 10.6 Verify NO new namespace introduced (grep for `agenticdsl::service` returns 0)
- [ ] 10.7 Verify NO existing ADR amended (**Tier 1/2/3 fallback protocol defined inline** per Oracle 审查 P2 修复):
  - **Tier 1 (cosmetic/doc fix)**: 在 ADR implementation notes 内的 cosmetic 修正 (typo / broken link / example 错误) — 不需新建 ADR,直接修正 ADR 的 implementation notes 段
  - **Tier 2 (ship-block defect, behavioral change)**: Spike ship gate 阻断的 defect 需新增 behavior → **新建 ADR-0052+** 而非 amend 现有 ADR (per proposal.md line 37)
  - **Tier 3 (architectural change)**: 改变 ADR 决策方向或扩展核心架构 → **新建 ADR-0052+ + Oracle consultation** 后再 ship
  - **约束**: 0 amendments to existing ADRs (Tier 1 除外); 所有 Tier 2/3 走 ADR-0052+ 路径
- [ ] 10.8 Verify BOTH Layer 3 dual memos committed to `docs/service-composition/observations/`
- [ ] 10.9 Verify all 5 escalation triggers wired and tested (5 unit tests PASS, per §6.6)
- [ ] 10.10 Run `openspec validate phase6-service-ification-v1 --strict` and confirm exit 0 (validates W3 full artifacts; distinct from §1.11 W1-only validation which was completed 2026-07-14)
- [ ] 10.11 ToolCoordinator RAII guard implementation present (nesting depth + cycle detection + thread_local tracking, per §6.1.1-§6.1.2 + Oracle Q4)

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
- [ ] 12.5 **Schedule Oracle round 4 session** (per Oracle 审查 P1 #4 + §13.4 / ADR-0051 §提升标准 #4): estimated **2026-08-04** (Sprint 24 D1, 3 days after Spike archive) — validates internal Spike evidence supports ADR-0050 §启动条件 #5 re-interpretation ("internal Spike evidence supports external agent/tool demand"); required prerequisite for ADR-0052 proposal per §13.5; output = Oracle verdict on whether Spike ship evidence justifies promoting to Candidate B v1

## 13. Spike → Candidate B Promotion Criteria (W4+, post-Spike)

- [ ] 13.1 Evidence threshold: ≥3 awkward patterns from ≥2 different Layer 1 categories observed
- [ ] 13.2 Layer 1 reviewer agreement: ≥2 reviewers independently identify the patterns
- [ ] 13.3 Layer 3 dual memos convergence: primary + reviewer memos agree on ≥1 major awkward pattern
- [ ] 13.4 ADR-0050 §启动条件 #5 re-evaluation: Oracle round 4 confirms internal Spike evidence supports "外部 agent/tool" demand
- [ ] 13.5 If all 13.1-13.4 met: propose ADR-0052 "Phase 6 Candidate B v1" (decides whether to launch Phase 6 with `phase6-service-ification-v2`)
- [ ] 13.6 If not met: ADR-0051 stays ✅ Approved (experimental); strategic direction re-evaluation required (owner: 项目负责人)
- [ ] 13.7 **Threshold tier clarification** (per Oracle 审查 P1 #5): §6.5 vs §13.1 are TWO DIFFERENT thresholds at different governance layers:
  - **§6.5 (event-trigger, lower bar)**: 2+ awkward pattern categories from Layer 1 + Layer 3 dual memos → **DECLARE_SERVICE formalization EVENT** (artifact creation trigger for Phase 6 v2+ design doc)
  - **§13.1 (decision-trigger, higher bar)**: ≥3 awkward patterns from ≥2 different Layer 1 categories → **ADR-0052 PROPOSAL** (governance action trigger for Phase 6 v1 launch)
  - **Both can fire concurrently**: §6.5 fires first (when 2 categories surface) and creates DECLARE_SERVICE design doc; §13.1 fires later (when ≥3 patterns surface across ≥2 categories) and proposes ADR-0052 to launch Phase 6 v1
  - **NOT a bug**: §6.5 (low bar) ⊂ §13.1 (high bar) — meeting §13.1 implies §6.5 already fired; meeting §6.5 does NOT imply §13.1 met
  - **Reference**: ADR-0051 §决策 6 (Escalation Trigger Re-classification) + §提升标准 1 (≥3 patterns × ≥2 categories)