# AgenticDSL 技能编程指南

> 基于 AgenticDSL v3.10 | 生成日期: 2026-05-19

---

## 目录

1. [AgenticDSL 编程能力总览](#1-agenticdsl-编程能力总览)
2. [完整节点类型参考](#2-完整节点类型参考)
3. [Superpowers 技能能力对照](#3-superpowers-技能能力对照)
4. [并行与资源管理模式分析](#4-并行与资源管理模式分析)
5. [AgenticDSL 实现示例](#5-agenticdsl-实现示例)
6. [能力矩阵与局限性](#6-能力矩阵与局限性)

---

## 1. AgenticDSL 编程能力总览

### 1.1 核心架构

```
┌─────────────────────────────────────────────────────┐
│                   DSLEngine                          │
├─────────────────────────────────────────────────────┤
│  MarkdownParser ──► ParsedGraph ──► TopoScheduler    │
│                        │               │             │
│                        ▼               ▼             │
│                  Node[]  ◄────  NodeExecutor          │
│                        │               │             │
│                        ▼               ▼             │
│                   Context         ToolRegistry       │
│                   (nlohmann::json)  LlamaAdapter     │
└─────────────────────────────────────────────────────┘
```

- **DSL 定义**：Markdown YAML 格式的 DAG 工作流
- **图执行**：拓扑排序调度，按 `next` 依赖链执行
- **上下文**：全局可变 `nlohmann::json`，所有节点可读写
- **LLM**：通过 `dsl_call` 节点调用，支持模板渲染
- **工具**：通过 `tool_call` 节点调用注册的工具
- **动态图**：`generate_subgraph` 让 LLM 生成新 DSL 图并追加执行

### 1.2 DSL 语法总览

```markdown
### AgenticDSL '/graph_name'

## /__meta__
execution_budget:
  max_llm_calls: 50
  max_tool_calls: 100
  max_total_nodes: 30
  max_depth: 10

## /graph/start
type: start
next: ["/graph/step1"]

## /graph/step1
type: assign
assign:
  greeting: "Hello {{user.name}}"
next: ["/graph/step2"]

## /graph/step2
type: dsl_call
llm_tool: llama-7b
output_keys: ["response"]
prompt: |
  你是一个助手。用户说: {{greeting}}
  请友好回复。
params:
  temperature: 0.7
  max_tokens: 200
next: ["/graph/end"]

## /graph/end
type: end
```

---

## 2. 完整节点类型参考

### 2.1 节点类型总表

| # | 枚举值 | 结构体 | DSL key | 用途 | 状态 |
|---|--------|--------|---------|------|------|
| 1 | `START` | `StartNode` | `type: start` | 图入口 | ✅ |
| 2 | `END` | `EndNode` | `type: end` | 图出口 | ✅ |
| 3 | `ASSIGN` | `AssignNode` | `type: assign` | 变量赋值/模板渲染 | ✅ |
| 4 | `DSL_CALL` | `DSLNode` | `type: dsl_call` | LLM 生成 (v3.10+) | ✅ |
| 5 | `TOOL_CALL` | `ToolCallNode` | `type: tool_call` | 调用注册工具 | ✅ |
| 6 | `RESOURCE` | `ResourceNode` | `type: resource` | 声明资源 (stub) | ⚠️ |
| 7 | `FORK` | `ForkNode` | `type: fork` | 并行分支 | ❌ |
| 8 | `JOIN` | `JoinNode` | `type: join` | 合并并行 | ❌ |
| 9 | `GENERATE_SUBGRAPH` | `GenerateSubgraphNode` | `type: generate_subgraph` | LLM 动态生成图 | ✅ |
| 10 | `ASSERT` | `AssertNode` | `type: assert` | 条件断言 | ✅ |
| 11 | `LLM_CALL`| `LLMCallNode` | `type: llm_call` | 旧版 LLM 调用 | ⚠️ 废弃 |

### 2.2 START / END

```yaml
## /main
type: start
next: ["/main/step1"]
---
## /main/done
type: end
```

| 字段 | 类型 | 说明 |
|------|------|------|
| `next` | `string[]` | 后续节点路径 |

### 2.3 ASSIGN (变量赋值)

```yaml
## /main/init
type: assign
assign:
  greeting: "Hello {{user.name}}"
  count: "{{attempts|default:0}}"
  upper: "{{text|upper}}"
next: ["/main/next"]
```

| 字段 | 类型 | 说明 |
|------|------|------|
| `assign` | `map<string, string>` | 键值对，值支持 Inja 模板 |
| `next` | `string[]` | 后续节点路径 |

**支持的内置过滤器**: `upper`, `lower`, `trim`, `add`, `length`, `default`, `capitalize`

### 2.4 DSL_CALL (LLM 调用，v3.10+ 推荐)

```yaml
## /main/think
type: dsl_call
llm_tool: llama-7b
output_keys: ["llm_response"]
prompt: |
  用户请求: {{user_input}}
  分析并回复。
params:
  temperature: 0.7
  max_tokens: 500
  top_p: 0.9
next: ["/main/process"]
```

| 字段 | 类型 | 必要 | 说明 |
|------|------|------|------|
| `llm_tool` | `string` | ✅ | ToolRegistry 中注册的 LLM 工具名 |
| `output_keys` | `string[]` | ✅ | 输出存储的 context 变量名 |
| `prompt` | `string` | ✅ | Inja 模板，支持 `{{var}}` 插值 |
| `params` | `object` | ❌ | temperature, max_tokens, top_p 等 |
| `next` | `string[]` | ❌ | 后续节点路径 |

### 2.5 TOOL_CALL (工具调用)

```yaml
## /main/search
type: tool_call
tool_name: web_search
arguments:
  query: "{{user_query}}"
  limit: "5"
output_keys: ["search_results"]
next: ["/main/parse"]
```

| 字段 | 类型 | 必要 | 说明 |
|------|------|------|------|
| `tool_name` | `string` | ✅ | 注册的工具名 |
| `arguments` | `map<string, string>` | ❌ | 参数映射，值支持 Inja 模板 |
| `output_keys` | `string[]` | ❌ | 输出变量。单 key 直接存，多 key 从 JSON 对象提取 |
| `next` | `string[]` | ❌ | 后续节点路径 |

### 2.6 GENERATE_SUBGRAPH (动态图生成)

```yaml
## /main/gen_plan
type: generate_subgraph
prompt: |
  根据需求: {{requirement}}
  生成 AgenticDSL Markdown 执行图。
  输出格式:
  ### AgenticDSL '/dynamic/plan'
  ## /dynamic/plan/step1
  type: assign
  ...
output_keys: ["generated_path"]
signature_validation: warn
next: ["/main/execute"]
```

**执行流程：**

```
  ┌──────────┐    ┌──────────┐    ┌──────────────┐    ┌──────────┐
  │ 渲染prompt │ → │ 调用 LLM │ → │ parse_from_   │ → │ 追加到    │
  │ (Inja)   │    │ generate  │    │ string()     │    │ 调度图    │
  └──────────┘    └──────────┘    └──────────────┘    └──────────┘
```

| 字段 | 类型 | 必要 | 说明 |
|------|------|------|------|
| `prompt` | `string` | ✅ | 指导 LLM 生成 DSL 的 prompt |
| `output_keys` | `string[]` | ✅ | 输出生成的图路径 |
| `signature_validation` | `string` | ❌ | strict/warn/ignore (v3.1) |
| `on_signature_violation` | `string` | ❌ | 验证失败时跳转路径 |

**约束**：
- 生成的图路径必须以 `/dynamic/` 开头
- 调用者需要在 context 中预先注入 `__rendered_prompt__` 字段
- 签名验证逻辑当前是 stub（warn 模式只打印不阻止）

### 2.7 ASSERT (条件断言)

```yaml
## /main/check
type: assert
condition: "{{result.status}} == success"
on_failure: "/main/retry"
next: ["/main/continue"]
```

| 字段 | 类型 | 必要 | 说明 |
|------|------|------|------|
| `condition` | `string` | ✅ | Inja 表达式，渲染后为 `"true"`/`"false"` |
| `on_failure` | `string` | ❌ | 断言失败时跳转路径 |
| `next` | `string[]` | ❌ | 断言通过时后续节点 |

**条件求值逻辑：**
1. 渲染条件字符串
2. 如果结果为 `"true"` → 通过
3. 如果结果为 `"false"` → 失败
4. 如果为数字 → 0=false, 非0=true
5. 其他值 → 抛出异常

### 2.8 RESOURCE (声明资源 — stub)

```yaml
## /main/db
type: resource
resource_type: postgres
uri: "postgresql://localhost/db"
scope: global
```

**⚠️ 当前仅声明，不建立实际连接。**

支持的 `resource_type`:
- `file`, `postgres`, `mysql`, `sqlite`, `api_endpoint`, `vector_store`, `custom`

### 2.9 FORK / JOIN (并行 — ❌ 未实现)

```yaml
## /main/fork
type: fork
branches: ["/branch/a", "/branch/b"]
---
## /main/join
type: join
wait_for: ["/branch/a", "/branch/b"]
merge_strategy: last_write_wins
```

**当前状态：`execute_fork()` 和 `execute_join()` 均抛出 `runtime_error`。**

### 2.10 LLM_CALL (废弃)

```yaml
## /main/old
type: llm_call
prompt: "..."
output_keys: ["response"]
```

**⚠️ v3.10 起废弃，使用 `dsl_call` 替代。**

---

## 3. Superpowers 技能能力对照

### 3.1 技能模式总览

| 技能 | 核心功能 | 执行模式 | 资源管理 | LLM依赖 | AgenticDSL可行性 |
|------|---------|---------|---------|---------|----------------|
| **brainstorming** | 需求探索→设计 | 顺序对话 | 文件 I/O | ✅ 高 | ⚠️ 部分 |
| **writing-plans** | 生成实现计划 | 顺序任务 | 文件 I/O | ✅ 高 | ✅ 主要 |
| **executing-plans** | 执行计划 | 顺序+验证 | 无 | ❌ | ✅ |
| **subagent-driven-development** | 子任务分发 | **并行派发** | 子agent隔离 | ✅ 高 | ⚠️ 受限 |
| **dispatching-parallel-agents** | 并行调查 | **真并行** | 无共享状态 | ❌ | ❌ |
| **finishing-a-development-branch** | 分支收尾 | 顺序菜单 | git VCS | ❌ | ✅ |
| **systematic-debugging** | 根因分析 | 顺序4阶段 | 无 | ✅ | ⚠️ 部分 |
| **verification-before-completion** | 验证门禁 | 顺序检查 | 命令行 | ❌ | ✅ |
| **using-git-worktrees** | 工作区隔离 | 顺序检测 | **git worktree** | ❌ | ⚠️ 受限 |
| **test-driven-development** | TDD 循环 | 顺序循环 | 测试框架 | ❌ | ✅ |
| **writing-skills** | 编写技能 | 顺序 TDD | 文件 I/O | ✅ | ⚠️ 部分 |
| **receiving-code-review** | 审核反馈 | 顺序处理 | 无 | ✅ | ⚠️ 部分 |

### 3.2 并行模式详解

#### Superpowers 如何实现并行

**`dispatching-parallel-agents`**（真正的并行模式）：

```
┌──────────────────────────────────────────────────┐
│                    Orchestrator                    │
├──────────────────────────────────────────────────┤
│                                                    │
│   task(agent=A, run_in_background=true)  ──────►  │
│   task(agent=B, run_in_background=true)  ──────►  │
│   task(agent=C, run_in_background=true)  ──────►  │
│                                                    │
│   wait_all() ◄──────── results collection          │
└──────────────────────────────────────────────────┘
```

**关键特征：**
- 每个子 agent 完全独立（独立 context、独立文件系统视角）
- 无共享状态，无数据竞争
- 父 agent 负责协调和汇总
- **这不是 AgenticDSL 内部的并行**——这是 agent 层面的任务派发

**`subagent-driven-development`**（顺序派发+审查）：

```
┌──────────────────────────────────────────┐
│  Task 1: dispatch implementer → review   │
│  Task 2: dispatch implementer → review   │
│  Task 3: dispatch implementer → review   │
└──────────────────────────────────────────┘
```

**关键特征：**
- 任务之间是顺序的（一个完成后再开始下一个）
- 但子 agent 可并行工作于同一个任务内
- 每任务后有两阶段 review：spec compliance → code quality

#### AgenticDSL 无法实现真并行的原因

```cpp
Context NodeExecutor::execute_fork(const ForkNode* node, const Context& ctx) {
    throw std::runtime_error(
        "ForkNode execution requires concurrent scheduler support, "
        "not implemented in NodeExecutor.");
}

Context NodeExecutor::execute_join(const JoinNode* node, const Context& ctx) {
    throw std::runtime_error(
        "JoinNode execution requires concurrent scheduler support, "
        "not implemented in NodeExecutor.");
}
```

- Fork/Join 实现抛出了 `runtime_error`
- Context 是全局可变 `nlohmann::json`——并行有数据竞争风险
- 调度器（`TopoScheduler`）目前是单线程拓扑执行

### 3.3 资源管理详解

#### Superpowers 资源管理方式

| 资源类型 | 管理方式 | 对应技能 | AgenticDSL 方式 |
|---------|---------|---------|----------------|
| **文件系统** | 直接读写文件 | 全部技能 | `tool_call` + 文件工具 |
| **git 工作区** | `git worktree` 命令 | `using-git-worktrees` | 未内置 |
| **子 agent** | `task()` API | 并行技能 | 未内置 |
| **测试框架** | bash 命令 | TDD 技能 | `tool_call` |
| **工具** | 内置工具集 | 全部技能 | ToolRegistry |

#### AgenticDSL 有限的资源管理

```yaml
## /resources/db_pool
type: resource
resource_type: postgres
uri: "postgresql://..."
scope: global
```

**当前仅声明不连接。** 标准库 5 个文件中 3 个是 0 字节 stub:

```
lib/
├── auth/verify_session.md         (0 bytes - stub)
├── human/confirm_action.md        (0 bytes - stub)
├── human/clarify_input.md         (0 bytes - stub)
├── math/add.md                    (实现: 两数相加)
└── utils/noop.md                  (实现: 空操作)
```

实际资源管理需要依赖 `tool_call` 节点调用外部工具：

```yaml
## /main/create_worktree
type: tool_call
tool_name: bash
arguments:
  command: "git worktree add {{branch}} {{base_branch}}"
output_keys: ["worktree_path"]
```

---

## 3.4 上下文生成与注入机制对比

这是两个系统最根本的架构差异，也是理解 AgenticDSL 设计哲学的关键。

### 3.4.1 AgenticDSL 的上下文注入机制

AgenticDSL 只在 **一个位置** 进行上下文注入——`generate_subgraph` 节点：

```cpp
// execution_session.cpp:71-77
// → 注入发生在 generate_subgraph 执行时
std::string ExecutionSession::inject_subgraphs_into_prompt(
    const std::string& base_prompt, const Context& context) const {
    Context ctx = context;
    // 构建 available_subgraphs（库子图 + 动态子图）
    ctx["available_subgraphs"] = build_available_subgraphs_context();
    // 用 Inja 模板引擎渲染到 prompt 中
    return InjaTemplateRenderer::render(base_prompt, ctx);
}

// execution_session.cpp:32-69
// → 注入的内容是子图的签名信息
nlohmann::json ExecutionSession::build_available_subgraphs_context() const {
    nlohmann::json libs = nlohmann::json::array();
    // 1. 静态标准库（/lib/**）
    for (const auto& entry : loader.get_available_libraries()) {
        if (entry.is_subgraph) {
            libs.push_back({
                {"path", entry.path},
                {"signature", { {"outputs", entry.output_schema} }},
                {"permissions", entry.permissions},
                {"stability", "stable"}
            });
        }
    }
    // 2. 动态子图（/dynamic/**）
    for (const auto& graph : *full_graphs_) {
        if (graph.path.rfind("/dynamic/", 0) == 0 && graph.output_schema) {
            libs.push_back({{"path", graph.path}, ...{"stability", "dynamic"}});
        }
    }
    return libs;
}
```

**关键特征：**
- **注入时机**：仅在 `generate_subgraph` 节点执行时
- **注入内容**：子图签名元数据（路径、输入输出、权限），**不是完整文档**
- **注入方式**：通过 Inja 模板引擎渲染到 prompt 字符串中
- **LLM 使用方式**：LLM 根据可用子图的签名生成 **新 DSL**（`### AgenticDSL '/dynamic/...'`）
- **输出处理**：解析为 `ParsedGraph` 结构 → 追加到 DAG → 继续执行

```
AgenticDSL 上下文流:
═══════════════════════

parse_from_string() → ParsedGraph → TopoScheduler → NodeExecutor
                            │
                      generate_subgraph 节点
                            │
                    ┌───────▼────────┐
                    │  inject_subgraphs │
                    │  _into_prompt()   │
                    └───────┬────────┘
                            │ 注入 available_subgraphs (签名列表)
                            ▼
                    ┌───────────────┐
                    │  LLM 生成 DSL  │  ← LLM输出: "AgenticDSL '/dynamic/...'"
                    └───────┬───────┘
                            │ parse_from_string()
                            ▼
                    ParsedGraph (新节点)
                            │
                    append_dynamic_graphs()
                            │
                            ▼
                    继续执行新图
```

### 3.4.2 Superpowers Skills 的上下文注入机制

Superpowers 的上下文注入发生在 **Agent 的系统提示层**：

```
Superpowers 上下文流:
══════════════════════

系统提示构建 (会话开始):
  System Prompt
  ├── 角色定义
  ├── AGENTS.md / CLAUDE.md
  ├── Skill 定义 ─── 完整 SKILL.md 内容 ← 整个文档注入
  └── 工具定义

用户输入
    │
    ▼
LLM Reasoning (每步)
    │
    ├── 读取系统提示中的 skill 内容
    ├── 判断 "这个场景需要 brainstorming 技能"
    ├── 按 brainstorming 的步骤执行
    │     ├── step 1: 探索项目上下文
    │     ├── step 2: 问澄清问题
    │     └── step 3: 提方案
    ├── 生成工具调用 (不是 DSL，是直接操作)
    └── 等待结果 → 继续推理
```

**关键特征：**
- **注入时机**：始终在系统提示中（整个会话生命周期）
- **注入内容**：完整的 `SKILL.md` 文档（所有步骤、规则、示例）
- **注入方式**：直接拼接到系统提示文本中
- **LLM 使用方式**：LLM 阅读技能内容，理解流程，每一步自主决策
- **输出处理**：LLM 直接输出工具调用或文本——没有中间解析层

### 3.4.3 核心差异对比

| 维度 | AgenticDSL | Superpowers Skills |
|------|-----------|-------------------|
| **注入时机** | 仅 `generate_subgraph` 节点运行时 | 整个会话生命周期 |
| **注入内容** | 子图签名（路径+签名+权限） | 完整 SKILL.md（全部步骤和规则） |
| **信息密度** | 低（仅元数据） | 高（全文档） |
| **注入方式** | Inja 模板渲染到 prompt | 直接拼接到系统提示 |
| **LLM 看到什么** | "可用子图: [{path: /lib/math/add, signature: ...}]" | brainstorming 的完整 9 步流程 |
| **LLM 输出** | DSL 标记 (`### AgenticDSL '/dynamic/...'`) | 工具调用 + 文本 |
| **输出处理** | `parse_from_string()` 解析为图 | 无中间层，直接执行工具 |
| **Context 影响范围** | 仅影响 `generate_subgraph` 的 DSL 生成 | 影响 LLM 的所有决策 |
| **Token 消耗（上下文）** | 低（仅在生成子图时注入） | 高（始终占用系统提示窗口） |
| **技能组合方式** | 通过 `available_subgraphs` 签名发现 | 多个 SKILL.md 都在系统提示中 |

### 3.4.4 从代码看差异

```cpp
// ===== AgenticDSL: 注入的是元数据 =====
// execution_session.cpp:32-69
ctx["available_subgraphs"] = [
    {
        "path": "/lib/math/add",
        "signature": {"outputs": {"type": "number"}},
        "stability": "stable"
    },
    {
        "path": "/dynamic/step_3",
        "signature": {"outputs": {"result": {"type": "string"}}},
        "stability": "dynamic"
    }
];
// ↓ LLM 根据这些签名决定生成什么样的 DSL 图

// ===== Superpowers: 注入的是完整文档 =====
// SKILL.md 全文在系统提示中:
// ---
// name: brainstorming
// ## 步骤
// 1. 探索项目上下文
// 2. 问澄清问题（一次一个）
// 3. 提 2-3 个方案
// 4. 呈现设计
// ...
// ---
// ↓ LLM 阅读整个流程，自主决定如何执行每一步
```

### 3.4.5 对架构设计的影响

这个差异决定了两个系统各自的优劣势：

| 系统 | 优势 | 劣势 |
|------|------|------|
| **AgenticDSL** | 上下文窗口仅 DSL 解析时占用，token 效率高；确定性执行可复现；DSL 图可以版本控制 | LLM 缺乏全局上下文感知；动态行为仅限于 `generate_subgraph` 的 DSL 输出 |
| **Superpowers** | LLM 始终了解所有技能，能灵活组合；可以处理未预定义的复杂场景 | 始终消耗大量系统提示 token；行为不确定；技能变更需重新加载系统提示 |

### 3.4.6 Oracle 监督协程的引入

为了解决 AgenticDSL 全局上下文感知不足的问题，增强七引入的 **Oracle 监督协程** 提供了一种混合机制：

```
AgenticDSL + Oracle 的上下文分层:
══════════════════════════════════

层 1: DAG 本地上下文 (Context JSON)
  └── 当前节点的局部变量
  └── 通过 assign / dsl_call 填充
  
层 2: generate_subgraph 注入 (available_subgraphs)
  └── 仅在需要生成动态 DSL 时注入
  └── 子图签名元数据

层 3: Oracle 监督上下文 (reflection 节点)
  └── 执行路径摘要（已执行+待执行节点）
  └── 上下文快照（关键值）
  └── 性能指标（失败率、重试次数、预算使用）
  └── 注入方式: EventBus 事件 → OracleSupervisor → LLM
```

这种分层保证了：
1. **大部分执行**：只有第 1 层（低成本）
2. **动态 DSL 生成**：需要第 2 层（中等成本）
3. **运行时自适应**：需要第 3 层（高成本，仅在反射点）

---

## 4. 能力矩阵与局限性

### 4.1 能力覆盖矩阵

| 能力维度 | Superpowers Skills | AgenticDSL | 差距 |
|---------|-------------------|------------|------|
| **顺序执行** | ✅ `todowrite` + 步骤 | ✅ `next: [...]` | 无 |
| **LLM 思考** | ✅ `task(oracle)` | ✅ `dsl_call` | 无 |
| **条件分支** | ✅ `if` 判断 | ✅ `assert` + `on_failure` | 无 |
| **变量传递** | ✅ context 参数 | ✅ Context (nlohmann::json) | 无 |
| **工具调用** | ✅ 内置工具 | ✅ `tool_call` + ToolRegistry | 无 |
| **动态图生成** | ✅ `generate_subgraph` | ✅ `GENERATE_SUBGRAPH` | 无 |
| **循环** | ✅ `while` | ⚠️ 递归 generate_subgraph | 有限 |
| **用户交互** | ✅ `question` 工具 | ❌ 无 `user_input` 实现 | 缺失 |
| **真并行** | ✅ agent 级并行 | ❌ Fork/Join 未实现 | 根本差距 |
| **子 agent 隔离** | ✅ `task()` 独立上下文 | ❌ 无子 agent 机制 | 根本差距 |
| **资源管理** | ✅ git worktree/文件 | ⚠️ 仅声明，不连接 | 需扩展 |
| **标准库** | ✅ 14+ 个技能 | ⚠️ 5 个中 3 个 stub | 需填充 |
| **签图验证** | ✅ 运行时校验 | ⚠️ 框架有但逻辑 stub | 需实现 |
| **模板渲染** | ✅ 字符串插值 | ✅ Inja 模板引擎 | 无 |
| **预算控制** | ❌ 无 | ✅ ExecutionBudget | AgenticDSL 优势 |

### 4.2 关键局限性

```
┌────────────────────────────────────────────────────┐
│                    AgenticDSL 局限性                  │
├────────────────────────────────────────────────────┤
│                                                    │
│  ❌ Fork/Join 未实现 → 无法并行执行                  │
│  ❌ 无子 agent → 无法派发独立任务                     │
│  ❌ 标准库 60% stub → 可复用能力少                    │
│  ❌ 资源声明不连接 → 无实际资源管理                   │
│  ❌ 无用户输入 → 无法交互式工作流                    │
│  ❌ Context 全局可变 → 并行不安全                    │
│  ❌ 无持久化/store → 状态无法持久化                  │
│                                                    │
└────────────────────────────────────────────────────┘
```

---

## 5. AgenticDSL 实现示例

### 5.1 顺序执行流程 (对应 writing-plans / executing-plans)

```markdown
### AgenticDSL '/plan_executor'

## /__meta__
execution_budget:
  max_llm_calls: 30
  max_total_nodes: 40

## /pe/start
type: start
next: ["/pe/load_requirement"]

## /pe/load_requirement
type: assign
assign:
  requirement: "{{user_input|default:'默认需求'}}"
  attempts: "0"
next: ["/pe/analyze"]

## /pe/analyze
type: dsl_call
llm_tool: llama-7b
output_keys: ["analysis"]
prompt: |
  分析以下需求，输出结构化分析（目标、约束、风险）:
  {{requirement}}
  
  输出格式:
  目标: 
  约束:
  风险:
  方案:
next: ["/pe/create_plan"]

## /pe/create_plan
type: dsl_call
llm_tool: llama-7b
output_keys: ["plan_dsl"]
prompt: |
  基于分析: {{analysis}}
  生成一个 AgenticDSL Markdown 执行计划。
  
  要求:
  - 每个任务 2-5 分钟
  - 包含 TDD 步骤
  - 输出完整的可执行图
next: ["/pe/validate_plan"]

## /pe/validate_plan
type: assert
condition: "{{plan_dsl|length}} > 50"
on_failure: "/pe/retry"
next: ["/pe/execute_plan"]

## /pe/retry
type: assign
assign:
  attempts: "{{attempts|add:1}}"
next: ["/pe/create_plan"]

## /pe/execute_plan
type: generate_subgraph
prompt: |
  执行以下计划并生成结果:
  {{plan_dsl}}
output_keys: ["execution_result"]
signature_validation: warn
next: ["/pe/verify"]

## /pe/verify
type: assert
condition: "{{execution_result.success}} == true"
on_failure: "/pe/failed"
next: ["/pe/success"]

## /pe/success
type: assign
assign:
  status: "success"
  output: "计划执行完成: {{execution_result}}"
next: ["/pe/end"]

## /pe/failed
type: assign
assign:
  status: "failed"
  error: "执行失败"
next: ["/pe/end"]

## /pe/end
type: end
```

### 5.2 条件跳转流程 (对应 TDD 红绿循环)

```markdown
### AgenticDSL '/tdd_cycle'

## /tdd/start
type: start
next: ["/tdd/write_test"]

## /tdd/write_test
type: assign
assign:
  test_code: "{{test_template}}"
next: ["/tdd/run_test"]

## /tdd/run_test
type: tool_call
tool_name: bash
arguments:
  command: "make test ARGS='--filter {{test_name}}'"
output_keys: ["test_output"]
next: ["/tdd/check_fail"]

## /tdd/check_fail
type: assert
condition: "{{test_output|find:'FAILED'}}"
on_failure: "/tdd/test_unexpected_pass"
next: ["/tdd/implement"]

## /tdd/implement
type: dsl_call
llm_tool: llama-7b
output_keys: ["implementation_code"]
prompt: |
  测试已失败。实现最小代码使其通过。
  
  测试代码: {{test_code}}
  当前实现文件: {{implementation_context}}
  
  输出最小实现代码。
next: ["/tdd/apply_code"]

## /tdd/apply_code
type: tool_call
tool_name: write_file
arguments:
  path: "{{implementation_path}}"
  content: "{{implementation_code}}"
output_keys: ["write_result"]
next: ["/tdd/rerun_test"]

## /tdd/rerun_test
type: tool_call
tool_name: bash
arguments:
  command: "make test ARGS='--filter {{test_name}}'"
output_keys: ["test_result"]
next: ["/tdd/check_pass"]

## /tdd/check_pass
type: assert
condition: "{{test_result|find:'PASSED'}}"
on_failure: "/tdd/retry_implement"
next: ["/tdd/refactor"]

## /tdd/retry_implement
type: assign
assign:
  attempts: "{{attempts|default:0|add:1}}"
next: ["/tdd/implement"]

## /tdd/test_unexpected_pass
type: dsl_call
llm_tool: llama-7b
output_keys: ["fix_message"]
prompt: |
  测试在实现前就通过了。测试可能有问题。
  请分析: {{test_code}}
next: ["/tdd/end"]

## /tdd/refactor
type: dsl_call
llm_tool: llama-7b
output_keys: ["refactored_code"]
prompt: |
  测试通过。重构实现代码:
  {{implementation_code}}
  
  保持测试通过，改进代码质量。
next: ["/tdd/end"]

## /tdd/end
type: end
```

### 5.3 调试流程 (对应 systematic-debugging)

```markdown
### AgenticDSL '/debug_investigation'

## /dbg/start
type: start
next: ["/dbg/collect_evidence"]

## /dbg/collect_evidence
type: dsl_call
llm_tool: llama-7b
output_keys: ["error_analysis"]
prompt: |
  调试问题: {{error_description}}
  
  错误信息: {{error_log}}
  代码上下文: {{code_context}}
  最近变更: {{recent_changes}}
  
  请分析根因，输出:
  1. 症状
  2. 可能原因
  3. 验证方法
  4. 修复方案
next: ["/dbg/create_hypothesis"]

## /dbg/create_hypothesis
type: assign
assign:
  hypothesis: "{{error_analysis.causes}}"
  fix_plan: "{{error_analysis.fix}}"
  verify_method: "{{error_analysis.verification}}"
next: ["/dbg/implement_fix"]

## /dbg/implement_fix
type: dsl_call
llm_tool: llama-7b
output_keys: ["fix_patch"]
prompt: |
  根据修复方案实施修复:
  
  根因: {{hypothesis}}
  修复方案: {{fix_plan}}
  受影响文件: {{affected_files}}
  
  输出最小化的代码修改。
next: ["/dbg/apply_and_verify"]

## /dbg/apply_and_verify
type: tool_call
tool_name: bash
arguments:
  command: "make test"
output_keys: ["test_result"]
next: ["/dbg/check_success"]

## /dbg/check_success
type: assert
condition: "{{test_result|find:'FAILED'}}"
on_failure: "/dbg/fix_success"
next: ["/dbg/iterate"]

## /dbg/iterate
type: assign
assign:
  iteration: "{{iteration|default:0|add:1}}"
  previous_hypothesis: "{{hypothesis}}"
next: ["/dbg/collect_evidence"]

## /dbg/fix_success
type: assign
assign:
  fix_applied: "true"
  final_status: "fixed"
next: ["/dbg/end"]

## /dbg/end
type: end
```

### 5.4 工具链调用（对应 verification-before-completion / finishing-a-development-branch）

```markdown
### AgenticDSL '/branch_finisher'

## /bf/start
type: start
next: ["/bf/verify_tests"]

## /bf/verify_tests
type: tool_call
tool_name: bash
arguments:
  command: "make test && echo 'TESTS_PASSED' || echo 'TESTS_FAILED'"
output_keys: ["test_output"]
next: ["/bf/check_tests"]

## /bf/check_tests
type: assert
condition: "{{test_output|find:'TESTS_PASSED'}}"
on_failure: "/bf/fix_tests"
next: ["/bf/detect_environment"]

## /bf/fix_tests
type: assign
assign:
  need_fix: "true"
  status: "tests_failing"
next: ["/bf/end"]

## /bf/detect_environment
type: tool_call
tool_name: bash
arguments:
  command: |
    echo "DIR: $(git rev-parse --git-dir)"
    echo "COMMON: $(git rev-parse --git-common-dir)"
    echo "BRANCH: $(git branch --show-current)"
output_keys: ["env_info"]
next: ["/bf/check_env"]

## /bf/check_env
type: assign
assign:
  is_worktree: "{{env_info.DIR != env_info.COMMON}}"
  branch_name: "{{env_info.BRANCH}}"
next: ["/bf/present_options"]

## /bf/present_options
type: tool_call
tool_name: user_choice
arguments:
  prompt: "实现完成。请选择: 1) Merge 2) PR 3) 保留 4) 丢弃"
  options: '["merge","pr","keep","discard"]'
output_keys: ["user_choice"]
next: ["/bf/execute_choice"]

## /bf/execute_choice
type: dsl_call
llm_tool: llama-7b
output_keys: ["action_plan"]
prompt: |
  用户选择: {{user_choice}}
  当前分支: {{branch_name}}
  工作目录: {{env_info}}
  
  输出执行该选择所需的命令序列。
next: ["/bf/perform_cleanup"]

## /bf/perform_cleanup
type: tool_call
tool_name: bash
arguments:
  command: "{{action_plan.commands}}"
output_keys: ["cleanup_result"]
next: ["/bf/end"]

## /bf/end
type: end
```

### 5.5 动态图生成（技能自举 — 最强大的模式）

```markdown
### AgenticDSL '/skill_self_bootstrapping'

## /sb/start
type: start
next: ["/sb/load_skill_context"]

## /sb/load_skill_context
type: assign
assign:
  available_nodes: |
    start, end, assign, dsl_call, tool_call,
    generate_subgraph, assert
  skill_goal: "{{user_skill_description}}"
  existing_skills: "{{installed_skills|default:'无'}}"
next: ["/sb/analyze_requirement"]

## /sb/analyze_requirement
type: dsl_call
llm_tool: llama-7b
output_keys: ["skill_analysis"]
prompt: |
  需要创建一个新技能: {{skill_goal}}
  
  可用节点: {{available_nodes}}
  已有技能: {{existing_skills}}
  
  分析:
  1. 这个技能的核心工作流是什么？
  2. 需要哪些 AgenticDSL 节点？
  3. 有没有已有的技能可以组合？
  4. 输出技能的设计方案。
next: ["/sb/generate_skill_dsl"]

## /sb/generate_skill_dsl
type: dsl_call
llm_tool: llama-7b
output_keys: ["skill_dsl"]
prompt: |
  基于方案: {{skill_analysis}}
  
  生成一个完整的 AgenticDSL Markdown 文件。
  
  它应该:
  - 以 `### AgenticDSL '/skill/<name>'` 开头
  - 包含 /__meta__ 预算配置
  - 包含 START → ... → END 的完整流程
  - 使用正确的节点类型和参数
  
  输出完整可执行的 DSL。
next: ["/sb/validate_dsl"]

## /sb/validate_dsl
type: assert
condition: "{{skill_dsl|find:'AgenticDSL'}}"
on_failure: "/sb/regenerate"
next: ["/sb/execute_skill"]

## /sb/regenerate
type: assign
assign:
  retry_count: "{{retry_count|default:0|add:1}}"
next: ["/sb/generate_skill_dsl"]

## /sb/execute_skill
type: generate_subgraph
prompt: |
  {{skill_dsl}}
output_keys: ["skill_execution_result"]
signature_validation: ignore
next: ["/sb/evaluate_result"]

## /sb/evaluate_result
type: dsl_call
llm_tool: llama-7b
output_keys: ["evaluation"]
prompt: |
  技能执行结果: {{skill_execution_result}}
  
  评估这个技能的有效性:
  1. 是否达到了目标: {{skill_goal}}
  2. 有什么可以改进的
  3. 是否能作为标准库使用
next: ["/sb/end"]

## /sb/end
type: end
```

### 5.6 标准库子图 (组合复用模式)

```markdown
# lib/math/add.md — 标准库子图

### AgenticDSL '/lib/math/add'
signature: "(a: number, b: number) -> {result: number}"

## /lib/math/add/start
type: start
next: ["/lib/math/add/compute"]

## /lib/math/add/compute
type: assign
assign:
  result: "{{a|add:b}}"
  operation: "add"
next: ["/lib/math/add/validate"]

## /lib/math/add/validate
type: assert
condition: "{{result}} >= {{a}} && {{result}} >= {{b}}"
on_failure: "/lib/math/add/error"
next: ["/lib/math/add/end"]

## /lib/math/add/end
type: end

## /lib/math/add/error
type: end
```

---

## 6. 从对比中得到的结论

### 6.1 AgenticDSL 擅长什么

| 领域 | 适合度 | 说明 |
|------|--------|------|
| **确定性工作流** | ✅ 高 | 顺序执行、条件分支、工具调用 |
| **LLM 编排** | ✅ 高 | dsl_call + generate_subgraph 组合 |
| **动态流程生成** | ✅ 高 | LLM 生成新 DSL → 执行 |
| **预算控制** | ✅ 高 | ExecutionBudget 内置支持 |
| **简单验证循环** | ✅ 高 | assert + on_failure 实现重试 |

### 6.2 核心差距：确定性 vs 灵活性

```
Superpowers Skills = LLM 即运行时
══════════════════════════════════
每步都由 LLM 决策 → 高度灵活但非确定
LLM 全程参与 "思考" → 昂贵但能自适应

AgenticDSL = DSL 即运行时
═══════════════════════════
预定义 DAG 确定性执行 → 高效但缺乏适应性
LLM 只在特定节点介入 → 路径确定后无法调整

差距本质: AgenticDSL 在运行时无法 "思考" 和 "调整"
```

AgenticDSL 的 DSL 图一旦生成，执行路径就确定了。当出现以下情况时，它无法自适应：
- 多次重试仍然失败（不知道换个策略）
- 上下文出现预期外的值（不知道调整）
- 循环检测到死循环（不知道跳出改道）
- 用户中途改变需求（不知道重新规划）
- 多个子目标冲突（不知道权衡优先级）

### 6.3 解决方案：Oracle 监督协程

在确定性 DAG 中引入**反射点（Reflection Point）**——DAG 执行到这些点时暂停，调用 LLM "先知"分析当前执行状态并给出纠正指令：

```
无监督模式 (纯 AgenticDSL)
┌─────┐  ┌─────┐  ┌─────┐  ┌─────┐
│ A   │→│ B   │→│ C   │→│ D   │  → 确定但僵化
└─────┘  └─────┘  └─────┘  └─────┘

反射点监督 (推荐)
┌─────┐  ┌─────┐  ╔═════╗  ┌─────┐  ┌─────┐
│ A   │→│ B   │→║Oracle║→│ C'  │→│ D   │  → 关键点注入智力
└─────┘  └─────┘  ╚═════╝  └─────┘  └─────┘
                     │
                LLM 分析路径
                输出纠正指令
                (重定向/改上下文/插节点)

持续监督 (高性能模式)
     ╔═════╗     ╔═════╗     ╔═════╗
     ║Oracle║ ←─ ║Oracle║ ←─ ║Oracle║ ←─ 持续监控
     ╚═════╝     ╚═════╝     ╚═════╝
         │           │           │
     ┌───▼───┐  ┌───▼───┐  ┌───▼───┐
     │ A→B→C │  │ D→E→F │  │ G→H→I │  → 高度自适应
     └───────┘  └───────┘  └───────┘
```

**核心机制**：
1. `reflect` 节点：标记图中需要 LLM 分析的位置
2. `correct` 节点：应用 Oracle 的纠正指令（重定向/改上下文/插子图）
3. Oracle 监督协程：与主调度器并行运行，通过 EventBus 通信

详见 `AGENTICDSL_ENHANCEMENT_ROADMAP.md` 增强七。

### 6.4 AgenticDSL 需要扩展的

| 领域 | 优先级 | 建议 |
|------|--------|------|
| **Fork/Join 实现** | 🔴 高 | 实现真正的并行执行 |
| **标准库填充** | 🔴 高 | 实现子图组合能力 |
| **Oracle 监督协程** | 🔴 高 | 弥补运行时智力差距 |
| **资源管理** | 🟡 中 | 实现资源连接与释放 |
| **用户输入节点** | 🟡 中 | 添加 `user_input` 交互节点 |
| **Context 隔离** | 🟢 低 | 为并行执行做隔离 |

### 6.5 与 Superpowers 技能的核心差异

```
Superpowers Skills                    AgenticDSL + Oracle
─────────────────                     ──────────────────
Agent 级能力 (task, subagent)         DSL 级能力 (节点, 图)
运行时反射 (调取技能链)               reflect 节点 + Oracle 协程
自由上下文传递                        结构化 JSON Context
并行 = 派发独立 Agent                并行 = Fork/Join (协程)
资源 = git worktree/文件             资源 = Resource 节点 + tool_call
标准库 = 14+ 个技能                  标准库 = 20+ DSL 子图
LLM 每步决策                         LLM 在 DSL 节点 + 反射点决策
```

### 6.6 三种模式对比

| 维度 | 纯 AgenticDSL | + Oracle 反射点 | Superpowers Skills |
|------|--------------|----------------|-------------------|
| **执行模型** | 确定性 DAG | 确定 DAG + 反射点 | LLM 全程驱动 |
| **LLM 调用** | 仅在 dsl_call/generate_subgraph | 增加 reflect 节点调用 | 每步都调 |
| **适应性** | ❌ 低 | ✅✅ 中高 | ✅✅✅ 高 |
| **确定性** | ✅✅✅ 极高 | ✅✅ 高 | ❌ 低 |
| **成本** | 🟢 低 | 🟡 中 | 🔴 高 |
| **调试难度** | 🟢 易复现 | 🟡 可复现 | 🔴 难复现 |
| **适用场景** | 已知稳定流程 | 半结构化任务 | 高不确定性探索 |

### 6.7 用一句话总结

> **AgenticDSL 是一个声明式 DAG 执行引擎**，强于 LLM 编排和动态图生成。
> 通过 **Oracle 监督协程**（反射点 + 纠正注入），可以在保持确定性执行优势的同时，
> 在关键决策点获得 LLM 的智力支持——这是**高效确定性与灵活适应性之间的最佳平衡**。

