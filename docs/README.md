# HydraForge 文档索引

## 目录结构

```
docs/
├── adr/              # Architecture Decision Records (架构决策, 阶段化分类)
│   └── plugin/       # Plugin 化实施候选 ADR (plugin-candidate, 2026-06-16+)
├── adr-management/   # ADR 元数据: 状态词汇表 + 关联性分析 (自 2026-06-16 移出 adr/)
├── specs/            # 规范文档 (当前有效版本)
├── guides/           # 用户和开发者指南
├── design/           # 设计文档
├── archive/          # 归档 (过期版本)
├── proposals/        # AgenticDSL 语言演进提案 (18 docs: 14 话题子目录 + 4 根文件)
└── active-status.md  # [统一看板] 当前活跃变更状态追踪 (替代 roadmap-status.md + implementation-roadmap.md)
```

---

## adr/ - Architecture Decision Records

架构决策记录，记录重要的架构决策及其背景、权衡。

| 文件 | 议题 | 状态 |
|------|------|------|
| `adr-0001-illm-provider-streaming-interface.md` | ILLMProvider 流式接口 | ✅ Approved |
| `adr-0002-eventbus-bounded-queue.md` | EventBus 有界队列 | ❌ Not Implemented (V1 归档, Phase 1 改用 ADR-0019 IInteractionBus MVP 承担事件通信) |
| `adr-0003-dslengine-thread-safety.md` | DSLEngine 线程安全 | ✅ Approved |
| `adr-0004-toolregistry-security.md` | ToolRegistry 安全模型 | ✅ Approved |
| `adr-0005-llm-backend-config-factory.md` | LLM 后端配置与工厂 | ✅ Approved |
| `adr-0006-harness-engine-thread-model.md` | HarnessEngine 后台线程 | ⛔ Superseded (被 ADR-0020 替代) |
| `adr-0007-context-compression.md` | 上下文压缩机制 | 🟡 Partial (快照有,无 LLM 压缩) |
| `adr-0008-structured-context.md` | 结构化 Context | ✅ Approved (2026-06-12 LayeredContext 实现完成) |
| `adr-0009-dsl-standard-library.md` | DSL 标准库规划 | ✅ Approved |
| `adr-0019-iinteraction-bus-mvp.md` | IInteractionBus 接口与 TUI Chat MVP | ✅ Approved (2026-06-24, Sprint 5 ship) |
| `adr-0020-thread-model-isolation.md` | 多智能体线程模型与隔离策略 | ✅ Approved (2026-06-24, Sprint 5 ship) |
| `adr-0021-pdk-design.md` | Plugin Development Kit (PDK) 设计 | ✅ Approved (2026-06-24, Sprint 5 ship) |
| `adr-0022-plugin-loading.md` | 插件加载机制 | ✅ Approved (2026-06-24, Sprint 5 ship) |
| `adr-0023-tool-result-standard.md` | ToolResult 标准化 | ✅ Approved (2026-06-24, Sprint 5 ship) |
| `adr-0031-execution-policy.md` | 执行策略 | 🟡 Partial (C3 P1-P2 ✅ Approved 2026-07-31; C4 P3-P4 🟡 active; §决策 8 4 项 defer 至 C6) |
| `adr-0033-session-hierarchy.md` | 会话层次结构 | ✅ Approved (Sprint 15 C5, 2026-07-02) |
| `adr-0002-impl-scope-audit.md` | ADR-0002 实施范围审计 (OpenSpec change `docs-code-drift-audit-2026-06` 产出) | 📋 审计补充 |
| `adr-0004-impl-scope-audit.md` | ADR-0004 实施范围审计 (同上) | 📋 审计补充 |
| `adr-0001-illm-provider-streaming-interface-impl-scope.md` | ADR-0001 实施范围审计 (C9) | 📋 审计补充 |
| `adr-0003-dslengine-thread-safety-impl-scope.md` | ADR-0003 实施范围审计 (C9) | 📋 审计补充 |
| `adr-0004-toolregistry-security-impl-scope.md` | ADR-0004 实施范围审计 (C9) | 📋 审计补充 |
| `adr-0005-llm-backend-config-factory-impl-scope.md` | ADR-0005 实施范围审计 (C9) | 📋 审计补充 |
| `adr-0007-context-compression-impl-scope.md` | ADR-0007 实施范围审计 (C9) | 📋 审计补充 |
| `adr-0008-structured-context-impl-scope.md` | ADR-0008 实施范围审计 (C9) | 📋 审计补充 |
| `adr-0019-iinteraction-bus-mvp-impl-scope.md` | ADR-0019 实施范围审计 (C9) | 📋 审计补充 |
| `adr-0020-thread-model-isolation-impl-scope.md` | ADR-0020 实施范围审计 (C9) | 📋 审计补充 |
| `adr-0022-plugin-loading-impl-scope.md` | ADR-0022 实施范围审计 (C9) | 📋 审计补充 |
| `adr-0023-tool-result-standard-impl-scope.md` | ADR-0023 实施范围审计 (C9) | 📋 审计补充 |
| `adr-0033-session-hierarchy-impl-scope.md` | ADR-0033 实施范围审计 (C9) | 📋 审计补充 |

