## ADDED Requirements

### Requirement: G3 Knowledge Base Plugin MVP Scope
The system MUST provide a G3 Knowledge Base PDK plugin at `pdk/g3_knowledge_base/` that exposes a single service endpoint `knowledge_base/query`, accepts `{question: string, session_id: string}` as args, maintains internal session state, and returns `{success: bool, answer: string?, error: string?}` as response. The plugin MUST use `IToolRegistry::register_tool_function()` (matching all existing PDK plugins including `inference/engine/init`) and `MockLLMProvider` (Sprint 19). ToolCategory MUST be `ToolCategory::Execute` with `allowed_layers={LayerProfile::Workflow}` only (ReadOnly per ADR-0004 V2 = filesystem read-only, does not cover LLM generation + session mutation).

#### Scenario: G3 plugin exposes knowledge_base/query endpoint
- **WHEN** the G3 plugin is loaded via `PluginLoader::load()`
- **THEN** the plugin MUST register exactly 1 tool via `pdk_register_tools(IToolRegistry&)` entry point (matching `pdk/llama_engine/` and `pdk/model_router/` patterns), calling `register_tool_function("knowledge_base/query", metadata, handler)` with `ToolCategory::Execute`

#### Scenario: G3 plugin accepts question and session_id args
- **WHEN** G3's tool handler is invoked with `std::unordered_map<std::string, std::string>{{"question", "<text>"}, {"session_id", "<id>"}}` (per v1 IToolRegistry contract; complex values JSON-encoded into string values)
- **THEN** the handler MUST parse both args as required (return error if either missing) and proceed to retrieval + LLM call

#### Scenario: G3 plugin returns mandatory error schema
- **WHEN** G3's tool handler returns
- **THEN** the return value MUST be an `nlohmann::json` object containing `{success: bool, answer: string?, error: string?}`; `success` field is mandatory boolean; either `answer` (if success) or `error` (if failure) MUST be present; implicit or absent error path is forbidden

### Requirement: G3 Plugin Multi-Turn Session Support
The G3 plugin MUST maintain an internal session store keyed by `session_id`, allowing multi-turn conversations where prior Q/A context is included in subsequent LLM calls. The session store MUST demonstrate session isolation (different `session_id`s have independent contexts).

#### Scenario: G3 plugin preserves prior Q/A in multi-turn
- **WHEN** G3 receives two calls with the same `session_id` and different `question` values
- **THEN** the second call MUST include the first call's Q/A in its LLM context (verified by MockLLMProvider receiving prior context in its args)

#### Scenario: G3 plugin isolates different session_ids
- **WHEN** G3 receives calls with `session_id="A"` and `session_id="B"` (different IDs)
- **THEN** session A's context MUST NOT leak into session B's LLM calls (verified by MockLLMProvider receiving empty prior context for B's first call)

### Requirement: G3 Plugin Hardcoded Retrieval
The G3 plugin MUST use hardcoded document snippets (3-5 entries) as the retrieval source, and MUST NOT require a real vector database, real embedding model, or real document index. This bounds the MVP scope to surface composition patterns without retrieval infrastructure.

#### Scenario: G3 plugin retrieval uses hardcoded snippets
- **WHEN** G3's tool handler executes the retrieval step
- **THEN** the retrieval MUST return from a hardcoded array of 3-5 document snippets (in-memory `std::vector<std::string>` or similar), and MUST NOT call any external database or embedding model

### Requirement: G3 Plugin Internal Agent Loop Bounded
The G3 plugin MUST internally call `MockLLMProvider` to generate answers (per its multi-turn semantics), but MUST NOT internally call other tools that require approval via `ToolCoordinator` (defect #5 prevention). ToolCategory is `ToolCategory::Execute` (NOT `ReadOnly`; ADR-0004 V2 ReadOnly = filesystem read-only, does not cover LLM generation + session mutation).

#### Scenario: G3 plugin does not call approval-required tools internally
- **WHEN** reviewing G3 plugin source code
- **THEN** G3's internal logic MUST only call `MockLLMProvider::generate()` and MUST NOT call any tool that triggers `ToolCoordinator` approval (verified by code review + runtime instrumentation showing `tool_coordinator_invocation_depth == 0` for G3-internal calls)

### Requirement: G3 Plugin Tool Handler Size Constraint
The G3 plugin's `knowledge_base/query` tool handler function MUST be ≤30 lines (excluding comments and error schema construction). This bounds the "awkward adapter" boilerplate size and makes Layer 1 pattern #1 (stateful tool) visible if it exceeds the threshold.

#### Scenario: G3 plugin tool handler is at most 30 lines
- **WHEN** `wc -l` is run on the G3 tool handler function body (excluding comments)
- **THEN** the line count MUST be ≤30 (verified by CI script in `tests/test_service_v1.cpp`)

### Requirement: G3 Plugin Directory Layout Matches pdk/llama_engine Pattern
The G3 plugin MUST be located at `pdk/g3_knowledge_base/` following the directory pattern established by `pdk/llama_engine/` (C14 ship) and `pdk/model_router/` (C7 ship).

#### Scenario: G3 plugin source files located at pdk/g3_knowledge_base/
- **WHEN** the G3 plugin source is reviewed
- **THEN** the entry point file MUST be at `pdk/g3_knowledge_base/` (matching `pdk/llama_engine/plugin.cpp` or similar pattern)