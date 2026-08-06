# cli-flags Specification

## Purpose

定义 `pdk_chat_demo` 启动参数的 vendored parser、集中声明表和行为契约。该 spec 将当前 `main.cpp` 手写的 `--mock` 与 `--session <id>` 扫描迁移到 cxxopts，同时新增 `-p/--print`、`--provider`、`--offline` 的解析入口。解析结果以项目内的 `CliOptions` 值结构体向下传递，确保 provider、session 和 chat loop 不依赖 cxxopts 类型。`--mode json|rpc`、`-c/-r` 和 RPC 行为仍由各自依赖的后续 change 负责。

## ADDED Requirements

### Requirement: cli-flags-vendored

cxxopts MUST be vendored under `external/` at a pinned version with its applicable license or NOTICE text. The `pdk_chat_demo` target MUST consume the repository copy through target-scoped include configuration and MUST NOT require a system cxxopts package, FetchContent download, or network access during configuration or build.

#### Scenario: offline demo build uses repository dependency

- **WHEN** a clean offline build configures and builds the `pdk_chat_demo` target
- **THEN** CMake resolves cxxopts from `external/`
- **AND** configuration does not call `find_package(cxxopts)` or FetchContent for cxxopts
- **AND** compilation succeeds without a system-installed cxxopts package

#### Scenario: vendored dependency metadata is reviewable

- **WHEN** a reviewer inspects the cxxopts directory
- **THEN** the pinned version or source revision is recorded
- **AND** the corresponding license or NOTICE text is present
- **AND** no generated download cache is required to reproduce the build

### Requirement: cli-flags-declarative

The parser MUST define the current CLI contract through a centralized flag declaration table or equivalent single declaration source. The table MUST describe each option's spelling, argument requirement, help text, and destination mapping. Downstream code MUST consume a project-owned `CliOptions` value rather than `cxxopts::ParseResult`.

#### Scenario: adding a flag changes one declaration source

- **WHEN** an implementation adds a future string flag such as `--system-prompt`
- **THEN** its spelling, argument requirement, description, and destination are added to the centralized declaration source
- **AND** generated help includes the new flag
- **AND** downstream startup code receives the validated value through `CliOptions`

#### Scenario: existing declarations have explicit defaults

- **WHEN** the parser is invoked without optional flags
- **THEN** `mock`, `print`, and `offline` use documented false defaults
- **AND** the session id and provider fields use their documented empty or configuration-derived defaults
- **AND** no uninitialized CLI field reaches startup initialization

### Requirement: cli-flags-behavior-equivalence

The migration MUST preserve the existing behavior of `--mock`, `--session <id>`, their combination, and the `--skill-child` early branch. `--skill-child` MUST be recognized before normal CLI parsing and MUST continue to dispatch to `skill_child_main` without DSLEngine initialization.

#### Scenario: mock flag preserves startup mode

- **WHEN** the demo is invoked with `--mock`
- **THEN** the parsed options set mock mode to true
- **AND** the existing mock provider initialization path is selected
- **AND** no real provider network request is made before the input loop

#### Scenario: session flag preserves selected session

- **WHEN** the demo is invoked with `--session demo-session`
- **THEN** the parsed options contain session id `demo-session`
- **AND** the existing session loading path receives that exact id
- **AND** session persistence and message semantics remain unchanged

#### Scenario: combined existing flags remain compatible

- **WHEN** the demo is invoked with `--mock --session demo-session`
- **THEN** mock mode is enabled and the selected session id is `demo-session`
- **AND** the two options do not overwrite each other
- **AND** the demo reaches the same mock session startup path as the pre-migration implementation

#### Scenario: skill child bypasses normal parser

- **WHEN** the executable is invoked with `--skill-child` and its child protocol arguments
- **THEN** `skill_child_main(argc, argv)` is called before normal option parsing
- **AND** DSLEngine and normal chat plugins are not initialized by the parent startup path
- **AND** the child protocol arguments are passed through unchanged

### Requirement: cli-flags-help-auto-gen

The parser MUST generate `--help` output from the centralized declarations. Help MUST list `--mock`, `--session`, `-p/--print`, `--provider`, and `--offline` with their argument forms or boolean semantics and descriptions. Help MUST exit successfully before normal application initialization.

#### Scenario: help lists all current flags

- **WHEN** the executable is invoked with `--help`
- **THEN** it prints generated usage and option descriptions
- **AND** the output contains `--mock`, `--session`, `--print`, `--provider`, and `--offline`
- **AND** the process exits with code 0

#### Scenario: help has no startup side effects

- **WHEN** `pdk_chat_demo --help` is run in an environment without provider credentials
- **THEN** help still completes successfully
- **AND** no provider is loaded, no session is opened, and no interactive prompt is entered

### Requirement: cli-flags-unknown-exit-code

Unknown flags, missing values, and invalid option forms MUST produce a non-zero exit code, a concise diagnostic, and a hint to use `--help`. The parser MUST stop before partial application startup and MUST NOT silently ignore unrecognized input.

#### Scenario: unknown flag is rejected

- **WHEN** the executable is invoked with `--not-a-real-flag`
- **THEN** parsing fails with a non-zero exit code
- **AND** the diagnostic identifies the invalid option or equivalent parse error
- **AND** the diagnostic points the user to `--help`
- **AND** DSLEngine initialization and the chat input loop do not run

#### Scenario: missing session value is rejected

- **WHEN** the executable is invoked with `--session` and no following value
- **THEN** parsing fails with a non-zero exit code
- **AND** the diagnostic indicates that `--session` requires a value
- **AND** the output includes a `--help` hint
- **AND** no session load is attempted

#### Scenario: short and long print spellings are equivalent

- **WHEN** the parser is invoked once with `-p` and once with `--print`
- **THEN** both invocations set the same `CliOptions.print` value
- **AND** both invocations return the same parse status
- **AND** generated help presents the short and long spellings as one option contract

### Requirement: cli-flags-new-options

The parser MUST accept `-p/--print`, `--provider <name>`, and `--offline` and expose their validated values to the existing startup configuration boundary. This requirement does not define a JSON/RPC output protocol, session enumeration, or provider persistence.

#### Scenario: provider override is parsed

- **WHEN** the executable is invoked with `--provider deepseek`
- **THEN** `CliOptions.provider` contains `deepseek`
- **AND** the startup configuration sees the explicit provider override
- **AND** the provider choice is not written back to the configuration file by the parser

#### Scenario: offline intent is parsed independently from mock

- **WHEN** the executable is invoked with `--offline`
- **THEN** `CliOptions.offline` is true
- **AND** `CliOptions.mock` remains false unless `--mock` is also supplied
- **AND** the parser does not claim that offline mode is equivalent to mock mode

#### Scenario: print flag is parsed without RPC semantics

- **WHEN** the executable is invoked with `-p`
- **THEN** `CliOptions.print` is true
- **AND** the same value is produced by `--print`
- **AND** parsing does not enable `--mode json|rpc` or any RPC server behavior
