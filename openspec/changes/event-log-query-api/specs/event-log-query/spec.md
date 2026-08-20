# event-log-query Specification

## ADDED Requirements

### Requirement: EventLog Query API

The EventLogWriter MUST support query operations: `read()` by causal time window and `query()` by predicate filter.

#### Scenario: read by causal time window

- GIVEN EventLog is written with 10000 events (various topics/agent_ids)
- WHEN `read(agent_id="agent-abc", start_causal_time=0, end_causal_time=UINT64_MAX)` is called
- THEN all events for agent-abc are returned sorted by causal_time
- AND the existing `static read(agent_id, log_dir)` signature is unchanged

#### Scenario: query by predicate filter

- GIVEN EventLog is written with 10000 events
- WHEN `query(filter=[topic="llm.request", agent_id="agent-abc"], max_count=1000)` is called
- THEN up to 1000 events are returned, sorted by causal_time, in < 100ms

#### Scenario: format-damaged file handling

- GIVEN the EventLog JSONL file has a partial/corrupted last line
- WHEN `read(...)` is called
- THEN events before the corrupted line are returned
- AND a warning is logged about the corruption

#### Scenario: C++20 compatibility

- GIVEN the project uses C++20 (CMakeLists.txt:13 CMAKE_CXX_STANDARD 20)
- WHEN the query API is compiled
- THEN `std::expected` (C++23) is NOT used
- AND the return type is `std::vector<BusEvent>` + `std::optional<ErrorCode>` error reporting