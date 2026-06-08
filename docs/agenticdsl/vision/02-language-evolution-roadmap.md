# VN-002: AgenticDSL 语言演进路线图

**ID**: VN-002
**日期**: 2026-05-20
**状态**: 讨论中（代码基线已验证，阶段顺序待细化）
**关联**: VN-001, all ADRs, all IPs
**调研依据**: Oracle Session 模型评估（bg_159ae7c8）、代码库深访（bg_eefd93cf, bg_e1537f2e, bg_20df819e, bg_31aa5a4a）

---

## 当前状态（基线）

```
AgenticDSL = 工作流 DAG 引擎
  ├── 节点类型: 10种 (START/END/ASSIGN/DSL_CALL/TOOL_CALL/RESOURCE/FORK/JOIN/GENERATE_SUBGRAPH/ASSERT)
  ├── 数据传递: Context = nlohmann::json（无类型, 平面结构）
  ├── 工具系统: ToolRegistry（DSLEngine 成员, 非单例, ~5 个默认工具）
  ├── 图组织: ParsedGraph（扁平）, 多图通过 append_graphs 组合
  ├── 调度: TopoScheduler（DAG 拓扑排序, 有 fork 模拟基础: start_fork_simulation/execute_fork_branches/complete_fork）
  ├── 会话: ExecutionSession（单次执行封装, 构造于 TopoScheduler::run() 内, run 结束即销毁, 无隔离概念）
  ├── 标准库: 4 个 lib/ 子图（auth/human/math/utils）
  ├── LLM 集成: `ILLMProvider` 流式接口（ADR-0001），`LlamaAdapterProvider` 适配本地 llama.cpp，`CloudLLMAdapter` 适配 OpenAI/Anthropic（C1 后 2026-06-08 已落地）
  ├── 线程模型: 单线程无锁, DSLEngine 多实例实现并发安全
  └── 技能示例: 14 个 examples/superpowers/ .agent.md
```

---

## 代码基线验证（2026-05-20）

代码库深访确认了以下关键发现，直接影响路线图估算：

### 已就绪（绿）

| 基础设施 | 发现 |
|---------|------|
| ToolRegistry | 非全局单例（DSLEngine 成员），可直接添加推理工具 |
| LlamaAdapter | 已有 `generate_stream()` 和 `IGenerationStream`，流式接口就绪 |
| Fork 模拟 | TopoScheduler 有 `start_fork_simulation()`、`execute_fork_branches()`、`complete_fork()` — 可扩展 COW |
| 解析器 | markdown_parser 支持完整 DSL 语法，新增节点类型只需在 dispatch switch 中加分支 |

### 需要新建（黄）

| 能力 | 当前缺失 |
|------|---------|
| YIELD 节点 | 不存在。NodeType 枚举、executor dispatch、scheduler resume 都需要新增 |
| SessionRegistry | 不存在。需要新类管理多 session 生命周期 |
| session_vars | 不存在。ExecutionSession 只有 Context，没有 session 级变量 |
| 类型信息 | 不存在。Node/ParsedGraph 无任何类型字段 |

### 需要改造（红）

| 能力 | 当前限制 |
|------|---------|
| 状态隔离 | Context 是平面 json，无 scope 概念。需要 4 层隔离或至少 json scope nesting |
| 并发推理 | 当前单线程。阶段2 需要多请求并发处理 → 需要加锁或 session-per-thread 模式 |

---

## 演进阶段

### 阶段 A：基础增强（当前 → 3个月）

**Oracle MVP 建议**：以下 5 项中，**S-1（Session 隔离）和 S-5（推理标准库）** 可并行推进。
S-3（Yield/Stream）和 S-4（Fork 语义）可推迟或简化。

| 调整项 | Oracle 建议 | 我们的决定 |
|-------|-----------|-----------|
| S-3 Yield/Stream | 状态机 yield 是正确的选择。将来可加内层 C++ 协程优化，不暴露给 DSL。 | ✅ 保留 |
| S-4 Fork 语义 | **开始只用 deep_copy**（最简单），per-field 声明留在 ADR 中作为未来扩展。80% 场景只需 deep_copy。 | ✅ 简化 |
| S-2 ModuleState | 先用 json scope nesting 做 MVP（`session.module_states["/lib/inference/kv_cache"] = {...}`），不加 schema 校验，不加 imports 声明。先把隔离跑起来。 | ✅ 简化 |

**目标**：从工作流 DAG 引擎进化为可构建中型系统的语言

| 能力 | 对应文档 | 关键改动 |
|------|---------|---------|
| S-1. Session 隔离 | ADR-002 | SessionRegistry、session_vars |
| S-2. Module State | ADR-003 | module_state 声明、imports_module_state |
| S-3. Yield/Stream | ADR-004 | YIELD 节点类型、状态机调度 |
| S-4. Fork 语义 | ADR-005 | fork_behavior per field、COW |
| S-5. 推理标准库 | ADR-006, IP-003 | lib/inference/ 全套子图 |

**标志性里程碑**：
- 一个推理 session 可以完整创建、配置、运行、销毁
- 推理标准库可被 Agent 工作流调用
- 多个 session 之间状态完全隔离

### 阶段 B：语言扩展（3个月 → 6个月）

**目标**：增加类型安全和模块化能力

