## 1. IToolHookRegistry L3 契约头文件

- [x] 1.1 新建 `include/agenticdsl/contract/itool_hook_registry.h`，定义 `enum class HookErrorPolicy { FailClosed, FailOpen }`
- [x] 1.2 验证：`grep -n "HookErrorPolicy" include/agenticdsl/contract/itool_hook_registry.h` 显示 1 处定义
- [x] 1.3 提交：`git commit -m "feat(contract): add HookErrorPolicy enum"`
- [x] 1.4 在 `itool_hook_registry.h` 中定义 `struct PreHookResult { enum Action { Continue, Deny, ModifyArgs }; ... }`
- [x] 1.5 验证：头文件自包含编译通过 `g++ -std=c++20 -I include -c -x c++ /dev/null -include include/agenticdsl/contract/itool_hook_registry.h`
- [x] 1.6 提交：`git commit -m "feat(contract): add PreHookResult data structure"`
- [x] 1.7 在 `itool_hook_registry.h` 中定义 `struct PostHookResult { bool modify_result; ToolResult modified_result; }`
- [x] 1.8 验证：结构体字段完整，`grep -n "modify_result\|modified_result" include/agenticdsl/contract/itool_hook_registry.h` 命中
- [x] 1.9 提交：`git commit -m "feat(contract): add PostHookResult data structure"`
- [x] 1.10 在 `itool_hook_registry.h` 中定义 `PreHook` / `PostHook` 类型别名（`std::function<...>`）
- [x] 1.11 验证：类型别名包含 `const ToolMetadata&`、`const ToolCallContext&`、`args` 三参数
- [x] 1.12 提交：`git commit -m "feat(contract): add PreHook/PostHook type aliases"`
- [x] 1.13 在 `itool_hook_registry.h` 中定义 `class IToolHookRegistry` 纯虚接口（`register_pre_hook` / `register_post_hook` + 虚析构）
- [x] 1.14 验证：`grep -n "virtual void register_pre_hook\|virtual void register_post_hook" include/agenticdsl/contract/itool_hook_registry.h` 命中 2 处
- [x] 1.15 提交：`git commit -m "feat(contract): add IToolHookRegistry interface"`
- [x] 1.16 在 `IToolHookRegistry` 接口中声明 `tool_glob` / `priority` / `HookErrorPolicy` 注册参数
- [x] 1.17 验证：参数顺序与 ADR-0069 §决策 3 一致（tool_glob, hook, priority, policy）
- [x] 1.18 提交：`git commit -m "feat(contract): finalize IToolHookRegistry registration API"`

## 2. ToolCoordinator Middleware 改造

- [x] 2.1 在 `src/common/tools/tool_coordinator.h` 中增加 `#include "agenticdsl/contract/itool_hook_registry.h"`
- [x] 2.2 验证：`clangd --check src/common/tools/tool_coordinator.h` 无 include 错误
- [x] 2.3 提交：`git commit -m "refactor(tools): include IToolHookRegistry in ToolCoordinator"`
- [x] 2.4 在 `ToolCoordinator` 类中新增 `IToolHookRegistry* hook_registry_` 成员（默认 nullptr）
- [x] 2.5 验证：`grep -n "hook_registry_" src/common/tools/tool_coordinator.h` 命中
- [x] 2.6 提交：`git commit -m "feat(tools): add nullable hook registry member"`
- [x] 2.7 在 `ToolCoordinator` 类中新增 `void set_hook_registry(IToolHookRegistry* registry)` setter
- [x] 2.8 验证：`grep -n "set_hook_registry" src/common/tools/tool_coordinator.h` 命中
- [x] 2.9 提交：`git commit -m "feat(tools): add set_hook_registry setter"`
- [x] 2.10 重构 `src/common/tools/tool_coordinator.cpp` execute 流：在 NestingGuard 之后、layer check 之前插入 pre_hooks 调用
- [x] 2.11 验证：pre_hooks 遍历逻辑按 priority 升序，同 priority 按注册顺序
- [x] 2.12 提交：`git commit -m "feat(tools): insert pre_hooks into ToolCoordinator execute flow"`
- [x] 2.13 实现 pre-hook 匹配逻辑：按 `tool_glob` 对当前 `meta.name` 做通配匹配
- [x] 2.14 验证：`tool_glob="shell/*"` 匹配 `shell/exec`，不匹配 `fs/read`
- [x] 2.15 提交：`git commit -m "feat(tools): add tool_glob matching for pre-hooks"`
- [x] 2.16 实现 pre-hook ModifyArgs 分支：用 `modified_args` 替换后续 `effective_args`
- [x] 2.17 验证：pre-hook 修改后的 args 进入 layer check / ApprovalHandler / call_tool
- [x] 2.18 提交：`git commit -m "feat(tools): support pre-hook args modification"`
- [x] 2.19 实现 pre-hook Deny 分支：返回 `ToolResult::error`，emit `tool.audit.denied`
- [x] 2.20 验证：Deny 时 layer check 与 ApprovalHandler 不执行
- [x] 2.21 提交：`git commit -m "feat(tools): implement pre-hook deny path"`
- [x] 2.22 实现 pre-hook 异常处理：FailClosed → 视为 Deny；FailOpen → 跳过并记 audit meta warning
- [x] 2.23 验证：两类 policy 各一个单元测试覆盖
- [x] 2.24 提交：`git commit -m "feat(tools): add HookErrorPolicy handling for pre-hooks"`
- [x] 2.25 在 call_tool 之后、audit.completed 之前插入 post_hooks 调用
- [x] 2.26 验证：post_hooks 按 priority 升序执行，同 priority 按注册顺序
- [x] 2.27 提交：`git commit -m "feat(tools): insert post_hooks into execute flow"`
- [x] 2.28 实现 post-hook 修改 result 分支：用 `modified_result` 替换后续 `ToolResult`
- [x] 2.29 验证：post-hook 修改后的 result 进入 `audit.completed` 与 `tool.execution.end`
- [x] 2.30 提交：`git commit -m "feat(tools): support post-hook result modification"`
- [x] 2.31 实现 post-hook 异常处理：FailClosed → 返回 error ToolResult；FailOpen → 返回原始 result
- [x] 2.32 验证：FailClosed 时 emit `tool.audit.denied`，FailOpen 时继续并记 warning
- [x] 2.33 提交：`git commit -m "feat(tools): add HookErrorPolicy handling for post-hooks"`
- [x] 2.34 空 hook registry 路径：当 `hook_registry_ == nullptr` 时，execute 行为与改造前逐字节一致
- [x] 2.35 验证：`ctest -R test_tool_coordinator` 在无 hook 用例上通过，且无 diff 回归
- [x] 2.36 提交：`git commit -m "fix(tools): preserve no-hook backward compatibility"`

