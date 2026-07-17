# Skill Evolution Pipeline 推荐方案

**日期**: 2026-07-16
**状态**: 🟡 推荐（基于项目内部设计 + SOTA 调研）
**目标读者**: ADR-0061（Agent 进化与固化）决策者

---

## 一、综合分析

### 1.1 项目内部已有基础

`docs/proposals/skill-system/` 4 个文档已经定义了：
- **5 维度 Skill 分类**（流程/领域/审查/UI/项目）
- **skill_invoke + skill_compose 节点语法**（已与 dsl_call 集成）
- **SkillCompiler 完整设计**（SectionParser → AxisClassifier → TemplateEngine → NodeGen → DAGBuilder）

### 1.2 SOTA 强信号（2024-2026）

| 信号 | 证据 | HydraForge 行动 |
|------|------|----------------|
| **SKILL.md 已是事实标准** | Anthropic Skills (2025-10 开放为 agentskills.io)、LangChain Skills (29%→95%)、Cline Skills | v1 必须对齐，不做自创格式 |
| **".agent.md 编译"是热点方向** | AFlow (ICLR 2025 Oral)、ADAS、Compiled AI、AgentIR ("LLVM for agent traces") | 与现有 SkillCompiler 设计结合 |
| **CPU 工具链是延迟瓶颈 (90%)** | Demystifying paper 2025 | 推动 tool call 到本地二进制 |
| **PASTE 式推测执行** | Microsoft Research 2026, 48.5% ↓ | ExecutionSession 加 speculative fork |
| **Behavioral regression 是空白** | AgentAssay 2026, Prompts 1% 覆盖率 (Hasan 2025) | HydraForge ctest 升级的差异化机会 |
| **Formal verification 是前沿** | λ_A (Coq)、Lean4Agent、Oroboro (LTL) | v2 长期方向，与 ADR-0004 V2 兼容 |

### 1.3 与 ADR-0061 现有草案的对照

| ADR-0061 草案 | SOTA 强化 | 行动 |
|---------------|----------|------|
| v1 手动固化工具链 | + AFlow/GEPA 提供 (a) 自动化路径 | **v1 先手动，v2 引入 GEPA-style 反思循环** |
| 性能化 v1 只做分析 | + PASTE/AgentDiet/NVIDIA SLM | **v1 分析 + 节点 SLM 替换；v2 加 PASTE 推测** |
| 可移植化 v1 支持 C++ → Wasm | + WebLLM/WasmEdge/Mozilla 3W | **v1 C++→Wasm；v2 浏览器端 WebLLM 集成** |
| 行为等价强制门 | + AgentAssay/Hotelling T² 模板 | **v1 即采用 AgentAssay-style token-efficient regression** |

---

## 二、4 阶段管线推荐方案

### 阶段 1：Skill.md（原型）

**目标**：快速验证 Agent 行为，复用现有技能生态。

**SOTA 模板对齐**：

| 元素 | Anthropic Skills | HydraForge 推荐 |
|------|------------------|-----------------|
| 文件名 | `SKILL.md` | ✅ 沿用 |
| Frontmatter | `name` + `description` | ✅ + `category` (axis) + `capabilities` |
| Body | 自由 Markdown | ✅ 自由 Markdown |
| Scripts | `scripts/` | ✅ + `lib/` (复用现有 SKILL.md 格式) |
| References | `references/` | ✅ 引用其他 Skill 或 .agent.md |
| Progressive disclosure | metadata 常驻 / body 按需 / scripts lazy | ✅ 三级加载（同 ADR-0055） |

**HydraForge 增强**：
- **Type-safe inputs/outputs**（Anthropic 没有，HydraForge 用 JSON Schema）
- **Capability tags**（FIPA DF 风格，可被 CapabilityRegistry 发现）
- **Iteration budget**（max_steps, timeout_ms, budget_limit）

**示例 Skill.md**：
```markdown
---
name: code-review
description: 审查代码中的安全漏洞、逻辑错误、可维护性问题
category: axis3-review
capabilities: [code_review, static_analysis]
timeout_ms: 30000
budget_limit_usd: 0.05
---

# Code Review Agent

## Input
- `code`: 待审查代码（required, string）
- `language`: 编程语言（required, enum: cpp/python/rust）
- `severity`: 严格程度（optional, enum: low/medium/high, default: medium）

## Process
1. 通读代码理解整体结构
2. 按以下维度检查：
   - 安全风险
   - 逻辑错误
   - 可维护性
3. 输出 JSON 审查报告

## Output
- `issues`: 问题列表（含 line/severity/message/suggestion）
- `summary`: 总结

## Hard Gate
- 必须返回非空 issues 列表
- 严重度必须分级
```

