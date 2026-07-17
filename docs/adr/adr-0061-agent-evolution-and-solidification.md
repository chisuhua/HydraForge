# ADR-0061: Agent 进化与固化（Solidification）

## 状态

✅ Approved (2026-07-16, 架构评审确认 + SOTA 调研确认)
✅ Updated (2026-07-16, 集成 Skill Evolution Pipeline 调研结论)

## 领域

Agent-as-Plugin 架构 / Skill 演化与编译

## 关联

- [ADR-0052 — Agent Plugin Manifest](./adr-0052-agent-plugin-manifest.md) — Skill.md frontmatter 规范
- [ADR-0053 — AgentDescriptor](./adr-0053-agent-descriptor-interface.md) — AgentForm 四形态
- [ADR-0054 — Capability Discovery](./adr-0054-capability-discovery.md) — Capability tags
- [ADR-0055 — SKILL.md 执行与隔离](./adr-0055-skill-isolation.md) — SkillInterpreter
- [ADR-0056 — WebAssembly Agent 运行时](./adr-0056-wasm-runtime.md) — wasi-sdk 编译
- [ADR-0058 — Tool Schema Validation](./adr-0058-tool-schema-validation.md) — input/output schema
- `docs/proposals/skill-system/01-taxonomy.md` — 5 维度 Skill 分类
- `docs/proposals/skill-system/02-invoke-compose.md` — skill_invoke / skill_compose 节点
- `docs/proposals/skill-system/03-taxonomy-mapping.md` — 39 Skills 映射
- `docs/proposals/skill-system/04-skill-compiler-design.md` — SKILL.md → .agent.md 编译器
- `docs/architecture/agent-evolution-pipeline.md` — 4 阶段管线主文档
- `docs/research/skill_evolution/` — SOTA 调研

## 背景

### 问题

Agent 的 4 阶段进化路径（Skill → DSL → C++ → Wasm）已经写入 `agent-evolution-pipeline.md`，但具体演化机制缺乏决策：

1. SKILL.md → .agent.md 固化的具体流程（沿用现有 SkillCompiler 还是重做）
2. SKILL.md 格式是否对齐工业事实标准（Anthropic Skills / LangChain Skills）
3. 性能化（DSL → C++）如何识别热点节点
4. 可移植化（DSL/C++ → Wasm）的具体路径
5. 演化前后行为等价如何保证（v1 必选 vs v2 长期）

### SOTA 关键信号（2024-2026 调研）

| 信号 | 证据 | 对 ADR-0061 的影响 |
|------|------|---------------------|
| **SKILL.md 是事实标准** | Anthropic Skills (2025-10)、LangChain Skills、Cline Skills | v1 必对齐，不自创格式 |
| **CPU 工具链占延迟 90%** | Demystifying paper 2025 | 推动 tool call 到本地二进制（C++/Wasm） |
| **Prompts 测试覆盖率仅 1%** | Hasan 2025 实证 39 frameworks × 439 apps | v1 必选 behavioral regression suite |
| **94.1% production agent configs 不完整** | λ_A 论文 2026 | v2 加 config structural completeness check |
| **PASTE 推测执行** | Microsoft Research 2026, 48.5% ↓ | v2 ExecutionSession speculative fork |
| **SLM 接管 80% 子任务** | NVIDIA position paper 2025 | v1 model_router SLM 优先路由 |

### 目标

定义 4 阶段管线的具体机制，集成项目内部 SkillCompiler 设计 + SOTA 最佳实践。

## 决策

### 决策 1 — 4 阶段管线（与 `agent-evolution-pipeline.md` 对齐）

```
┌─────────────────────────────────────────────────────────────┐
│  Agent 内部实现进化路径                                       │
├─────────────────────────────────────────────────────────────┤
│                                                              │
│   SKILL.md ──(Solidification)──→ .agent.md                 │
│       │                              │                        │
│       │                              ├──(Performance)──→ C++ │
│       │                              │                       │
│       │                              └──(Portability)──→ Wasm│
│                                                              │
│   阶段 1 (原型)        阶段 2 (生产)   阶段 3 (性能) 阶段 4  │
│   SKILL Interpreter    DSL Engine      Native C++      Wasm  │
└─────────────────────────────────────────────────────────────┘
```

### 决策 2 — SKILL.md v1 格式：与 Anthropic Skills 对齐

**采用事实标准**，不自创格式：

