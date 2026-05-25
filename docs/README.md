# HydraForge 文档索引

## 目录结构

```
docs/
├── adr/              # Architecture Decision Records (架构决策)
├── specs/            # 规范文档 (当前有效版本)
├── guides/           # 用户和开发者指南
├── design/           # 设计文档
├── archive/          # 归档 (过期版本)
└── superpowers/      # Superpowers skill 工件
```

---

## adr/ - Architecture Decision Records

架构决策记录，记录重要的架构决策及其背景、权衡。

| 文件 | 议题 | 状态 |
|------|------|------|
| `adr-0001-illm-provider-streaming-interface.md` | ILLMProvider 流式接口 | ✅ 已批准 |
| `adr-0002-eventbus-bounded-queue.md` | EventBus 有界队列 | ✅ 已批准 |
| `adr-0003-dslengine-thread-safety.md` | DSLEngine 线程安全 | ✅ 已批准 |
| `adr-0004-toolregistry-security.md` | ToolRegistry 安全模型 | ✅ 已批准 |
| `adr-0005-llm-backend-config-factory.md` | LLM 后端配置与工厂 | ✅ 已批准 |
| `adr-0006-harness-engine-thread-model.md` | HarnessEngine 后台线程 | ✅ 已批准 |
| `adr-0007-context-compression.md` | 上下文压缩机制 | ✅ 已批准 |
| `adr-0008-structured-context.md` | 结构化 Context | ✅ 已批准 |
| `adr-0009-dsl-standard-library.md` | DSL 标准库规划 | ✅ 已批准 |
| `adr-0010-memory-system.md` | 记忆系统标准接口 | ✅ 已批准 |
| `adr-0011-knowledge-graph.md` | 知识图谱与 Meta-KG 导航 | ✅ 已批准 |
| `adr-0012-vector-memory.md` | 向量语义记忆 | ✅ 已批准 |
| `adr-0013-user-profile.md` | 用户画像管理 | ✅ 已批准 |
| `adr-0014-conversation-context.md` | 对话上下文隔离 | ✅ 已批准 |
| `adr-0015-iper-loop.md` | IPER 闭环推理 | ✅ 已批准 |
| `adr-0016-try-catch.md` | 异常自动快照回溯 | ✅ 已批准 |
| `adr-0017-counterfactual.md` | 反事实推理 | ✅ 已批准 |
| `adr-0018-graph-guided.md` | 图引导假设生成 | ✅ 已批准 |

---

## specs/ - 规范文档 (当前有效版本)

核心规范文档，定义系统行为。

| 文件 | 议题 | 说明 |
|------|------|------|
| `architecture.md` | AgenticOS 架构 | 8 层架构定义 |
| `layer0.md` | L0 运行时规范 | DSL 引擎核心行为 |
| `layer0-refactor.md` | L0 重构计划 | Layer0 重构计划 |
| `dsl.md` | DSL 规范 v3.9 | 最新 DSL 语言规范 |
| `dsl-lib.md` | DSL 库规范 | DSL 子图和工具库 |
| `phase2-standard-library.md` | Phase 2 标准库规划 | ADR-0010~0018 子图清单 |
| `stdlib.md` | 标准库规范 | 内置工具和子图 |
| `memory.md` | 记忆系统 | 上下文和记忆管理 |

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

**其他归档文件**：
- `AgenticDSL whitepaper.md` - 白皮书 (过期)
- `AgenticDSL_LibSpec_v1.1.md` - 旧库规范 (过期)
- `AgenticDSL_SystemPrompt_v3.6.md` - 旧 System Prompt (过期)
- `Roadmap.md` - 旧路线图 (过期)
- `Application_guide.md` - 旧应用指南 (过期)
- `brain-thinking-spec.md` - 旧思考规范 (过期)

---

## agenticdsl/ - AgenticDSL 语言演进文档

> **与 docs/adr/ 和 docs/specs/ 的关系**：`docs/adr/` 记录引擎实现决策，`docs/specs/` 描定当前引擎行为（v3.10）；
> `docs/agenticdsl/` 记录**语言演进提案**，讨论 AgenticDSL 应该往哪个方向演化及其实现路径。

文档组织按**话题领域**（而非文档类型），共 13 个子目录：

| 目录 | 话题 | 关联现有文档 |
|------|------|------------|
| `vision/` | 自举愿景与演进路线图 | [specs/dsl.md](specs/dsl.md), [specs/architecture.md](specs/architecture.md) |
| `skill-system/` | 技能分类体系、invoke/compose 语法、39 技能全量映射 | [superpowers/](superpowers/), [adr/adr-0009](adr/adr-0009-dsl-standard-library.md) |
| `session-state/` | 四层隔离模型、ModuleState/Yield/Fork 语义、Oracle 问答 | [adr/adr-0014](adr/adr-0014-conversation-context.md), [adr/adr-0008](adr/adr-0008-structured-context.md) |
| `inference-stdlib/` | 推理标准库接口设计与子图规格 | [adr/adr-0001](adr/adr-0001-illm-provider-streaming-interface.md), [specs/dsl-lib.md](specs/dsl-lib.md) |
| `language-extensions/` | 类型系统、模块命名空间、标准库扩展 | [specs/dsl.md](specs/dsl.md), [specs/dsl-lib.md](specs/dsl-lib.md) |
| `implementation-roadmap/` | 6 步实施计划与代码映射 | [src/](../src/) |
| `research/` | 推理引擎调研报告（vLLM/SGLang/llama.cpp） | [adr/adr-0001](adr/adr-0001-illm-provider-streaming-interface.md) |
| `architecture/` | 推理架构、路由器、质量评估器设计 | [adr/adr-0001](adr/adr-0001-illm-provider-streaming-interface.md), [adr/adr-0008](adr/adr-0008-structured-context.md) |
| `optimization/` | 推理优化方向方案（6 维度） | — |
| `implementation/` | 自举实施路径、阶段 0 实施方案 | [adr/adr-0001](adr/adr-0001-illm-provider-streaming-interface.md), [adr/adr-0005](adr/adr-0005-llm-backend-config-factory.md) |
| `testing/` | 测试策略（金字塔、Mock 策略、CI） | — |
| `api/` | CloudLLMAdapter API 设计 | [adr/adr-0001](adr/adr-0001-illm-provider-streaming-interface.md), [adr/adr-0005](adr/adr-0005-llm-backend-config-factory.md) |
| `operations/` | 安全规范、性能基准 | [adr/adr-0004](adr/adr-0004-toolregistry-security.md) |

详细索引见 [agenticdsl/README.md](agenticdsl/README.md)。

---

## superpowers/ - Superpowers Skill 工件

参见 `superpowers/` 目录内部索引。

---

## 文档更新记录

| 日期 | 更新内容 |
|------|---------|
| 2026-05-20 | 新增 agenticdsl/ 目录（16 篇语言演进文档），按话题组织 |
| 2026-05-23 | 扩展至 30+ 篇文档，新增 research/architecture/optimization/implementation/testing/api/operations 7 个目录 |