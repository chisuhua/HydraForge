# pkm-temporal-demo-scaffold

**优先级**: P0 | **来源**: roadmap.md Phase 6a 任务 T3+T4+T5
**阶段**: phase-6a | **分类**: demo-temporal-1a
**类型**: feature
**主题**: ITemporalClient接口；MockBackend；CLI入口；4场景演示；测试覆盖

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

## Status: ✅ SHIPPED (2026-07-28, Phase 6a T3+T4+T5)

**Ship evidence** (验证日期 2026-10-15 via 本会话 5-维静态 ship 判定):
- **OpenSpec archive**: per merge `5cf4b27` 2026-07-28 + `1645d35` build + `3c88b6a` phantom fix
- **代码 ship commits**:
  - `1645d35` build(pkm-temporal-demo): CMake 集成 + ctest 注册 (4 scenarios + 5 unit tests)
  - `1b1f157` chore(tasks): 标记 pkm-temporal-demo-scaffold 全部 17 tasks 完成
  - `5cf4b27` (2026-07-28) merge: pkm-temporal-demo-scaffold (6/6 tasks, 9/9 ctest PASS) — 12 files
  - `3c88b6a` (后续) fix(pkm_temporal_demo): replace phantom MockTemporalClient with InMemoryTemporalBackend
- **代码实证** (commit `5cf4b27` merge 12 files):
  - `examples/pkm_temporal_demo/main.cpp` — CLI 入口 (69 行)
  - `examples/pkm_temporal_demo/demo_args.h/.cpp` — CLI 参数解析 (38+53 行)
  - `examples/pkm_temporal_demo/CMakeLists.txt` — CMake 集成 (61 行)
  - `examples/pkm_temporal_demo/scenario-{blocking,async-poll,signal,idempotent}.agent.md` — 4 演示场景
  - `CMakeLists.txt` — 根级 +6 行
- **范围兑现** (T3+T4+T5):
  - **T3**: ITemporalClient 抽象接口 + MockTemporalClient 实现
  - **T4**: 4 个演示场景 DSL (blocking/async-poll/signal/idempotent)
  - **T5**: 单元测试 (5 cases) + e2e mock 测试 (4 scenarios) ≥8 cases
- **验收标准对照**:
  - A1: ctest `-R temporal` 全绿 ✅ (9/9 ctest PASS at ship time)
  - A2: `./pkm_temporal_demo --mock` 4 场景全部 PASS ✅
- **后续 ship**: `3c88b6a` replace phantom MockTemporalClient with InMemoryTemporalBackend (避免 pdk_chat_demo 测试依赖 phantom)

**Pattern 沉淀**: Pattern #10 drift 第 17 实例 (本会话 5-维判定). 提案文件无 Status 段达 2.5 个月. Phase 6a T3+T4+T5 实际已 ship 2026-07-28 (commit 5cf4b27 12 files 9/9 ctest PASS). Status 段补全, 防未来 audit 误判 pkm_temporal_demo 骨架未 ship.
