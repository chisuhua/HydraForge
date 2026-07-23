# SKILL 目录

> **命令式 SKILL（`.skill.md`）** vs **LLM Prompt Template（`.md`）**

## 区别

| 特征 | `.skill.md` | `.md` |
|------|-------------|-------|
| 内容 | 命令式 DSL 语句（`call_tool` / `assign` / `return`） | 自然语言提示词模板 |
| 执行器 | `SkillInterpreter::run()` (隔离进程 + seccomp) | LLM provider 注入 |
| 隔离 | ✅ 进程级隔离 (posix_spawn + seccomp BPF) | ❌ 与主进程同地址空间 |
| 速度 | 微秒级 IPC | LLM 推理延迟 |
| 用例 | 确定性工具编排 | 非确定性推理生成 |

## 文件列表

| 文件 | 形态 | 说明 |
|------|------|------|
| `code-review-run.skill.md` | 命令式 | `code_review/run` 工具调用示例（示例代码审查技能） |
| `../../skills/code-review/SKILL.md` | Prompt Template | LLM 提示词模板（mock-only，需 LLM 注入） |

## 创建新 SKILL

1. 确定形态：确定性工具流程 → 命令式；非确定性生成 → Prompt Template
2. 创建 `.skill.md` 文件（命令式）或 `.md` 文件（Prompt Template）
3. 在 `config.json` 中添加条目
4. 重启 demo 验证

详细语法见 [skill-dsl-syntax.md](skill-dsl-syntax.md)。