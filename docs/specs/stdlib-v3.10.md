# DSL 标准库 v3.10

> 合并自 `docs/specs/dsl-lib.md` (v3.10) 和 `docs/specs/stdlib.md` (v1.0)，
> 合并日期 2026-06-12，作为 project-organization 计划 Stage 2 / Task 8 的产出。
> `docs/specs/phase2-standard-library.md` (v1.0) 因其覆盖的 13 个 ADR 已被归档，
> 移至 `docs/archive/specs/phase2-standard-library-v1.0.md`。

## 版本

v3.10 (2026-05-13 → 2026-06-12 unified)

## 概述

DSL 标准库是 AgenticDSL 执行器的内置子图集合，提供可复用的人类交互、错误恢复、数据处理、身份认证、推理模式、工作流、工具封装、认知策略等子图。

**使用原则**：优先调用标准库，避免重复生成逻辑。
**调用方式**：`next: "/lib/<category>/<name>"`
**命名空间**：所有库子图位于 `/lib/...` 路径下，禁止跨内部节点跳转。

## 命名空间结构

```
/lib
├── human/                 # 人类交互 (来自 stdlib.md v1.0)
│   ├── clarify_intent
│   └── confirm_action
├── error/                 # 错误恢复 (来自 stdlib.md v1.0)
│   ├── retry_with_backoff
│   └── fallback_to_default
├── auth/                  # 身份认证 (来自 stdlib.md v1.0)
│   ├── verify_session
│   └── login_required     # 系统预置跳转入口(详见 §auth/verify_session)
├── data/                  # 数据处理 (来自 stdlib.md v1.0)
│   ├── validate_email
│   └── extract_entities   # NER 工具封装(详见 §data/validate_email)
├── utils/                 # 通用工具 (来自 stdlib.md v1.0)
│   ├── noop
│   └── assign_from_template
├── reasoning/             # 推理模式 (来自 dsl-lib.md v3.10)
│   ├── react
│   └── plan
├── workflow/              # 工作流 (来自 dsl-lib.md v3.10)
│   ├── code_review
│   └── data_analysis
├── tools/                 # 工具封装 (来自 dsl-lib.md v3.10)
│   ├── fs/read
│   ├── fs/write
│   ├── git/clone
│   ├── git/diff
│   └── net/http_get
├── cognitive/             # 认知策略 (来自 dsl-lib.md v3.10)
│   ├── route
│   └── summarize
└── math/                  # 数学工具 (来自 dsl-lib.md v3.10)
    └── add
```

### 路径约定

引用格式：`/lib/<category>/<name>`

工具子分类：`/lib/tools/<tool_family>/<name>` (如 `/lib/tools/fs/read`)

示例：
- `/lib/reasoning/react`
- `/lib/human/clarify_intent`
- `/lib/tools/fs/read`
- `/lib/workflow/code_review`

---

## 标准库清单

| 路径 | 用途 | 稳定性 | 来源 |
|------|------|--------|------|
| `/lib/human/clarify_intent` | 请求人类澄清模糊意图 | stable | stdlib.md §1 |
| `/lib/human/confirm_action` | 请求人类确认高风险操作 | stable | stdlib.md §2 |
| `/lib/error/retry_with_backoff` | 指数退避重试（最多3次） | stable | stdlib.md §3 |
| `/lib/error/fallback_to_default` | 降级到默认响应 | stable | stdlib.md §4 |
| `/lib/auth/verify_session` | 验证用户会话有效性 | stable | stdlib.md §5 |
| `/lib/auth/login_required` | 跳转到登录流程（系统预置） | stable | stdlib.md §5 |
| `/lib/data/validate_email` | 校验并标准化邮箱格式 | stable | stdlib.md §6 |
| `/lib/data/extract_entities` | 从文本提取结构化实体（NER） | stable | stdlib.md §6 |
| `/lib/utils/noop` | 空操作（占位/调试） | stable | stdlib.md §7 |
| `/lib/utils/assign_from_template` | 安全从模板生成结构化输出 | stable | stdlib.md §8 |
| `/lib/reasoning/react` | ReAct 推理循环：思考 → 行动 → 观察 | stable | dsl-lib.md §1 |
| `/lib/reasoning/plan` | 任务分解 | stable | dsl-lib.md §2 |
| `/lib/tools/fs/read` | 安全读取文件内容 | stable | dsl-lib.md §3 |
| `/lib/tools/fs/write` | 安全写入文件内容 | stable | dsl-lib.md §4 |
| `/lib/tools/git/clone` | Git 仓库克隆 | stable | dsl-lib.md §5 |
| `/lib/tools/git/diff` | Git 差异比较 | stable | dsl-lib.md §6 |
| `/lib/tools/net/http_get` | HTTP GET 请求 | experimental | dsl-lib.md §7 |
| `/lib/workflow/code_review` | 自动化代码审查工作流 | stable | dsl-lib.md §8 |
| `/lib/workflow/data_analysis` | 数据分析工作流 | experimental | dsl-lib.md §9 |
| `/lib/cognitive/route` | 意图路由 | stable | dsl-lib.md §10 |
| `/lib/cognitive/summarize` | 摘要生成 | stable | dsl-lib.md §11 |
| `/lib/math/add` | 基础加法 | stable | dsl-lib.md §12 |

