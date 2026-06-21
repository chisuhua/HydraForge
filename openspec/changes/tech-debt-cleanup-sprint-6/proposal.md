## Why

2026-06-21 综合健康审计（基于 code-review-graph 2498 节点 / 14793 边全图分析）发现 3 类可执行债务：(1) `topo_scheduler::execute` 308 行超大函数（Hub 出度 89）与 `markdown_parser::create_node_from_json` 216 行 if-else 分发链违反 OCP；(2) engine.cpp 仍含 10 个跨模块完整类型 include，PIMPL-lite 收益打折；(3) 工作树 7 个 untracked 文件（plugin_loader WIP）+ 3 个 ADR 文档状态滞后（0020/0021/0022）。需在 3 周 Sprint 内闭环，对齐 ADR-0019 §1.4 完全退出标准与 ADR-0021 PDK 治理节奏。

## What Changes

- **拆分超大函数**：`TopoScheduler::execute` (308 行) 拆为 `prepare_dag_state()` + `dispatch_ready_nodes()` + `handle_node_completion()` 3 子函数，`execute()` 缩至 ≤ 60 行编排层
- **节点工厂注册表**：新增 `NodeFactoryRegistry` 类（`unordered_map<NodeType, Factory>`），消除 `create_node_from_json` 的 216 行 if-else 链，新增节点类型无需修改本函数（OCP）
- **engine.cpp 工厂化**：各模块暴露 `create_*()` 工厂函数（`scheduler::create()` / `budget::create_controller()` / `llm::create_provider()`），engine.cpp 跨模块 include 数 10 → 3（仅 types + 2 contract）
- **ADR 状态同步**：ADR-0020 → ✅ Resolved，ADR-0021/0022 → 🟡 Partial，AGENTS.md / docs/README.md 一致化
- **WIP 锁定**：plugin_loader 在制品（WIP commit）锁定 Sprint 5 进度，tasks.md 14/59 标记完成
- **清理死注释**：`yaml_json.cpp` 2 处 `[DEBUG-removed]` 注释（闭合 2026-06-09 审计 P1-2 残留）
- **plugin_loader 测试增强**：test_plugin_loader.cpp 新增 7 个 test case（ABI mismatch / dlsym 失败 / 搜索路径扫描 / RAII unload / 并发加载）

**无 API breaking change**（仅内部实现重构，contract 接口保持稳定）。

## Capabilities

### New Capabilities

- `dag-scheduler-pipeline`: TopoScheduler 调度流水线化（prepare → dispatch → handle 三段式契约）
- `node-factory-registry`: 节点类型工厂注册表模式（OCP 扩展点）

### Modified Capabilities

- `tech-debt-cleanup`: 扩展现有 spec，新增 3 个 Requirement（scheduler-pipeline / node-factory / engine-factory-deps），闭合 2026-06-09 审计 P1-2 残留

## Impact

**修改文件**：
- `src/modules/scheduler/topo_scheduler.{h,cpp}`（拆函数）
- `src/modules/parser/markdown_parser.{h,cpp}`（注册表重构）
- `src/core/engine.cpp`（工厂调用替换直接构造）
- `src/common/log/log.h` 周边（新增 log::trace/error）
- `src/modules/plugin/plugin_loader.{h,cpp}` 周边（untracked 提交）
- `tests/test_scheduler.cpp` / `tests/test_parser.cpp` / `tests/test_plugin_loader.cpp`（新增测试）
- `docs/adr/adr-002{0,1,2}*.md`（状态更新）
- `docs/README.md` / `AGENTS.md`（ADR 表格同步）
- `src/common/utils/yaml_json.cpp`（删 2 行死注释）

**API 稳定性**：
- `DSLEngine` 公共 API 零变化
- `TopoScheduler` / `NodeExecutor` 公共 API 零变化（仅内部函数提取）
- `MarkdownParser::create_node_from_json` 签名保持（内部调用注册表）

**依赖变更**：
- 新增 `<unordered_map>` / `<functional>` 包含（NodeFactoryRegistry）
- CMake 无变化（不引入新依赖）

**测试影响**：
- baseline 32/32 ctest pass → 目标 ≥ 39/39（新增 ≥ 7 scheduler test + ≥ 7 plugin test）
- TSan 验证：node factory 注册表并发安全（plugin_loader 共享注册场景）

**风险域**：
- 🔴 scheduler 是核心调度路径，回归测试覆盖必须充分
- 🟡 engine.cpp 工厂化可能暴露隐藏的依赖关系，需逐步迁移

## Non-goals

- **不重写** CognitiveWorker / DomainWorkerPool（Sprint 2/3 已 ship）
- **不修改** PDK 公共 API（ADR-0021 T4b 治理节奏锁定）
- **不引入** 新第三方依赖（仅标准库 + 现有 vendor）
- **不实现** ADR-0007 LLM 压缩 / ADR-0031/0033 实质化（属 P3 长期项）
- **不重构** `external/` vendor 代码（httplib/yaml-cpp/nlohmann_json/inja）
- **不删除** `src/modules/plugin/plugin_loader.cpp`（仅 WIP 提交，不完成 Sprint 5 ship）