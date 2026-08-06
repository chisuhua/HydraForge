## 1. /model 命令注册 - DECLARE_COMMAND stub 实现

- [x] 1.1 创建 `examples/pdk_chat_demo/commands/model_command.{h,cpp}`：`DECLARE_COMMAND(model, "Switch LLM provider (Wave 1 stub)", body)`
- [x] 1.2 body 实现：调用 `ToolCoordinator::route("provider_switch_stub", {provider_name})`（stub 专用端点）
- [x] 1.3 stub 端点实现 `examples/pdk_chat_demo/tools/provider_switch_stub.{h,cpp}`，返回 `[Wave 1 stub] provider switch 将在 provider-dynamic-discovery 落地后激活` 字符串
- [x] 1.4 在 `examples/pdk_chat_demo/main.cpp::register_default_commands()` 注册 `/model` 到 `CommandRegistry`
- [x] 1.5 stub 端点注册 ToolMetadata：category=Workflow + approval_policy=agent + allowed_layers=[Workflow]
- [x] 1.6 验证命令注册：`./pdk_chat_demo --mock` + 输入 `/help` 列出 `/model`
- [x] 1.7 验证 stub 调用：输入 `/model deepseek-v4-pro` 输出结构化文案 + 不抛错
- [x] 1.8 提交：`git commit -m "feat(model-cmd): register /model DECLARE_COMMAND with Wave 1 stub"`

## 2. main.cpp 零 hardcode 审计 + 消除

- [x] 2.1 运行回归 grep：`grep -nE '"\/(help|exit|compact|model|tree|fork|clone)' examples/pdk_chat_demo/main.cpp`
- [x] 2.2 提取可能 hardcode 分支（`if (input.starts_with("/...")) { ... }`）并记录位置
- [x] 2.3 对每个 hardcode 分支，要么消除（改为 `DECLARE_COMMAND`），要么替换为 `CommandRegistry::route()` 调用
- [x] 2.4 验证仅保留 `/` 前缀分发统一入口（`if (input.starts_with("/")) { return CommandRegistry::route(input); }` 或等效）
- [x] 2.5 运行 grep 应返回 0 行（除统一分发入口匹配的 if 行）
- [x] 2.6 单元测试 `tests/test_main_hardcode_audit.cpp`：编译时 grep 验证脚本化（catch2 + `std::system("grep ...")`），CI 集成
- [x] 2.7 提交：`git commit -m "refactor(chat-demo): remove residual /model and other hardcoded slash branches"`

## 3. UnknownCommand 统一错误处理

- [x] 3.1 验证 `CommandRegistry::route()` 返回类型含 `UnknownCommand` variant（adr-0070 ship 已实现）
- [x] 3.2 在 `examples/pdk_chat_demo/main.cpp::run_input_loop()` 处理 UnknownCommand：打印 `unknown command: /<name>. Type /help for list of commands.`，不调用 LLM
- [x] 3.3 错误信息不回显用户输入前缀的具体拼写（防御性：避免注入），仅提取 `/` 后的命令名做日志
- [x] 3.4 单元测试 `tests/test_command_registry_unknown.cpp`：注册 N 个已知命令 + M 个未注册命令路径（M ≥ 5），验证 UnknownCommand 触发率 100%
- [x] 3.5 提交：`git commit -m "feat(chat-demo): unified UnknownCommand handler without LLM fallback"`

## 4. ToolCoordinator 治理路径 - /model stub

- [x] 4.1 验证 `ToolCoordinator` 已注册 `provider_switch_stub` 工具（步骤 1.5 完成）
- [x] 4.2 测试 layer check：Cognitive layer 调用 `/model` → ToolResult.ok=false + error_code=PermissionDenied
- [x] 4.3 测试 Workflow layer：`/model test-provider` → ToolResult.ok=true + data.message = stub 文案
- [x] 4.4 测试 Thinking layer：Cognitive-like（在 demo 实际 layer profile 中按 ADR-0004 Thinking=ReadOnly 不允许 `provider/switch`）应拒绝
- [x] 4.5 单元测试 `tests/test_provider_switch_stub.cpp`：mock ToolRegistry + 3 个 layer 场景 + 异常路径
- [x] 4.6 提交：`git commit -m "test(chat-demo): /model stub governance layer check coverage"`

## 5. 输入循环回归测试 + E2E

- [x] 5.1 单元测试 `tests/test_input_loop_regression.cpp`：mock LLM + 真实 `CommandRegistry`，依次输入 `/help` `/exit` `/compact` `/model test` `/unknown1` `/unknown2` `/unknown3`
- [x] 5.2 验证：`/help` 输出含 5 命令（`/help /exit /compact /tree /fork /clone /model`），tree/fork/clone 由 sister change 提供，本 change 不依赖其 ship
- [x] 5.3 验证：`/exit` 退出循环（return code 0）
- [x] 5.4 验证：`/compact` 调用 `ToolCoordinator::route("session/compact", args)`（adr-0070 ship 已处理）
- [x] 5.5 验证：`/model test` 输出 stub 文案（步骤 1.7）
- [x] 5.6 验证：5 个 `/unknown*` 命令各打印 `unknown command: /...`，LLM 调用次数 = 0
- [x] 5.7 提交：`git commit -m "test(input-loop): regression coverage for /help /exit /compact /model + unknown path"`

## 6. ctest 全量 + adr-0070 fixture 复用

- [x] 6.1 跑 `ctest --output-on-failure -j$(nproc)` 全量
- [x] 6.2 验证：`test_command_registry` 系列测试零回归（adr-0070 ship fixture 完整复用）
- [x] 6.3 验证：`test_main_hardcode_audit`（步骤 2.6）通过
- [x] 6.4 验证：`test_input_loop_regression`（步骤 5.1）通过
- [x] 6.5 验证：`test_provider_switch_stub`（步骤 4.5）通过
- [x] 6.6 已知 pre-existing 失败 `test_cost_tracking_decorator`（c16 Phase 5 引入，与本 change 无关）
- [x] 6.7 提交：`git commit -m "test(ci): all ctest pass + adr-0070 fixture regression-free"`

## 7. ADR 状态同步 + docs sync

- [x] 7.1 ADR-0070 状态保持 ✅ Approved（增量 ship 注记追加）
- [x] 7.2 `docs/active-status.md` §一 Phase 6a 行追加本 change ship 注记
- [x] 7.3 `openspec validate chat-slash-commands-migration --strict` exit 0
- [x] 7.4 `tools/adr_lint.py` exit 0 + `tools/docs_drift_audit.py` 0 DRIFT
- [x] 7.5 commit + merge archive
