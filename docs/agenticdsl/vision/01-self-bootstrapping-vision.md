# VN-001: AgenticDSL 自举愿景

**ID**: VN-001
**日期**: 2026-05-20
**状态**: 讨论中（初步确认方向，待阶段边界细化）
**关联**: VN-002, ADR-006, IP-001
**调研依据**: Oracle 架构评估（bg_d56cae06）、代码库基线分析（bg_eefd93cf, bg_20df819e）、语言自举先例研究（bg_9fc5400b）、推理控制面模式研究（bg_421ff221）

---

## 自举定义

**自举**：通过 AgenticDSL 驱动推理计算图，驱动推理输出，而推理的输出内容的质量可以持续驱动 AgenticDSL 运行时工作。

**自举后目标**：AgenticDSL 运行时提供推理服务，以 MCP 或 OpenAI/Anthropic 兼容接口提供推理 API 服务。服务分层：低质量用本地推理，高质量继续使用云端输出。

## 核心命题

AgenticDSL 可以超越传统编程语言，不是因为语法更优雅或性能更好，而是因为它的执行模型是 **Agent 驱动的**。Agent 可以：

1. **编写 AgenticDSL 代码** — Agent 生成 .agent.md 工作流
2. **改进 AgenticDSL 语言自身** — Agent 生成新 Skill 注册到运行时
3. **可编程推理策略** — 推理参数从 C++ 硬编码变为 DSL 工作流动态编排
4. **持续自进化** — Oracle 监控执行 → 发现瓶颈 → 生成优化 → 注册新 Skill

这是 LLM 时代的 **Forth Bootstrap**：

```
Forth (1960s):
  机器码 → Forth解释器(42字节) → Forth编译器 → 完整OS

AgenticDSL (2020s):
  硬编码参数 → AgenticDSL运行时 → 可编程推理策略 → 持续自进化
                                    ↓
                             Oracle监控 → 生成新Skill → 更优系统
                                    ↓
                              持续进化 → 超越传统语言
```

> ⚠️ 注意（C1 后 2026-06-08 已更新）：C1 迁移后 LLM 访问统一通过 `ILLMProvider` 接口
> （ADR-0001），调用链为 `DSL → ILLMProvider → (CloudLLMAdapter | LlamaAdapterProvider → LlamaAdapter
> | MockLLMProvider)`。阶段演进的核心是从 **"硬编码推理参数"** 到 **"DSL 动态控制推理参数"**。
> 同时需要接入云端 LLM 作为老师模型，确保推理质量。

---

## 四阶段自举链路

### 阶段 0：硬编码参数（当前）

```
云端 LLM (老师模型) ──→ AgenticDSL Runtime ──→ AgenticDSL Workflows
      │                                        │
      │                                        ▼
      │                                  ILLMProvider
      │                                        │
      └──────────────────┬─────────────────────┼──────────────┐
                         ▼                     ▼              ▼
                   CloudLLMAdapter     LlamaAdapterProvider   MockLLMProvider
                         │                     │
                         ▼                     ▼
                      HTTP               LlamaAdapter ──→ llama.cpp (本地推理, 低质量)
```

> C1 后 (2026-06-08) 已实现：`ILLMProvider` 流式接口（ADR-0001），云端走
> `CloudLLMAdapter`，本地走 `LlamaAdapterProvider → LlamaAdapter`。

- 推理通过 `ILLMProvider` 接口调用（C1 后 2026-06-08）：本地走 `LlamaAdapterProvider → LlamaAdapter` 直连 llama.cpp C API；云端走 `CloudLLMAdapter` 调 HTTP
- 推理参数（temperature, seed 等）在 C++ 代码或配置文件中硬编码
- Agent 工作流可以调用推理，但 **不能动态调整推理策略**
- **云端 LLM 作为老师模型**：提供高质量推理输出，指导 AgenticDSL 工作
- 当前架构完全可工作

### 阶段 1：可编程推理策略 + 云端集成

