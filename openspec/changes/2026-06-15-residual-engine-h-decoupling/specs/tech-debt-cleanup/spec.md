### Requirement: engine-h-zero-cross-module

`src/core/engine.h` MUST NOT 包含任何 `modules/` 或 `common/` 下的非 types 头文件。
例外: `common/llm/llm_types.h`(types 头文件,允许保留)。
抽象 MUST 通过 `include/agenticdsl/contract/` 下的 i* 接口实现:
- `IProviderFactory` 替代 `common/llm/mock_provider.h` 直接 include
- `IToolRegistry` 替代 `common/tools/registry.h` 直接 include
- `TraceRecord` POD 上移到 `include/agenticdsl/types/trace_record.h` 替代 `modules/trace/trace_exporter.h` 直接 include

#### Scenario: engine.h 跨模块 include 退出

- **WHEN** `grep -c '#include "modules/\|#include "common/' src/core/engine.h`
- **THEN** MUST = 1 (仅 llm_types.h types 头文件)

#### Scenario: IProviderFactory 接口存在

- **WHEN** 检查 `include/agenticdsl/contract/iprovider_factory.h`
- **THEN** MUST 含 `IProviderFactory` 接口 + `create(config)` 虚函数
- **AND** MUST 有 MockProviderFactory + CloudProviderFactory + LlamaProviderFactory 3 个实现

#### Scenario: IToolRegistry 接口存在

- **WHEN** 检查 `include/agenticdsl/contract/itool_registry.h`
- **THEN** MUST 含 `IToolRegistry` 接口 + `call_tool(name, args)` 虚函数
- **AND** `ToolRegistry : public IToolRegistry` 在 `src/common/tools/registry.h`

#### Scenario: TraceRecord POD 上移

- **WHEN** 检查 `include/agenticdsl/types/trace_record.h`
- **THEN** MUST 含 TraceRecord POD 结构体定义(纯数据,无实现)
- **AND** `src/modules/trace/trace_exporter.h` MUST NOT 重新定义 TraceRecord

#### Scenario: ADR-0019 §1.4 状态变更

- **WHEN** 检查 `docs/adr/adr-0019-iinteraction-bus-mvp.md` §1.4
- **THEN** MUST 标记为 ✅ 已解决(2026-06-XX)

#### Scenario: 全量测试零回归

- **WHEN** `ctest --output-on-failure`
- **THEN** MUST ≥ 25/25 PASS(含新增 IProviderFactory/IToolRegistry/TraceRecord 测试)
- **AND** TSan 干净
- **AND** ASan 干净