### 阶段 2：.agent.md（固化）

**目标**：从非结构化 Skill 转为结构化、可审计、可优化的 Agent 图。

**SOTA 三种路径选择**：

| 路径 | 代表 | HydraForge 推荐 | 理由 |
|------|------|----------------|------|
| **A. 模板驱动**（变量填充） | 现有 SkillCompiler（`04-skill-compiler-design.md`） | ✅ **v1 直接采用** | 已设计好 5 轴模板，确定性输出 |
| **B. MCTS 搜索** | AFlow (ICLR 2025) | 🔵 v2 引入 | cost 高，但找到更优结构 |
| **C. 反思式进化** | GEPA (ICLR 2026 Oral) | 🔵 v2 持续优化 | 平均 +6% vs GRPO，35× 更少 rollout |

**v1 流程**（沿用现有 SkillCompiler）：

```
SKILL.md
  → SectionParser (提取 ## 章节)
  → AxisClassifier (按 metadata 判断轴)
  → TemplateEngine (按轴选模板)
  → NodeGen (Section → Node list)
  → DAGBuilder (连接 + 验证)
  → .agent.md (含 metadata 保留来源)
```

**v2 增量**：
- 加 GEPA-style reflection loop：基于 ctest trace 自动重写 Skill 章节
- 加 Meta-Agent-style 三级错误归因（Node / Edge / Skill）

**v1 模板示例**（轴 3 审查）：

```yaml
## metadata
- version: 1.0
- source_skill: skills/code-review/SKILL.md
- compile_timestamp: 2026-07-16
- axis: 3-review

## nodes
### start
type: start
next: [/code-review/fork_dims]

### fork_dims
type: fork
branches:
  - /code-review/security
  - /code-review/logic
  - /code-review/maintainability
next: [/code-review/join]

### security
type: generate
prompt: "Review this {{language}} code for security issues..."
tools: [code_read]
output: security_issues
next: [/code-review/join]

### logic
type: generate
prompt: "Review this {{language}} code for logic errors..."
tools: [code_read]
output: logic_issues
next: [/code-review/join]

### maintainability
type: generate
prompt: "Review this {{language}} code for maintainability..."
tools: [code_read]
output: maintainability_issues
next: [/code-review/join]

### join
type: join
inputs: [security_issues, logic_issues, maintainability_issues]
output: all_issues
next: [/code-review/format]

### format
type: assign
output: "..."  # to_json(all_issues)
type: end
```

### 阶段 3：C++（性能化）

**目标**：在保持 DSL 外层编排的同时，替换热点节点为 C++ 实现。

**SOTA 加速技术**：

| 技术 | 来源 | HydraForge 应用 |
|------|------|----------------|
| **PASTE** | Microsoft Research 2026 | ExecutionSession 加 speculative tool fork |
| **AgentDiet** | 2025 | 移除 trajectory 冗余 token（40-60% 节省）|
| **SLM 替换** | NVIDIA position paper | generate 节点用 1-3B 本地模型替代 70B |
| **Compiled AI** | 2026 | "compile once, run forever" 静态生成 |

**v1 实施**：

```
DSL (.agent.md)
  → Trace 分析 (基于 ExecutionSession 已 ship 的 TraceRecord)
  → 标记热点节点 (>100ms 或 >总时间 10%)
  → 识别候选（generate → SLM / tool → C++）
  → 生成 C++ TypedNode
  → 替换节点
  → Behavioral regression 验证（见阶段 5）
```

**v1 不做**：
- 自动 PASTE 推测（v2）
- 全自动 node 替换（v2）

**v1 模板示例**（Code Review Agent 性能化）：

```cpp
// pdk/code_review_agent/src/cpp_review.cpp
DECLARE_TOOL(review_security, "C++ security analyzer", ReadOnly, "plan",
    auto code = args["code"].get<std::string>();
    auto lang = args["language"].get<std::string>();
    
    // 调用本地静态分析器（clang -fsyntax-only + ccc-analyzer）
    SecurityAnalyzer analyzer;
    return analyzer.scan(code, lang).to_json();
)

DECLARE_TOOL(review_logic, "C++ logic checker", ReadOnly, "plan",
    // 调用本地 linter (clang-tidy)
    ...
)

DECLARE_TOOL(review_maintainability, "C++ maintainability scorer", ReadOnly, "plan",
    // 计算圈复杂度（基于 LLVM lib）
    ...
)
```

**Phase 2 增量**：
- 加 `model_router` plugin 的 SLM 优先路由
- PASTE-style pattern-aware speculative tool fork
- AgentDiet-style trajectory compaction