> 执行器必须预加载并校验以上子图。社区可扩展，但不得修改其 `signature`。

---

## 人类交互 (`/lib/human/`)

### 1. `/lib/human/clarify_intent`

**路径**: `/lib/human/clarify_intent`
**版本**: v1.0
**类型**: subgraph
**描述**: 暂停执行，请求人类澄清用户模糊意图（适用于 LLM 无法确定场景）

**输入**（通过 `assign` 设置）：

```yaml
assign:
  lib_human_prompt: string      # 显示给人类的问题（必填）
  lib_human_options: array?     # 可选按钮列表（如 ["查订单", "改地址"]）
```

**输出**：

```yaml
lib_human_response:
  intent: string      # 若提供选项，则为选中值；否则为原始输入
  raw: string         # 原始人类输入
```

**终止模式**：`soft`（仅结束本子图，返回父流程）
**依赖工具**：`request_human_intent`（必须由执行器注册）

**示例调用**：

```yaml
type: assign
assign:
  lib_human_prompt: "用户说‘还没到’，请判断真实意图"
  lib_human_options: ["查物流", "催发货", "投诉"]
next: "/lib/human/clarify_intent"
```

---

### 2. `/lib/human/confirm_action`

**路径**: `/lib/human/confirm_action`
**版本**: v1.0
**类型**: subgraph
**描述**: 请求人类确认高风险操作（如删除、支付）

**输入**：

```yaml
assign:
  lib_confirm_prompt: string    # 操作描述（必填）
  lib_risk_level: string        # "low" | "medium" | "high"（影响 UI 提示）
```

**输出**：

```yaml
lib_confirm_result:
  confirmed: boolean
  reason: string?               # 若拒绝，可附理由
```

**行为**：若未确认，跳转到 `/end`（`soft` 终止）
**依赖工具**：`request_human_confirmation`

---

## 错误恢复 (`/lib/error/`)

### 3. `/lib/error/retry_with_backoff`

**路径**: `/lib/error/retry_with_backoff`
**版本**: v1.0
**类型**: subgraph
**描述**: 对失败操作进行指数退避重试（1s, 2s, 4s）

**输入**：

```yaml
assign:
  lib_retry_target: string    # 要重试的节点路径（如 "/main/api_call"）
  lib_retry_input: object     # 重试所需上下文快照（建议包含原始参数）
```

**行为**：
- 最多重试 3 次
- 成功后写入 `lib_retry_result`
- 失败后跳转到 `on_error` 或终止

**输出**：

```yaml
lib_retry_result: any         # 成功时的返回值
```

**注意**：目标节点必须是幂等的！

---

### 4. `/lib/error/fallback_to_default`

**路径**: `/lib/error/fallback_to_default`
**版本**: v1.0
**类型**: subgraph
**描述**: 当主流程失败时，返回友好默认响应

**输入**：

```yaml
assign:
  lib_fallback_message: string  # 默认回复内容
```

