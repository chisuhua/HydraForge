# SKILL.md 命令式 DSL 语法文档

> **关联**: ADR-0055 (Skill 隔离执行模型), `openspec/changes/skill-interpreter-real-loading/`
> **版本**: V1 (2026-07-22)

## 概述

SKILL.md 是 Agent-as-Plugin 架构的四种 Agent 形态之一（SKILL/DSL/C++/Wasm，见 ADR-0052），适合快速迭代的原型 Agent。

**两种 SKILL.md 形态**：

| 形态 | 后缀 | 用途 | 执行方式 |
|------|------|------|---------|
| 命令式 DSL | `.skill.md` | 工具调用编排，隔离进程执行 | `SkillInterpreter::run()` |
| LLM Prompt Template | `.md` | 提示词模板，嵌入 LLM 调用 | mock-only / LLM 注入 |

## 命令式 DSL 语法（V1）

V1 实现 ADR-0055 §决策 2 指定的最小子集，采用行导向解释。

### 文件结构

```markdown
---
name: my-skill
version: 0.1
description: 技能描述
---

# 语句从这里开始
call_tool("tool_name", {"key": "value"})
assign result = call_tool("another_tool", {})
return result
```

### Frontmatter (YAML)

每个 `.skill.md` 文件必须以 YAML frontmatter 开头（`---` 定界）：

| 字段 | 必填 | 描述 |
|------|:----:|------|
| `name` | ✅ | 技能唯一标识 |
| `version` | ❌ | 版本号（推荐提供） |
| `description` | ❌ | 技能描述 |

### 语句

#### `call_tool("name", {args})`

调用宿主工具。通过 IPC 发送 JSON-RPC 请求到父进程。

- `name`: 字符串，工具名（如 `"fs.read"`）
- `args`: JSON 对象，工具参数

结果自动存入变量表，变量名为工具名的 `/` 替换为 `_`（如 `fs.read` → `fs_read`）。

```
call_tool("fs.read", {"path": "file.txt"})
# fs_read 变量可用
```

#### `assign key = expr`

创建或更新内部变量。

```
assign greeting = "hello"
assign count = 42
assign combined = "{{greeting}} world"
```

#### `return expr`

结束技能执行，将值传回父进程。触发 IPC `{"method":"return","value":...}`。

```
return fs_read
return {"status": "ok", "data": fs_read}
```

#### `emit_event("topic", {payload})`

通过父进程向事件总线发射事件。桥接到 `IInteractionBus::emit(string, string)`。

```
emit_event("user.input", {"text": "hello"})
```

#### `llm_generate("prompt")`

通过父进程的 LLM provider 生成文本（需 `cap.allow_llm=true`）。

```
llm_generate("Summarize: {{content}}")
# llm_result 变量可用: llm_result["content"]
```

#### `consume_budget(amount)`

扣减 USD 预算。父进程内部 `std::atomic<double>` 计数器，超限触发 SIGKILL。

```
consume_budget(0.01)
```

### 变量插值

V1 **仅**支持 `{{var}}` 语法（inja 默认，不支持 `${var}`）。

```
assign name = "world"
call_tool("fs.read", {"path": "hello-{{name}}.txt"})
```

`args` 是内置只读变量（技能调用时传入的参数）。

### 安全边界

| 机制 | 强制位置 | 说明 |
|------|---------|------|
| `allowed_tools` | 父进程 IPC 循环 | 白名单不匹配返回 `"tool not allowed"` |
| `allowed_topics` | 父进程 IPC 循环 | emit_event topic 白名单 |
| `max_steps` | 父进程 IPC 循环 | 超限 SIGKILL（子进程计数器仅作提示） |
| `timeout_ms` | 父进程 poll | 超限 SIGKILL |
| `budget_limit_usd` | 父进程原子计数器 | 超限 SIGKILL |
| seccomp BPF | 子进程加载后不可逆 | 禁止 open/socket/fork 等系统调用 |
| PR_SET_PDEATHSIG | 子进程入口 | 父进程死亡时子进程自动 SIGKILL |
| RLIMIT_AS | 子进程入口 | 256MB 地址空间上限 |
| RLIMIT_NOFILE | 子进程入口 | 8 个 fd 上限 |
| C4 invariant | 子进程入口 | Threads==1 检查，检测全局静态线程 |

### 关键字列表（V1）

| 关键字 | 是否 IPC | 说明 |
|--------|:--------:|------|
| `call_tool` | ✅ | 工具调用 |
| `assign` | ❌ | 内部变量赋值 |
| `return` | ✅ | 结束执行，返回值 |
| `emit_event` | ✅ | 事件发射 |
| `llm_generate` | ✅ | LLM 生成（需允许） |
| `consume_budget` | ✅ | 预算扣减 |

### V2 deferred

- `read_context(key)` — ADR-0055 修订 3，LayeredContext 桥接期后
- `if` / `loop` / `while` — 条件分支通过 `call_tool` 返回值路由
- `${var}` 语法 — 扩展 inja LexerConfig
- `derive_capability()` — 三方交集（manifest / skill_meta / os_policy）
- `IBudgetController::consume(double)` — 统一预算管理