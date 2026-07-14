## ADDED Requirements

### Requirement: PDK Service Composition v1 Contract
The system MUST provide an in-process service composition contract for PDK-developed Agents that allows Agent A to invoke Agent B as a discoverable, named endpoint. The v1 contract MUST register service endpoints via `IToolRegistry::register_tool_function()` (matching all existing PDK plugins including `inference/engine/init`) and MUST NOT introduce a new `DECLARE_SERVICE` macro.

#### Scenario: Agent A invokes Agent B via registered tool
- **WHEN** Agent A (e.g., G1 Coding Assistant) calls `call_tool("knowledge_base/query", {{"question", "..."}, {"session_id", "..."}})` via existing `IToolRegistry`
- **THEN** Agent B (G3 Knowledge Base) handler executes and returns an `nlohmann::json` result containing `{success: bool, answer: string?, error: string?}`

#### Scenario: Service endpoint is registered at plugin load time
- **WHEN** a PDK plugin (e.g., G3) is loaded via `PluginLoader::load()`
- **THEN** its service endpoints (e.g., `knowledge_base/query`) MUST be discoverable via `ToolRegistry::has_tool()` lookup

### Requirement: Transport-Agnostic Service Contract
The system MUST enforce a transport-agnostic service contract where every service endpoint signature uses value semantics only. Service signatures MUST NOT expose `&` references, `shared_ptr<>`, or raw pointers in args or return types, to preserve future IPC transport migration seam.

#### Scenario: Service endpoint uses string args per current IToolRegistry contract
- **WHEN** reviewing a service endpoint (e.g., `knowledge_base/query`) source code
- **THEN** the handler function signature MUST accept `std::unordered_map<std::string, std::string> args` (matching current `IToolRegistry` contract); complex values MUST be JSON-encoded into string values; return type MUST be `nlohmann::json` (value type)
- **AND** v2+ may introduce `call_tool_json()` overload per Oracle Q5 decision to support native JSON args without string encoding

#### Scenario: Service endpoint does not leak in-process affordances
- **WHEN** reviewing a service endpoint (e.g., `knowledge_base/query`) source code
- **THEN** the implementation MUST NOT accept `IInteractionBus&`, `Context&`, `ExecutionSession&`, or other in-process references in its public signature

### Requirement: Logical Isolation, Not Physical
The system MUST explicitly declare that PDK Service Composition v1 provides **logical** isolation (per-agent thread + session, per ADR-0020 + ADR-0033) and NOT **physical** isolation (process-level). A crash in Agent B MUST be documented as capable of terminating Agent A and the entire process.

#### Scenario: Documentation states logical-not-physical isolation
- **WHEN** reading the Spike contract documentation (ADR-0051 §不变量 + `docs/service-composition/spike-onboarding.md`)
- **THEN** the documentation MUST contain an explicit warning that "v1 isolation is logical, not physical; single Agent uncaught exception may terminate entire process"

#### Scenario: Single Agent crash is detected and reported
- **WHEN** Agent B throws an uncaught exception during a service call from Agent A
- **THEN** the process MUST terminate (logical isolation proven insufficient) and the test/instrumentation MUST record this for escalation

### Requirement: Awkward Pattern Detection Methodology
The system MUST provide a 3-layer awkward pattern detection methodology that surfaces emergent patterns from G1+G3 v1 demo. Each layer MUST be operational before the demo completes.

