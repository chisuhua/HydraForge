# AgenticDSL 标准库设计文档

> **⚠️ 归档说明 (2026-06-03)**：本文档为 ADR-0009 (DSL 标准库规划) 的前置设计稿，已被 ADR-0009 正式批准并取代。归档至 `docs/archive/superpowers/specs/`，仅供历史参考。

> **作者**: Oracle  
> **日期**: 2026-05-12  
> **版本**: 1.0  
> **关联 ADR**: ADR-8 (LayeredContext), ADR-? (标准库加载机制)

---

## 1. 概述

本文档定义 AgenticDSL 引擎的标准库（Standard Library）组织结构、核心子图集合、加载机制和用户扩展策略。

### 1.1 设计目标

| 目标 | 说明 |
|------|------|
| **Phase 1 可用** | 提供 10+ 个立即可用的实用子图 |
| **可扩展** | 用户能无缝添加自定义子图 |
| **易导航** | 功能域分层，路径即命名空间 |
| **可维护** | 新增子图只需创建 `.md` 文件 |
| **兼容现有 parser** | 不改变 YAML DSL 语法 |

### 1.2 术语

- **子图 (Subgraph)**: 一个完整的 `graph_type: subgraph` DSL 块，可独立调用
- **路径 (Path)**: 子图唯一标识，如 `/lib/math/add`
- **签名 (Signature)**: 输入输出类型声明，如 `(a: number, b: number) -> sum: number`
- **权限 (Permissions)**: 子图执行所需的能力清单

---

## 2. 目录结构设计

### 2.1 组织原则

采用**功能域 + 域内粒度**混合组织：

```
lib/
├── <domain>/           # 功能域目录
│   ├── <atomic>.md     # 原子操作（细粒度）
│   ├── <compound>.md   # 组合操作（中等粒度）
│   └── <pattern>.md    # 完整模式（粗粒度）
```

**为什么不用层级组织（L1/L2/L3）？**
- 用户每次引用时都要判断"这个操作是 L1 还是 L2"
- 功能域更符合直觉，查找路径直接对应用途

### 2.2 目录结构

```
lib/
├── core/               # 控制流基础构件
│   ├── noop.md         # 软终止占位
│   ├── sequence.md     # 顺序执行包装
│   ├── parallel.md     # 并行分支
│   └── retry.md        # 重试包装器
├── reasoning/          # 推理与决策模式
│   ├── react.md        # ReAct 推理循环
│   ├── plan_and_solve.md
│   ├── reflect.md      # 自我反思
│   └── choose_tool.md  # 工具选择
├── code/               # 代码分析操作
│   ├── read_file.md    # 读取文件
│   ├── write_file.md   # 写入文件
│   ├── grep_search.md  # 文本搜索
│   └── analyze_diff.md # 分析 diff
├── fs/                 # 文件系统原子操作
│   ├── list_dir.md     # 列出目录
│   ├── read_file.md    # 读取文件（底层）
│   └── file_exists.md  # 检查存在
├── git/                # Git 操作
│   ├── diff.md         # 查看变更
│   ├── log.md          # 提交历史
│   └── blame.md        # 行级追溯
├── math/               # 数学工具
│   ├── add.md          # 已存在
│   ├── calculate.md    # 通用计算
│   └── compare.md      # 数值比较
├── human/              # 人机交互
│   ├── confirm_action.md
│   ├── clarify_input.md
│   └── notify.md       # 通知用户
├── text/               # 文本处理
│   ├── summarize.md    # 文本摘要
│   ├── extract_json.md # JSON 提取
│   └── format.md       # 格式化
└── utils/              # 通用工具
    ├── log_debug.md    # 调试日志
    └── validate_schema.md
```

### 2.3 路径命名规则

