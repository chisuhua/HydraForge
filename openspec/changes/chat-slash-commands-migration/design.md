## Context

`adr-0070-declare-command` (shipped 2026-08-04, `openspec/changes/archive/2026-08-04-adr-0070-declare-command/`) 已落地 `ICommandRegistry` + `CommandRegistry` + `DECLARE_COMMAND` 宏机制，并示范交付 `/help` `/exit` `/compact` 三个内置命令（`/compact` 委托至 `session/compact` 工具并经 ToolCoordinator 治理路径）。`pdk_chat_demo/main.cpp::process_slash_command` 已具备统一 `/` 前缀分发骨架，但 `/model` 等命令仍可能残留 hardcode 分支或完全缺失。

`improvements/chat-async-io-steering.md` 与 `pi-agent` 借鉴路径 §三 P0.2 描述 EventHandler 流式渲染（Wave 2 `chat-streaming-render`），其前置是命令层统一化——先有统一命令入口与事件注册，再有事件订阅消费。本 change 作为 P0.2 闭环前置 Wave 1 步骤，仅迁移 `/model` 主线与审计消除其他 hardcode 分支，不引入流式渲染。

`/model` 实际切换功能依赖 `provider-dynamic-discovery`（Wave 2 提案），本 change 仅注册 stub 命令（返回 "待 provider-dynamic 落地后激活"），避免引入未 ship 的 provider 切换 API。

## Goals / Non-Goals

**Goals:**
- `/model` slash 命令经 `DECLARE_COMMAND("model", summary, body)` 注册到 `CommandRegistry`，body 调用 `ToolCoordinator::call_tool("provider/switch", args)`，Wave 1 stub 阶段返回 "Wave 2 provider-dynamic 落地后激活"。
- `examples/pdk_chat_demo/main.cpp` 输入循环零 hardcode slash 命令分支（`grep -nE '^\s*if\s*\(.*"/[a-z]' examples/pdk_chat_demo/main.cpp` 返回 0 行；唯一例外是 `/` 前缀分发统一入口的 `if (input.starts_with("/"))` 或等效分发逻辑，已 ship）。
- 未注册 `/` 命令的统一处理：`CommandRegistry::route()` 返回 `UnknownCommand` 错误，main.cpp 输出 `unknown command: /<name>. Type /help for list of commands.` 并 **不** 进入 LLM 调用。
- 输入循环回归测试覆盖：`/help` `/exit` `/compact` `/model` + 5 个未注册命令路径（每条仅打印 unknown 不进 LLM）。
- 复用 `adr-0070` 已 ship 的 `tests/test_command_registry.cpp` fixture，不重复实现命令派发逻辑。
- `/model` stub 经 ToolCoordinator 治理路径：Cognitive layer 拒绝 / Workflow layer 准入（与 `/compact` 同治理需求）。
- ctest 全量零回归（除 pre-existing `test_cost_tracking_decorator`）。

**Non-Goals:**
- EventHandler 流式渲染（→ Wave 2 `chat-streaming-render`）。
- CLI flag `--system-prompt` / `--append-system-prompt`（→ `cli-args-cxxopts` + Wave 2 `chat-streaming-render`）。
- `/tree` `/fork` `/clone` 三个命令（→ 同期 `session-tree-commands`，本 change 仅审计 main.cpp 中是否存在这些 hardcode 分支消除，命令本身在该 sister change 落地）。
- `/model` 实际 provider 切换实现（依赖 Wave 2 `provider-dynamic-discovery` ship）。
- EventBus 新主题注册（ADR-0068 附录 A 已有 `session.persisted` 主题，不在本 change 新增事件主题）。
- `/model --list` 子命令（列出可用模型）：Wave 1 仅 stub，Wave 2 完整实现依赖 `available_models()` API（已在 c16 ship）。

## Decisions

### Decision 1: `/model` 注册路径复用 DECLARE_COMMAND 宏

**Rationale**:
- 与 adr-0070 ship 决策 2 + 决策 4 一致：所有 `/` 前缀命令经 `DECLARE_COMMAND` 注册，`CommandRegistry::route()` 单点派发。
- `/model` 命令体调用 `ToolCoordinator::call_tool("provider/switch", args{provider_name})`，与 `/compact` 调用 `session/compact` 模式完全对齐。
- 测试 fixture 复用 `tests/test_command_registry.cpp`，新增约 20 行（注册 + stub 返回断言），不重写派发逻辑。

**Alternatives Considered**:
- **直接在 main.cpp 写 `if (input == "/model ...")`**：退化为 hardcode，违反 proposal MUST 约束。
- **封装为独立命令子系统**：破坏 adr-0070 "单一注册表" 决策。

### Decision 2: `/model` Wave 1 stub 仅返回文案，不调用 provider 切换

