# Tasks: Residual engine.h Decoupling (Stage 4 Task 19 残留)

> **变更类型**: 真实实现 — 全部 task 标 [ ],需实际执行
> **2026-06-17 v2 修订**: Oracle 审查后修复 6 P0 + 6 P1. 关键认知: LLMProviderFactory 仅在 ADR-0005 §3 设计草图 (md), 编译代码不存在; IToolRegistry 必须扩展至 ~7 虚函数覆盖 ToolRegistry 公共 API; SecureToolRegistry 需 API 兼容性改造; 6 个 SimpleCognitiveOrchestrator 调用点需迁移; 工作量从 10 → 25 工作日 (5 周).
> **2026-06-18 v3 修订**: Oracle T2 深度审查 (实施 T1 后). 关键修正:
> 1. **设计 bug 修复**: design.md §決策2.1 line 181 的 `ToolRegistry base_registry_;` 值成员设计**不可实施** (ToolRegistry non-copyable + 违反装饰器语义 + test_secure_tool_registry.cpp 全部 9 测试失败). 改用**委托式多继承** (SecureToolRegistry 保持现有 registry_ref_/registry_shared_ 成员, 加 `public IToolRegistry` + 9 override 转发).
> 2. **虚函数数修正**: 9 个 (不是 v2 的 7-8 个). `has_cost_callback()` 零调用点 → YAGNI 移除. `register_tool` 模板 → 加 `register_tool_function(std::function<...>)` 桥接.
> 3. **6 调用点迁移修正**: 不使用 `get_tool_registry_concrete() + static_cast` (设计缺陷 — 未来 SecureToolRegistry 注入时 UB). 改用 **SimpleCognitiveOrchestrator 改为 `IToolRegistry*`** (依赖倒置, 6 调用点零修改).
> 4. **工作量诚实修正**: T2 从 5 天 → 3.5 天 (选项 A 委托式多继承, 比 v2 简化).
> 5. **NodeExecutor 副作用**: `node_executor.cpp:20` 也是 `ToolRegistry&` — T4 PIMPL 后需同步改 (1 处).
> **测试基线**: 28/28 (per `ctest` after T1 complete, 27 baseline + 1 test_provider_factory).

## 任务依赖图

```
T1 ✅ LLMProviderFactory 从零构建 (COMPLETE, commit 355d52c/14ba62b/f9062e6/9fe4266)
  ├── T1.1 IProviderFactory 接口定义 ✅
  ├── T1.2 LLMProviderFactory 实现 (单一 backend_name 路由) ✅
  ├── T1.3 MockProviderFactory 实现 ✅
  ├── T1.4 DSLEngine 注入 provider_factory_ (PIMPL-lite) ✅ — 跨模块 include 3→2
  ├── T1.5 test_provider_factory.cpp ✅ — 28/28 测试零回归
  ↓
T2 IToolRegistry 9 虚函数 + SecureToolRegistry 委托多继承 (3.5 天, v3 修订)
  ├── T2.1 IToolRegistry 接口定义 (9 虚函数: has_tool/call_tool/list_tools
  │       + register_tool_function 桥接 + register_llm_tool/is_llm_tool/
  │       get_llm_params/call_llm_tool/set_cost_callback) — 0.5d
  ├── T2.2 ToolRegistry : public IToolRegistry + 9 override — 0.5d
  ├── T2.3 SecureToolRegistry 委托式多继承 (保持现有 registry_ref_/shared_,
  │       加 public IToolRegistry, 9 override 转发 — 0.5d, NOT base_registry_ 值成员)
  ├── T2.4 SimpleCognitiveOrchestrator 改为 IToolRegistry* (依赖倒置) — 0.25d
  │       6 调用点 (test_simple_orchestrator × 5 + slice_01 × 1) 零修改
  ├── T2.5 test_tool_registry_interface.cpp (≥ 5 case) — 1.0d
  ├── Buffer: ADR-0004 V1.1 amendment + design review — 0.25d
  ↓
T4 engine.h 移除 tools/registry.h + PIMPL-lite tool_registry_ + 析构外置 (3 天)
  ├── T4.1 tool_registry_ 改 unique_ptr<IToolRegistry> (PIMPL-lite) — 1d
  ├── T4.2 NodeExecutor 同步改 ToolRegistry& → IToolRegistry& (依赖 T2) — 0.5d
  ├── T4.3 engine.h 移除 1 include (tools/registry.h) + 析构外置到 engine.cpp — 1.5d
  ↓
T5 验证 + 同步 (3 天)
  ├── T5.1 跑 38+ ctest + TSan + ASan — 1d
  ├── T5.2 同步 7 ADR + 6 docs + R5 重分类 — 1d
  ├── T5.3 openspec validate + single commit — 1d
```

