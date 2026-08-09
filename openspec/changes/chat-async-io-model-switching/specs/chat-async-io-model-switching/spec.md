## ADDED Requirements

### Requirement: /model command registers via DECLARE_COMMAND

A new `/model <name>` command SHALL be registered via the existing DECLARE_COMMAND macro (ADR-0070 pattern). The command SHALL parse `name` as `provider/model` or `provider` (model defaults to provider default).

#### Scenario: /model <name> with valid provider
- **WHEN** user types `/model deepseek` in the chat prompt
- **THEN** the command is resolved via command_registry
- **AND THEN** `ChatSession::request_model_switch("deepseek")` is invoked
- **AND THEN** the command returns success message `"Model switched to deepseek (next turn)"`

#### Scenario: /model with empty argument
- **WHEN** user types `/model` (no argument)
- **THEN** the command returns error `"Usage: /model <provider>[/<model>]"`
- **AND THEN** no switch occurs

#### Scenario: /model with invalid provider
- **WHEN** user types `/model invalid_provider`
- **THEN** the command returns error `"Unknown provider: invalid_provider"`
- **AND THEN** `request_model_switch` is NOT called

### Requirement: ChatSession request_model_switch API

The `ChatSession` SHALL expose `void request_model_switch(const std::string& provider_name)` and `std::string next_model() const` methods.

The `request_model_switch` SHALL validate the provider (via provider-dynamic-discovery or mock-mode check), store in `next_model_` atomic, and print confirmation.

#### Scenario: request_model_switch stores in next_model_
- **WHEN** `request_model_switch("deepseek")` is called in mock mode with mock provider
- **THEN** `next_model_` is set to `"deepseek"`
- **AND THEN** `next_model()` returns `"deepseek"`
- **AND THEN** mock mode rejects (returns error) since deepseek not mock

#### Scenario: request_model_switch in mock mode rejects non-mock
- **WHEN** `request_model_switch("openai")` is called with current mode = mock
- **THEN** the function returns false (rejected)
- **AND THEN** `next_model_` is NOT modified
- **AND THEN** a warning is logged

### Requirement: per-turn model switch at chat() entry

The `ChatSession::chat()` method SHALL check `next_model_` at entry. If non-empty and different from current model, the method SHALL swap the active provider via `LLMProviderFactory` before proceeding.

#### Scenario: chat() applies next_model_ at entry
- **WHEN** `chat(input)` is called and `next_model_ = "deepseek"`
- **AND WHEN** current mode is live (not mock)
- **THEN** the active provider is swapped to deepseek
- **AND THEN** `next_model_` is cleared (set to empty)
- **AND THEN** the chat proceeds with deepseek provider

#### Scenario: chat() with empty next_model_ preserves current model
- **WHEN** `chat(input)` is called and `next_model_` is empty
- **THEN** no provider swap occurs
- **AND THEN** chat proceeds with current provider

### Requirement: session JSONL persists next_model_

The `ChatSession::save_to_disk()` SHALL write `next_model_` to session_meta JSONL. The `load_from_disk()` SHALL restore `next_model_` on load.

#### Scenario: save_to_disk writes next_model_
- **WHEN** `save_to_disk()` is called and `next_model_ = "deepseek"`
- **THEN** the session_meta JSONL contains `"next_model": "deepseek"`

#### Scenario: load_from_disk restores next_model_
- **WHEN** session_meta JSONL contains `"next_model": "deepseek"`
- **AND WHEN** `load_from_disk(session_id)` is called
- **THEN** `next_model_` is set to `"deepseek"`
- **AND THEN** the next chat() will switch to deepseek

### Requirement: mock mode hard-rejects non-mock provider

When the demo is running in `--mock` mode, `request_model_switch()` SHALL reject any non-mock provider name. The rejection SHALL be logged but NOT throw an exception.

#### Scenario: mock mode rejects openai
- **WHEN** mode is mock and `request_model_switch("openai")` is called
- **THEN** the function returns false
- **AND THEN** `next_model_` remains unchanged
- **AND THEN** stderr logs `[chat] mock mode rejects provider: openai`

#### Scenario: mock mode accepts mock provider name
- **WHEN** mode is mock and `request_model_switch("mock")` is called
- **THEN** the function returns true
- **AND THEN** `next_model_` is set to `"mock"`
- **AND THEN** no actual provider swap occurs (already mock)