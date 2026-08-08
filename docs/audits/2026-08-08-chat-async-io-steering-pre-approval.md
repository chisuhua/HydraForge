# chat-async-io-steering Pre-Approval Verification

**Date**: 2026-08-08
**Subject**: Pre-approval feasibility audit for `improvements/chat-async-io-steering.md`
**Verdict**: ⛔ **NOT APPROVABLE as single change** — requires decomposition into 4 sub-changes (Phase 0 + Phase A + Phase B + Phase C)

## Context

`chat-async-io-steering` (Wave 3, P2) proposes adding async I/O, steering queues, turn interruption, and `/model` runtime switching to `pdk_chat_demo`. Before approval, three pre-conditions were verified:

1. Is the `stop_token → loop_agent` chain real or built on air?
2. Should the change be split into sub-phases?
3. Is the pre-existing ToolRegistry SIGSEGV a blocker?

This document records the verified findings and the resulting phase decomposition.

## Finding 1: stop_token chain is NOT wired

### Scope
Traced every `std::stop_token` / `std::stop_source` propagation hop from `ChatSession` through `pdk/loop_agent`, `NodeExecutor`, `ToolCoordinator`, and the agent-loop APIs.

### Result
The chain is **absent**. Only the lower-level primitives (`ILLMProvider`, `run_stream_to_bus`, `DomainWorkerPool` jthread tokens) are real and tested. The chat-execution path explicitly constructs `std::stop_token{}` at every layer, discarding any hypothetical caller token.

### Identified breaks

| # | Hop | Location | Verdict |
|---|---|---|---|
| 1 | ChatSession has no cancellation state | `chat_session.h:95-107, 137-141` | ❌ Missing |
| 2 | `loop/run` arguments carry no cancel handle | `chat_session.cpp:250-261` | ❌ Missing |
| 3 | loop-agent entry parses no token | `pdk/loop_agent/src/pdk_entry.cpp:170-190` | ❌ Missing |
| 4 | Provider bridge discards caller token | `pdk_entry.cpp:229` (forces `std::stop_token{}`) | ❌ Explicit discard |
| 5 | React/Plan/ForkJoin APIs lack `std::stop_token` param | `react_loop.h:80`, `plan_execute_loop.h:198-256`, `fork_join_loop.h:138-139` | ❌ Missing |
| 6 | `NodeExecutor::dispatch_to_tool` has no token param | `node_executor.cpp:349-351` | ❌ Missing |
| 7 | `ToolCoordinator::execute` has no token param or check | `tool_coordinator.cpp:195-203` | ❌ Missing |
| 8 | SIGINT/SIGTERM calls `std::exit(0)`, not `request_stop()` | `main.cpp:71-79` | ❌ Missing |

### Test coverage gap
No existing test cancels a real `ChatSession::chat()` or `loop/run` mid-flight. Cancellation tests exist only at:
- `tests/test_stream_to_bus.cpp:93-108` (provider-level)
- `tests/test_llm_streaming.cpp:127-158` (stream-level)
- `tests/test_orchestration_dual_consumer.cpp:96-112` (provider forwarding)

### Minimum delta
To make propagation real, 7 steps are required (see Phase B below).

## Finding 2: ToolRegistry SIGSEGV root cause

### Bug signature
- Trigger: `pdk_chat_demo --mock` startup → DSL YAML validation fails → early return → `StartupCleanupGuard` runs.
- Historical fix (commit `c7a95d7`) addressed normal path and YAML early-return path.
- Remaining unsafe path: **`signal_handler` directly calls `unload_all_plugins(*g_loader)` then `std::exit(0)`**, skipping engine/registry destruction.

### Root cause (highest likelihood)
The `std::function` callbacks in `ToolRegistry::tools_` retain plugin `.so` code pointers. When `dlclose()` fires before registry destruction, the implicit destructor of `tools_` executes target destructors inside the unloaded image → SIGSEGV.

The member order in `engine.h:199-205` declares `plugin_loader_` before `tool_registry_` to ensure reverse-destruction order, but the signal handler bypasses this guarantee.

### Minimum-delta fix (Candidate 1)
Change `signal_handler` at `examples/pdk_chat_demo/main.cpp:71-79`:
- Replace direct `unload_all_plugins()` with `g_shutdown_requested.store(true)`.
- Main loop detects atomic flag and runs the normal ordered cleanup path (`engine.reset()` → `unload_all_plugins(loader)`).