### adr/plugin/ - Plugin 化候选清单

> 自 2026-06-16 起，本目录存放**计划通过 Plugin 实现**的活跃 ADR（与根目录共用同一编号空间与工具链扫描范围）。详见 [adr/plugin/README.md](adr/plugin/README.md)。

| 文件 | 议题 | 状态 |
|------|------|------|
| `adr/plugin/adr-0034-model-router.md` | IModelRouter 模型路由接口（plugin-candidate, C7 完整 ship） | ✅ Approved (2026-07-02) |

### adr-management/ - ADR 元数据

> 自 2026-06-16 起，状态词汇表与关联性分析从 `adr/` 移至本目录。

| 文件 | 议题 | 说明 |
|------|------|------|
| `STATUS-GLOSSARY.md` | ADR 状态词汇表 | 6 个标准标签定义 + 维护规则 |
| `relationships.md` | ADR 关联性分析 | 由 `tools/adr_relationships.py` 自动生成 |

> ADR 编号 0024-0028 为未来 Phase-4 / Phase-6 规划保留；占位文件 0029/0035 已删除（2026-06-12）；13 个已废弃 ADR 已归档到 [docs/archive/adr/](archive/adr/README.md)（ADR-0034 已迁出归档至 plugin 候选区）。

---

## specs/ - 规范文档 (当前有效版本)

> ⚠️ **DEPRECATED** ⚠️ [`../SPECS-ALIGNMENT.md`](SPECS-ALIGNMENT.md) (2026-07-06) — 该规范对齐计划文件自标半准确,不再维护。当前维护规范以下方表格为准,新增 spec 请直接添加到此目录并在 ADR 中交叉引用。

核心规范文档，定义系统行为。

| 文件 | 议题 | 说明 |
|------|------|------|
| `architecture.md` | AgenticOS 架构 | 8 层架构定义 |
| `layer0.md` | L0 运行时规范 | DSL 引擎核心行为 |
| `layer0-refactor.md` | L0 重构计划 | Layer0 重构计划 |
| `dsl.md` | DSL 规范 v3.10 | 最新 DSL 语言规范 |
| `stdlib-v3.10.md` | DSL 标准库 v3.10 | 合并自 dsl-lib + stdlib (Stage 2 / Task 8) |
| `memory-v3.10.md` | DSL 内存记忆规范 v3.10 | 合并自 memory.md (MEP-001 v3.2 Draft) + dsl.md §10.3 (Stage 2 / Task 9) |

---

## guides/ - 用户和开发者指南

面向用户的指南和参考文档。

| 文件 | 议题 | 说明 |
|------|------|------|
| `app-dev-guide.md` | 应用开发指南 | 使用 AgenticDSL 开发应用 |
| `app-dev-guide-cpp.md` | C++ 开发指南 | C++ API 使用 |
| `developer-guide.md` | 开发者指南 | 开发规范和最佳实践 |
| `rt-guide.md` | 运行时指南 | 运行时配置和部署 |
| `reference.md` | DSL 参考 | DSL 语法快速参考 |
| `contract-template.md` | 契约模板 | Agent 间交互模板 |
| `example.md` | 示例 | DSL 示例说明 |
| `training-guide.md` | 培训指南 | 新手入门 |

---

## design/ - 设计文档

设计文档和提案。

| 文件 | 议题 | 说明 |
|------|------|------|
| `design-v0.md` | 设计 v0 | 初始设计 |
| `design-v1.md` | 设计 v1 | 设计迭代 |
| `design-v3.1.md` | 设计 v3.1 | v3.1 版本设计 |

---

## archive/ - 归档 (过期版本)

过期的文档，不再维护，仅供历史参考。

