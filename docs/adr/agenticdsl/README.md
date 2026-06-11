# AgenticDSL 文档索引

> 本文档完整记录 AgenticDSL 从当前工作流 DAG 引擎到自举系统演进的
> 愿景、架构决策、实施计划和语言规范。

---

## 如何阅读：推荐的文档顺序

```
新读者从这里开始：
1. vision/        → 理解"为什么做"和"做到什么样"
2. skill-system/  → 掌握核心抽象：技能分类体系
3. session-state/ → 理解状态隔离和并发模型
4. inference-stdlib/ → 推理控制面的 DSL 化
5. language-extensions/ → 未来的语言能力扩展
6. implementation-roadmap/ → 开始实施前阅读
```

---

## 文档结构：按话题组织（而非按文档类型）

```
docs/agenticdsl/
├── README.md                                ← 本文档
├── AGENTICDSL_ENHANCEMENT_ROADMAP.md        ← 增强路线图（早期文档，保留）
├── AGENTICDSL_SKILL_PROGRAMMING_GUIDE.md    ← Skill 编程指南（早期文档，保留）
│
├── vision/                    # 愿景 —— 为什么做、做到什么样
├── skill-system/              # 技能体系 —— 分类、调用、映射
├── session-state/             # 会话与状态 —— 隔离、模块、yield
├── inference-stdlib/          # 推理标准库 —— LLM 推理的 DSL 控制面
├── language-extensions/       # 语言扩展 —— 类型、模块、标准库
├── implementation-roadmap/    # 实施路线 —— 分步计划、代码映射
├── research/                  # 调研报告 —— 推理引擎深度调研
├── architecture/              # 架构设计 —— 推理架构、路由器、质量评估器
├── optimization/              # 优化方案 —— 推理优化策略
├── implementation/            # 实施计划 —— 自举路径、阶段 0 实施方案
├── testing/                   # 测试策略 —— 测试金字塔、Mock、CI
├── api/                       # API 设计 —— CloudLLMAdapter 接口
└── operations/                # 运维规范 —— 安全规范、性能基准
```

---

## 文档清单

### Vision（愿景）

| # | 标题 | 描述 | 关联现存文档 |
|---|------|------|------------|
| 01 | [自举愿景](vision/01-self-bootstrapping-vision.md) | AgenticDSL 四阶段自举链路，与传统语言的根本区别 | [specs/dsl.md](../specs/dsl.md) |
| 02 | [语言演进路线图](vision/02-language-evolution-roadmap.md) | 三阶段演进目标，每个里程碑的能力和标志 | [specs/layer0.md](../specs/layer0.md) |

### Skill System（技能体系）

| # | 标题 | 描述 | 关联现存文档 |
|---|------|------|------------|
| 01 | [Skill 分类体系](skill-system/01-taxonomy.md) | 5 维度分类，跨轴处理，分类判断流程 | [examples/skill_porting/skills/](../../../examples/skill_porting/skills/) |
| 02 | [Skill Invoke/Compose](skill-system/02-invoke-compose.md) | skill_invoke / skill_compose 节点语法定义 | [specs/dsl.md](../specs/dsl.md) |
| 03 | [Skill 全量映射表](skill-system/03-taxonomy-mapping.md) | 当前 6 技能（规划 39）的 5 维度映射 + 触发词 | [examples/skill_porting/skills/](../../../examples/skill_porting/skills/) |
| 04 | [Skill 编译器设计](skill-system/04-skill-compiler-design.md) | SKILL.md → .agent.md 模板驱动的编译映射方案 | [SKILL.md files](../../../.opencode/skills/) |

### Session & State（会话与状态）

| # | 标题 | 描述 | 关联现存文档 |
|---|------|------|------------|
| 01 | [Session 四层隔离模型](session-state/01-isolation-model.md) | Global/Session/Module/Execution 四层定义 | [adr-0014](../adr/adr-0014-conversation-context.md) |
| 02 | [Session 内部状态模型](session-state/02-internal-state-model.md) | ModuleState + Yield/Stream + Fork 语义 | [adr-0008](../adr/adr-0008-structured-context.md) |
| 03 | [Oracle 问答实录](session-state/03-oracle-qa.md) | 6 个架构问题的 Oracle 完整回答 | — |

### Inference Standard Library（推理标准库）

| # | 标题 | 描述 | 关联现存文档 |
|---|------|------|------------|
| 01 | [推理标准库接口设计](inference-stdlib/01-interface-design.md) | 控制面/实现面分离，工具接口清单 | [adr-0001](../adr/adr-0001-illm-provider-streaming-interface.md) |
| 02 | [推理标准库规格](inference-stdlib/02-specification.md) | lib/inference/ 每个子图的完整规格 | [specs/dsl-lib.md](../specs/dsl-lib.md) |

### Language Extensions（语言扩展）

| # | 标题 | 描述 | 关联现存文档 |
|---|------|------|------------|
| 01 | [类型系统扩展](language-extensions/01-type-system.md) | 从无类型到结构化类型的渐进路径 | [specs/dsl.md](../specs/dsl.md) §4 |
| 02 | [模块与命名空间](language-extensions/02-module-namespace.md) | package/module/import/export 语法提案 | [specs/dsl.md](../specs/dsl.md) 路径约定 |
| 03 | [标准库扩展清单](language-extensions/03-standard-library.md) | 三批扩展计划 + 编写规范 | [specs/dsl-lib.md](../specs/dsl-lib.md) |

### Implementation Roadmap（实施路线）