## Finding 3: Recommended decomposition into 4 phases

The original proposal cannot ship as one change because Phase B requires the SIGSEGV fix (Phase 0) and Phase A infrastructure. Phase C depends on Phase B's stop_token plumbing.

### Phase 0: SIGSEGV pre-fix (P0, BLOCKING)

| Item | Detail |
|---|---|
| Files | `examples/pdk_chat_demo/main.cpp:71-79` |
| Change | signal handler sets atomic flag only; main thread runs ordered cleanup |
| Tests | subprocess YAML validation regression; SIGINT under ASan |
| Effort | 1-2 days |
| Dependency | None (independent) |
| Improvement file | `improvements/fix-tool-registry-signal-handler-shutdown.md` (to create) |

### Phase A: queue infrastructure (P1, independent)

| Item | Detail |
|---|---|
| Files | `examples/pdk_chat_demo/chat_session.{h,cpp}` |
| Change | add `steering_queue_` + `follow_up_queue_` (bounded, dual-mutex); separate input thread |
| Dependency | None (no stop_token yet) |
| Tests | sync mock path validates queue behavior; E2E enqueue/dequeue |
| Effort | 3-4 days |
| Improvement file | `improvements/chat-async-io-queue-infra.md` (to create) |

### Phase B: stop_token chain wiring (P0, 7-step delta)

| Step | Detail |
|---|---|
| 1 | ChatSession owns `std::stop_source`; `chat()` accepts `std::stop_token` |
| 2 | Add `cancellation_id` to `loop/run` args (token is not serializable) |
| 3 | Create cancellation registry (handle → shared `stop_source`/`stop_token`) |
| 4 | loop-agent parses `cancellation_id`; Provider bridge forwards token |
| 5 | ReactLoop / PlanExecuteLoop / ForkJoinLoop APIs gain `std::stop_token` param |
| 6 | `NodeExecutor::dispatch_to_tool` and `ToolCoordinator::execute` forward token; YieldNode passes real token to `generate_stream()` |
| 7 | End-to-end mid-loop cancel test (blocking mock provider + `request_stop()`) |
| Dependency | Phase 0 must be shipped |
| Effort | 1.5-2 weeks |
| Improvement file | `improvements/chat-async-io-cancellation-chain.md` (to create) |

### Phase C: `/model` runtime switching (P2)

| Item | Detail |
|---|---|
| Files | ChatSession + command layer + provider_agent coordination |
| Change | next-turn model switch without forcing current-turn cancel |
| Dependency | Phase B (needs stop_token); provider-dynamic-discovery (✅ archived 2026-08-06) |
| Tests | `/model <name>` swap; concurrent turn completion semantics |
| Effort | 1 week |
| Improvement file | (sub-section of `chat-async-io-steering.md` or split out) |

### Execution order

```
Phase 0 (SIGSEGV fix, 1-2d) ──────── can run alongside Phase A
   ↓                                   ↓
   └─────────→ Phase A (queue infra, 3-4d)
                  ↓
            Phase B (stop_token chain, 1.5-2 weeks)
                  ↓
            Phase C (/model switching, 1 week)
```

## Recommendations

1. **Decompose `improvements/chat-async-io-steering.md`** into 4 separate improvement files (one per phase) to enable independent tracking and approval.
2. **Create Phase 0 OpenSpec change** immediately — SIGSEGV blocks Phase B and any steering E2E test.
3. **Phase A can start in parallel** with Phase 0 (no shared code paths).
4. **Phase B + C should wait** for Phase 0 + A to ship before proposing.

## Verification artifacts

This synthesis was produced from two parallel background investigations:

- **bg_d5409f67** — stop_token chain trace: cited 15+ file:line locations across `chat_session.{h,cpp}`, `pdk_entry.cpp`, agent-loop headers, `node_executor.cpp`, `tool_coordinator.cpp`, `orchestration_illm_provider.cpp`, `stream_to_bus.cpp`, `domain_worker_pool.cpp`, plus 5 test files.
- **bg_20f7d991** — ToolRegistry SIGSEGV root cause: cited 12 file:line locations across `registry.{h,cpp}`, `main.cpp`, `engine.h`, `plugin_loader.cpp`, `pdk_entry.cpp` (provider_agent + llama_engine), and historical commit `c7a95d7`.

Both reports are reproducible from the cited file:line references.