```
云端 LLM (老师模型) ──→ AgenticDSL Runtime
      │                        │
      │                        ▼
      │              推理标准库 (lib/inference/*.md)
      │                ├── engine.md      引擎生命周期
      │                ├── model.md       模型管理
      │                ├── session.md     推理会话
      │                ├── sampling.md    采样参数
      │                ├── kv_cache.md    KV-cache 策略
      │                └── memory.md      内存配置
      │                        │
      │                        ▼
      │              ILLMProvider (参数来自 DSL, C1 后 2026-06-08)
      │                        │
      └──────────────────────→ 本地推理 (低质量)
```

- **云端 LLM 深度集成**：作为默认推理后端，确保输出质量
- **推理标准库暴露策略控制面**：每个子图封装一个决策维度
- **实现面**（CUDA kernel launch、page table 管理）仍隐藏在 C++ tool 内部
- Agent 根据 workload 特征选择最优策略组合
- **调用路径**：云端 LLM 为主，本地 llama.cpp 为辅助/降级

### 阶段 2：Agent 编排推理 + 质量评估闭环

```
云端 LLM (老师模型) ──→ AgenticDSL Runtime ◄─── Oracle 监控链路
      │                        │
      │                        ▼
      │              推理标准库 (lib/inference/)
      │                        │
      │                        ▼
      │              质量评估节点 (assert + on_failure)
      │                        │
      │              ┌─────────┴─────────┐
      │              ▼                   ▼
      │        高质量输出          低质量输出
      │              │                   │
      │              ▼                   ▼
      │        直接使用          触发策略调整
      │                                │
      └──────────────────────────────→ 本地推理 (降级)
```

- **Agent 工作流决定"如何推理"**：编排子图、选择策略、调整参数
- **质量评估闭环**：DSL 内嵌 `assert` 节点评估输出质量，触发策略调整
- **混合评估**：快速规则评估（DSL 内嵌）+ 深度质量评估（云端老师模型）
- **服务分层**：高质量任务 → 云端，低质量任务 → 本地 llama.cpp
- Oracle 监控全程执行，收集性能数据供后续自进化使用

### 阶段 3：持续自进化 + 服务化

```
Oracle 监控循环（oracle_background.agent.md）：
  观察执行 → 分析瓶颈 → 生成新 Skill → 注册到运行时 → 验证效果 → 循环
        │
        ▼
AgenticDSL Runtime 提供推理 API 服务
  ├── MCP 接口
  ├── OpenAI/Anthropic 兼容接口
  └── 服务分层：
      ├── 高质量服务（云端 LLM）
      └── 低质量服务（本地 llama.cpp）
```

- Oracle 以 background 进程形式持续运行
- 发现性能瓶颈时自动生成优化 Skill
- 新 Skill 通过 A/B 测试验证后自动部署
- **系统提供推理 API 服务**：MCP 或 OpenAI/Anthropic 兼容接口
- **服务分层**：根据质量要求自动选择云端或本地后端
- 系统持续自优化，无需人为干预

---

## 与传统语言的根本区别

| 维度 | 传统语言（Python/Rust/C++） | AgenticDSL |
|------|---------------------------|------------|
| 编程者 | 人类 | Agent |
| 代码生成 | 人类手写 | Agent 根据上下文生成 |
| 类型系统 | 编译时 | 运行时 + LLM 推断 |
| 性能优化 | 手动（profiling → 改代码） | 自动（Oracle → 新 Skill） |
| 语言演进 | 委员会 / RFC | Agent 自行扩展 |
| 错误修复 | 人类 debug | Agent 分析 trace → 自修复 |
| 部署 | 编译/打包 | .agent.md 热加载 |

---

## 成功标准

```
阶段0 → 阶段1:  云端 LLM 深度集成，推理标准库（lib/inference/）可被 Agent 工作流调用，
               推理参数从 C++ 硬编码变为 DSL 可编程
阶段1 → 阶段2:  Agent 工作流成功动态编排推理策略，质量评估闭环建立，
               服务分层（云端/本地）正常工作
阶段2 → 阶段3:  Oracle 监控循环至少完成一次"发现→生成→部署"闭环，
               系统提供推理 API 服务（MCP/OpenAI 兼容）
```

