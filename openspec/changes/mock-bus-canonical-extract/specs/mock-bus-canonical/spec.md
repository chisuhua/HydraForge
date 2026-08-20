# mock-bus-canonical Specification

## ADDED Requirements

### Requirement: Canonical MockBus Fixture

A canonical `MockBus` test fixture MUST be extracted into `tests/test_helpers/mock_bus.h`, replacing 9 duplicated MockBus implementations across the test suite.

#### Scenario: Canonical MockBus supports sync and async subscribe

- GIVEN the canonical MockBus fixture is created
- WHEN `subscribe("llm.request", handler)` is called
- THEN the handler is triggered synchronously on emit
- AND `subscribe_async("tool.*", handler)` queues the handler on a background thread

#### Scenario: 9 existing MockBus implementations are migrated

- GIVEN the canonical MockBus fixture exists
- WHEN all 9 test files reference it
- THEN local MockBus classes are deleted
- AND type references are adjusted (`MockBus` -> `test::MockBus`)
- AND assertion semantics are preserved (per the assertion adjustment whitelist)

#### Scenario: helper APIs are available

- GIVEN the canonical MockBus is instantiated
- WHEN events are emitted
- THEN `count(topic)` returns the event count for that topic
- AND `last(topic)` returns the last event for that topic
- AND `clear()` resets the event vector without unsubscribing handlers