| # | 标题 | 描述 | 关联现存文档 |
|---|------|------|------------|
| 01 | [增量实现路线图](implementation-roadmap/01-roadmap.md) | 6 步计划、3 条路径、时间估算 | [src/](../../src/) |
| 02 | [代码扩展点映射](implementation-roadmap/02-code-mapping.md) | 每个文件的具体改动位置 | [src/](../../src/) |

### Research（调研报告）

| # | 标题 | 描述 | 关联现存文档 |
|---|------|------|------------|
| 01 | [推理引擎调研报告](research/inference-engine-research.md) | vLLM/SGLang/llama.cpp 深度调研 | [adr-0001](../adr/adr-0001-illm-provider-streaming-interface.md) |

### Architecture（架构设计）

| # | 标题 | 描述 | 关联现存文档 |
|---|------|------|------------|
| 01 | [总体推理架构](architecture/inference-architecture.md) | 分层架构、核心组件 | [adr-0001](../adr/adr-0001-illm-provider-streaming-interface.md) |
| 02 | [推理路由器](architecture/inference-router.md) | 云端/本地路由决策、回退机制 | — |
| 03 | [质量评估器](architecture/quality-evaluator.md) | 快速规则+深度评估混合方案 | [adr-0008](../adr/adr-0008-structured-context.md) |

### Optimization（优化方案）

| # | 标题 | 描述 | 关联现存文档 |
|---|------|------|------------|
| 01 | [推理优化策略](optimization/inference-optimization-strategies.md) | 6 维度优化方案 | — |

### Implementation（实施计划）

| # | 标题 | 描述 | 关联现存文档 |
|---|------|------|------------|
| 01 | [自举实施路径](implementation/self-bootstrapping-path.md) | 4 阶段自举计划 | [adr-0001](../adr/adr-0001-illm-provider-streaming-interface.md) |
| 02 | [阶段 0 实施方案](implementation/phase-0-implementation.md) | 接口统一→CloudLLMAdapter→LLMRouter | [adr-0001](../adr/adr-0001-illm-provider-streaming-interface.md), [adr-0005](../adr/adr-0005-llm-backend-config-factory.md) |

### Testing（测试策略）

| # | 标题 | 描述 | 关联现存文档 |
|---|------|------|------------|
| 01 | [测试策略](testing/test-strategy.md) | 测试金字塔、Mock 策略、CI 工作流 | — |

### API（API 设计）

| # | 标题 | 描述 | 关联现存文档 |
|---|------|------|------------|
| 01 | [CloudLLMAdapter API](api/cloud-llm-adapter.md) | OpenAI/Anthropic 适配器接口 | [adr-0001](../adr/adr-0001-illm-provider-streaming-interface.md), [adr-0005](../adr/adr-0005-llm-backend-config-factory.md) |

### Operations（运维规范）

| # | 标题 | 描述 | 关联现存文档 |
|---|------|------|------------|
| 01 | [安全规范](operations/security.md) | API Key 管理、数据隐私、输入验证 | [adr-0004](../adr/adr-0004-toolregistry-security.md) |
| 02 | [性能基准](operations/performance-benchmark.md) | 延迟/吞吐量/质量指标体系 | — |

---

## 与现有 docs/ 的关系

### `docs/adr/`（引擎级 ADR）vs `docs/agenticdsl/`（语言演进）

| 维度 | `docs/adr/` | `docs/agenticdsl/` |
|------|-------------|-------------------|
| 层次 | 引擎实施级 | 语言设计级 |
| 状态 | ✅ 已批准，已实现 | 📝 草案，待讨论 |
| 编号 | `adr-0001` ~ `adr-0018`（顺序编号） | 按话题编号（`skill-system/01`, `session-state/02`） |
| 变更方式 | 通过新 ADR 修订 | 通过讨论修订 |
| 读者 | C++ 开发者 | 架构师 + Agent 开发者 |
| 关系 | 下层约束 — agenticdsl 设计不得破坏现有 adr | 上层提案 — 依赖现有 adr 提供的基础能力 |

**一句话**：`docs/adr/` 说"引擎是如何实现的"；`docs/agenticdsl/` 说"语言应该往哪个方向演化"。

### `docs/specs/`（引擎规约）vs `docs/agenticdsl/language-extensions/`（语言扩展）

| 维度 | `docs/specs/` | `docs/agenticdsl/language-extensions/` |
|------|---------------|----------------------------------------|
| 时效 | 当前有效（v3.10） | 未来提案 |
| 内容 | DSL 规范、标准库规约、架构定义 | 类型系统、模块命名空间、标准库扩展 |
| 稳定性 | 稳定，被代码引用 | 草案，待讨论 |
| 向后兼容 | — | 必须兼容 `specs/dsl.md` |

**一句话**：`docs/specs/` 是**基线**（现在能做什么）；`docs/agenticdsl/language-extensions/` 是**增量**（将来能做什么）。

---

## 阅读顺序建议

```
[愿景] vision/01 → vision/02
  ↓
[技能] skill-system/01 → skill-system/02 → skill-system/03 → skill-system/04
  ↓
[状态] session-state/01 → session-state/02 → session-state/03
  ↓
[推理] inference-stdlib/01 → inference-stdlib/02
  ↓
[语言] language-extensions/01 → language-extensions/02 → language-extensions/03
  ↓
[路线图] implementation-roadmap/01 → implementation-roadmap/02
  ↓
[调研] research/01
  ↓
[架构] architecture/01 → architecture/02 → architecture/03
  ↓
[优化] optimization/01
  ↓
[实施] implementation/01 → implementation/02
  ↓
[测试] testing/01
  ↓
[API] api/01
  ↓
[运维] operations/01 → operations/02
```

## 文档状态说明

- **草案**：内容已填写，待审核确认
- **已批准**：内容经讨论确认，进入实施阶段
- **已废弃**：已被后续文档替代
