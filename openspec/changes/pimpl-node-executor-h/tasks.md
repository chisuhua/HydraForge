# Tasks: 解耦 NodeExecutor ApprovalHandler 依赖

## 1. 创建 `IApprovalHandler` 抽象接口

- [x] 1.1 创建 `include/agenticdsl/policy/iapproval_handler.h` 文件头注释 (功能/作者/日期)
- [x] 1.2 添加 `#pragma once` 保护
- [x] 1.3 添加 `#include "agenticdsl/policy/iexecution_policy.h"` (复用值类型)
- [x] 1.4 定义 `class IApprovalHandler` 含 1 虚函数 `process_request(meta, ctx, preview)` — 注：实际签名为 `const ToolPreview&` (与既有 `ApprovalHandler` 一致), proposal.md 早期版本误写为 `std::string& preview`, 以实际接口为准
- [x] 1.5 验证编译 0 错误: `cmake --build build --target agenticdsl_core -j$(nproc)` (此时 0 调用方)
- [x] 1.6 验证 `grep -n 'iapproval_handler' src/ include/` 输出新建文件 ✓

## 2. `ApprovalHandler` 适配 (`: public IApprovalHandler`)

- [x] 2.1 `src/common/policy/approval_handler.h`: 添加 `: public IApprovalHandler` 继承
- [x] 2.2 `process_request` 方法添加 `override` 标记
- [x] 2.3 验证 `ApprovalHandler` 公开 API 不变 (构造 + process_request + 默认 timeout) — 注：原 API 列表中 `set_callback` 不存在, 实际为构造时注入 ApprovalCallback
- [x] 2.4 验证编译 0 错误 — `agenticdsl_common` + `agenticdsl_modules_executor` PASS
- [x] 2.5 验证 ctest 49/49 PASS (Step 1+2 兼容性变更, 注：实际是 49 测试非 spec 中写的 48)

## 3. `node_executor.h` 解耦

- [x] 3.1 `src/modules/executor/node_executor.h`: 移除 `#include "common/policy/approval_handler.h"`
- [x] 3.2 添加 `#include "agenticdsl/policy/iapproval_handler.h"`
- [x] 3.3 成员 `ApprovalHandler* approval_handler_` → `IApprovalHandler* approval_handler_`
- [x] 3.4 `set_approval_handler(ApprovalHandler*)` → `set_approval_handler(IApprovalHandler*)` (BREAKING)
- [x] 3.5 验证编译错误 (预期调用方未更新) ✓ — `execution_session.h:77` 等处确认无法隐式转换

## 4. 调用方更新 (5 处)

- [x] 4.1 `src/core/engine.cpp`: `node_executor_->set_approval_handler(approval_handler_.get())` — 注：实际代码是 `scheduler_cfg.approval_handler = approval_handler_.get()` (自动 upcast, 无源码修改)
- [x] 4.2 `tests/test_tool_coordinator.cpp`: 无 `set_approval_handler` 直接调用, 跳过
- [x] 4.3 `tests/test_layer_profile.cpp`: 无 `set_approval_handler` 直接调用, 跳过
- [x] 4.4 `tests/test_executor_with_mock_provider.cpp`: 无 `set_approval_handler` 直接调用, 跳过
- [x] 4.5 `tests/test_pdk_macros.cpp`: 无 `set_approval_handler` 直接调用, 跳过
- [x] 4.6 验证编译 0 错误 — 全模块构建 PASS

**注**: spec 列出的 4 个 test 文件均无 `set_approval_handler` 直接调用, 仅通过 ExecutionSession 间接传递. 实际级联消费者为 `execution_session.h/.cpp` + `topo_scheduler.h` + `factory.h` 三个调度层文件, 全部更新为 `IApprovalHandler*`.

## 5. 行为验证

- [x] 5.1 完整重建: `cmake --build build -j$(nproc) --target all` 0 错误
- [x] 5.2 完整测试: `cd build && ctest --output-on-failure` **49/49 PASS** (实际总数非 spec 中写的 48)
- [x] 5.3 重点验证: `test_tool_coordinator` PASS
- [x] 5.4 重点验证: `test_executor_with_mock_provider` PASS
- [x] 5.5 重点验证: `test_layer_profile` PASS
- [x] 5.6 验证 `grep -rn 'set_approval_handler(' src/ tests/` 全 `IApprovalHandler*` 类型 ✓

## 6. 架构合规性 + Ship Gate

- [x] 6.1 运行 `python3 tools/adr_lint.py` — 报 1 pre-existing 错误 (`adr-0036-three-layer-service-protocol.md` 缺 `## 状态` 章节, 与本 change 无关, 留作 follow-up)
- [x] 6.2 验证 `lsp_diagnostics` 在改动文件 (iapproval_handler.h, approval_handler.h, node_executor.h, factory.h, topo_scheduler.h) 无新增错误 — 仅 `iapproval_handler.h` 有 2 minor warnings (cppcoreguidelines-special-member-functions + unused-includes), 与现有 `IExecutionPolicy` / `IInteractionBus` 模式一致
- [x] 6.3 `git diff --stat` 显示合理修改范围 — 我自己的修改约 17 行 +/- (approval_handler.h + node_executor.h); 级联 (topo_scheduler.h + factory.h + execution_session.h/cpp) 由 PIMPL 集成完成
- [ ] 6.4 按 4 步骤分 4-5 个 commit — **跳过** (用户指令: "DO NOT run `git commit` or `git push` — never commit unless explicitly requested")
- [x] 6.5 更新 AGENTS.md 添加 Sprint 19 ship 记录 ✓
- [x] 6.6 更新 ADR-0031 §决策 5 记录 IApprovalHandler 抽象层 ✓

## 7. 归档

- [x] 7.1 `openspec validate pimpl-node-executor-h --strict` exit 0 ✓ — 输出 `Change 'pimpl-node-executor-h' is valid`
- [ ] 7.2 `openspec archive pimpl-node-executor-h --yes` — **跳过** (用户指令: 不 commit/push, archive 是 commit 后动作)

## 完成情况

**完成度**: 34/36 (94%) — 任务 6.4 + 7.2 按用户指令跳过
**测试**: 49/49 ctest PASS
**openspec validate**: exit 0