## 3. ADR-0068 事件发射点对齐

- [x] 3.1 在 `ToolCoordinator::execute()` 中 pre_hooks 结束后、layer check 前发射 `tool.execution.start`
- [x] 3.2 验证：payload 包含最终生效 args（已应用 pre-hook ModifyArgs）
- [x] 3.3 提交：`git commit -m "feat(tools): emit tool.execution.start after pre-hooks"`
- [x] 3.4 在 `ToolCoordinator::execute()` 中 post_hooks 结束后、return 前发射 `tool.execution.end`
- [x] 3.5 验证：payload 包含最终生效 result（已应用 post-hook 修改）
- [x] 3.6 提交：`git commit -m "feat(tools): emit tool.execution.end after post-hooks"`
- [x] 3.7 在 pre-hook / post-hook Deny / FailClosed 路径发射 `tool.audit.denied`
- [x] 3.8 验证：reason 字段包含 hook 名与 deny_reason / 异常摘要
- [x] 3.9 提交：`git commit -m "feat(tools): emit tool.audit.denied on hook deny/failure"`
- [x] 3.10 与 ADR-0068 Registry 对齐事件 topic 与 payload schema（字段名/类型）
- [x] 3.11 验证：对比 `docs/adr/adr-0068-event-emission-contract.md` 附录 A，字段一致
- [x] 3.12 提交：`git commit -m "docs(tools): align event payload with ADR-0068 registry"`

## 4. budget_agent 真实 Plugin 用例

- [ ] 4.1 定位现有 `pdk/budget_agent/` 目录或新建示例目录
- [ ] 4.2 验证：目录存在且可被 `pdk/CMakeLists.txt` 或独立 plugin 构建脚本识别
- [ ] 4.3 提交：`git commit -m "chore(pdk): locate budget_agent plugin directory"`
- [ ] 4.4 实现 `budget_agent` pre-hook：检查当前 `ToolCallContext` 中预算是否超限
- [ ] 4.5 验证：预算正常时返回 `Continue`，超限时返回 `Deny`（reason="budget exceeded"）
- [ ] 4.6 提交：`git commit -m "feat(pdk): add budget_agent budget-check pre-hook"`
- [ ] 4.7 注册 hook 时使用 `tool_glob="*"` 与 `HookErrorPolicy::FailClosed`
- [ ] 4.8 验证：plugin 加载后任意工具调用均触发预算检查
- [ ] 4.9 提交：`git commit -m "feat(pdk): register budget_agent pre-hook with FailClosed policy"`
- [ ] 4.10 为 budget_agent pre-hook 编写集成测试，验证 Deny 路径结果
- [ ] 4.11 验证：`ctest -R budget_agent` 或对应集成测试通过
- [ ] 4.12 提交：`git commit -m "test(pdk): add budget_agent pre-hook integration test"`

## 5. 测试（5 类核心 + 向后兼容）

