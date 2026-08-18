# adr-0037-causal-clock

**优先级**: P2 | **来源**: ADR-0037 因果排序 (Oracle 重构)
**阶段**: phase-6a | **分类**: 架构对齐
**类型**: feature
**主题**: ADR-0042状态对齐；服务化评估

## 架构依据
- ADR-0037 §决策 1: 单进程逻辑时钟 + 因果向量
- Oracle 评审: 纯增量变更，BusEvent.causal_time 已在 Change A 预留
- 不跨进程 (跨进程时升级为 Lamport)

## 范围
- CausalClock: atomic<uint64_t> + tick/now/merge/happens_before
- InMemoryBus::emit() 自动 tick + attach
- Causal monotonicity soak: 3 producer → consumer per-producer monotonic

## 关键场景
（无）

## 技术约束
（无）

## 验收标准
- ctest -R test_causal_clock 5/5 PASS
- ctest 全量零回归
