# pkm-temporal-demo-scaffold

**优先级**: P0 | **来源**: roadmap.md Phase 6a 任务 T3+T4+T5
**阶段**: phase-6a | **分类**: demo-temporal-1a
**类型**: feature

## 架构依据
- roadmap.md Phase 6a: pkm_temporal_demo PDK 骨架 (ITemporalClient + Mock + CLI + pdk_entry)
- roadmap.md Phase 6a: Demo 项目 (main.cpp + 4 场景 + config.json)
- roadmap.md Phase 6a: 测试 (unit + e2e mock, ≥8 test cases)

## 范围
- ITemporalClient 抽象接口 + MockTemporalClient 实现
- 4 个演示场景 DSL (blocking/async-poll/signal/idempotent)
- ctest -R temporal ≥8 cases

## 关键场景
（无）

## 技术约束
（无）

## 验收标准
- ctest -R temporal 全绿
- ./pkm_temporal_demo --mock 4 场景全部 PASS