**输出**：

```yaml
final_response: "{{ lib_fallback_message }}"
```

**终止模式**：`hard`（结束整个工作流）
**用途**：作为 `on_error` 的兜底处理

---

## 身份认证 (`/lib/auth/`)

### 5. `/lib/auth/verify_session`

**路径**: `/lib/auth/verify_session`
**版本**: v1.0
**类型**: subgraph
**描述**: 验证当前用户会话是否有效

**输入**：自动从 `session.token` 读取
**依赖工具**：`session_store`（需支持 `verify(token) → { user_id, valid }`）

**输出**：

```yaml
lib_auth_output:
  user_id: string
  is_valid: boolean
```

**失败行为**：自动跳转到 `/lib/auth/login_required`
**终止模式**：`soft`（允许父图处理未登录逻辑）

> **关联子图**：`/lib/auth/login_required` 是系统预置的跳转入口，用于未登录场景（见目录树注释）。

---

## 数据处理 (`/lib/data/`)

### 6. `/lib/data/validate_email`

**路径**: `/lib/data/validate_email`
**版本**: v1.0
**类型**: subgraph
**描述**: 校验并标准化邮箱格式

**实现**：调用内置 `codelet`

**输入**：

```yaml
assign:
  lib_email_input: string
```

**输出**：

```yaml
lib_email_output:
  is_valid: boolean
  normalized: string?   # 小写标准化格式（如 "user@example.com"）
```

**依赖**：`/codelets/internal/validate_email`（执行器预置）

> **关联子图**：`/lib/data/extract_entities` 是基于 NER 工具的实体抽取封装（见目录树注释）。

---

## 通用工具 (`/lib/utils/`)

### 7. `/lib/utils/noop`

**路径**: `/lib/utils/noop`
**版本**: v1.0
**类型**: subgraph
**描述**: 空操作节点，用于占位、调试或默认分支

**输出**：无
**终止模式**：`soft`

**示例**：

```yaml
next: "{% if debug %}/lib/utils/noop{% else %}/main/real_step{% endif %}"
```

---

### 8. `/lib/utils/assign_from_template`

**路径**: `/lib/utils/assign_from_template`
**版本**: v1.0
**类型**: subgraph
**描述**: 安全地从 Inja 模板生成结构化对象（避免 LLM 直接生成 JSON）

**输入**：

```yaml
assign:
  lib_template: string          # 模板字符串（如 "订单{{ id }}状态{{ status }}"）
  lib_context: object           # 模板渲染上下文
  lib_output_key: string        # 输出字段名（如 "summary"）
```

**输出**：

```yaml
<lib_output_key>: string      # 渲染结果
```

**用途**：替代 LLM 生成自然语言摘要，提升可靠性

---

## 推理模式 (`/lib/reasoning/`)

### 9. ReAct 推理循环 (`/lib/reasoning/react`)

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

---

### 10. 任务分解 (`/lib/reasoning/plan`)

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

---

## 工作流 (`/lib/workflow/`)

### 11. 代码审查工作流 (`/lib/workflow/code_review`)

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

---

### 12. 数据分析工作流 (`/lib/workflow/data_analysis`)

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

---

## 工具封装 (`/lib/tools/`)

### 13. 文件读取工具 (`/lib/tools/fs/read`)

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

---

### 14. 文件写入工具 (`/lib/tools/fs/write`)

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

---

### 15. Git 克隆 (`/lib/tools/git/clone`)

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

---

### 16. Git 差异 (`/lib/tools/git/diff`)

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

---

### 17. HTTP GET (`/lib/tools/net/http_get`)

**路径**: `/lib/tools/net/http_get`
**版本**: 3.10
**类型**: subgraph
**稳定性**: experimental
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

---

## 认知策略 (`/lib/cognitive/`)

### 18. 意图路由 (`/lib/cognitive/route`)

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

---

### 19. 摘要生成 (`/lib/cognitive/summarize`)

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

## 数学工具 (`/lib/math/`)

### 20. 基础加法 (`/lib/math/add`)