| 规则 | 示例 | 说明 |
|------|------|------|
| 标准库前缀 | `/lib/` | 解析器自动识别 `is_standard_library = true` |
| 功能域 | `/lib/reasoning/` | 对应 `lib/reasoning/` 目录 |
| 子图名 | `/lib/reasoning/react` | 对应 `lib/reasoning/react.md` |
| 节点路径 | `/lib/reasoning/react/step1` | 子图内节点 ID 拼接 |

---

## 3. Phase 1 核心子图定义

### 3.1 子图清单（10个）

按优先级和依赖关系排序：

| # | 路径 | 粒度 | 用途 | 权限 | 状态 |
|---|------|------|------|------|------|
| 1 | `/lib/core/noop` | 原子 | 软终止占位 | `[]` | ✅ 已有 |
| 2 | `/lib/math/add` | 原子 | 两数相加 | `[]` | ✅ 已有 |
| 3 | `/lib/fs/list_dir` | 原子 | 列出目录内容 | `["file:read"]` | 📝 待实现 |
| 4 | `/lib/fs/read_file` | 原子 | 读取文件内容 | `["file:read"]` | 📝 待实现 |
| 5 | `/lib/code/read_file` | 组合 | 读取+分析代码结构 | `["file:read"]` | 📝 待实现 |
| 6 | `/lib/text/summarize` | 组合 | LLM 文本摘要 | `["llm:call"]` | 📝 待实现 |
| 7 | `/lib/human/confirm_action` | 组合 | 破坏性操作确认 | `["human:interactive"]` | 📝 待实现 |
| 8 | `/lib/core/retry` | 模式 | 失败重试包装器 | `[]` | 📝 待实现 |
| 9 | `/lib/reasoning/choose_tool` | 组合 | 从可用工具选择 | `["llm:call"]` | 📝 待实现 |
| 10 | `/lib/reasoning/react` | 模式 | ReAct 推理循环 | `["llm:call", "tool:call"]` | 📝 待实现 |

### 3.2 子图详细定义

#### `/lib/fs/list_dir`

```yaml
graph_type: subgraph
signature: "(path: string) -> {entries: array<string>, count: number}"
permissions: ["file:read"]
nodes:
  - id: validate
    type: assert
    condition: "{{ inputs.path }}"
    error_message: "path is required"
    next: ["/lib/fs/list_dir/invoke"]

  - id: invoke
    type: tool_call
    tool: filesystem
    arguments:
      action: "list"
      path: "{{ inputs.path }}"
    output_keys: ["raw_entries"]
    next: ["/lib/fs/list_dir/format"]

  - id: format
    type: assign
    assign:
      entries: "{{ raw_entries.files }}"
      count: "{{ raw_entries.files | length }}"
    output_keys: ["entries", "count"]
    next: ["/end_soft"]
```

#### `/lib/text/summarize`

```yaml
graph_type: subgraph
signature: "(text: string, max_length: number = 200) -> {summary: string}"
permissions: ["llm:call"]
nodes:
  - id: generate
    type: dsl_call
    prompt_template: |
      Summarize the following text in {{ inputs.max_length }} characters or less:
      ---
      {{ inputs.text }}
      ---
      Output only the summary, no preamble.
    llm_tool_name: "default"
    output_keys: ["summary"]
    next: ["/end_soft"]
```

#### `/lib/core/retry`

```yaml
graph_type: subgraph
signature: "(target_path: string, max_attempts: number = 3) -> {result: any, attempts: number, success: boolean}"
permissions: []
nodes:
  - id: init
    type: assign
    assign:
      attempt: "0"
      success: "false"
    output_keys: ["attempt", "success"]
    next: ["/lib/core/retry/loop"]

  - id: loop
    type: fork
    condition: "{{ attempt < max_attempts and not success }}"
    true_branch: ["/lib/core/retry/execute"]
    false_branch: ["/lib/core/retry/finalize"]

  - id: execute
    type: tool_call
    tool: call_subgraph
    arguments:
      path: "{{ inputs.target_path }}"
    output_keys: ["result"]
    next: ["/lib/core/retry/check"]

  - id: check
    type: assign
    assign:
      success: "{{ result.error is not defined }}"
      attempt: "{{ attempt + 1 }}"
    output_keys: ["success", "attempt"]
    next: ["/lib/core/retry/loop"]

  - id: finalize
    type: assign
    assign:
      result: "{{ result | default(none) }}"
      attempts: "{{ attempt }}"
      success: "{{ success }}"
    output_keys: ["result", "attempts", "success"]
    next: ["/end_soft"]
```