#### Scenario: Layer 1 static code review checklist
- **WHEN** an engineer reviews any code that bridges Agent semantics to tool interface (e.g., G3's tool handler)
- **THEN** the engineer MUST run the 5-item Layer 1 checklist and record findings: (1) stateful tool, (2) nested agent behind tool, (3) context threading via args, (4) error flattening, (5) sync-async impedance

#### Scenario: Layer 2 runtime instrumentation via existing audit events
- **WHEN** `call_tool` is invoked
- **THEN** the instrumentation MUST use existing `tool.audit.{invoked,completed,denied}` events (C4 / ADR-0031) and log 5 fields per service invocation: `caller_session_id` (from audit payload), `callee_tool_name` (from audit event topic), `args_keys_only` (audit MUST redact values; defense-in-depth per C4), `return_latency_ms` (audit timestamp diff between `invoked` and `completed`), `callee_internally_invoked_llm` (G3 plugin MUST self-report boolean via audit metadata field; NOT monitored by ToolCoordinator)
- **AND** MUST NOT introduce new ToolRegistry instrumentation; reuse existing C4 audit event bus

#### Scenario: Layer 3 post-demo observation memo
- **WHEN** the G1+G3 v1 demo completes
- **THEN** the primary engineer AND the reviewer MUST each independently write a 1-page "what felt wrong" memo capturing tacit awkward patterns not captured by Layers 1 and 2

### Requirement: Escalation Trigger Monitoring
The system MUST implement 5 escalation triggers that surface when v1 contract is insufficient. Triggers are reclassified into 3 categories: runtime safety (hard kill), plugin health (logged), and design review (manual).

#### Scenario: Trigger #1 (Runtime safety) — ToolCoordinator RAII guard nesting depth
- **WHEN** G1 calls G3 (1 ToolCoordinator invocation) AND G3 internally calls another tool requiring approval (2nd ToolCoordinator invocation)
- **THEN** ToolCoordinator RAII guard MUST detect `tool_coordinator_invocation_depth > 2` AND escalate to HARD KILL (process terminate)

#### Scenario: Trigger #2 (Runtime safety) — Call stack cycle detection
- **WHEN** same tool name appears on call stack twice (e.g., G1 calls G3 AND G3 calls G1, detected by comparing tool name against active invocation stack)
- **THEN** the system MUST trigger IMMEDIATE HARD KILL and surface the cycle as a critical finding

#### Scenario: Trigger #3 (Plugin health) — Error-as-success ratio exceeds threshold
- **WHEN** G3's tool handler returns error responses counted from `tool.audit.completed` events within a session AND error count ÷ total count > 10%
- **THEN** the instrumentation MUST log escalation trigger #3 (Error Flattening Silent Defect) via audit log but MUST NOT hard kill

#### Scenario: Trigger #4 (Plugin health) — Session store growth exceeds bound
- **WHEN** G3's internal session store accumulates more than 1000 unique `session_id` entries (counted by G3 self-check, reported via audit event metadata)
- **THEN** the instrumentation MUST log escalation trigger #4 (session store unbounded growth) via audit log but MUST NOT hard kill

#### Scenario: Trigger #5 (Design review) — Awkward pattern category diversity
- **WHEN** 2+ awkward patterns from DIFFERENT Layer 1 categories are observed (e.g., #1 + #5, NOT #1 + #1) AND mirrored by Layer 3 dual "what felt wrong" memos
- **THEN** the system MUST trigger ADR-0051 design review (manual; requires DECLARE_SERVICE formalization exploration as v2 candidate)

### Requirement: Mandatory Error Schema for Service Responses
The system MUST enforce that all service endpoint responses use the schema `{success: bool, error: string?, payload: object?}` to prevent error flattening silent defects. Empty or undefined error paths MUST be explicit.

#### Scenario: Service response includes explicit success flag
- **WHEN** G3 returns a result to G1
- **THEN** the response MUST contain `success` field as boolean (never implicit or absent)

#### Scenario: Service response distinguishes success from error
- **WHEN** G3's internal MockLLMProvider call fails
- **THEN** G3 MUST return `{success: false, error: "<reason>"}` and MUST NOT return `{success: true, error: "<reason>"}` (which would mask error as success)

### Requirement: v1 Onboarding Documentation for Future Teams
The system MUST provide onboarding documentation at `docs/service-composition/spike-onboarding.md` that allows G2/G4/G5 teams to self-assess whether their Agent fits the Spike contract or needs DECLARE_SERVICE formalization (v2). The documentation MUST be 2-3 pages and readable in 15 minutes.

#### Scenario: Onboarding doc contains v1 contract summary
- **WHEN** a G2/G4/G5 team engineer reads `docs/service-composition/spike-onboarding.md`
- **THEN** the doc MUST contain: (1) what v1 IS (in-process / string args→JSON result / register_tool_function-based / no new macros), (2) what v1 IS NOT (not networked / not async / not streaming / not multi-tenant), (3) v1 contract normative spec (~1 page), (4) "Does your Agent fit v1?" decision tree, (5) trigger thresholds for pushing DECLARE_SERVICE

#### Scenario: Decision tree guides Agent fit self-assessment
- **WHEN** a G2/G4/G5 team engineer answers the 4-question decision tree in onboarding doc
- **THEN** the engineer MUST be able to determine: (a) clean fit, (b) awkward pattern with flag, (c) does not fit, push for v2 — without consulting platform engineer

### Requirement: Ship Gate Hard Blocks (W1 Remediation)
The system MUST enforce 5 W1 remediation ship gate hard blocks before ADR-0051 status flips from 🔍 Proposed to ✅ Approved. These are W1-corrective gates (replacing pre-Oracle gates); W2-W3 implementation gates are separate. Any unmet ship gate MUST abort the change archive process.

#### Scenario: Ship gate requires all W1 critical blockers resolved
- **WHEN** the OpenSpec change `phase6-service-ification-v1` is being archived
- **THEN** the archive script MUST verify that all W1 critical blockers are resolved: proposal.md design.md specs/ tasks.md and ADR-0051 all corrected per Oracle Q1-Q5 decisions

#### Scenario: Ship gate requires ADR-0051 created with Proposed status
- **WHEN** the OpenSpec change `phase6-service-ification-v1` is being archived
- **THEN** the archive script MUST verify that ADR-0051 exists at `docs/adr/adr-0051-phase6-pdk-composition-spike.md` with status 🔍 Proposed

#### Scenario: Ship gate requires openspec validate exit 0
- **WHEN** the OpenSpec change `phase6-service-ification-v1` is being archived
- **THEN** the archive script MUST verify `openspec validate phase6-service-ification-v1 --strict` exit 0

#### Scenario: Ship gate requires second Metis review with zero CRITICAL
- **WHEN** the OpenSpec change `phase6-service-ification-v1` is being archived
- **THEN** the archive script MUST verify that a second Metis review has completed with 0 CRITICAL findings

#### Scenario: Ship gate requires Stage Gate passed + Sprint 23 capacity confirmed
- **WHEN** the OpenSpec change `phase6-service-ification-v1` is being archived
- **THEN** the archive script MUST verify Stage Gate 2026-07-18 evaluation has passed AND Sprint 23 capacity is confirmed (BEFORE W2-W3 implementation begins)

### Requirement: Kill Criteria for v1 Demo
The system MUST enforce 4 kill criteria that abort v1 demo when triggered, to prevent sunk cost escalation. Each criterion MUST be testable.

#### Scenario: HARD KILL on crash propagation
- **WHEN** G3's uncaught exception propagates to G1's process termination during service call
- **THEN** ToolCoordinator RAII guard MUST log crash + cycle info via audit event; v1 demo MUST escalate findings to ADR-0050 (Candidate B may be wrong)

#### Scenario: HARD KILL on W2 D10 zero E2E call
- **WHEN** W2 D10 (end of week 2) has elapsed with zero successful G1-to-G3 end-to-end calls
- **THEN** the demo MUST HARD KILL (premise is broken)

#### Scenario: SOFT KILL on stuck awkward patterns
- **WHEN** 2+ awkward patterns from different Layer 1 categories are observed AND no DECLARE_SERVICE direction emerges within 2 days of Oracle round 3 consultation
- **THEN** the demo MUST upgrade to HARD KILL

#### Scenario: DRIFT KILL on W3 end
- **WHEN** W3 ends without convergence
- **THEN** the demo MUST DRIFT KILL with "what we learned" doc; MUST NOT extend to W4+