## Tasks

### T1. ✅ COMPLETE (LLMProviderFactory 从零构建, 7 commits)

> **2026-06-18 完成**: 7 commits (355d52c/14ba62b/f9062e6/f7ef5bf/9fe4266/21b79d7/b4da645)
> 28/28 测试零回归. 跨模块 include 3→2 (mock_provider.h 已移除).

- [x] IProviderFactory 接口定义 (include/agenticdsl/contract/iprovider_factory.h)
- [x] LLMProviderFactory 路由 (mock/openai/anthropic/deepseek/qwen/local/llama)
- [x] MockProviderFactory 包装 MockLLMProvider
- [x] DSLEngine 注入 provider_factory_ (PIMPL-lite)
- [x] test_provider_factory.cpp 6 case (含多线程 1000x create)

### T2. IToolRegistry 9 虚函数 + SecureToolRegistry 委托多继承 (3.5 天, v3 修订)

> **2026-06-18 v3 修订**: Oracle 深度审查后修正 3 个设计缺陷:
> 1. **不使用 `base_registry_` 值成员** (non-copyable + 违反装饰器 + 9 个 secure 测试失败) — 保持现有 `registry_ref_/registry_shared_/registry_holder_` 成员, 加 `public IToolRegistry` + 9 override 委托
> 2. **9 个虚函数** (不是 v2 估的 7-8 个): 移除未使用的 `has_cost_callback`, 用 `register_tool_function(std::function<...>)` 桥接 register_tool 模板
> 3. **SimpleCognitiveOrchestrator 改为 `IToolRegistry*`** (依赖倒置), 6 调用点零修改 — 不使用 `get_tool_registry_concrete() + static_cast` 方案 (未来 SecureToolRegistry 注入时 UB)

- **文件**:
  - `include/agenticdsl/contract/itool_registry.h` (新建, 9 虚函数)
  - `src/common/tools/registry.h` (加 `override` + 实现 `register_tool_function` 桥接)
  - `include/agenticdsl/tools/secure_tool_registry.h` (委托式多继承, NOT base_registry_ 值成员)
  - `include/agenticdsl/cognitive/simple_orchestrator.h` (改为 `IToolRegistry*`)
  - `src/modules/cognitive/simple_orchestrator.cpp` (无逻辑修改, 仅类型变化)
  - `tests/test_tool_registry_interface.cpp` (新建, ≥ 5 case)
- **粒度**: 3.5 天
- **验收**:
  - [ ] IToolRegistry 接口定义 (9 虚函数 + CostCallback/ToolFunc typedef)
  - [ ] `has_cost_callback` 不在 IToolRegistry (YAGNI 移除)
  - [ ] `register_tool` 模板保持 (非虚函数), 加 `register_tool_function(name, std::function<...>)` 桥接
  - [ ] `call_tool` 返回 `nlohmann::json`, 参数 `unordered_map<string,string>`
  - [ ] ToolRegistry : public IToolRegistry (9 override)
  - [ ] SecureToolRegistry : public IToolRegistry (委托多继承, 保持 registry_ref_ 成员, 9 override 转发到 wrapped ToolRegistry)
  - [ ] SecureToolRegistry::call_tool 调 call_direct, 转换 Result.allowed + payload → json
  - [ ] SimpleCognitiveOrchestrator ctor/member 改 `IToolRegistry*`
  - [ ] 6 个 `get_tool_registry()` 调用点零修改 (test_simple_orchestrator × 5 + slice_01 × 1)
  - [ ] test_tool_registry_interface ≥ 5 case 通过
  - [ ] test_secure_tool_registry 9 个测试零回归
  - [ ] 28+5=33+ 测试零回归