```markdown
---
name: code-review
description: 审查代码中的安全漏洞、逻辑错误、可维护性问题
category: axis3-review                # 来自 docs/proposals/skill-system/01
capabilities: [code_review, static_analysis]
input_schema:                          # JSON Schema 2020-12
  type: object
  properties:
    code: {type: string}
    language: {type: string, enum: [cpp, python, rust]}
    severity: {type: string, enum: [low, medium, high], default: medium}
  required: [code, language]
output_schema:
  type: object
  properties:
    issues: {type: array}
    summary: {type: string}
requires_isolation: true               # SKILL 必须隔离（ADR-0055）
timeout_ms: 30000
budget_limit_usd: 0.05
activation_events: [onTool:code_review/run]   # ADR-0057 懒加载
trust_level: high                      # ADR-0052 trust
---

# Code Review Agent

## Process
1. 通读代码理解整体结构
2. 按以下维度检查：安全风险 / 逻辑错误 / 可维护性
3. 输出 JSON 审查报告

## Hard Gate
- 必须返回非空 issues 列表
- 严重度必须分级
```

**与 Anthropic Skills 兼容性**：
- ✅ `name` + `description`（Anthropic 必需）
- ✅ YAML frontmatter
- ✅ 三级 progressive disclosure（metadata always / body on trigger / scripts lazy）
- ✅ `scripts/` 目录允许 C++ 二进制作为 deterministic tool
- 🟡 `capabilities` / `requires_isolation` / `trust_level` 是 HydraForge 扩展

**与项目内部 SkillCompiler 对齐**：
- 复用 `04-skill-compiler-design.md` 的 SectionParser + 5 轴模板
- frontmatter 提供 AxisClassifier 需要的元数据
- SKILL.md body 是 TemplateEngine 的变量来源

### 决策 3 — 固化（Solidification）v1：沿用 SkillCompiler

**流程**：

```
SKILL.md
  → SectionParser (提取 ## 章节)
  → AxisClassifier (frontmatter.category)
  → TemplateEngine (按轴选模板 + body 变量填充)
  → NodeGen (Section → Node list)
  → DAGBuilder (连接 next + 验证)
  → .agent.md
```

**沿用现有 `04-skill-compiler-design.md` 的 5 轴模板**：

| 轴 | 模板类型 | 适用 Skill |
|----|---------|-----------|
| 1 流程/方法论 | 顺序流水线 + 分支循环 | brainstorming, TDD |
| 2 领域/工具 | 工具调用序列 | cmake, git_master |
| 3 审查/质量 | Fork-Join 并行 | review_work |
| 4 UI/前端 | LLM 生成流水线 | frontend_ui_ux |
| 5 项目专用 | 工具命令 | openspec_* |

**触发方式**：
- 手动：`hf skill solidify <name>` CLI
- 自动：CI 检测 Skill 稳定（无变更 30 天 + 使用次数 > 100）后自动触发
- **v1 不做**：运行时自动固化（v2 引入 GEPA-style 反思循环）

**v1 必选行为门**：
- **必须**通过 N 个回归测试（见决策 6）
- 固化产物保留 `source_skill` 路径（可逆元数据）
- 行为指纹 (Hotelling T²) 偏差 < 阈值

### 决策 4 — 性能化（Performance）v1：Trace 分析 + SLM 替换

**v1 流程**：

```
DSL (.agent.md)
  → Trace 分析（基于 ExecutionSession 已 ship 的 TraceRecord）
  → 标记热点节点（>100ms 或 >总时间 10%）
  → 识别候选：
     - generate 节点 → 替换为 SLM（本地 1-3B 模型）
     - tool_call 节点 → 替换为 C++ DECLARE_TOOL
  → 生成 C++ TypedNode
  → 替换节点（保留 DAG 结构）
  → Behavioral regression 验证
```

**SOTA 借鉴**：

| 技术 | 来源 | HydraForge 应用 |
|------|------|----------------|
| Trace-based hot path detection | Demystifying 2025, AgentSight 2025 | 复用 ExecutionSession TraceRecord |
| SLM 替换 | NVIDIA position paper 2025 | model_router plugin 加 SLM 优先 |
| Trajectory compaction | AgentDiet 2025 | v2 候选，节省 40-60% token |
| Speculative tool fork | PASTE 2026 | v2 候选，48.5% ↓ 延迟 |
| Pattern mining | PASTE 2026 | v2 候选 |

