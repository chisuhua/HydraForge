## ADDED Requirements

### Requirement: Signal Handler Delegates to Main Thread Cleanup

The `pdk_chat_demo` binary SHALL route SIGINT and SIGTERM signals to the main thread for ordered cleanup rather than performing cleanup directly from the signal handler context.

The signal handler SHALL set a single `std::atomic<bool>` flag (`g_shutdown_requested`) to `true` and SHALL NOT call `unload_all_plugins()`, `std::exit()`, or any other non-async-signal-safe operation.

The main interactive loop SHALL observe the flag after each blocking operation and SHALL exit the loop with a non-error code when the flag is set.

#### Scenario: SIGINT during interactive loop
- **WHEN** user presses Ctrl+C while the demo is waiting at `User>` prompt or executing a chat turn
- **THEN** signal handler sets `g_shutdown_requested = true` and returns immediately
- **AND THEN** main loop observes the flag within one iteration
- **AND THEN** main loop exits with code 0 (graceful shutdown)
- **AND THEN** `DSLEngine` is destroyed before `PluginLoader::unload_all_plugins()` runs
- **AND THEN** process exits without SIGSEGV

#### Scenario: SIGTERM during plugin load
- **WHEN** SIGTERM arrives between plugin registration and `ChatSession` construction
- **THEN** signal handler sets the flag
- **AND THEN** main loop exits the load phase
- **AND THEN** `StartupCleanupGuard::~StartupCleanupGuard()` runs ordered destruction
- **AND THEN** process exits cleanly with code 0 or appropriate error code

#### Scenario: Multiple SIGINT signals
- **WHEN** user presses Ctrl+C twice in rapid succession
- **THEN** signal handler remains idempotent (flag is already set)
- **AND THEN** no UB from non-signal-safe operations
- **AND THEN** process exits within bounded time

### Requirement: Atomic Shutdown Flag is Async-Signal-Safe

The `g_shutdown_requested` flag SHALL be declared as `std::atomic<bool>` with a global lifetime that exceeds all signal handler invocations.

The flag SHALL be initialized to `false` before `std::signal()` is called.

The flag SHALL be the only state mutated by the signal handler.

#### Scenario: Signal arrives during startup before flag init
- **WHEN** SIGINT arrives during early startup (before `g_shutdown_requested` is constructed)
- **THEN** signal handler returns without action (handler is registered after init)
- **AND THEN** demo continues startup as normal

#### Scenario: Signal handler atomic store
- **WHEN** signal handler executes
- **THEN** only `g_shutdown_requested.store(true, std::memory_order_release)` runs
- **AND THEN** no heap allocation, no I/O, no library calls

### Requirement: Main Loop Observes Shutdown Flag

The main interactive loop in `examples/pdk_chat_demo/main.cpp` SHALL check `g_shutdown_requested.load(std::memory_order_acquire)` after every blocking call (`std::getline`, `session.chat`).

When the flag is observed as `true`, the loop SHALL break immediately and proceed to the normal shutdown sequence.

#### Scenario: Flag set during blocking stdin read
- **WHEN** `std::getline(std::cin, input)` is blocked waiting for user input
- **AND WHEN** SIGINT sets the flag
- **THEN** `std::getline` may return false (EOF) or the loop exits after observation
- **AND THEN** demo proceeds to shutdown sequence

#### Scenario: Flag set during chat turn execution
- **WHEN** `session.chat(input)` is running
- **AND WHEN** SIGINT sets the flag
- **THEN** chat turn completes naturally (Phase B will wire cancellation)
- **AND THEN** main loop observes flag after `chat()` returns
- **AND THEN** demo proceeds to shutdown

### Requirement: Destruction Order Preserved on Signal-Initiated Shutdown

When shutdown is initiated by a signal, the destruction sequence SHALL be:

1. `ChatSession` destructor runs (if constructed)
2. `DSLEngine` destructor runs (destroys `ToolRegistry` member)
3. `PluginLoader::unload_all_plugins()` runs (calls `dlclose()` on each plugin)

This order SHALL be enforced by the main-thread shutdown path and SHALL NOT depend on the signal handler.

#### Scenario: Signal before ChatSession construction
- **WHEN** signal arrives before `ChatSession session(...)` at line 395
- **THEN** `StartupCleanupGuard` destructor runs (engine.reset → unload)
- **AND THEN** `tool_registry_` destroyed before `plugin_loader_` (member order guarantee from `engine.h:199-205`)

#### Scenario: Signal after ChatSession construction
- **WHEN** signal arrives after `ChatSession session(...)` at line 395
- **THEN** explicit shutdown block runs: session.save_to_disk(), guard.reset_engine(), unload_all_plugins(loader)
- **AND THEN** destruction order matches Step 1-2-3 above