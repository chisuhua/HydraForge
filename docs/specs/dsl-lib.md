# DSL 标准库

## 版本

v3.10 (2026-05-13)

## 概述

DSL 标准库是 AgenticDSL 执行器的内置子图集合，提供可复用的推理模式、工作流、工具封装和认知策略。

## 目录结构

```
lib/
├── reasoning/                   # 推理模式
│   ├── react.md              # ReAct 推理循环
│   └── plan.md               # 任务分解
│
├── workflow/                   # 工作流
│   ├── code_review.md        # 代码审查
│   └── data_analysis.md      # 数据分析
│
├── tools/                     # 工具封装
│   ├── fs/
│   │   ├── read.md
│   │   └── write.md
│   ├── git/
│   │   ├── clone.md
│   │   └── diff.md
│   └── net/
│       └── http_get.md
│
├── cognitive/                 # 认知策略
│   ├── route.md             # 意图路由
│   └── summarize.md         # 摘要生成
│
└── math/                     # 数学工具
    └── add.md
```

### 路径约定

引用格式：`/lib/<category>/<name>`

示例：
- `/lib/reasoning/react`
- `/lib/tools/fs/read`
- `/lib/workflow/code_review`

---

## 标准库清单

| 路径 | 用途 | 稳定性 |
|------|------|--------|
| `/lib/reasoning/react` | ReAct 推理循环：思考 → 行动 → 观察 | stable |
| `/lib/reasoning/plan` | 任务分解：将复杂任务分解为可执行子任务 | stable |
| `/lib/tools/fs/read` | 安全读取文件内容 | stable |
| `/lib/tools/fs/write` | 安全写入文件内容 | stable |
| `/lib/tools/git/clone` | Git 仓库克隆 | stable |
| `/lib/tools/git/diff` | Git 差异比较 | stable |
| `/lib/tools/net/http_get` | HTTP GET 请求 | experimental |
| `/lib/workflow/code_review` | 自动化代码审查工作流 | stable |
| `/lib/workflow/data_analysis` | 数据分析工作流 | experimental |
| `/lib/cognitive/route` | 意图路由 | stable |
| `/lib/cognitive/summarize` | 摘要生成 | stable |
| `/lib/math/add` | 基础加法 | stable |

> 执行器必须预加载并校验以上子图。社区可扩展，但不得修改其 `signature`。

---

## 核心子图定义

### 1. ReAct 推理循环

**路径**: `/lib/reasoning/react`
**版本**: 3.10
**类型**: subgraph
**描述**: ReAct 推理循环：思考 → 行动 → 观察

```markdown
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

AgenticDSL `/lib/reasoning/react/finish`
type: end
output:
  result: "{{ $.observation }}"
```

### 2. 任务分解

**路径**: `/lib/reasoning/plan`
**版本**: 3.10
**类型**: subgraph
**描述**: 将复杂任务分解为可执行的子任务

```markdown
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

### 3. 文件读取工具

**路径**: `/lib/tools/fs/read`
**版本**: 3.10
**类型**: subgraph
**描述**: 安全读取文件内容

```markdown
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

### 4. 文件写入工具

**路径**: `/lib/tools/fs/write`
**版本**: 3.10
**类型**: subgraph
**描述**: 安全写入文件内容

```markdown
variables:
  path: string          # 文件路径
  content: string       # 内容
  max_size: int = 1048576  # 最大 1MB

AgenticDSL `/lib/tools/fs/write/validate`
type: assert
condition: "{{ not $.path.contains('..') and not $.path.starts_with('/etc') }}"
on_fail: "/lib/tools/fs/write/error"

AgenticDSL `/lib/tools/fs/write/validate_size`
type: assert
condition: "{{ $.content.length <= $.max_size }}"
on_fail: "/lib/tools/fs/write/error_size"

AgenticDSL `/lib/tools/fs/write/execute`
type: tool_call
tool: fs.write
arguments:
  path: "{{ $.path }}"
  content: "{{ $.content }}"
output_keys: ["success"]

AgenticDSL `/lib/tools/fs/write/error`
type: end
error: "path_not_allowed"

AgenticDSL `/lib/tools/fs/write/error_size`
type: end
error: "content_too_large"
```

### 5. Git 克隆

**路径**: `/lib/tools/git/clone`
**版本**: 3.10
**类型**: subgraph
**描述**: Git 仓库克隆

```markdown
variables:
  url: string              # 仓库 URL
  branch: string = "main"  # 分支名
  depth: int = 1           # 浅克隆深度

AgenticDSL `/lib/tools/git/clone/execute`
type: tool_call
tool: git.clone
arguments:
  url: "{{ $.url }}"
  branch: "{{ $.branch }}"
  depth: "{{ $.depth }}"
output_keys: ["repo_path"]
```

### 6. Git 差异

**路径**: `/lib/tools/git/diff`
**版本**: 3.10
**类型**: subgraph
**描述**: Git 差异比较

```markdown
variables:
  repo_path: string      # 仓库路径
  base: string = "HEAD"  # 基准 commit
  head: string = ""      # 对比 commit（空则使用工作区）

AgenticDSL `/lib/tools/git/diff/execute`
type: tool_call
tool: git.diff
arguments:
  repo_path: "{{ $.repo_path }}"
  base: "{{ $.base }}"
  head: "{{ $.head }}"
output_keys: ["diff_content"]
```

### 7. HTTP GET

**路径**: `/lib/tools/net/http_get`
**版本**: 3.10
**类型**: subgraph
**描述**: HTTP GET 请求

