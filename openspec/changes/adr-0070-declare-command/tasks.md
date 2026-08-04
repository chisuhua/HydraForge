## 1. DECLARE_COMMAND 宏头文件

- [x] 1.1 新建 `include/agenticdsl/pdk/command_macros.h`，定义 `CommandSpec` 结构体（含 `name`、`description`、`usage`、`plugin_origin`、`handler` 字段）
- [x] 1.2 验证 `CommandSpec` 字段与 ADR-0070 §决策 3 一致：`name` 以 `/` 开头、`description` 非空、`plugin_origin` 自动由宏填充
- [x] 1.3 在 `include/agenticdsl/pdk/command_macros.h` 定义 `CommandContext` 结构体（含 `user_input`、`session` 引用或标识、`context` 片段、`IToolCoordinator*` 指针——**仅暴露 `IToolCoordinator*`，禁止暴露 `IToolRegistry&` 以杜绝绕过治理路径**）
- [x] 1.4 定义 `DECLARE_COMMAND` 宏，编译期展开 `CommandSpec` 静态实例并注册到 `ICommandRegistry`
- [x] 1.5 验证宏模式遵循 ADR-0021 P1-P6：静态链接、Runtime 零感知、不依赖动态反射
- [x] 1.6 添加头文件 include guard 并运行 `clang-format` 检查格式
- [x] 1.7 验证：编译 `agenticdsl_pdk` INTERFACE 目标不报错，`cmake --build build --target agenticdsl_pdk` 成功
- [x] 1.8 提交：`git commit -m "feat(pdk): add DECLARE_COMMAND macro and CommandSpec (adr-0070)"`

## 2. ICommandRegistry L3 契约

- [x] 2.1 新建 `include/agenticdsl/contract/icommand_registry.h`，定义 `ICommandRegistry` 纯虚接口
- [x] 2.2 接口方法：`register_command(const CommandSpec&) -> bool`、`resolve_command(const std::string& name) -> std::optional<CommandSpec>`、`list_commands() -> std::vector<CommandSpec>`、`has_conflict(const std::string& name, const std::string& plugin_origin) -> std::optional<std::string>`
- [x] 2.3 确保 `ICommandRegistry` 仅依赖值类型与 `IToolCoordinator` 前向声明，不直接 include `tool_registry.h` 完整定义，避免循环 include
- [x] 2.4 验证接口可在 `src/core/engine.h` 与 `src/common/tools/registry.h` 两侧前向声明使用
- [x] 2.5 运行 `cmake --build build` 检查契约头文件独立编译通过
- [x] 2.6 提交：`git commit -m "feat(contract): add ICommandRegistry L3 interface (adr-0070)"`

## 3. CommandRegistry L1 实现

- [x] 3.1 新建 L1 实现文件（建议 `src/common/pdk/command_registry.h` + `src/common/pdk/command_registry.cpp`）
- [x] 3.2 实现 `register_command`：检查 `/exit` 保留字、检查命名冲突、返回 bool 与诊断信息
- [x] 3.3 实现 `resolve_command`：按 `/` 前缀名称精确匹配，未找到返回 `std::nullopt`
- [x] 3.4 实现 `list_commands`：返回全部已注册命令（内置 + plugin），按名称字典序排列
- [x] 3.5 实现 `/help` 内置命令：由 `list_commands` 自动生成名称 + description + usage 文本，无特权显示差异
- [x] 3.6 实现 `/exit` 保留字保护：任何 plugin 注册 `/exit` 返回 false，诊断信息包含 `plugin_origin`
- [x] 3.7 实现冲突诊断：第二次同名注册返回 false，诊断字符串包含双方 `plugin_origin`
- [x] 3.8 确保 L1 实现持有 `IToolCoordinator*` 指针（构造时注入），用于委托工具命令的治理路径
- [x] 3.9 验证：单独编译 `agenticdsl_common` 目标成功，`cmake --build build --target agenticdsl_common`
- [x] 3.10 提交：`git commit -m "feat(common): add CommandRegistry L1 implementation (adr-0070)"`

