# ADR-0009: DSL 标准库规划

## 状态

**已批准** (2026-05-12)

## 背景

HydraForge 的 `lib/` 目录目前几乎为空，只有 `math/add.md` 有实现。根据调研：
- LangChain LCEL：Chain-based 推理模式
- DeerFlow 2.0：.hcl + .md 混合 DSL
- AutoGPT：策略分解模式
- 行业共识：ReAct loops + planning + tools 是核心原子

**设计目标**：
- Phase 1 有实用的 DSL 子图
- 用户可扩展
- 结构清晰易维护
- 与现有解析器兼容

---

## 决策

### 1. 目录结构：按功能域分层

```
lib/
├── reasoning/                   # 推理模式 (L3 - Cognitive)
│   ├── react.md              # ReAct 推理循环
│   └── plan.md               # 任务分解
│
├── workflow/                   # 工作流 (L2 - Execution)
│   ├── code_review.md        # 代码审查
│   └── data_analysis.md      # 数据分析
│
├── tools/                     # 工具封装 (L1 - Tools)
│   ├── fs/
│   │   ├── read.md
│   │   └── write.md
│   ├── git/
│   │   ├── clone.md
│   │   └── diff.md
│   └── net/
│       └── http_get.md
│
├── cognitive/                 # 认知策略 (L4)
│   ├── route.md             # 意图路由
│   └── summarize.md         # 摘要生成
│
└── math/                     # (已有)
    └── add.md
```

**路径约定**：
- 引用格式：`/lib/<category>/<name>`
- 示例：`/lib/reasoning/react`, `/lib/tools/fs/read`

### 2. 核心子图定义

#### 2.1 ReAct 推理循环 (Phase 1 高优先级)

```markdown
AgenticDSL `/lib/reasoning/react`
version: "3.10"
type: subgraph
description: "ReAct 推理循环：思考 → 行动 → 观察"

# 参数输入
variables:
  task: string              # 用户任务
  max_iterations: int = 5  # 最大迭代次数

# 内部状态
state:
  iteration: int = 0
  thought: string = ""
  action: string = ""
  observation: string = ""

AgenticDSL `/lib/reasoning/react/think`
type: tool_call
tool: llm.call
arguments:
  prompt: |
    你是一个推理助手。基于当前任务和历史信息，进行推理。

    任务：{{ $.task }}
    历史想法：{{ $.state.thought }}
    历史行动：{{ $.state.action }}
    历史观察：{{ $.state.observation }}

    请进行推理，给出下一步行动。格式：
    想法：[你的推理]
    行动：[要执行的工具名]
    参数：[工具参数，如果不需要则写"无"]

    推理：
next: "/lib/reasoning/react/act"

AgenticDSL `/lib/reasoning/react/act`
type: tool_call
tool: "{{ $.state.action }}"
arguments: {{ $.state.action_args }}
next: "/lib/reasoning/react/observe"

AgenticDSL `/lib/reasoning/react/observe`
type: tool_call
tool: llm.call
arguments:
  prompt: |
    观察行动结果。

    行动结果：{{ $.last_result }}

    如果任务已完成，给出"完成：[总结]"
    如果需要继续，给出"继续：[原因]"

    观察：
next: "/lib/reasoning/react/check"

AgenticDSL `/lib/reasoning/react/check`
type: decision
condition: "{{ $.iteration >= $.max_iterations }}"
on_true: "/lib/reasoning/react/finish"
on_false: "/lib/reasoning/react/think"
```

#### 2.2 任务分解 (Phase 1 高优先级)

```markdown
AgenticDSL `/lib/reasoning/plan`
version: "3.10"
type: subgraph
description: "将复杂任务分解为可执行的子任务"

variables:
  task: string

AgenticDSL `/lib/reasoning/plan/decompose`
type: tool_call
tool: llm.call
arguments:
  prompt: |
    将以下任务分解为 3-7 个可执行的子任务。

    任务：{{ $.task }}

    输出格式（JSON）：
    {
      "subtasks": [
        {"id": 1, "description": "子任务描述", "depends_on": []},
        ...
      ]
    }
output_keys: ["subtasks"]
next: "/lib/reasoning/plan/create_graph"

AgenticDSL `/lib/reasoning/plan/create_graph`
type: generate_subgraph
arguments:
  template: "sequential_tasks"
  tasks: "{{ $.subtasks }}"
next: "/main/execute_tasks"
```