- [x] 5.1 新建 `tests/test_tool_coordinator_hooks.cpp` 测试文件
- [x] 5.2 验证：`file(GLOB test_*.cpp)` 自动收录，CMake 重新配置后 `ctest -N | grep test_tool_coordinator_hooks` 命中
- [x] 5.3 提交：`git commit -m "test(tools): scaffold test_tool_coordinator_hooks.cpp"`
- [x] 5.4 编写 Deny 测试：pre-hook 返回 Deny，验证 `tool.audit.denied` 被发射且 layer/ApprovalHandler 未执行
- [x] 5.5 验证：`ctest -R test_tool_coordinator_hooks --output-on-failure` 中 Deny case 通过
- [x] 5.6 提交：`git commit -m "test(tools): add pre-hook deny test"`
- [x] 5.7 编写 ModifyArgs 测试：pre-hook 修改 args，验证 `call_tool` 收到修改后参数
- [x] 5.8 验证：ModifyArgs case 通过
- [x] 5.9 提交：`git commit -m "test(tools): add pre-hook modify args test"`
- [x] 5.10 编写 FailClosed 测试：pre-hook 抛异常 + FailClosed policy，验证结果视为 Deny
- [x] 5.11 验证：FailClosed case 通过
- [x] 5.12 提交：`git commit -m "test(tools): add pre-hook fail-closed test"`
- [x] 5.13 编写 FailOpen 测试：pre-hook 抛异常 + FailOpen policy，验证调用继续并记录 warning
- [x] 5.14 验证：FailOpen case 通过
- [x] 5.15 提交：`git commit -m "test(tools): add pre-hook fail-open test"`
- [x] 5.16 编写 post-hook 修改 result 测试：验证 `tool.execution.end` 与 `audit.completed` payload 为修改后结果
- [x] 5.17 验证：post-hook result modification case 通过
- [x] 5.18 提交：`git commit -m "test(tools): add post-hook modify result test"`
- [x] 5.19 编写无 hook 向后兼容测试：构造 `ToolCoordinator` 但不 set hook registry，验证行为与旧路径一致
- [x] 5.20 验证：向后兼容 case 通过
- [x] 5.21 提交：`git commit -m "test(tools): add no-hook backward compatibility test"`
- [x] 5.22 编写 priority 排序测试：同 priority 与不同 priority 的 hook 执行顺序
- [x] 5.23 验证：priority 排序 case 通过
- [x] 5.24 提交：`git commit -m "test(tools): add hook priority ordering test"`
- [x] 5.25 编写 tool_glob 匹配测试：验证 `shell/*`、`*` 等模式生效
- [x] 5.26 验证：tool_glob matching case 通过
- [x] 5.27 提交：`git commit -m "test(tools): add tool_glob matching test"`
- [x] 5.28 运行全量 ctest：`cmake --build build && ctest --output-on-failure`
- [x] 5.29 验证：全量测试零回归（baseline + 新增 cases 全部 PASS）
- [x] 5.30 提交：`git commit -m "test(tools): verify full ctest regression"`

## 6. 文档同步与 Ship Gate

- [ ] 6.1 更新 `docs/adr/adr-0069-tool-coordinator-hooks.md` 状态：从 🔍 Proposed 改为 🟡 Partial
- [ ] 6.2 验证：ADR 状态行正确，且 §决策 7 Approved 条件未全部满足
- [ ] 6.3 提交：`git commit -m "docs(adr): mark ADR-0069 as Partial after hook middleware ship"`
- [ ] 6.4 在 ADR-0069 中追加 Ship Evidence 段：列出本 change 关键 commit 与测试覆盖
- [ ] 6.5 验证：ship evidence 段包含 5 类测试通过记录与 ctest 结果
- [ ] 6.6 提交：`git commit -m "docs(adr): add ADR-0069 ship evidence"`
- [ ] 6.7 运行 `python3 tools/adr_lint.py`
- [ ] 6.8 验证：输出 `0 errors`，所有 ADR 状态一致
- [ ] 6.9 提交：`git commit -m "ci: adr_lint passes"`
- [ ] 6.10 运行 `python3 tools/docs_drift_audit.py`
- [ ] 6.11 验证：输出 `0 DRIFT`
- [ ] 6.12 提交：`git commit -m "ci: docs_drift_audit passes"`
- [ ] 6.13 运行 `openspec validate adr-0069-tool-coordinator-hooks`
- [ ] 6.14 验证：输出 `valid`
- [ ] 6.15 提交：`git commit -m "ci: openspec validate passes"`
- [ ] 6.16 将 OpenSpec change 从 `openspec/changes/adr-0069-tool-coordinator-hooks/` 归档到 `openspec/changes/archive/2026-MM-DD-adr-0069-tool-coordinator-hooks/`
- [ ] 6.17 验证：归档目录包含 proposal.md / design.md / tasks.md / specs/ 与 .openspec.yaml
- [ ] 6.18 提交：`git commit -m "chore(openspec): archive adr-0069-tool-coordinator-hooks"`

---

## Ship Summary

- **目标 ctest**：全量零回归（baseline + 新增 test_tool_coordinator_hooks cases）
- **关键文件**：
  - `include/agenticdsl/contract/itool_hook_registry.h`（新建）
  - `src/common/tools/tool_coordinator.h` / `.cpp`（修改）
  - `tests/test_tool_coordinator_hooks.cpp`（新建）
  - `pdk/budget_agent/` 或等效 plugin 目录（新增 pre-hook 用例）
- **ADR 状态**：ADR-0069 从 🔍 Proposed → 🟡 Partial
- **验证清单**：`adr_lint` 0 errors / `docs_drift_audit` 0 DRIFT / `openspec validate` valid
