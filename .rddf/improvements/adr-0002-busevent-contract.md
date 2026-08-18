# adr-0002-busevent-contract

**优先级**: P1 | **来源**: ADR-0002 EventBus + ADR-0019 IInteractionBus (Oracle 重构)
**阶段**: phase-6a | **分类**: 架构对齐
**类型**: feature
**主题**: canonical topic registry

## 架构依据
- Oracle 评审 (2026-07-26): 一次性收敛到 BusEvent 公开契约
- ADR-0002: EventBus 需统一 BusEvent 类型
- 当前 InMemoryBus 使用 pair<string,ToolResult>，无统一事件信封

## 范围
- 新建 BusEvent 公开契约 (topic, payload:ToolResult, timestamp, causal_time, priority)
- emit/subscribe 一次性迁移到 BusEvent (唯一破坏性变更)
- 更新 4 个实现者: InMemoryBus + 3 测试 Mock
- soak test: dispatch_thread + 10000 events

## 关键场景
（无）

## 技术约束
（无）

## 验收标准
- ctest 全量零回归
- nm -C | grep emit.*BusEvent 确认 ABI