### T3. ✅ COMPLETE (TraceRecord 上移到 include/agenticdsl/types/, commit 01666fa)

> **2026-06-18 完成**: TraceRecord data-only struct 从 src/modules/trace/trace_exporter.h
> 上移到 include/agenticdsl/types/trace_record.h. 27/27 测试零回归.

- [x] TraceRecord struct 在 `include/agenticdsl/types/trace_record.h`
- [x] trace_exporter.h 改为 include 新头文件
- [x] AGENTS.md / phase1-roadmap / roadmap-status 同步
- [x] 27 测试零回归

### T4. engine.h 移除 1 include + PIMPL-lite tool_registry_ + 析构外置 (3 天, v3 修订)

> **2026-06-18 v3 修订**: T1 已完成 mock_provider.h 移除. T4 改为移除 tools/registry.h (1 个 include). NodeExecutor 同步改 (v3 新增子任务).

- **文件**:
  - `src/core/engine.h` (核心修改)
  - `src/core/engine.cpp` (析构移到类外)
  - `src/modules/executor/node_executor.h/cpp` (ToolRegistry& → IToolRegistry&, v3 新增)
- **粒度**: 3 天
- **验收**:
  - [ ] `tool_registry_` 改 `std::unique_ptr<IToolRegistry>` (PIMPL-lite)
  - [ ] `get_tool_registry()` 返回 `IToolRegistry&` (无 concrete 兼容方法 — 全部走 IToolRegistry)
  - [ ] DSLEngine::register_tool 模板改用 `tool_registry_->register_tool_function(...)` 桥接
  - [ ] NodeExecutor 构造参数改 `IToolRegistry&` (1 处)
  - [ ] 析构外置到 engine.cpp (= default)
  - [ ] 移除 `#include "common/tools/registry.h"` (line 41)
  - [ ] 加 `#include "agenticdsl/contract/itool_registry.h"`
  - [ ] `grep -c '#include "modules/\|#include "common/' src/core/engine.h` = 1 (仅 llm_types.h)
  - [ ] `grep -c '#include "agenticdsl/contract/' src/core/engine.h` ≥ 5

### T5. 验证 + 同步 (3 天, v3 修订)

> **2026-06-18 v3 修订**: T1 已 ship, 文档同步已在 commit e39c967 部分完成.
> T5 范围调整为: 最终验证 (T2+T4 完整 build + ctest) + 7 ADR 同步 + R5 重分类 + single commit.

- **粒度**: 3 天
- **验收**:
  - [ ] `cmake --build build && ctest --output-on-failure` ≥ 33+5 = 38+ PASS
  - [ ] TSan 干净
  - [ ] ASan 干净
  - [ ] `tools/adr_lint.py docs/adr/` exit 0
  - [ ] `openspec validate 2026-06-15-residual-engine-h-decoupling` exit 0
  - [ ] **ADR-0019 §1.4 状态更新为 ✅ 已解决** (T5.2 执行时, 不是现在)
  - [ ] 7 ADR 同步 (T2+T4 新增 9 虚函数语义 + 委托式多继承 + 依赖倒置说明)
  - [ ] R5 重分类: project-organization.md R5 → P1 active (已重分类过, 确认保留)
  - [ ] Single commit `refactor(core): complete engine.h decoupling (IProviderFactory facade + IToolRegistry 9-method + TraceRecord POD)`

## 总工作量

~9 工作日 (T1: 5d 完成 + T2: 3.5d + T4: 3d + T5: 3d, 含 buffer)
实际 T1 已 ship 5d; 剩余 9.5d.