**v1 不做**：
- 自动 PASTE 推测（v2）
- AgentDiet trajectory 压缩（v2）
- 全自动 node 替换（v2）

**v1 模板示例**（Code Review Agent 性能化）：

```cpp
// pdk/code_review_agent/src/cpp_review.cpp
DECLARE_TOOL(review_security, "C++ security analyzer", ReadOnly, "plan",
    auto code = args["code"].get<std::string>();
    auto lang = args["language"].get<std::string>();
    return SecurityAnalyzer::scan(code, lang).to_json();
)

DECLARE_TOOL(review_logic, "C++ logic checker", ReadOnly, "plan",
    return LogicChecker::check(args["code"], args["language"]).to_json();
)
```

**Phase 2 增量**：
- `model_router` plugin 加 SLM 优先路由（local 1-3B model 优先）
- ExecutionSession speculative tool fork（PASTE pattern mining）
- AgentDiet-style trajectory compaction

### 决策 5 — 可移植化（Portability）v1：C++ → Wasm（wasi-sdk）

**v1 流程**：

```bash
wasi-sdk-clang++ \
  -O3 -flto \
  --target=wasm32-wasi \
  -o wasm/code_review.wasm \
  src/cpp_review.cpp
```

**Wasm host function 白名单**（与 ADR-0056 共享）：

| Host function | 用途 | 默认 capability |
|--------------|------|----------------|
| `host_call_tool(name, args)` | 调用 OS 工具 | ✅ |
| `host_emit_event(topic, payload)` | 事件推送 | ✅ |
| `host_consume_budget(amount)` | 预算消耗 | ✅ |
| `host_log(level, message)` | 日志 | ✅ |
| `host_read_file(path)` | 文件读取 | ⚠️ 需声明 |
| `host_write_file(path, data)` | 文件写入 | ⚠️ 需声明 |

**v1 不做**：
- DSL → Wasm bytecode（FLUX 风格，v2）
- 浏览器端 WebLLM 集成（v2）

**Phase 2 增量**：
- DSL → Wasm bytecode 编译器
- WebLLM runtime（MLC.AI / WasmEdge）
- Mozilla 3W Stack（WebLLM + WASM + WebWorkers）

### 决策 6 — 行为等价 v1：AgentAssay 模板（核心新增）

**关键洞察**：Hasan 2025 实证研究 39 frameworks × 439 apps，发现 **prompts（trigger）测试覆盖率仅 1%**——这是工业空白。HydraForge 应直接填补。

**v1 行为等价框架**：

```cpp
// tests/behavioral_regression/skill_evolution_test.cpp
TEST_CASE("code-review SKILL vs .agent.md 行为等价") {
    // 1. 准备 N 个测试用例（包含 edge cases）
    auto cases = load_test_cases("code-review/test_cases.json");
    
    // 2. SKILL 执行
    SkillInterpreter skill_int;
    auto skill_results = run_all(cases, [&](auto& c) {
        return skill_int.run("skills/code-review/SKILL.md", c);
    });
    
    // 3. DSL 执行
    DSLEngine dsl;
    auto dsl_results = run_all(cases, [&](auto& c) {
        return dsl.run(load("agents/code-review.agent.md"), c);
    });
    
    // 4. 行为指纹对比（Hotelling T²）
    auto fp_skill = compute_fingerprint(skill_results);
    auto fp_dsl = compute_fingerprint(dsl_results);
    
    REQUIRE(hotelling_t2(fp_skill, fp_dsl) < THRESHOLD);
}

TEST_CASE("code-review .agent.md vs C++ 性能化等价") {
    // 类似，对比 DSL vs C++ 输出
}

TEST_CASE("code-review C++ vs Wasm 跨平台等价") {
    // 对比 C++ vs Wasm 输出
}
```

**三值 verdict**（直接借鉴 AgentAssay 2026）：

| Verdict | 含义 | 行动 |
|---------|------|------|
| **Pass** | Hotelling T² < 阈值 | 接受升级 |
| **Fail** | Hotelling T² ≥ 阈值 | 阻断升级，回滚 |
| **Inconclusive** | 证据不足 | 增加测试用例或调整阈值 |

**Adaptive budget**：

```cpp
struct RegressionBudget {
    uint32_t max_tokens = 10000;
    uint32_t adaptive_test_count = 50;
    double confidence_threshold = 0.95;
};
```