### 阶段 4：Wasm（可移植化）

**目标**：跨平台分发、强隔离、边缘部署。

**SOTA 选项**：

| 选项 | 代表 | v1 推荐 | v2 推荐 |
|------|------|:-------:|:-------:|
| **C++ → Wasm** | WasmEdge, wasi-sdk | ✅ | — |
| **DSL → Wasm bytecode** | FLUX Runtime | 🔵 | ✅ |
| **浏览器端 WebLLM** | MLC.AI | ❌ | ✅ |

**v1 实施**（与 ADR-0056 一致）：

```bash
# C++ review 编译为 Wasm
wasi-sdk-clang++ \
  -O3 -flto \
  --target=wasm32-wasi \
  -o wasm/code_review.wasm \
  src/cpp_review.cpp
```

**v1 Wasm 模板**：

```cpp
// pdk/code_review_agent/wasm/code_review.wasm
// 编译产物，host function 白名单：
//   host_call_tool, host_emit_event, host_consume_budget

#include <hydraforge_wasm.h>

extern "C" int agent_run(const char* args_json, char* result, int max_len) {
    // 入口函数（与 .agent.md 入口对应）
    auto args = json::parse(args_json);
    auto security = call_tool("security/scan", args);
    auto logic = call_tool("logic/check", args);
    auto maint = call_tool("maintainability/score", args);
    auto result = merge({security, logic, maint});
    return copy_to(result, result, max_len);
}
```

**v2 增量**：
- DSL → Wasm bytecode（FLUX Runtime 风格）
- 浏览器端 WebLLM 集成（3W Stack 模式）
- Mozilla 3W 多运行时组合

---

## 三、行为等价验证（横切关注点）

**这是 SOTA 共识的核心缺口**：Hasan 2025 实证研究发现 39 frameworks × 439 apps 中 **prompts 测试覆盖率仅 1%**。HydraForge 应填补。

**v1 实施**（直接借鉴 AgentAssay 模板）：

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
    
    // 4. 行为指纹对比 (Hotelling T²)
    auto fingerprint_skill = compute_fingerprint(skill_results);
    auto fingerprint_dsl = compute_fingerprint(dsl_results);
    
    REQUIRE(hotelling_t2(fingerprint_skill, fingerprint_dsl) < THRESHOLD);
}

TEST_CASE("code-review .agent.md vs C++ 性能化等价") {
    // 类似，对比 DSL vs C++ 输出
}

TEST_CASE("code-review C++ vs Wasm 跨平台等价") {
    // 对比 C++ vs Wasm 输出
}
```

**AgentAssay 三值 verdict**：

| Verdict | 含义 | 行动 |
|---------|------|------|
| **Pass** | 等价 | 接受升级 |
| **Fail** | 不等价 | 阻断升级，回滚 |
| **Inconclusive** | 证据不足 | 增加测试用例或调整阈值 |

**Adaptive budget**：

```cpp
struct RegressionBudget {
    uint32_t max_tokens = 10000;          // 每次 regression 最多 token
    uint32_t adaptive_test_count = 50;    // 自动调整测试数量
    double confidence_threshold = 0.95;   // 置信度阈值
};
```

**Phase 2 增量**：
- LTL-style behavioral assertions（Oroboro 风格）
- Lean4 type-level equivalence proof（Lean4Agent 风格）
- λ_A 风格的 config structural completeness check

---

## 四、与现有 HydraForge 架构的整合

### 4.1 与 ADR-0053（AgentDescriptor）整合

```cpp
struct AgentDescriptor {
    // ...
    std::vector<AgentForm> forms;
    // Skill.md / .agent.md / C++ / Wasm 四选一/多
};

