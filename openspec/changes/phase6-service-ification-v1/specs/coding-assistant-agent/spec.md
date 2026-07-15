## ADDED Requirements

### Requirement: G1 Coding Assistant Plugin MVP Scope
The system MUST provide a G1 Coding Assistant PDK plugin at `pdk/g1_coding_assistant/` that implements a 2-step ReAct loop, discovers exactly 1 tool (`knowledge_base/query`), accepts a mock code string as input, and synthesizes a final review comment. The plugin MUST use the `DEFINE_AGENT(name, loop_type)` macro (Sprint 20, 2-parameter signature: `DEFINE_AGENT(CodingAssistant, AgentLoopType::React)`) and construct its `DSLEngine` with `MockLLMProvider` (Sprint 19). G1 registers no tools of its own; the single tool manifest entry comes from discovering `knowledge_base/query` via `ToolRegistry::has_tool()`. The `DSLEngine` is constructed by the plugin's `pdk_init()` entry point with `engine->set_llm_provider(std::make_unique<MockLLMProvider>())` (or equivalent injection).

#### Scenario: G1 plugin registered via DEFINE_AGENT macro
- **WHEN** the G1 plugin module is loaded via `PluginLoader::load()`
- **THEN** the plugin MUST register an Agent instance using `DEFINE_AGENT(CodingAssistant, AgentLoopType::React)` and construct a `DSLEngine` with `MockLLMProvider` injected via `engine->set_llm_provider(std::make_unique<MockLLMProvider>())`

#### Scenario: G1 plugin accepts mock code input
- **WHEN** a caller invokes G1 with `{"request": "审查这段代码", "code": "<mock code string>"}`
- **THEN** the plugin MUST treat `code` as opaque mock string (no real code parsing) and proceed to ReAct loop

#### Scenario: G1 plugin runs 2-step ReAct loop
- **WHEN** G1's ReAct loop executes
- **THEN** the loop MUST execute exactly 2 steps: (1) invoke `knowledge_base/query` tool with question derived from `request`, (2) synthesize final review comment using G3's response plus `code` content

#### Scenario: G1 plugin returns synthesized review comment
- **WHEN** the 2-step ReAct loop completes
- **THEN** G1 MUST return a `ToolResult` containing `{success: true, answer: "<synthesized review comment>"}` (synthesis is trivial but completes the loop)

### Requirement: G1 Plugin Single-Tool Constraint
The G1 plugin MUST discover exactly 1 tool (`knowledge_base/query`) and MUST NOT register additional tools in the v1 MVP scope, to surface the single-caller single-callee composition pattern without multi-tool noise.

#### Scenario: G1 plugin tool manifest size is 1
- **WHEN** the G1 plugin's tool manifest is inspected at runtime
- **THEN** the manifest MUST contain exactly 1 entry (`knowledge_base/query`), verified by unit test `tests/test_service_v1.cpp`

### Requirement: G1 Plugin Reuses Sprint 20 DEFINE_AGENT
The G1 plugin MUST use the existing `DEFINE_AGENT(name, loop_type)` macro (Sprint 20, 2-parameter form) and MUST NOT introduce new agent loop macros. This enforces the "no new macros in v1" constraint. The `DSLEngine` instance must be constructed by the plugin's `pdk_init()` (or equivalent entry point) with `MockLLMProvider` set via `engine->set_llm_provider(...)` before the agent loop starts.

#### Scenario: G1 plugin source uses DEFINE_AGENT(React)
- **WHEN** reviewing the G1 plugin source code
- **THEN** the plugin MUST use `DEFINE_AGENT(CodingAssistant, AgentLoopType::React)` (2-parameter syntax, matching `include/agenticdsl/pdk/agent_macros.h` actual macro definition) and MUST NOT define a new agent loop macro

### Requirement: G1 Plugin MockLLMProvider Wiring
The G1 plugin MUST use `MockLLMProvider` (Sprint 19) for all LLM calls within the ReAct loop, to avoid requiring a real LLM runtime for the v1 demo.

#### Scenario: G1 plugin uses MockLLMProvider
- **WHEN** the G1 plugin constructs its DSLEngine or agent loop
- **THEN** the LLM provider MUST be `MockLLMProvider` (or equivalent mock), not `LlamaAdapter`

### Requirement: G1 Plugin Directory Layout Matches pdk/llama_engine Pattern
The G1 plugin MUST be located at `pdk/g1_coding_assistant/` following the directory pattern established by `pdk/llama_engine/` (C14 ship) and `pdk/model_router/` (C7 ship).

#### Scenario: G1 plugin source files located at pdk/g1_coding_assistant/
- **WHEN** the G1 plugin source is reviewed
- **THEN** the entry point file MUST be at `pdk/g1_coding_assistant/` (matching `pdk/llama_engine/plugin.cpp` or similar pattern)