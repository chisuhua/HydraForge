## Context

`examples/pdk_chat_demo/main.cpp:76-83` 当前仍采用手写命令行解析。启动阶段先检查 `argv[1]` 是否为 `--skill-child`，随后用 `argc > 1 && argv[1] == "--mock"` 判断 mock 模式，再把参数复制到 `std::vector<std::string>`，通过遍历相邻元素寻找 `--session <id>`。这段逻辑只覆盖两个 flag，缺少统一的 usage 文案、参数类型校验、未知 flag 诊断和退出码契约。新增 flag 需要继续修改多个分散条件，容易产生顺序依赖和行为漂移。

本 change 引入 cxxopts 作为 `pdk_chat_demo` 的命令行解析层。cxxopts 按单头文件库方式 vendored 到 `external/`，遵循项目对 nlohmann_json 和 inja 的本地依赖惯例。构建不得依赖系统安装的 cxxopts，不使用 FetchContent、包管理器或联网下载。

现有行为必须保持：`--mock` 仍启用 mock provider，`--session <id>` 仍选择要加载的 session，`--skill-child` 仍在 DSLEngine 初始化前进入子进程入口。新增 flag 只在本 change 明确纳入的范围内生效，不能借 CLI 重写顺便改变 provider 初始化、session 持久化或交互循环的语义。

本 change 位于 Wave 3 的 `demo-chat-v2` 阶段。Wave 2 的 `chat-streaming-slash-tui` 与 `session-tree-tui` 依赖集中化的 flag 声明和解析结果，后续新增选项应能够复用同一入口，而不再扩展手写扫描。`docs/adr/adr-0070-declare-command.md` 将 CLI flag 重写列为后续决策项，其 `DECLARE_COMMAND` 注册模式是本 change 的相邻 prior art：声明集中、运行时行为明确、输入层入口与能力层实现分离。本 change 复用其分层思想，但不把 CLI flag 建模为 slash command 或 ToolRegistry 工具。

## Goals / Non-Goals

**Goals:**

- 将 cxxopts 固定版本 vendored 到 `external/`，构建时只使用仓库内文件。
- 保持 `--mock`、`--session <id>`、两者组合以及 `--skill-child` 的既有行为等价。
- 通过集中化的 flag declaration table 生成 parser 配置、help 文案和解析结果映射。
- 让 `--help` 自动列出全部已声明 flag、参数形式、默认值和说明，不再手写拼接帮助文本。
- 对未知 flag、缺少参数和不合法参数给出错误信息并返回非零 exit code，同时提示使用 `--help`。
- 新增 `-p/--print`、`--provider` 和 `--offline` 三个 flag，并为后续 `--system-prompt` 等声明保留清晰扩展位置。
- 用单元测试覆盖每个声明的解析结果，用 demo E2E 测试覆盖真实 argv 到启动行为的关键路径。

**Non-Goals:**

- 不实现 slash command 的注册、路由或 TUI 行为。slash command 继续由 `adr-0070-declare-command` 及其后续变更负责。
- 不实现 session tree 的 CLI 选择能力，例如 `-c`、`-r`、`--tree` 或相关交互，交给 `session-tree-tui`。
- 不实现 `--mode json|rpc`。RPC 模式依赖 ADR-0059 的跨进程协议，不在本 change 引入。
- 不实现 `-c` 续最近 session 或 `-r` 选择 session。两者依赖 `session-manager-jsonl` 的枚举与选择 API。
- 不实现 RPC server、stdio protocol、网络监听或跨进程生命周期管理。
- 不改变 provider 工厂、offline backend、session store 或 chat loop 的内部实现，只把 CLI 值传入既有配置路径。
- 不把所有未来 pi-agent flags 一次性实现。未纳入本 change 的 flag 只保留后续声明位置和依赖说明。

## Decisions

### Decision 1: Vendor cxxopts into `external/`

**Rationale**:

- 项目已经将 nlohmann_json、inja、yaml-cpp 等第三方依赖置于 `external/`，本地 vendoring 是当前可复现构建惯例。
- 单头文件 vendoring 不需要系统包探测，也不会让 demo 在不同发行版上产生不同的 include 路径或版本行为。
- 版本、许可证和源文件都能随仓库审查，符合 PDK 示例的离线构建约束。
- CMake 只需把 cxxopts include path 作为目标级依赖提供给 `pdk_chat_demo`，不添加全局 `include_directories()`。

**Alternatives Considered**:

- **FetchContent**：会引入联网配置、缓存和构建环境差异，违反本项目 demo 可离线构建的约束。
- **系统包或 `find_package(cxxopts)`**：要求用户预装依赖，且不同发行版可能提供不同版本，和 nlohmann_json/inja 的仓库内惯例不一致。
- **继续手写解析**：短期改动最少，但每个后续 flag 都会扩大条件扫描和测试矩阵，不能满足集中化 help 与未知 flag 错误契约。

### Decision 2: Use a data-driven flag declaration table

**Rationale**:

- 每个 flag 的名称、短名称、参数类型、默认值、help 文案和目标字段集中在一处，新 flag 主要表现为新增一行声明。
- cxxopts 负责词法解析和标准错误信息，声明表负责把 parser result 映射到 demo 的轻量 `CliOptions` 值结构体，避免业务代码直接依赖 `cxxopts::ParseResult`。
- `--mock` 这类布尔开关、`--session` 和 `--provider` 这类字符串参数、`--offline` 和 `--print` 都能在同一表中保持一致的描述方式。
- 这种模式与 ADR-0070 的声明式注册思路相同，但范围限于进程启动参数，不引入宏、动态注册或插件扫描。

**Alternatives Considered**:

- **在 `main()` 中继续按 flag 写 `if` 分支**：能够快速迁移现有两个选项，但 help、默认值和解析映射会继续分散。
- **只封装一个 `parse_args()`，内部仍使用顺序条件链**：改善 main.cpp 可读性，却没有真正形成可审计的 flag contract，也无法保证新增 flag 只修改一处声明。
- **把解析结果直接以 `cxxopts::ParseResult` 向下传递**：让业务层耦合第三方库，后续更换 parser 或做单元测试都需要携带 parser 对象，不符合最小边界原则。

### Decision 3: Preserve behavior through layered regression tests

**Rationale**:

- 单元层直接构造 argv，逐项验证 `--mock`、`--session <id>`、`-p/--print`、`--provider`、`--offline`、组合输入、默认值和错误输入，能够精确定位解析映射问题。
- help 层使用稳定的 snapshot 或关键行断言，确认输出由声明生成，并包含全部当前 flag，而不是只确认进程成功退出。
- E2E 层运行 `pdk_chat_demo --mock` 的 argv 入口，验证 mock 模式和 session id 到启动配置的传递仍然成立。`--skill-child` 早期分支单独回归，确保 parser 不会拦截子进程协议参数。
- 未知 flag 与缺失参数测试必须检查非零返回码和 `--help` 提示，防止 cxxopts 异常被吞掉后错误地继续启动。

**Alternatives Considered**:

- **只做编译测试**：无法发现 flag 拼写、默认值、help 漂移或 exit code 错误。
- **只做整 demo E2E**：启动后会进入交互和 provider 初始化，失败定位慢，且难以覆盖所有 parser 分支。
- **只对 help 做 snapshot**：help 可能正确而实际字段映射错误，不能替代 per-flag parse assertions。

### Decision 4: Keep parser errors at the CLI boundary

**Rationale**:

- `parse_cli_args()` 负责把 cxxopts 的异常和无效输入转换为统一的诊断路径，输出简短错误、usage/help 提示并返回非零状态。
- `main()` 在完成 `--skill-child` 的早期分支后才调用解析器，避免子进程 IPC 参数被普通 CLI 逻辑改变。
- 业务初始化只接收已验证的 `CliOptions`，不需要在 provider、session 或 chat loop 中重复检查 CLI 字符串。

**Alternatives Considered**:

- **让 cxxopts 异常一路传播到 `main()`**：会把第三方异常格式暴露给用户，并让不同启动路径的错误处理不一致。
- **错误后继续使用默认值**：未知 flag 或缺少值可能被静默忽略，违背非零退出码和安全失败要求。

## Risks / Trade-offs

- **[Risk] cxxopts 许可证与仓库分发策略不兼容** → 缓解：Task 1 在加入源码前核对上游 LICENSE/NOTICE，保留许可证文件或在 external 目录记录来源与固定版本，并在验证阶段检查文件存在。
- **[Risk] `--session=id`、`--session id`、短选项组合等语法与旧行为存在差异** → 缓解：以既有支持语法为基线，明确只保证 proposal 覆盖的 `--session <id>` 形式，同时为等号形式、缺值、重复 flag 建立测试并记录最终选定语义。
- **[Risk] cxxopts 默认 help 或异常格式随版本改变** → 缓解：固定 vendored 版本，snapshot 只锁定项目声明的关键行和退出行为，避免依赖无关空格；升级依赖必须单独审查 snapshot。
- **[Risk] 新增 `--provider` 或 `--offline` 解析成功，但下游配置尚未消费** → 缓解：在 `CliOptions` 到现有 provider/config 初始化的映射处增加字段传递测试；若某字段暂时只改变启动配置，不宣称已实现完整 backend 语义。
- **[Risk] flag declaration table 与 cxxopts option group 的映射出现重复或遗漏** → 缓解：单元测试枚举声明表与 help 输出中的关键名称，并通过 E2E 检查已声明 flag 能进入实际启动路径。
- **[Trade-off] 增加一个 vendored header 会扩大仓库体积** → 接受：换取离线、可复现和无系统依赖构建，且 cxxopts 的单头文件形式不会增加运行时组件。
- **[Trade-off] 本 change 不提前实现 `--mode`、`-c`、`-r`** → 接受：这些选项有明确的 ADR 或 sister change 依赖，提前声明会造成看似可用但语义不完整的 CLI 契约。

## Migration Plan

1. 在 `external/` 加入固定版本 cxxopts 和许可证信息，并以目标级 include 配置接入 `pdk_chat_demo`。
2. 新增轻量 `CliOptions` 与集中 flag declaration table，替换 main.cpp 当前手写 `argv` 遍历，保留 `--skill-child` 早期分支。
3. 将既有 `--mock`、`--session` 和新增 `-p/--print`、`--provider`、`--offline` 映射到现有初始化路径。
4. 增加 parser 单元测试、help 输出测试和 demo E2E argv 回归测试。
5. 运行 ctest 全量，并执行 `openspec validate cli-args-cxxopts --json` 作为 change gate。

无数据迁移，无用户配置文件迁移。若回滚，删除 cxxopts 接入和新 parser 文件即可恢复原手写解析；运行时 session 数据格式不变。

## Open Questions

- **OQ1**: `--provider` 的值是否覆盖配置文件中的 provider，还是只作为显式启动覆盖项？→ 本 change 采用显式 CLI 值优先于配置文件默认值，但不改配置文件持久化。
- **OQ2**: `--offline` 是否强制 mock provider？→ 本 change 只传递 offline intent 给既有初始化层，不把它等同于 `--mock`，具体 backend 选择仍由 provider/config 契约决定。
- **OQ3**: `-p/--print` 是否立即退出？→ 本 change 只完成解析和 `CliOptions` 传递，print 模式的完整非交互输出协议由后续 chat streaming 变更定义；不在本 change 添加 RPC 或 JSON 输出。