> **注**: `call_subgraph` 工具需由 executor 支持，或改用 `dsl_call` 生成调用代码。

#### `/lib/reasoning/react`

```yaml
graph_type: subgraph
signature: "(query: string, available_tools: array<string>, max_steps: number = 5) -> {answer: string, trace: array<string>}"
permissions: ["llm:call", "tool:call"]
nodes:
  - id: init
    type: assign
    assign:
      step: "0"
      trace: "[]"
      finished: "false"
    output_keys: ["step", "trace", "finished"]
    next: ["/lib/reasoning/react/loop"]

  - id: loop
    type: fork
    condition: "{{ step < max_steps and not finished }}"
    true_branch: ["/lib/reasoning/react/think"]
    false_branch: ["/lib/reasoning/react/answer"]

  - id: think
    type: dsl_call
    prompt_template: |
      ReAct Loop Step {{ step }}:
      Query: {{ inputs.query }}
      Previous thoughts/actions: {{ trace | join("\n") }}
      Available tools: {{ inputs.available_tools | join(", ") }}

      Decide ONE of:
      1. Thought: [your reasoning]
      2. Action: tool_name(tool_args)
      3. Answer: [final answer]

      Respond as JSON: {"type": "thought|action|answer", "content": "..."}
    llm_tool_name: "default"
    output_keys: ["decision"]
    next: ["/lib/reasoning/react/route"]

  - id: route
    type: fork
    condition: "{{ decision.type == 'action' }}"
    true_branch: ["/lib/reasoning/react/act"]
    false_branch: ["/lib/reasoning/react/update_trace"]

  - id: act
    type: tool_call
    tool: "{{ decision.tool_name }}"
    arguments: "{{ decision.args }}"
    output_keys: ["observation"]
    next: ["/lib/reasoning/react/update_trace"]

  - id: update_trace
    type: assign
    assign:
      trace: "{{ trace + [decision.content] }}"
      step: "{{ step + 1 }}"
      finished: "{{ decision.type == 'answer' }}"
    output_keys: ["trace", "step", "finished"]
    next: ["/lib/reasoning/react/loop"]

  - id: answer
    type: assign
    assign:
      answer: "{{ trace | last }}"
      trace: "{{ trace }}"
    output_keys: ["answer", "trace"]
    next: ["/end_soft"]
```

---

## 4. 加载机制

### 4.1 现有加载流程

```
StandardLibraryLoader::instance()
  ├── load_builtin_libraries()      # 硬编码签名（低优先级）
  └── (可选) load_from_directory()  # 从 lib/ 递归加载 .md 文件
```

当前问题：
- 文件系统库和内置库可能重复注册
- 无用户扩展机制
- `load_from_directory()` 未被自动调用

### 4.2 改进后的加载流程

```
StandardLibraryLoader::instance()
  ├── load_builtin_libraries()           # 二进制内置（优先级 1）
  ├── load_from_directory("./lib/")      # 项目标准库（优先级 2）
  ├── load_from_directory("~/.config/agenticdsl/lib/")  # 用户全局库（优先级 3）
  └── load_from_directory("./agentic_lib/")              # 项目本地库（优先级 4）
```

**冲突解决**：后加载的同名路径**覆盖**先加载的。允许用户覆盖标准库行为。

### 4.3 代码修改点

文件：`src/modules/library/library_loader.cpp`

