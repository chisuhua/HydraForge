## Context

Phase B Step 1+2+3+4 已 ship 完整 7 步 wiring 基础设施：
- Step 1+2: CancellationRegistry + ChatSession state
- Step 3: loop_agent + NodeExecutor + ToolCoordinator token forwarding
- Step 4: 3 loop APIs (ReactLoop/PlanExecuteLoop/ForkJoinLoop) accept std::stop_token

Step 5 验证端到端取消行为，确保 wiring 真实可达且未来重构不破坏取消语义。

## Goals / Non-Goals

**Goals:**
- Mock Blocking Provider 作为测试 helper（不修改 production 代码）
- 5 E2E tests 验证 cancellation 链路真实可达
- 100ms 内 cancellation 断言（典型 LLM 推理 1s+，mock provider 应更短）
- Full regression verification

**Non-Goals:**
- 不修改 production 代码
- 不实现 SIGINT 集成（mock 模式无 stdin 注入）
- 不实现真实 LLM cancellation 集成（已由 ILLMProvider token-aware API 支持）

## Decisions

### Decision 1: Mock Blocking Provider 设计 — 简单 polling loop

**选择**: `generate()` 内 `while (!token.stop_requested())` + 10ms sleep

**理由**:
- **零依赖**: 无需第三方库
- **可观察**: sleep 间隔可调（测试可加速）
- **简单**: 11 行代码实现

**替代方案**:
- **condition_variable**: 增加复杂度，无明显收益
- **第三方 mock 库**: 引入新依赖

### Decision 2: E2E test 设计 — token identity check + 100ms 断言

**选择**:
- 用 `std::stop_token::stop_requested()` + 计时断言（< 100ms）
- Token identity 通过 `resolve_source()` 获取 shared_ptr 比较

**理由**:
- **可断言**: 100ms 阈值明确
- **可验证 identity**: shared_ptr.get() == source.get() 验证 token 是同一来源

### Decision 3: 不集成 SIGINT

**选择**: Mock 模式 E2E 不模拟 SIGINT

**理由**:
- SIGINT 集成需要 fork+exec 子进程（signal_shutdown 测试已验证此路径）
- Mock Blocking Provider 已直接验证 cancellation 链路可达
- 避免测试重复

## Risks / Trade-offs

| Risk | Mitigation |
|------|------------|
| 100ms 阈值在 CI 慢机器上 flaky | 阈值可在 follow-up 调整为相对时间（如 5x sleep 间隔） |
| Mock provider 与真实 LLM 行为差异 | 测试目的为验证 wiring，真实 LLM 测试由现有 test_e2e_real_llm 覆盖 |
| Token identity check 失败 | Step 1+2 已 ship 验证 resolve_source 实现 |

## Migration Plan

### 部署

无 schema / API 变更 → 零迁移成本

### 回滚

删除测试文件即可（无 production 影响）

## Open Questions

无 — Step 1+2+3+4 提供稳定基础，Step 5 是验证性 ship。