> 说明: `dsl-lib.md` v3.10 的目录树中列出 `/lib/math/add.md` 但未提供详细定义（仅有目录结构条目和清单表中的一行）。该子图作为预置数学原语存在，使用标准算术加法语义，由执行器内置实现，无需调用 LLM。

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

## 使用说明（来自 stdlib.md v1.0）

### 如何调用？

LLM 程序员只需在 `next` 中跳转至库子图入口：

```yaml
next: "/lib/human/clarify_intent"
```

### 如何传参？

通过 `assign` 节点提前写入约定的 `lib_*` 字段：

```yaml
type: assign
assign:
  lib_human_prompt: "请确认操作"
  lib_human_options: ["是", "否"]
next: "/lib/human/confirm_action"
```

### 如何获取结果？

库子图输出统一写入 `lib_<name>_output` 或特定路径（见各模块说明），父图可直接使用。

### 如何覆盖？

如需定制库行为，LLM 可生成同路径子图并声明：

```yaml
metadata:
  override: true
```

### 执行器责任

- 启动时预加载所有 `/lib/**` 子图
- 在 `llm_call` 的 `execution_context` 中注入可用库清单：

```yaml
available_subgraphs:
  - path: "/lib/human/clarify_intent"
    description: "请求人类澄清意图"
    input_keys: ["lib_human_prompt", "lib_human_options"]
    output_keys: ["lib_human_response"]
```

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

## 人类交互
- /lib/human/clarify_intent - 请求人类澄清意图
- /lib/human/confirm_action - 请求人类确认操作

## 错误恢复
- /lib/error/retry_with_backoff - 指数退避重试
- /lib/error/fallback_to_default - 降级到默认响应

## 身份认证
- /lib/auth/verify_session - 验证用户会话
- /lib/auth/login_required - 跳转登录

## 数据处理
- /lib/data/validate_email - 校验邮箱
- /lib/data/extract_entities - 抽取实体

## 通用工具
- /lib/utils/noop - 空操作
- /lib/utils/assign_from_template - 模板渲染

## 数学
- /lib/math/add - 基础加法
```

---

## 稳定性说明

| 稳定性 | 含义 |
|--------|------|
| `stable` | API 稳定，签名不变，向后兼容 |
| `experimental` | API 可能在未来 minor 版本变更 |
| `deprecated` | 已废弃，建议迁移到替代实现（暂无） |

当前 experimental 子图：
- `/lib/tools/net/http_get`
- `/lib/workflow/data_analysis`

## 维护规则

1. 新子图添加必须使用 v3.10+ 的命名空间格式 (`/lib/<category>/<name>@<version>`)
2. `experimental` 标签子图经过 2 个 minor 版本验证后可提升为 `stable`
3. 不再维护的子图标 `deprecated` 但保留实现
4. v3.10 之前的子图规范（来自 dsl-lib.md / stdlib.md）仍可读，但以本文档为准

---

## 版本历史

| 版本 | 日期 | 变更 |
|------|------|------|
| v3.10 | 2026-05-13 | dsl-lib.md 完全重写，对齐 ADR-9 新结构 |
| v3.10 | 2026-06-12 | 合并 dsl-lib.md + stdlib.md 为 stdlib-v3.10.md（Stage 2 / Task 8） |
| v3.9 | 2025-xx-xx | 上一版本（已废弃） |

---

## 参见

- [`docs/specs/dsl.md`](dsl.md) — DSL 语言规范
- [`docs/specs/architecture.md`](architecture.md) — 系统架构
- [`docs/specs/memory-v3.10.md`](memory-v3.10.md) — DSL 内存记忆规范 v3.10 (Stage 2 / Task 9)
- [`docs/archive/specs/phase2-standard-library-v1.0.md`](../archive/specs/phase2-standard-library-v1.0.md) — 已归档的 Phase 2 规划（31+ 子图，关联 13 个已废弃 ADR）
- [`docs/archive/adr/`](../archive/adr/README.md) — 已归档的 ADR
- [`docs/adr/adr-0009-dsl-standard-library.md`](../adr/adr-0009-dsl-standard-library.md) — DSL 标准库规划 ADR

---

*文档版本: v3.10*
*最后更新: 2026-06-12*