| 能力 | 对应文档 | 关键改动 |
|------|---------|---------|
| S-6. Skill Invoke | ADR-007, LS-001 | type: skill_invoke 节点 |
| S-7. Skill Compose | ADR-007, LS-001 | skill_compose 数据流声明 |
| S-8. 类型系统 | LS-001 | 从无类型 → 结构化类型 |
| S-9. 模块系统 | LS-002 | package/module/import/export |
| S-10. 自托管推理 | VN-001 阶段2 | llm_inference.agent.md |

**标志性里程碑**：
- Skill 可以像函数一样声明式调用
- 类型错误在解析阶段即可发现
- AgenticDSL 自身提供 LLM 推理服务

### 阶段 C：自进化（6个月+）

**目标**：实现完全的自举和自我优化

| 能力 | 对应文档 | 关键改动 |
|------|---------|---------|
| S-11. Oracle 后台监控 | VN-001 | oracle_background.agent.md |
| S-12. 技能自生成 | VN-001 | Agent 生成新 Skill 并注册 |
| S-13. A/B 测试框架 | IP-001 | 新 Skill 的自动验证 |
| S-14. 标准库大规模扩展 | LS-003 | 20+ 标准库子图 |

**标志性里程碑**：
- Oracle 完成第一个完整的"发现→生成→部署"闭环
- 系统在不重启的情况下自我优化

---

## 演进原则

1. **向后兼容** — 每个阶段的新增节点类型不破坏现有 .agent.md
2. **增量可验证** — 每个 S-x 完成后可以独立验证功能
3. **Agent 优先** — 所有新增语法都要考虑"Agent 生成它的难度"
4. **可观测性优先** — 每个阶段必须同时增强 trace/监控能力

---

## 阶段与自举阶段的关系

```
自举阶段 (VN-001)              演进阶段 (VN-002)
─────────────────────────────────────────────
阶段0: 硬编码参数              基线（已可工作，参数在 C++ 中固定）
阶段1: 可编程推理策略            Stage A (S-1~S-5) — 推理标准库暴露控制面
阶段2: Agent 编排推理           Stage B (S-6~S-10) — Agent 工作流动态选择策略
阶段3: 持续自进化               Stage C (S-11~S-14)
```

---

## 风险与缓解

### 工程风险

| 风险 | 缓解 | Oracle 评级 |
|------|------|-----------|
| 新增状态层导致性能下降 | 所有路径在 parse 时验证，运行时零开销 | 🟢 低风险：新增语法对现有图无影响 |
| Agent 生成的 DSL 质量不可控 | 引入 spec_compliance 验证节点 | 🟡 中风险：阶段2/3 才遇到 |
| 自举链路中断 | 保留外部 LLM API 作为 fallback | 🟢 低风险：fallback 模式明确 |
| 类型系统限制了 DSL 灵活性 | 类型标注是可选的，无标注时使用当前 JSON 行为 | 🟢 低风险：渐进式采用 |
| **阶段2 范围蔓延** — 为了自举不断往运行时加能力 | 严格设立范围边界：C++ 负责网络/并发，DSL 只负责推理策略 | 🔴 **最高风险** |

### 自进化阶段特有风险（Oracle 评估）

| 风险 | 描述 | 缓解 |
|------|------|------|
| **局部最优陷阱** | 系统优化到"快速响应简单问题"，但丧失解决复杂问题的能力 | 多指标仪表盘，不只关注单一指标 |
| **反馈循环发散** | 优化 A 让指标变好 → 进一步优化 A → 忽视 B → B 恶化 → 系统崩溃 | 周期性全面评估，拓宽优化目标 |
| **不可逆状态** | 新 Skill 修改了基础设施，回退困难 | 每个优化必须有回退机制（canary → rollback）；定期 checkpoint |
| **Oracle 验证 Oracle 的自指问题** | Agent 生成的新 Skill 由另一个 Agent 验证，可能产生系统性盲区 | 人在回路中做最终批准 |

## Oracle 推荐迁移路径

基于当前代码库现状的分析，建议的增量和安全引入顺序：

```
Step 0: session_vars（ExecutionSession 加 json 字段，0 破坏）
Step 1: SessionRegistry（新类，0 破坏，map<string, ExecutionSession>）
Step 2: ModuleState MVP（json scope nesting，不加 schema，不加 imports 声明）
Step 3: YIELD 节点类型（新增，不影响现有图）
Step 4: Fork 语义简化版（仅 deep_copy，不加 per-field 声明）
Step 5: 推理标准库工具注册
Step 6: 推理标准库子图编写

关键原则：所有新功能都是 ADDITIVE（新增语法/节点类型/工具），
         而不是 MODIFY（改变现有行为）。
         现有流程图完全不受影响。
```

---

## 关联文档

| 文档 | 关系 |
|------|------|
| [VN-001: 自举愿景](01-self-bootstrapping-vision.md) | 路线图的顶层抽象，本文为其阶段划分提供实施细节 |
| [IP-001: 实施路线图](../implementation-roadmap/01-roadmap.md) | 路线图中技术路线的代码实现步骤 |
| [docs/specs/dsl.md](../../specs/dsl.md) | DSL v3.10 规范 — 路线图的基线版本 |
| [docs/specs/layer0.md](../../specs/layer0.md) | L0 运行时规范，路线图的 C++ 运行时基础 |
| [LS-001: 类型系统扩展](../language-extensions/01-type-system.md) | 阶段 A 类型能力的具体设计 |
| [LS-002: 模块与命名空间](../language-extensions/02-module-namespace.md) | 阶段 B 模块能力的具体设计 |