```markdown
variables:
  url: string                    # 目标 URL
  headers: map[string]string = {}  # 请求头
  timeout: int = 30000          # 超时（毫秒）

AgenticDSL `/lib/tools/net/http_get/execute`
type: tool_call
tool: http.get
arguments:
  url: "{{ $.url }}"
  headers: "{{ $.headers }}"
  timeout: "{{ $.timeout }}"
output_keys: ["status_code", "body", "headers"]
```

### 8. 代码审查工作流

**路径**: `/lib/workflow/code_review`
**版本**: 3.10
**类型**: subgraph
**描述**: 自动化代码审查工作流

```markdown
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

AgenticDSL `/lib/workflow/code_review/summarize`
type: end
output:
  review_result: "{{ $.last_result }}"
```

### 9. 数据分析工作流

**路径**: `/lib/workflow/data_analysis`
**版本**: 3.10
**类型**: subgraph
**描述**: 数据分析工作流

```markdown
variables:
  data_source: string      # 数据源路径或 URL
  analysis_type: string    # 分析类型：summary, trend, correlation

AgenticDSL `/lib/workflow/data_analysis/load`
type: tool_call
tool: fs.read
arguments:
  path: "{{ $.data_source }}"
next: "/lib/workflow/data_analysis/analyze"

AgenticDSL `/lib/workflow/data_analysis/analyze`
type: tool_call
tool: llm.call
arguments:
  prompt: |
    对以下数据进行 {{ $.analysis_type }} 分析。

    数据：{{ $.content }}

    提供分析结果和建议。
output_keys: ["analysis_result"]
next: "/lib/workflow/data_analysis/finish"

AgenticDSL `/lib/workflow/data_analysis/finish`
type: end
output:
  result: "{{ $.analysis_result }}"
```

### 10. 意图路由

**路径**: `/lib/cognitive/route`
**版本**: 3.10
**类型**: subgraph
**描述**: 根据用户意图路由到合适的处理流程

```markdown
variables:
  user_input: string       # 用户输入
  available_routes: array # 可用路由列表

AgenticDSL `/lib/cognitive/route/classify`
type: tool_call
tool: llm.call
arguments:
  prompt: |
    分析用户输入，确定最佳路由。

    用户输入：{{ $.user_input }}
    可用路由：{{ $.available_routes }}

    输出格式：
    路由：[选择的路由名]
    理由：[选择理由]
output_keys: ["selected_route", "reasoning"]
next: "/lib/cognitive/route/dispatch"

AgenticDSL `/lib/cognitive/route/dispatch`
type: generate_subgraph
arguments:
  template: "route_dispatch"
  route: "{{ $.selected_route }}"
  context: "{{ $.user_input }}"
```

### 11. 摘要生成

**路径**: `/lib/cognitive/summarize`
**版本**: 3.10
**类型**: subgraph
**描述**: 生成文本摘要

```markdown
variables:
  content: string              # 待摘要内容
  max_length: int = 200        # 最大摘要长度
  format: string = "paragraph" # 摘要格式：paragraph, bullet, json

AgenticDSL `/lib/cognitive/summarize/generate`
type: tool_call
tool: llm.call
arguments:
  prompt: |
    为以下内容生成摘要。

    内容：{{ $.content }}

    要求：
    - 长度：不超过 {{ $.max_length }} 字符
    - 格式：{{ $.format }}
    - 保留关键信息
output_keys: ["summary"]
next: "/lib/cognitive/summarize/finish"

AgenticDSL `/lib/cognitive/summarize/finish`
type: end
output:
  summary: "{{ $.summary }}"
```

---

## 子图管理 API

### 生成子图

**路径**: `/lib/dslgraph/generate`

标准库提供 `generate_subgraph` 原语，用于动态生成子图：

```markdown
AgenticDSL `/lib/dslgraph/generate`
type: generate_subgraph
arguments:
  template: "sequential_tasks"  # 模板类型
  tasks: "{{ $.subtasks }}"    # 任务列表
output_keys: ["generated_paths", "success"]
```

### 签名验证

| 参数 | 类型 | 说明 |
|------|------|------|
| `prompt_template` | string | 生成提示模板 |
| `signature_validation` | string | 验证模式：strict, warn, ignore |
| `archive_on_success` | string | 成功时归档路径 |

---

## 用户扩展机制

### 本地库目录

```
~/.hydraforge/lib/           # 用户本地库目录
├── custom/
│   └── my_agent.md         # 用户自定义子图
└── override/
    └── code_review.md      # 覆盖标准库
```

### 覆盖优先级

```
用户本地库 (~/.hydraforge/lib/) > 标准库 (lib/)
```

### 加载顺序

1. 标准库 (`lib/`)
2. 用户本地库 (`~/.hydraforge/lib/`) — 相同路径会覆盖标准库

---

## LLM 可发现的子图列表

执行器通过 `build_available_subgraphs_context()` 提供给 LLM：

```
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
- /lib/tools/net/http_get  - HTTP GET

## 认知
- /lib/cognitive/route    - 意图路由
- /lib/cognitive/summarize - 摘要生成
```

---

## 版本历史

| 版本 | 日期 | 变更 |
|------|------|------|
| v3.10 | 2026-05-13 | 完全重写，对齐 ADR-9 新结构 |
| v3.9 | 2025-xx-xx | 上一版本（已废弃） |

---

*文档版本: v3.10*
*最后更新: 2026-05-13*