## 4. pdk_chat_demo 输入循环改造

- [x] 4.1 读取 `examples/pdk_chat_demo/main.cpp` 当前输入循环，定位硬编码 `exit` / `quit` 分支
- [x] 4.2 移除硬编码 `exit` / `quit` 分支，改为统一 `/` 前缀分发
- [x] 4.3 注入 `CommandRegistry` 实例到 main 函数或 demo 的顶层对象
- [x] 4.4 实现 `/` 前缀判断：若以 `/` 开头，调用 `ICommandRegistry::resolve_command` 并执行 handler；否则原样进入 `session.chat()`
- [x] 4.5 实现未注册命令错误处理：输入 `/foo` 返回友好错误并提示 `/help`
- [x] 4.6 确保 `/exit` 保留字行为保持：用户输入 `/exit` 正常退出，不受 plugin 干扰
- [x] 4.7 验证非 `/` 开头输入行为不变：例如普通聊天文本仍进入 LLM chat 路径
- [x] 4.8 编译 `pdk_chat_demo` 示例：`cmake --build build --target pdk_chat_demo`
- [x] 4.9 提交：`git commit -m "feat(demo): refactor pdk_chat_demo input loop to command registry dispatch (adr-0070)"`

## 5. ToolCoordinator 接线（pdk_chat_demo）

- [x] 5.1 验证 `DSLEngine::set_tool_coordinator(unique_ptr<ToolCoordinator>)` API 存在；若不存在，须扩展 `DSLEngine` 头文件添加该方法与对应 getter
- [x] 5.2 在 `examples/pdk_chat_demo/main.cpp` 创建 `DSLEngine` 后立即构造 `ToolCoordinator` 实例：`auto coordinator = make_unique<ToolCoordinator>();`
- [x] 5.3 注入到 engine：`engine.set_tool_coordinator(std::move(coordinator));`
- [x] 5.4 验证 `ToolCoordinator` 构造所需的依赖（`IToolRegistry&` / `IApprovalHandler*`）在 demo 中可获取
- [x] 5.5 启动断言：`engine.tool_coordinator() != nullptr`（若 DSLEngine 暴露该 getter）
- [x] 5.6 验证 demo 启动日志包含 `tool_coordinator: enabled`，便于运行时确认治理路径就绪
- [x] 5.7 编译 `pdk_chat_demo` 示例：`cmake --build build --target pdk_chat_demo`
- [x] 5.8 提交：`git commit -m "feat(demo): inject ToolCoordinator for command governance path (adr-0070)"`

## 6. /compact 真实 plugin 命令

- [x] 6.1 在 `examples/pdk_chat_demo/` 或 `pdk/` 下新增 `/compact` 命令实现（经 `IToolCoordinator::execute()` 调用 `session/compact` 工具）
- [x] 6.2 使用 `DECLARE_COMMAND` 宏注册 `/compact`，handler 内组装 `ToolCallContext` 并调用 `command_context.tool_coordinator()->execute("session/compact", ...)`（**严禁**直接调用 `IToolRegistry::call_tool`）
- [x] 6.3 确保 `session/compact` 工具已存在于 `lib/` 或 `ToolRegistry` 中；若不存在，先注册一个 mock/stub 工具用于验证
- [x] 6.4 验证 `/compact` 调用链经过 `ToolCoordinator` layer check、`ApprovalHandler`、ADR-0069 hooks（若已存在）
- [x] 6.5 在 demo 中加载 `/compact` plugin 并验证 `/compact` 可被用户输入触发
- [x] 6.6 提交：`git commit -m "feat(demo): add /compact plugin command as delegated tool proof (adr-0070)"`

## 7. 测试