**Rationale**:
- `provider-dynamic-discovery` 与 `available_models()` API 已 ship（c16 + 内部 Tool），但 `provider/switch` tool 尚未在 ToolCoordinator 注册（其完整切换逻辑涉及 ModelRegistry mutation + 配置持久化，超出本 change 范围）。
- stub 阶段输出文本与 `provider-dynamic-discovery` ship 后再激活符合 Wave 1 / Wave 2 拆分：未 ship 的依赖前置 stub 占位是 rdd 治理经验证模式（参考 `skill-interpreter-real-loading` §决策 4 "硬编码 default_skill_capability"）。
- stub 文案明确告知用户依赖状态：`/model deepseek-v4-pro` → `[Wave 1 stub] provider switch 将在 provider-dynamic-discovery 落地后激活 (TBD: provider/switch tool 注册 + 配置持久化)`。

**Alternatives Considered**:
- **完整实现 provider/switch tool**：超出 Wave 1 范围，依赖项未 ship 会引入 broken code。
- **stub 调用不存在的 `pd_dynamic::switch(.)` API**：导致命令体 in-flight 失败。

### Decision 3: 未注册命令统一处理在 CommandRegistry 层而非 main.cpp

**Rationale**:
- adr-0070 设计 `CommandRegistry::route()` 返回 `std::variant<ToolResult, UnknownCommand>`，`UnknownCommand` 携带命令名。
- main.cpp 仅打印 `unknown command: /<name>. Type /help for list of commands.`，**不** 进入 LLM 调用（避免兜底 LLM "编造" 命令响应）。
- 集中错误处理在注册表层，保持 main.cpp 简洁。

**Alternatives Considered**:
- **main.cpp 处理 UnknownCommand**：分散错误处理逻辑，违反决策 1 集中派发。

### Decision 4: main.cpp 输入循环仅改动 `if (input.starts_with("/"))` 后逻辑

**Rationale**:
- adr-0070 ship 时已替换为统一 `CommandRegistry::route()` 调用，残留 hardcode 应极有限（grep 验证中应仅返回统一分发入口本身）。
- 本 change 仅消除 adr-0070 ship 后回归或新增的 hardcode（如新增 `/model` 时误用 `if` 分支而非 `DECLARE_COMMAND`）。
- 改动范围应 < 30 行 main.cpp，diff 局部且可 review。

**Alternatives Considered**:
- **整体重构 main.cpp**：超出 Wave 1 范围，引入不必要回归风险。

## Risks / Trade-offs

- **[Risk] `/model` stub 阶段用户期望 vs 实际行为差距** → 缓解：stub 输出明确告知 "Wave 1 stub"，并提供 hover hint `provider-dynamic-discovery 落地后激活`，用户不会误以为命令失败。
- **[Risk] Cognitive layer 拒绝 `/model` 调用导致用户困惑** → 缓解：layer check 错误信息明确包含建议路径（"Workflow layer required for /model; current layer: Cognitive. Use --layer override at startup"），与 adr-0070 `/compact` 模式一致。
- **[Risk] 残留 hardcode 分支 grep 规则可能漏判** → 缓解：grep pattern 同时验证 `"/compact"` `"/help"` `"/exit"` `"/model"` `"/tree"` `"/fork"` `"/clone"` 等 7 个内置命令全部无 main.cpp 匹配（除统一分发入口）。
- **[Risk] 复用 `tests/test_command_registry.cpp` fixture 可能未来破坏 ADR-0070 测试契约** → 缓解：本 change 新增的 TEST_CASE 通过 tag `[chat-slash-cmd]` 区分，`adr-0070` 相关 `TEST_CASE_METHOD` 零改动。
- **[Trade-off] Wave 2 `chat-streaming-render` 需要时才能验证端到端命令 → 渲染 → LLM 调用链** → 缓解：Wave 1 stub + grep 验证 + mock 测试覆盖命令派发与治理，渲染层 Wave 2 单元独立验证。
- **[Trade-off] `provider/switch` tool 在 Wave 1 stub 阶段尚未注册** → ToolCoordinator `call_tool("provider/switch", .)` 会触发 `ToolNotRegistered` 错误，本 change 在 stub body 内显式调用已知 stub 路径而非 ToolCoordinator（如 body 直接 log），降低 stub → real 的二次重构面积。

## Migration Plan

无数据迁移。

部署：
1. PR merge 后 `pdk_chat_demo` 二进制自动支持 `/model` 命令（注册即生效）。
2. 现有用户无须额外配置，`/help` 立即列出 5 个内置命令。
3. Wave 2 `provider-dynamic-discovery` ship 后激活 `/model` 真实切换（仅需替换 stub body，无需改 main.cpp 或命令注册）。

回滚：PR revert 即可（约 5 文件 + < 200 LOC）。

## Open Questions

- **OQ1**: `/model` Wave 1 阶段是否提供 `--list` 子命令列出当前可用 models（仅 list 不切换）？→ 默认不提供（依赖 `available_models()` 调用，stub 阶段无意义），Wave 2 一并落地。
- **OQ2**: 未注册命令错误信息是否包含建议相近命令（如 "Did you mean /help?"）？→ Wave 1 不实现（需 fuzzy match 库），Wave 2 可选。
- **OQ3**: `/model` Wave 1 stub 是否记录 telemetry 标记 dependency `provider-dynamic-discovery` for future wire？→ 是，stub body 输出结构化日志 `[wave-1-stub] pending: provider-dynamic-discovery`，便于 dashboard 追踪。