| 目录 | 内容 |
|------|------|
| `v3.8/` | DSL v3.8 规范 (过期) |
| `v3.7/` | DSL v3.7 规范 (过期) |
| `v3.6/` | DSL v3.6 规范 (过期) |
| `v3.5/` | DSL v3.5 规范 (过期) |
| `v3.4/` | DSL v3.4 规范 (过期) |
| `v3.3/` | DSL v3.3 规范 (过期) |
| `v3.2/` | DSL v3.2 规范 (过期) |
| `v3.1/` | DSL v3.1 规范 (过期) |
| `v3.0/` | DSL v3.0 规范 (过期) |
| `v2.3/` | DSL v2.3 规范 (过期) |
| `adr/` | 归档 ADR (13 个, 2026-06-12) — 见 [archive/adr/README.md](archive/adr/README.md) |
| `specs/` | 归档 Spec (Phase 2 标准库 v1.0, 2026-06-12) — 见 [archive/specs/phase2-standard-library-v1.0.md](archive/specs/phase2-standard-library-v1.0.md) |
| `compiler/` | SKILL Compiler 预研设计 (2026-05-24,设计已决但未实施;2026-07-06 归档) — 见 [archive/compiler/README.md](archive/compiler/README.md) |

**看板归档 (2026-07-07)**：
- `archive/roadmap-status.md` — Phase 0-4 Sprint 日志看板 (已过期，被 `active-status.md` 替代)
- `archive/implementation-roadmap.md` — 2026-06-03 旧实施路线图 (已过期，被 master plan + active-status.md 替代)

**其他归档文件**：
- `AgenticDSL_LibSpec_v1.1.md` - 旧库规范 (过期)
- `AgenticDSL_SystemPrompt_v3.6.md` - 旧 System Prompt (过期)
- `Roadmap.md` - 旧路线图 (过期)
- `brain-thinking-spec.md` - 旧思考规范 (过期)
- ~~`AgenticDSL whitepaper.md`~~ (2026-07-06 删除,内容近似空)
- ~~`Application_guide.md`~~ (2026-07-06 删除,内容近似空)

---

## proposals/ - AgenticDSL 语言演进提案

> **与 docs/adr/ 和 docs/specs/ 的关系**：`docs/adr/` 记录引擎实现决策，`docs/specs/` 描定当前引擎行为（v3.10）；
> `docs/proposals/` 记录**语言演进提案**，讨论 AgenticDSL 应该往哪个方向演化及其实现路径。
>
> 注:此目录原位于 `docs/adr/agenticdsl/`(2026-06-12 升级为顶层目录,语义边界更清晰)。

文档组织按**话题领域**（而非文档类型），共 14 个子目录：

| 目录 | 话题 | 关联现有文档 |
|------|------|------------|
| `vision/` | 自举愿景与演进路线图 | [specs/dsl.md](specs/dsl.md), [specs/architecture.md](specs/architecture.md) |
| `skill-system/` | 技能分类体系、invoke/compose 语法、当前 6 技能映射（规划 39） | [examples/skill_porting/skills/](../../examples/skill_porting/skills/), [adr/adr-0009](../adr/adr-0009-dsl-standard-library.md) |
| `session-state/` | 四层隔离模型、ModuleState/Yield/Fork 语义、Oracle 问答 | [adr/adr-0014](../adr/adr-0014-conversation-context.md), [adr/adr-0008](../adr/adr-0008-structured-context.md) |
| `inference-stdlib/` | 推理标准库接口设计与子图规格 | [adr/adr-0001](../adr/adr-0001-illm-provider-streaming-interface.md), [specs/stdlib-v3.10.md](specs/stdlib-v3.10.md) |
| `language-extensions/` | 类型系统、模块命名空间、标准库扩展 | [specs/dsl.md](specs/dsl.md), [specs/stdlib-v3.10.md](specs/stdlib-v3.10.md) |
| `implementation-roadmap/` | 6 步实施计划与代码映射 | [src/](../../src/) |
| `research/` | 推理引擎调研报告（vLLM/SGLang/llama.cpp） | [adr/adr-0001](../adr/adr-0001-illm-provider-streaming-interface.md) |
| `architecture/` | 推理架构、路由器、质量评估器设计 | [adr/adr-0001](../adr/adr-0001-illm-provider-streaming-interface.md), [adr/adr-0008](../adr/adr-0008-structured-context.md) |
| `optimization/` | 推理优化方向方案（6 维度） | — |
| `implementation/` | 自举实施路径、阶段 0 实施方案 | [adr/adr-0001](../adr/adr-0001-illm-provider-streaming-interface.md), [adr/adr-0005](../adr/adr-0005-llm-backend-config-factory.md) |
| `testing/` | 测试策略（金字塔、Mock 策略、CI） | — |
| `api/` | CloudLLMAdapter API 设计 | [adr/adr-0001](../adr/adr-0001-illm-provider-streaming-interface.md), [adr/adr-0005](../adr/adr-0005-llm-backend-config-factory.md) |
| `operations/` | 安全规范、性能基准 | [adr/adr-0004](../adr/adr-0004-toolregistry-security.md) |