## 服务分层标准

| 维度 | 云端 LLM（高质量） | 本地 llama.cpp（低质量） |
|------|-------------------|------------------------|
| **任务类型** | 代码生成、复杂推理、创意写作 | 简单查询、数据提取、模板填充 |
| **输入复杂度** | 长上下文、多步骤、需要深度理解 | 短上下文、单步骤、模式匹配 |
| **质量要求** | 高（准确率 > 95%） | 中（准确率 > 80%） |
| **延迟要求** | 可接受较高延迟（< 5s） | 低延迟（< 1s） |
| **成本敏感** | 不敏感 | 敏感 |
| **隐私要求** | 可接受数据上云 | 必须本地处理 |

---

## 开放问题

- Q: 阶段3 中 Agent 生成的新 Skill 是否需要人工审核？
- Q: 自举完成后如何保证系统不退化（不出现"AI 写的代码 AI 看不懂"）？
- Q: 自举链路的回退策略是什么？

---

## 调研验证

### Oracle 架构评估（核心结论）

| 问号 | Oracle 结论 |
|------|------------|
| 自举计划可信吗？ | ✅ **可行。关键区分："编译器自举" vs "语言生态自举"** — C++ 运行时 + CUDA kernel 构成"自举平台"（如同硬件指令集之于 Forth），推理标准库是 DSL 的"微码层"。自举核心是参数从硬编码变为 DSL 可编程，而非重写 C++ 运行时。 |
| 推理调用路径：HTTP 还是库函数直调？ | ✅ **库函数直调是正解** — C1 迁移（2026-06-08）后本地路径为 `ILLMProvider → LlamaAdapterProvider → LlamaAdapter`（编译时链接 llama.cpp C API）；云端路径为 `ILLMProvider → CloudLLMAdapter`（HTTP）。对内永远走库函数直调（零拷贝、低延迟、原生流式）。HTTP 仅为**可选的外层**——需要对外暴露推理能力或对接云端时才加。 |
| 阶段2 范围风险？ | ⚠️ **核心风险不是 HTTP，是范围蔓延** — 重新定义后的阶段2（Agent 编排推理策略）比原版（HTTP 服务）更聚焦、更可行。但仍需警惕"为了让 Agent 编排得更灵活，不断往 DSL 里加能力"的倾向。 |
| 阶段3 (自进化) 最大风险？ | ⚠️ **"Oracle 验证 Oracle"的自指问题** — Agent 生成的新 Skill 需要人在回路中做最终批准。局部最优陷阱和反馈循环发散是经典失效模式。 |
| 最关键的建议？ | 🚨 **按边界切分，逐步推进** — 库函数直调（已有）→ 推理标准库（新增子图）→ Agent 编排（新增能力）→ 自进化（远期）。每一步都是纯增量，不破坏现有功能。 |

### 代码库基线验证（2026-05-20）

通过 4 个 explore 代理对代码库的深度审计，确认了当前能力基线：

| 能力维度 | 当前状态 | 自举所需 | 差距评级 |
|---------|---------|---------|---------|
| lib/ 标准库 | 4 目录，~5 个 .md 文件（auth/human/math/utils） | 推理标准库 8+ 子图 | 🔴 需要全新扩展 |
| Session 管理 | ExecutionSession 单次 run()，用完即弃 | 持久 Session 生命周期 + SessionRegistry | 🔴 需要新建 |
| 状态隔离 | Context = 平面 nlohmann::json | 4 层隔离 + ModuleState | 🔴 差距大 |
| 线程模型 | 单线程无锁，DSLEngine 多实例 | 并发推理请求处理 | 🟡 需要改造 |
| YIELD/Stream | 不存在 | YIELD 节点类型 + 状态机调度 | 🟡 全新能力 |
| Fork | 有模拟基础（start_fork_simulation/execute_fork_branches/complete_fork） | 无 COW，无 per-field 声明 | 🟢 可扩展 |
| 类型系统 | 无 | 签名校验 + 可选类型标注 | 🟡 纯新增 |
| 工具注册 | ToolRegistry 是 DSLEngine 成员（非单例） | 推理工具注入 | 🟢 基础设施就绪 |
| LLM 接口 | `ILLMProvider` 流式接口（ADR-0001, C1 后 2026-06-08），本地走 `LlamaAdapterProvider → LlamaAdapter` 封装 llama.cpp，云端走 `CloudLLMAdapter` | tool_call 包装的推理工具 | 🟢 已有基础 |