**v1 集成点**：
- 与 Sprint 14 C5 `AgentAssert` 自然衔接（`assert_behavior_fingerprint(agent_id, fp)`）
- 与 Sprint 19 cost-tracking decorator 集成（控制 regression 测试 token 开销）
- 与 ExecutionSession TraceRecord 集成（trace-first offline 模式）

**Phase 2 增量**：
- LTL-style behavioral assertions（Oroboro 风格）
- Lean4 type-level equivalence proof（Lean4Agent 风格）
- λ_A-style config structural completeness check
- Cost-aware speculative execution regression（CASE 论文 2026）

### 决策 7 — v1 不做（v2 候选）

**明确推迟到 v2**：

| 技术 | 来源 | v2 候选 |
|------|------|---------|
| MCTS 工作流搜索 | AFlow (ICLR 2025) | `adr-0061-aflow-search` |
| 反思式 prompt 进化 | GEPA (ICLR 2026 Oral) | `adr-0061-gepa-loop` |
| 模式感知推测执行 | PASTE (Microsoft 2026) | `adr-0061-paste-speculation` |
| Trajectory 压缩 | AgentDiet 2025 | `adr-0061-trajectory-compaction` |
| DSL → Wasm bytecode | FLUX Runtime 风格 | `adr-0061-dsl-wasm` |
| 浏览器端 WebLLM | MLC.AI 2024-2026 | `adr-0061-webllm` |
| Formal verification | Lean4Agent / λ_A / Oroboro | `adr-0061-formal-lint` |
| Multi-framework IR | AgentIR ("LLVM for agents") | `adr-0061-trajectory-ir` |
| DAG 抽象代数 | λ_A typed lambda calculus | `adr-0061-agent-calculus` |

**理由**：v1 验证框架 + 流程，v2 加自动化。手动流程先确保可用，再做自动化。

### 决策 8 — 与现有 ADR 集成

| 集成点 | 来自 ADR |
|--------|---------|
| `AgentForm::Skill/DSL/Cpp/Wasm` 四形态 | ADR-0053 |
| `requires_isolation: true` 强制 SKILL | ADR-0055 |
| `capabilities` CapabilityRegistry 索引 | ADR-0054 |
| `input_schema/output_schema` JSON Schema | ADR-0058 |
| `manifest.implementation_forms: ["skill", "dsl", "cpp", "wasm"]` | ADR-0052 |
| `activation_events` 懒加载 | ADR-0057 |
| C++ DECLARE_TOOL 模板 | ADR-0021 |
| C++ → Wasm 编译 | ADR-0056 |

## 替代方案

### 方案 A：完全采用 AFlow (MCTS) 做 SKILL.md → .agent.md

**否决理由**：
- AFlow 需要大量 GPU 算力（MCTS 搜索）
- v1 不可控（确定性输出 → 不可控）
- v1 优先 manual + 模板驱动（沿用现有 SkillCompiler），v2 引入 AFlow

### 方案 B：DSL 阶段采用 GEPA 反思循环

**否决理由**：
- GEPA 持续 fine-tune，与 v1 Ship 目标不符
- v1 先固化行为等价（Hotelling T²），v2 加 GEPA 持续优化

### 方案 C：行为等价采用 Lean4 形式化证明

**否决理由**：
- Lean4 形式化成本极高（835 configs 中 94.1% 不完整）
- v1 优先 AgentAssay 实证测试（5-20× 便宜），v2 加 LTL/Lean4

## 不变量

- SKILL.md 必须包含 `requires_isolation: true`（ADR-0055）
- 4 阶段固化产物必须保留 `source_skill` 路径（可逆元数据）
- 行为等价 v1 必须通过 AgentAssay 模板（Hotelling T² 指纹）
- Wasm 形态必须 capability-limited（ADR-0056）
- C++ 节点必须有 TraceExporter 埋点（与 ExecutionSession 兼容）

## 权衡

| 决策 | 选择 | 理由 |
|------|------|------|
| SKILL.md 格式 | Anthropic 兼容 | 事实标准，生态最广 |
| 固化路径 | 模板驱动 + SkillCompiler | 沿用现有设计，确定性输出 |
| 性能化 | Trace + SLM | 已 ship 基础设施可用 |
| 可移植化 | wasi-sdk C++ → Wasm | 工具链成熟 |
| 行为等价 | AgentAssay 实证 | token-efficient + 工业空白 |
| v2 自动化 | 推迟 | 手动流程先验证，再自动化 |

