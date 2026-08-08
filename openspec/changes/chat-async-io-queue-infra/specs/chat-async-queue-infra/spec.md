## ADDED Requirements

### Requirement: ChatSession has separate steering and follow-up queues

The `ChatSession` SHALL maintain two distinct bounded queues for async input coordination:
- `steering_queue_`: messages intended to interrupt and adjust the current agent turn
- `follow_up_queue_`: messages intended to be processed after the current turn completes

Each queue SHALL have a default maximum capacity of 32 entries and SHALL be configurable via the `ChatSession` constructor.

The queues SHALL implement a producer-consumer pattern with separate mutexes for each queue (avoid single-mutex contention).

#### Scenario: Steering command enqueued
- **WHEN** user input is classified as a steering command (e.g., `/model`, `/cancel`)
- **THEN** input is placed into `steering_queue_`
- **AND THEN** `steering_queue_` size increases by 1

#### Scenario: Follow-up message enqueued
- **WHEN** user input is classified as a regular message
- **AND WHEN** an agent turn is currently executing
- **THEN** input is placed into `follow_up_queue_`
- **AND THEN** `follow_up_queue_` size increases by 1

#### Scenario: Queue size is queryable
- **WHEN** `ChatSession::queue_size(QueueKind)` is called
- **THEN** the current number of entries in the specified queue is returned
- **AND THEN** the call is thread-safe (lock-free read or atomic)

### Requirement: Queue overflow handling is documented and deterministic

When a queue is at capacity, the system SHALL reject the new input and SHALL NOT silently drop previously queued entries.

The rejection behavior SHALL be:
- `steering_queue_`: reject new input + log warning to stderr
- `follow_up_queue_`: reject new input + log warning to stderr

The default capacity of 32 MUST be sufficient for typical interactive sessions (assuming ≤ 1 input per second).

#### Scenario: Steering queue overflow
- **WHEN** `steering_queue_` is at capacity (32 entries)
- **AND WHEN** user submits a new steering command
- **THEN** the new command is rejected
- **AND THEN** a warning is logged to stderr including the rejected input (length only, not content, to avoid secret leakage)
- **AND THEN** `steering_queue_` size remains at capacity (no entries dropped)

#### Scenario: Follow-up queue overflow
- **WHEN** `follow_up_queue_` is at capacity (32 entries)
- **AND WHEN** user submits a new follow-up message
- **THEN** the new message is rejected
- **AND THEN** a warning is logged to stderr including the rejected input length (not content)
- **AND THEN** `follow_up_queue_` size remains at capacity

### Requirement: Input thread produces, ChatSession consumes

The `ChatSession` SHALL run a dedicated input thread that reads from stdin and classifies inputs into steering vs follow-up queues.

The input thread SHALL be:
- Started in the `ChatSession` constructor
- Joined in the `ChatSession` destructor (RAII)

Classification rules:
- Lines starting with `/` (command) → `steering_queue_`
- Empty lines → ignored (no enqueue)
- Other lines → `follow_up_queue_`

#### Scenario: Input thread starts on construction
- **WHEN** `ChatSession` is constructed
- **THEN** the input thread is started
- **AND THEN** the input thread is bound to stdin

#### Scenario: Input thread joins on destruction
- **WHEN** `ChatSession` is destroyed
- **THEN** the input thread is signaled to stop
- **AND THEN** the destructor waits for the thread to complete (join)
- **AND THEN** both queues are safely destroyed after thread join

### Requirement: Queue state is observable for testing and debugging

The `ChatSession` SHALL expose:
- `queue_size(QueueKind)`: returns current entry count
- `try_clear_queue(QueueKind)`: atomically empties the queue (returns count cleared)

These APIs MUST be thread-safe and MUST NOT block the caller.

#### Scenario: Queue size query returns current count
- **WHEN** `queue_size(QueueKind::Steering)` is called after 5 enqueues
- **THEN** the function returns 5
- **AND THEN** the call completes in O(1) time

#### Scenario: Clear queue returns count cleared
- **WHEN** `try_clear_queue(QueueKind::FollowUp)` is called with 3 entries
- **THEN** the function returns 3
- **AND THEN** `follow_up_queue_` size becomes 0
- **AND THEN** the call completes in O(1) time

### Requirement: No blocking operations on the agent turn critical path

Queue operations MUST be O(1) and MUST NOT block the agent turn execution.

The producer (input thread) MUST NOT block the consumer (agent turn) for more than the time required to acquire the queue mutex.

#### Scenario: Enqueue operation is non-blocking
- **WHEN** input thread calls `steering_queue_.push(item)` with queue at capacity
- **THEN** the call returns immediately (reject + log)
- **AND THEN** the input thread continues reading from stdin

#### Scenario: Consumer dequeue is non-blocking
- **WHEN** agent turn requests next steering item
- **AND WHEN** `steering_queue_` is empty
- **THEN** the operation returns false (no item) without blocking
- **AND THEN** the agent turn continues without yield