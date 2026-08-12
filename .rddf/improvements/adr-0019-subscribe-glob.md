# adr-0019-subscribe-glob

**优先级**: P1 | **来源**: ADR-0019 IInteractionBus (Oracle 重构)
**阶段**: phase-6a | **分类**: 架构对齐
**类型**: feature

## 架构依据
- Oracle 评审: 不新增 subscribe_topic，扩展 subscribe 接受 glob
- ADR-0046: PDK Plugin 间通信需要 topic-based subscribe
- 依赖 Change A (BusEvent 契约已定义)

## 范围
- subscribe() 接受 glob pattern (无通配符=精确匹配, O(1))
- InMemoryBus 双路径分发: exact map + wildcard list
- Race test: subscribe/unsubscribe 并发 dispatch

## 关键场景
（无）

## 技术约束
（无）

## 验收标准
- ctest -R test_interaction_bus_glob 全绿
- ctest 全量零回归