// 4 阶段管线 = AgentForm 的演化路径
```

### 4.2 与 ADR-0055（SKILL 隔离）整合

```cpp
// SkillInterpreter 必须支持 SKILL.md 新格式（YAML frontmatter）
SkillInterpreter::run("skills/code-review/SKILL.md", args);
```

### 4.3 与 ADR-0056（Wasm 运行时）整合

```cpp
// C++ → Wasm 编译产物通过 WasmRuntime 加载
WasmRuntime::load("wasm/code_review.wasm");
```

### 4.4 与 ADR-0058（Schema 校验）整合

```cpp
// Skill.md frontmatter 包含 input_schema / output_schema
// DSL Validator + Runtime validator 一致
```

### 4.5 与 Sprint 14 C5（AgentAssert）整合

```cpp
// 行为契约检查（已有） → 增强为 behavioral fingerprint
AgentAssert::check(agent_id, fingerprint);
```

---

## 五、ADR-0061 推荐更新

基于本调研和现有 ADR-0061 草案，推荐 ADR-0061 调整如下：

### 决策 1 — v1 固化采用 SKILL.md 事实标准

**理由**：
- Anthropic Skills / LangChain Skills / Cline Skills 已统一 SKILL.md 格式
- 复用现有 SkillCompiler 设计（`04-skill-compiler-design.md`）
- **不是新创格式**，直接对齐

### 决策 2 — v1 行为等价采用 AgentAssay 模板

**理由**：
- Hasan 2025 实证显示 prompts 测试覆盖率仅 1%（行业空白）
- AgentAssay 提供 token-efficient 三值 verdict + Hotelling T² 指纹
- 与 Sprint 14 C5 AgentAssert 自然衔接

### 决策 3 — v1 性能化采用 SLM 替换 + Trace 分析

**理由**：
- NVIDIA 2025 证明 SLM 接管 80% 常规子任务（10-30× 便宜）
- PASTE / AgentDiet 等加速技术 v1 暂不做（v2）
- Trace 分析基于已 ship 的 ExecutionSession (Sprint 18)

### 决策 4 — v1 可移植化采用 C++ → Wasm（wasi-sdk）

**理由**：
- wasi-sdk 工具链成熟
- v1 不做 DSL → Wasm（FLUX 风格，v2）
- v1 不做浏览器端 WebLLM（v2）

### 决策 5 — v1 不做自动 PASTE/GEPA/AFlow

**理由**：
- 这些需要 LLM 辅助或大量算力
- v1 验证框架 + 流程，v2 加自动化
- 先确保手动流程可用，再做自动化

### 决策 6 — v2 候选方向（并行调研中）

- GEPA-style 反思循环（持续优化 Skill）
- AFlow-style MCTS 搜索（自动发现新 Skill）
- PASTE-style 推测执行（ExecutionSession speculative fork）
- AgentDiet-style trajectory compaction
- λ_A-style config lint（94.1% production configs incomplete 验证）

---

## 六、推荐的可立即创建 OpenSpec Changes

| Change ID | 名字 | 工作量 | 优先级 |
|-----------|------|--------|--------|
| **adr-0061-skill-std** | SKILL.md 与 Anthropic Skills / Cline Skills 标准对齐 | 1 week | **P0** |
| **adr-0061-behavioral-regression** | AgentAssay-style behavioral regression suite | 2 weeks | **P0** |
| **adr-0061-trajectory-ir** | AgentIR-style trajectory IR 升级 ParsedGraph | 3 weeks | P1 |
| **adr-0061-slm-routing** | model_router plugin 加 SLM 优先路由（NVIDIA 建议） | 1 week | P1 |
| **adr-0061-paste-speculation** | ExecutionSession 加 PASTE-style speculative fork | 4 weeks | P2 |
| **adr-0061-wasm-boundary** | DSLEngine Wasm sandbox abstraction + WebLLM prototype | 4 weeks | P2 |
| **adr-0061-gepa-loop** | GEPA-style reflection loop（持续 Skill 进化） | 3 weeks | P2 |
| **adr-0061-formal-lint** | λ_A-style config structural completeness check | 2 weeks | P2 |

---

## 七、参考来源

### 项目内部（已读完）

- `docs/proposals/skill-system/01-taxonomy.md` — 5 维度分类
- `docs/proposals/skill-system/02-invoke-compose.md` — skill_invoke / skill_compose
- `docs/proposals/skill-system/03-taxonomy-mapping.md` — 39 Skills 映射
- `docs/proposals/skill-system/04-skill-compiler-design.md` — SKILL.md → .agent.md 编译器

### SOTA（librarian 调研）

- 详见 `02-sota-survey.md`（43 论文 + 6 工业实践）

### HydraForge 相关 Sprint

- Sprint 14 C5: AgentAssert 行为契约
- Sprint 14 C6: DECLARE_TOOL V2
- Sprint 18 P1.T3: execution_session.cpp move
- Sprint 19: cost-tracking decorator
- Sprint 20: React/PlanExecute/ForkJoin Loop dispatcher

### 相关 ADR

- ADR-0021: PDK Design
- ADR-0043: PDK Tool Naming
- ADR-0052: Agent Plugin Manifest
- ADR-0053: AgentDescriptor 与 pdk_register_agent
- ADR-0054: Capability Discovery
- ADR-0055: SKILL.md 执行与隔离
- ADR-0056: WebAssembly Agent 运行时
- ADR-0058: Tool Schema Validation