```cpp
void StandardLibraryLoader::load_all() {
    load_builtin_libraries();
    load_from_directory("./lib/");
    
    if (auto user_dir = get_user_lib_dir(); !user_dir.empty()) {
        load_from_directory(user_dir);
    }
    if (auto project_dir = get_project_custom_lib_dir(); !project_dir.empty()) {
        load_from_directory(project_dir);
    }
}

// 新增辅助函数
std::string get_user_lib_dir() {
    if (const char* home = std::getenv("HOME")) {
        return std::string(home) + "/.config/agenticdsl/lib/";
    }
    return "";
}

std::string get_project_custom_lib_dir() {
    return "./agentic_lib/";
}
```

**可选增强**：冲突检测日志（debug 模式输出 "overriding /lib/math/add from built-in to ./lib/math/add"）。

---

## 5. 用户扩展机制

### 5.1 三层扩展模型

| 层级 | 目录 | 用途 | 优先级 |
|------|------|------|--------|
| **内置层** | 二进制 / `lib/` | 官方标准库 | 最低 |
| **用户全局层** | `~/.config/agenticdsl/lib/` | 用户跨项目复用 | 中 |
| **项目本地层** | `./agentic_lib/` | 项目特定子图 | 最高 |

### 5.2 自定义命名空间

用户可通过前缀隔离自定义子图：

```yaml
# ./agentic_lib/mycompany/auth.md
graph_type: subgraph
path: "/usr/mycompany/auth"  # 非 /lib/ 前缀，不会与官方冲突
signature: "(token: string) -> {valid: boolean}"
```

**规则**：
- `/lib/*` → 视为标准库扩展，参与 `build_available_subgraphs_context()` 的 `stable` 组
- `/usr/*` 或 `/custom/*` → 视为用户库，参与 `custom` 组（新增稳定性标记）

### 5.3 扩展示例

用户创建 `./agentic_lib/verify_signature.md`：

```markdown
### AgenticDSL `/lib/crypto/verify_signature`
```yaml
graph_type: subgraph
signature: "(message: string, signature: string, public_key: string) -> {valid: boolean}"
permissions: ["crypto:verify"]
nodes:
  - id: verify
    type: tool_call
    tool: crypto_verify
    arguments:
      message: "{{ inputs.message }}"
      signature: "{{ inputs.signature }}"
      key: "{{ inputs.public_key }}"
    output_keys: ["valid"]
    next: ["/end_soft"]
```
```

加载后 LLM 可见：

```json
{
  "path": "/lib/crypto/verify_signature",
  "signature": {"outputs": {"valid": {"type": "boolean"}}},
  "permissions": ["crypto:verify"],
  "stability": "stable"
}
```

---

## 6. 权限与预算控制

### 6.1 权限声明

每个子图必须在 YAML 中显式声明 `permissions`：

| 权限类型 | 示例 | 说明 |
|----------|------|------|
| 文件访问 | `file:read`, `file:write` | 文件系统操作 |
| 网络 | `network:http` | HTTP 请求 |
| LLM 调用 | `llm:call` | 调用语言模型 |
| 工具调用 | `tool:call` | 调用外部工具 |
| 人机交互 | `human:interactive` | 需要用户确认 |
| 自定义 | `crypto:sign` | 领域特定权限 |

### 6.2 预算扣减

- 每个子图内部的 LLM 调用、工具调用正常扣减预算
- 子图调用本身不额外扣减（无 overhead）
- 子图可声明 `budget_hint`（metadata 中），提示典型资源消耗

---

## 7. 实施计划

### 7.1 文件结构变更