- [x] 7.1 新建 `tests/test_command_registry.cpp`，包含 Catch2 测试框架
- [x] 7.2 测试：注册冲突 — 两个 plugin 注册同名命令，第二次返回 false，诊断含双方 `plugin_origin`
- [x] 7.3 验证冲突测试失败时诊断信息包含 `plugin_origin_a` 与 `plugin_origin_b`
- [x] 7.4 测试：`/help` 列举 — 注册 2 个 plugin 命令 + 内置 `/help` + `/exit`，`list_commands` 返回数量正确且无重复
- [x] 7.5 验证 `/help` 输出包含 `name`、`description`、`usage` 字段，且 plugin 与内置命令无特权显示差异
- [x] 7.6 测试：委托治理路径 — `/compact` handler 调用 `IToolCoordinator::execute("session/compact", ...)`，断言调用次数 = 1 且参数正确
- [x] 7.7 验证 `ToolCoordinator` 被触发（可通过 mock 或 spy 捕获 `execute()` 调用）
- [x] 7.8 **测试：治理路径覆盖** — 故意编写错误 handler 调用 `IToolRegistry::call_tool` 直接绕过；断言编译期或运行期拒绝；若无法禁止，断言治理链路未被触发即视为失败
- [x] 7.9 测试：非 `/` 开头输入 — 模拟输入 `"hello world"`，确认不调用 `resolve_command`，进入 chat 路径
- [x] 7.10 测试：`/exit` 保留字 — plugin 尝试注册 `/exit` 返回 false，内置 `/exit` 行为可用
- [x] 7.11 测试：pdk_chat_demo ToolCoordinator 接线 — 启动 demo 后断言 `engine.tool_coordinator() != nullptr`
- [x] 7.12 运行 ctest：`ctest --output-on-failure`，确保新增测试通过且全量零回归
- [x] 7.13 提交：`git commit -m "test(pdk): add command registry conflict / help / delegation / bypass-prevention tests (adr-0070)"`

## 8. 文档同步与验证

- [x] 8.1 更新 ADR-0070 状态为 🟡 Partial（若当前为 🔍 Proposed），按 §决策 7 标注转 Approved 条件
- [x] 8.2 运行 `python3 tools/adr_lint.py`，确保 0 错误
- [x] 8.3 运行 `python3 tools/docs_drift_audit.py`，确保 0 DRIFT
- [x] 8.4 运行 `openspec validate adr-0070-declare-command`，确保 exit 0
- [x] 8.5 若验证失败，读取错误信息并修改对应文件，重复 8.2-8.4
- [x] 8.6 检查 `tests/AGENTS.md` 是否需要新增测试文件说明
- [x] 8.7 检查 `proposal-suggestions.md` 状态：从 "已批准" 标记为 "已创建 change"
- [x] 8.8 提交：`git commit -m "docs: sync ADR-0070 status and run validation gates (adr-0070)"`

## 9. 收尾

- [x] 9.1 `git add openspec/changes/adr-0070-declare-command/` 并提交 artifacts 变更
- [x] 9.2 最终 `git log --oneline` 确认提交顺序清晰
- [x] 9.3 准备 plan-done handoff 写 `.rddf/state/.plan-handoff.json`（若适用）
- [x] 9.4 关闭本 change 相关 TODO 与跟踪 issue

---

## Ship Summary (Target)

- **ctest**: 全量零回归（当前基线 + 新增 command registry 测试）
- **OpenSpec change**: `openspec/changes/adr-0070-declare-command/`
- **Proposal**: `improvements/adr-0070-declare-command.md`（2026-08-01 审批）
- **Diff**: 4 artifacts + 新增头文件/实现/测试/示例改造 + ToolCoordinator 接线（实施阶段）
- **adr_lint**: 0 错误
- **docs_drift_audit**: 0 DRIFT

## Follow-ups

1. `DECLARE_SHORTCUT` 实际触发（L4-2 异步 I/O 改造后）
2. `/tree` TUI 本体（L4-6，依赖 L0-1 SessionManager）
3. `command.invoked` 等事件发射（ADR-0068 Registry 实施）
4. CLI flag 重写（L4-4 cxxopts）
5. `session.before.*` 生命周期事件（依赖 ADR-0068 附录 A 注册）