详细索引见 [proposals/README.md](proposals/README.md)。

---

## superpowers/ - superpowers plans 目录

> **2026-06-24 更新**：`docs/superpowers/plans/2026-06-22-sprint7-scheduler-pipeline-tightened.md`
> (668 行) 已通过 `git mv` 移至 `docs/archive/superpowers/plans/`。该 plan 是 Sprint 7 启动
> 时的执行计划,已 ship + 延展至 Sprint 8 + Sprint 9 step 1,本计划已不再 active。
>
> **2026-06-25 更新**: 目录中保留 2 个 active 跟踪 plan (均 ship + 归档闭环相关):
> - `2026-06-24-tech-debt-full-closure.md` (1769 行) — 13 步全路径 plan (阶段 A+B 100% + 阶段 C handoff 至 `2026-06-24-engine-include-final-decoupling`),已 ship
> - `2026-06-24-engine-include-final-decoupling.md` (895 行) — 6.3.x 收官 plan (engine.cpp includes 10→3 + 3 engine_factory tests),已 ship + change 已 archive
>
> **2026-07-06 更新** (OpenSpec change `docs-cleanup-phase-2`):5 个已 ship plan 同步归档至 `archive/superpowers/plans/`:
> - `2026-06-24-engine-include-final-decoupling.md` (Sprint 6 已 ship + change 已 archive)
> - `2026-06-24-tech-debt-full-closure.md` (Sprint 6 已 ship)
> - `2026-06-25-sprint-10-pre-existing-sanitizer-cleanup.md` (Sprint 10 已 ship + change 已 archive)
> - `2026-07-02-c7-model-router-mvp.md` (Sprint 17 Phase 1 MVP 已 ship,被 Phase 2 超集覆盖)
> - `2026-07-02-c7-phase2-model-router-plugin.md` (Sprint 17 Phase 2 已完整 ship,ADR-0034 ✅ Approved + 61/61 ctest)
>
> 后续 superpowers plans 由各 Sprint 启动时按需创建。
>
> **历史归档(2026-06-03)**:`docs/superpowers/` 原 3 个文件已归档至 `docs/archive/superpowers/`:
> - `specs/2026-05-12-dsl-standard-library-design.md` → 已被 `docs/adr/adr-0009-dsl-standard-library.md` 取代
> - `specs/2026-05-13-memory-state-interface-design.md` → 已被 `docs/adr/adr-0010-memory-system.md` 取代
> - `plans/2026-06-02-test-fixes-for-prephase.md` → 7 任务已全部执行,12 个测试 100% 通过

---

## 文档更新记录

| 日期 | 更新内容 |
|------|---------|
| 2026-05-20 | 新增 agenticdsl/ 目录（16 篇语言演进文档），按话题组织 |
| 2026-06-12 | 提升 agenticdsl/ 为 docs/proposals/，明确与 docs/adr/ 语义边界 |
| 2026-05-23 | 扩展至 30+ 篇文档，新增 research/architecture/optimization/implementation/testing/api/operations 7 个目录 |
| 2026-06-08 | C1 迁移：ADR-0019/0020/0023 状态更新；ADR-0030~0036 补录；`dsl.md`/`dsl-lib.md` 版本升至 v3.10 |
| 2026-06-12 | Stage 2 / Task 7：归档 13 个已废弃 ADR 到 `docs/archive/adr/`；移除 `phase-2-memory/`, `phase-3-reasoning/`, `phase-5-async/`, `phase-5-policy/`, `phase-7-router/`, `phase-8-kernel/` 6 个空目录 |
| 2026-06-12 | Stage 2 / Task 8：合并 `dsl-lib.md` + `stdlib.md` 为 `stdlib-v3.10.md`；归档 `phase2-standard-library.md` 到 `docs/archive/specs/` |
| 2026-06-12 | Stage 2 / Task 9：合并 `memory.md` (MEP-001 v3.2 Draft) + `dsl.md` §10.3 为 `memory-v3.10.md`；`memory.md` 已删除 |