```
新增:
├── lib/
│   ├── core/
│   │   ├── noop.md          # 已存在，确认内容
│   │   ├── sequence.md      # 新
│   │   └── retry.md         # 新
│   ├── reasoning/
│   │   ├── react.md         # 新
│   │   ├── choose_tool.md   # 新
│   │   └── reflect.md       # 新
│   ├── code/
│   │   ├── read_file.md     # 新
│   │   ├── write_file.md    # 新
│   │   └── grep_search.md   # 新
│   ├── fs/
│   │   ├── list_dir.md      # 新
│   │   ├── read_file.md     # 新
│   │   └── file_exists.md   # 新
│   ├── git/
│   │   ├── diff.md          # 新
│   │   └── log.md           # 新
│   ├── math/
│   │   ├── add.md           # 已存在，确认内容
│   │   └── calculate.md     # 新
│   ├── human/
│   │   ├── confirm_action.md # 空文件，需填充
│   │   ├── clarify_input.md  # 空文件，需填充
│   │   └── notify.md        # 新
│   ├── text/
│   │   ├── summarize.md     # 新
│   │   └── extract_json.md  # 新
│   └── utils/
│       ├── log_debug.md     # 新
│       └── validate_schema.md # 新

修改:
├── src/modules/library/
│   ├── library_loader.cpp   # 添加三层加载 + 冲突处理
│   └── library_loader.h     # 添加 load_all() / get_user_lib_dir()

新增测试:
├── tests/test_library_loader.cpp  # 扩展测试用例
└── tests/test_subgraph_integration.cpp  # 子图调用集成测试
```

### 7.2 实施步骤

| 步骤 | 任务 | 预计时间 | 验收标准 |
|------|------|----------|----------|
| 1 | 创建目录结构（含 `.gitkeep`） | 10 min | `find lib/ -type d` 匹配设计 |
| 2 | 填充 10 个核心子图 `.md` | 3-4 h | 每个子图可独立解析通过 |
| 3 | 清理空文件（human/*.md） | 15 min | 无空 `.md` 文件 |
| 4 | 修改 `library_loader.cpp` 支持三层加载 | 1 h | 单元测试通过 |
| 5 | 添加冲突检测日志 | 30 min | DEBUG 输出可见 |
| 6 | 扩展 `test_library_loader.cpp` | 1 h | 覆盖加载、冲突、用户扩展 |
| 7 | 运行全量测试 | 15 min | `ctest --output-on-failure` 全绿 |
| 8 | 文档更新 | 30 min | README 包含标准库说明 |

**总预计时间**: Short (1 天)

---

## 8. 风险与回退

| 风险 | 影响 | 缓解措施 |
|------|------|----------|
| 子图签名与实际输出不匹配 | 运行时类型错误 | CI 中添加 schema 校验 |
| 权限声明不完整 | 安全漏洞 | Code Review 检查清单 |
| 用户覆盖内置库导致行为异常 | 调试困难 | DEBUG 日志输出覆盖记录 |
| 子图数量膨胀后加载变慢 | 启动延迟 | 延迟加载（按需解析 .md） |

---

## 9. 附录：与现有机制的兼容性

### 9.1 Parser 兼容性

所有子图使用现有 YAML DSL 语法：
- `graph_type: subgraph` ✓ 已支持
- `signature` 字段 ✓ 已支持
- `permissions` 字段 ✓ 已支持
- `nodes` 数组 ✓ 已支持

### 9.2 Scheduler 兼容性

- 子图路径以 `/lib/` 开头 → `is_standard_library = true` ✓ 自动识别
- 子图内节点路径格式 `/<subgraph>/<id>` ✓ 已支持
- `build_available_subgraphs_context()` 自动包含 ✓ 已支持

### 9.3 无需修改的组件

- `MarkdownParser`
- `TopoScheduler`
- `NodeExecutor`
- `ExecutionSession`（除可选的 `stability: "custom"` 标记）

---

## 10. 变更记录

| 版本 | 日期 | 变更 |
|------|------|------|
| 1.0 | 2026-05-12 | 初始设计 |

---

> **下一步**: 通过设计评审后，使用 `writing-plans` skill 生成详细实施计划。