## 后续行动

### 立即可创建的 OpenSpec Changes

| Change ID | 名字 | 工作量 | 优先级 |
|-----------|------|--------|--------|
| **adr-0061-skill-std** | SKILL.md 与 Anthropic Skills / Cline Skills 标准对齐 | 1 week | **P0** |
| **adr-0061-behavioral-regression** | AgentAssay-style behavioral regression suite | 2 weeks | **P0** |
| **adr-0061-skill-compiler** | 实施 SkillCompiler（沿用 `04-skill-compiler-design.md`） | 3 weeks | **P0** |
| **adr-0061-slm-routing** | model_router plugin 加 SLM 优先路由 | 1 week | P1 |
| **adr-0061-cpp-wasm-toolchain** | wasi-sdk 集成 + C++ → Wasm CI | 2 weeks | P1 |
| **adr-0061-trajectory-ir** | AgentIR-style trajectory IR 升级 ParsedGraph | 3 weeks | P1 |
| **adr-0061-paste-speculation** | ExecutionSession 加 PASTE-style speculative fork | 4 weeks | P2 |
| **adr-0061-aflow-search** | AFlow-style MCTS 工作流搜索 | 4 weeks | P2 |
| **adr-0061-gepa-loop** | GEPA-style reflection loop | 3 weeks | P2 |
| **adr-0061-formal-lint** | λ_A-style config structural completeness check | 2 weeks | P2 |
| **adr-0061-dsl-wasm** | DSL → Wasm bytecode 编译器 | 4 weeks | P2 |
| **adr-0061-webllm** | 浏览器端 WebLLM 集成 | 4 weeks | P2 |

### 与已有 Sprint 集成

- Sprint 14 C5（AgentAssert）：扩展为 behavioral fingerprint
- Sprint 14 C6（DECLARE_TOOL V2）：新增 SLM 优先级
- Sprint 18 P1.T3（execution_session.cpp）：加 hot path 标记
- Sprint 19（cost-tracking decorator）：集成 regression 测试成本
- Sprint 20（React/PlanExecute/ForkJoin）：性能化目标用 LoopType dispatch

## 参考

- [ADR-0053 — AgentDescriptor](./adr-0053-agent-descriptor-interface.md)
- [ADR-0054 — Capability Discovery](./adr-0054-capability-discovery.md)
- [ADR-0055 — SKILL.md 执行与隔离](./adr-0055-skill-isolation.md)
- [ADR-0056 — WebAssembly Agent 运行时](./adr-0056-wasm-runtime.md)
- [ADR-0058 — Tool Schema Validation](./adr-0058-tool-schema-validation.md)
- `docs/proposals/skill-system/01-taxonomy.md`
- `docs/proposals/skill-system/04-skill-compiler-design.md`
- `docs/architecture/agent-evolution-pipeline.md`
- `docs/research/skill_evolution/02-sota-survey.md`
- `docs/research/skill_evolution/03-evolution-pipeline-recommendations.md`

### 关键 SOTA 引用（详见 02-sota-survey.md）

- **Anthropic Skills**: https://www.anthropic.com/news/skills
- **LangChain Skills**: https://www.langchain.com/blog/langchain-skills
- **AFlow**: arXiv:2410.10762 (ICLR 2025 Oral)
- **GEPA**: arXiv:2507.19457 (ICLR 2026 Oral)
- **Meta-Agent**: arXiv:2605.25233
- **DSPy**: arXiv:2310.03714 (ICLR 2024)
- **PASTE**: arXiv:2603.18897 (Microsoft Research 2026)
- **AgentAssay**: arXiv:2603.02601
- **Hasan 2025 (Prompts 1% coverage)**: arXiv:2509.19185
- **Demystifying 2025 (CPU 90% bottleneck)**: arXiv:2511.00739
- **λ_A**: arXiv:2604.11767 (94.1% incomplete configs)
- **Lean4Agent**: arXiv:2606.06523
- **Oroboro**: arXiv:2509.20364
- **AgentIR**: github.com/WhitzardAgent/agentir
- **Compiled AI**: arXiv:2604.05150
- **NVIDIA SLM**: developer.nvidia.com/blog/how-small-language-models-are-key-to-scalable-agentic-ai