#### 2.3 文件操作工具封装 (Phase 1 高优先级)

```markdown
AgenticDSL `/lib/tools/fs/read`
version: "3.10"
type: subgraph
description: "安全读取文件内容"

variables:
  path: string          # 文件路径（相对或绝对）
  max_lines: int = 0   # 0表示全部

AgenticDSL `/lib/tools/fs/read/validate`
type: assert
condition: "{{ not $.path.contains('..') and not $.path.starts_with('/etc') }}"
on_fail: "/lib/tools/fs/read/error"

AgenticDSL `/lib/tools/fs/read/execute`
type: tool_call
tool: fs.read
arguments:
  path: "{{ $.path }}"
  max_lines: "{{ $.max_lines }}"
output_keys: ["content"]

AgenticDSL `/lib/tools/fs/read/error`
type: end
error: "path_not_allowed"
```

#### 2.4 代码审查工作流 (Phase 2)

```markdown
AgenticDSL `/lib/workflow/code_review`
version: "3.10"
type: subgraph
description: "自动化代码审查工作流"

variables:
  repo_url: string
  pr_number: int = 0

AgenticDSL `/lib/workflow/code_review/clone`
type: tool_call
tool: git.clone
arguments:
  url: "{{ $.repo_url }}"
next: "/lib/workflow/code_review/fetch_pr"

AgenticDSL `/lib/workflow/code_review/review`
type: dsl_call
graph: "/lib/reasoning/react"
arguments:
  task: "审查代码变更，给出改进建议"
next: "/lib/workflow/code_review/summarize"
```

### 3. DSL 加载机制

**调研结论：StandardLibraryLoader 已支持子目录递归加载！**

```cpp
// library_loader.cpp - 已有实现
fs::recursive_directory_iterator iter(dir);
for (auto& entry : iter) {
    if (entry.path().extension() == ".md") {
        load_file(entry.path());
    }
}
```

**无需修改代码**，只需在 `lib/` 下创建子目录和 `.md` 文件。

### 4. 用户扩展机制

#### 4.1 用户本地库

```
~/.hydraforge/lib/           # 用户本地库目录
├── custom/
│   └── my_agent.md         # 用户自定义子图
└── override/
    └── code_review.md      # 覆盖标准库
```

#### 4.2 覆盖优先级

```
用户本地库 (~/.hydraforge/lib/) > 标准库 (lib/)
```

#### 4.3 加载顺序

```cpp
void StandardLibraryLoader::load_all() {
    // 1. 标准库
    load_from_directory("lib/");

    // 2. 用户本地库（覆盖标准库）
    auto user_lib = get_user_library_path();
    if (fs::exists(user_lib)) {
        load_from_directory(user_lib);  // 相同路径会覆盖
    }
}
```

### 5. 标准库 API

#### 5.1 LLM 可发现的子图列表

```cpp
// StandardLibraryLoader::build_available_subgraphs_context()
std::string build_available_subgraphs_context() {
    return R"(
可用标准库子图：

## 推理模式
- /lib/reasoning/react     - ReAct 推理循环
- /lib/reasoning/plan     - 任务分解

## 工作流
- /lib/workflow/code_review - 代码审查
- /lib/workflow/data_analysis - 数据分析

## 工具
- /lib/tools/fs/read       - 安全读取文件
- /lib/tools/fs/write     - 安全写入文件
- /lib/tools/git/clone    - Git 克隆
- /lib/tools/git/diff     - Git 差异

## 认知
- /lib/cognitive/route    - 意图路由
- /lib/cognitive/summarize - 摘要生成
)";
}
```

#### 5.2 DSL 中引用标准库

```yaml
AgenticDSL `/main/agent`
type: start
next: "/lib/reasoning/react"

AgenticDSL `/main/execute`
type: dsl_call
graph: "/lib/workflow/code_review"
arguments:
  repo_url: "https://github.com/example/repo"
next: "/main/end"
```

---

## Phase 1 vs Phase 2 实现计划

### Phase 1 (必须实现)

