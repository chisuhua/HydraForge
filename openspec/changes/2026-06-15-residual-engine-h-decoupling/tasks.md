# Tasks: Residual engine.h Decoupling (Stage 4 Task 19 残留)

> **变更类型**: 真实实现 — 全部 task 标 [ ],需实际执行

## 任务依赖图

```
T1 IProviderFactory 抽象
 ├── T1.1 接口定义
 ├── T1.2 MockProviderFactory 实现
 ├── T1.3 DSLEngine 注入
 ├── T1.4 CloudProviderFactory 实现
 ├── T1.5 test_provider_factory.cpp
 ↓
T2 IToolRegistry 抽象
 ├── T2.1 接口定义
 ├── T2.2 ToolRegistry 加 override
 ├── T2.3 NodeExecutor 改用 IToolRegistry
 ├── T2.4 test_tool_registry_interface.cpp
 ↓
T3 TraceRecord 上移
 ├── T3.1 拆分 trace_exporter.h → trace_record.h + trace_exporter.h
 ├── T3.2 验证编译
 ↓
T4 engine.h 移除 3 include
 ├── T4.1 移除 mock_provider.h include
 ├── T4.2 移除 registry.h include
 ├── T4.3 移除 trace_exporter.h include
 ↓
T5 验证 + ADR 更新
 ├── T5.1 全量 ctest 25/25
 ├── T5.2 TSan/ASan 干净
 ├── T5.3 ADR-0019 §1.4 更新
```

## Tasks

- [ ] T1. IProviderFactory 抽象
  - 文件:
    - `include/agenticdsl/contract/iprovider_factory.h` (新建)
    - `src/common/llm/mock_provider_factory.h/cpp` (新建)
    - `src/common/llm/cloud_provider_factory.h/cpp` (新建)
    - `src/common/llm/llama_provider_factory.h/cpp` (新建)
    - `src/core/engine.h` (使用 IProviderFactory 而非直接 include mock_provider.h)
    - `src/core/engine.cpp` (工厂注入)
    - `tests/test_provider_factory.cpp` (新建, ≥ 6 test cases)
  - 粒度: 4 天
  - 验收:
    - [ ] IProviderFactory 接口定义在 `include/agenticdsl/contract/`
    - [ ] MockProviderFactory::create() 返回 MockLLMProvider
    - [ ] CloudProviderFactory::create() 返回 CloudLLMAdapter
    - [ ] DSLEngine 默认使用 MockProviderFactory
    - [ ] 多线程 1000x create() 无 data race
    - [ ] test_provider_factory ≥ 6 case 通过
    - [ ] `grep "common/llm/mock_provider" src/core/engine.h` = 0

- [ ] T2. IToolRegistry 抽象
  - 文件:
    - `include/agenticdsl/contract/itool_registry.h` (新建)
    - `src/common/tools/registry.h` (加 `override`)
    - `src/modules/executor/node_executor.h` (使用 IToolRegistry)
    - `tests/test_tool_registry_interface.cpp` (新建, ≥ 5 test cases)
  - 粒度: 3 天
  - 验收:
    - [ ] IToolRegistry 接口定义在 `include/agenticdsl/contract/`
    - [ ] ToolRegistry : public IToolRegistry
    - [ ] call_tool/register_tool/has_tool 3 个虚函数
    - [ ] 现有 16/16 test_tool_registry 零回归
    - [ ] test_tool_registry_interface ≥ 5 case 通过
    - [ ] `grep "common/tools/registry" src/core/engine.h` = 0

- [ ] T3. TraceRecord 上移到 include/agenticdsl/types/
  - 文件:
    - `include/agenticdsl/types/trace_record.h` (新建, POD)
    - `src/modules/trace/trace_exporter.h` (改为 include 新头文件)
    - `src/modules/trace/trace_exporter.cpp` (实现,不变)
  - 粒度: 1 天
  - 验收:
    - [ ] TraceRecord POD 在 `include/agenticdsl/types/trace_record.h`
    - [ ] trace_exporter.h 不再定义 TraceRecord (仅 include 新头文件)
    - [ ] 现有 trace 相关测试零回归
    - [ ] `grep "modules/trace/trace_exporter" src/core/engine.h` = 0

- [ ] T4. engine.h 移除 3 个 include
  - 文件: `src/core/engine.h`
  - 验收:
    - [ ] 移除 `#include "common/llm/mock_provider.h"`
    - [ ] 移除 `#include "common/tools/registry.h"`
    - [ ] 移除 `#include "modules/trace/trace_exporter.h"`
    - [ ] 替换为对应的 i* 抽象 include (IProviderFactory, IToolRegistry) + TraceRecord POD
    - [ ] `grep -c '#include "modules/\|#include "common/' src/core/engine.h` = 1 (仅 llm_types.h)

- [ ] T5. 验证 + ADR-0019 §1.4 更新
  - 验收:
    - [ ] `cmake --build build && ctest --output-on-failure` 25+/25+ PASS
    - [ ] TSan 干净 (新增并发测试覆盖)
    - [ ] ASan 干净
    - [ ] `openspec validate 2026-06-15-residual-engine-h-decoupling` exit 0
    - [ ] `docs/adr/adr-0019-iinteraction-bus-mvp.md` §1.4 状态更新为 ✅ 已解决
    - [ ] Single commit `refactor(core): complete engine.h decoupling (IProviderFactory + IToolRegistry + TraceRecord)`
    - [ ] 提交信息符合 conventional commits

## 总工作量

~10 工作日 (2 周单人)

## 验证清单

- [ ] 25+ Phase 0 测试零回归
- [ ] 5+ 新增测试 (IProviderFactory/IToolRegistry/TraceRecord)
- [ ] TSan 干净 (含新增并发测试)
- [ ] ASan 干净
- [ ] CI 6 jobs 全绿
- [ ] `openspec validate` 0 error
- [ ] engine.h 跨模块 include 退出 = 1 (仅 llm_types.h types 头文件)
- [ ] ADR-0019 §1.4 状态 ✅ 已解决

## 提交策略

**Single commit**: `refactor(core): complete engine.h decoupling (IProviderFactory + IToolRegistry + TraceRecord)`
**包含**: T1-T5 全部代码 + 测试 + ADR 更新

## 风险

- T1 工厂注入可能引入循环依赖 → DSLEngine 注入而非 NodeExecutor 持有
- T2 抽象破坏旧调用方 → 方法签名不变 + `override` 关键字
- T3 POD 上移触发 include 循环 → POD 无依赖,放在 types/