**积极信号**：ToolRegistry 是成员而非全局单例、已有 fork 模拟基础设施、`ILLMProvider` 接口（C1 后 2026-06-08）已统一 LLM 访问、`LlamaAdapter` / `CloudLLMAdapter` 已就绪 — 说明部分基础设施就位，阶段0→1 的距离小于预期。

### 历史先例对比

| 先例 | 核心模式 | AgenticDSL 类比 |
|------|---------|----------------|
| **Forth 自举**（lbForth, Planckforth） | 12 个原子 primitives（C 实现）→ 用 Forth 写编译器 → 抛弃汇编 | 10 个节点类型 + ToolRegistry → 推理标准库子图 → 自进化 |
| **Maru Lisp**（1750 LoC metacircular evaluator） | 在 Lisp 中实现 Lisp，证明自举核心可以极小 | 需要找到 AgenticDSL 的"极小自举核心" |
| **T-diagram 技术** | 第一阶段在宿主语言中实现最小编译器 → 逐阶段替换 | 阶段0 的 C++ 运行时 = 宿主平台，DSL 标准库 = 自举增长层 |
| **vLLM Semantic Router DSL** | 声明式规则动态选择模型/策略 | 推理标准库的控制面参数抽象 |

### 综合建议

1. **采纳 Oracle 的"自举定义"** — AgenticDSL 是语言生态自举（DSL 控制推理参数）而非编译器自举（重写运行时）。这让 4 阶段计划更可信。

2. **阶段2 的范围边界** — 严格约定：C++ 层负责网络监听 + 请求调度 + 并发管理；DSL 层（llm_inference.agent.md）只负责推理策略编排。llm_inference.agent.md 不是全能 Web 服务器，而是"推理请求处理 DAG"。

3. **阶段0→1 距离小于预期** — ToolRegistry 和 `ILLMProvider`（含 `LlamaAdapterProvider`、`CloudLLMAdapter`，C1 后 2026-06-08）已就绪，推理工具注册几乎没有障碍。主要工作量在 Session 隔离和 YIELD 节点。

4. **阶段1→2 距离大于预期** — 需要 SessionRegistry、并发模型改造、网络 listener 支持。

5. **自进化的护栏** — 阶段3 需要：回退机制（canary → rollback）、多指标仪表盘（不只关注单一指标）、定期 checkpoint。

---

## 关联文档

| 文档 | 关系 |
|------|------|
| [VN-002: 语言演进路线图](02-language-evolution-roadmap.md) | 自举目标的具体化，定义了每个阶段的能力里程碑 |
| [docs/specs/dsl.md](../../specs/dsl.md) | 当前 DSL v3.10 规范 — 自举链路的起点 |
| [docs/specs/architecture.md](../../specs/architecture.md) | AgenticOS 八层架构 — 自举系统的部署环境 |
| [docs/adr/adr-0003-dslengine-thread-safety.md](../../adr/adr-0003-dslengine-thread-safety.md) | 定义引擎线程模型，决定自举后服务质量 |
| [docs/adr/adr-0001-illm-provider-streaming-interface.md](../../adr/adr-0001-illm-provider-streaming-interface.md) | 当前流式接口 — 阶段1推理标准库的基础 |
| [ADR-001: 技能分类体系](../skill-system/01-taxonomy.md) | 技能分类是自举链路中"自进化"能力的理论框架 |
| [ADR-006: 推理标准库接口设计](../inference-stdlib/01-interface-design.md) | 阶段1/2的关键依赖 — Agent 通过推理标准库自托管推理 |