| # | 子图 | 描述 | 工作量 |
|---|------|------|--------|
| 1 | `/lib/reasoning/react` | ReAct 推理循环 | 1 周 |
| 2 | `/lib/reasoning/plan` | 任务分解 | 0.5 周 |
| 3 | `/lib/tools/fs/read` | 安全文件读取 | 0.5 周 |
| 4 | `/lib/tools/fs/write` | 安全文件写入 | 0.5 周 |
| 5 | 用户本地库机制 | `~/.hydraforge/lib/` | 0.5 周 |

### Phase 2 (扩展功能)

| # | 子图 | 描述 | 优先级 |
|---|------|------|--------|
| 6 | `/lib/workflow/code_review` | 代码审查工作流 | 🔴 高 |
| 7 | `/lib/tools/git/clone` | Git 克隆 | 🔴 高 |
| 8 | `/lib/tools/git/diff` | Git 差异 | 🟡 中 |
| 9 | `/lib/cognitive/route` | 意图路由 | 🟡 中 |
| 10 | `/lib/cognitive/summarize` | 摘要生成 | 🟡 中 |
| 11 | `/lib/tools/net/http_get` | HTTP GET | 🟢 低 |
| 12 | `/lib/workflow/data_analysis` | 数据分析 | 🟢 低 |

---

## 权衡

### 为什么按功能域分层？

| 方案 | 优点 | 缺点 |
|------|------|------|
| **按功能域（推荐）** | 语义清晰，查找直观，与架构分层对应 | 目录可能变深 |
| 按抽象层级 | 与 L1/L2/L3 对应 | 对用户不直观 |
| 扁平结构 | 简单 | 难以组织大量子图 |

**选择按功能域的理由**：
- 与 AgenticOS 8 层架构对应
- 用户按功能查找，无需理解抽象层级
- 未来扩展清晰

### 为什么不需要修改加载器？

StandardLibraryLoader 已使用 `fs::recursive_directory_iterator`，自动支持任意深度的子目录。

---

## 实现要求

### Phase 1 必须完成

| # | 任务 | 验证方式 |
|---|------|---------|
| 1 | 创建 `lib/reasoning/` 目录 | 目录存在 |
| 2 | 实现 `/lib/reasoning/react` | DSL 可引用并执行 |
| 3 | 实现 `/lib/reasoning/plan` | 任务分解正确 |
| 4 | 实现 `/lib/tools/fs/read` | 路径安全检查生效 |
| 5 | 实现用户本地库机制 | `~/.hydraforge/lib/` 可覆盖标准库 |

### 测试用例

```cpp
TEST_CASE("StandardLibrary loads reasoning subgraphs") {
    auto loader = StandardLibraryLoader();
    loader.load_all();

    auto subgraph = loader.find("/lib/reasoning/react");
    REQUIRE(subgraph != nullptr);
    CHECK(subgraph->type == NodeType::Subgraph);
}

TEST_CASE("User library overrides standard library") {
    // 创建用户本地库
    fs::create_directories("~/.hydraforge/lib/");
    write_file("~/.hydraforge/lib/reasoning/react.md", custom_react);

    auto loader = StandardLibraryLoader();
    loader.load_all();

    // 应加载用户版本
    auto subgraph = loader.find("/lib/reasoning/react");
    CHECK(subgraph->description == "Custom ReAct");
}
```

---

## 影响范围

| 组件 | 变更 |
|------|------|
| `lib/` | 新增子目录和 .md 文件 |
| `src/modules/library/library_loader.cpp` | 无需修改（已支持递归） |
| `docs/` | 更新标准库文档 |

---

## 替代方案

### 替代 1：扁平结构（被否决）

**否决理由**：难以组织大量子图，用户查找困难。

### 替代 2：按抽象层级分层（被否决）

**否决理由**：对用户不直观，需要理解 L1/L2/L3 概念。

### 替代 3：不支持用户扩展（被否决）

**否决理由**：用户无法自定义子图，限制实用性。

---

## 结论

采用按功能域分层 + 用户本地库覆盖机制：

- **目录结构**：`reasoning/` + `workflow/` + `tools/` + `cognitive/`
- **核心子图**：ReAct 推理循环 + 任务分解
- **加载机制**：无需修改（已支持递归）
- **用户扩展**：`~/.hydraforge/lib/` 覆盖标准库
- **Phase 1**：实现 reasoning + tools 基础

此设计支持：
- **Phase 1**：实用的 ReAct + 工具子图
- **Phase 2**：完整 workflow + cognitive 子图

---

*文档版本: v1.0*
*最后更新: 2026-05-12*