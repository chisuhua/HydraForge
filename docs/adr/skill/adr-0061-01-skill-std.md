# ADR-0061-01: SKILL.md 标准对齐（Anthropic Skills / Cline Skills）

**日期**: 2026-07-16
**状态**: ✅ Approved (P0, 父 ADR-0061 拆分)
**父 ADR**: [../adr-0061-agent-evolution-and-solidification.md](../adr-0061-agent-evolution-and-solidification.md)

---

## 背景

SKILL.md 已是工业事实标准：
- **Anthropic Skills** (2025-10) 开放为 agentskills.io 标准
- **LangChain Skills** (2026-03) 首批 11 个 skill，eval 29%→95%
- **Cline Skills** 三层加载，CLI/VSCode/JetBrains 通用

HydraForge 应直接对齐标准，不自创格式，便于复用现有技能生态。

## 决策

### 决策 1 — SKILL.md v1 格式

```markdown
---
# 必需（Anthropic 兼容）
name: code-review
description: 审查代码中的安全漏洞、逻辑错误、可维护性问题

# HydraForge 扩展（与 ADR-0052/0053/0055 对齐）
category: axis3-review                # 5 维度分类
capabilities: [code_review, static_analysis]  # CapabilityRegistry 索引
input_schema: { ... }                # JSON Schema 2020-12 (ADR-0058)
output_schema: { ... }
requires_isolation: true             # SKILL 必须隔离 (ADR-0055)
timeout_ms: 30000
budget_limit_usd: 0.05
activation_events: [onTool:code_review/run]  # 懒加载 (ADR-0057)
trust_level: high                    # 信任等级 (ADR-0052)
---

# Code Review Agent

## Process
1. 通读代码理解整体结构
2. 按以下维度检查：安全风险 / 逻辑错误 / 可维护性
3. 输出 JSON 审查报告

## Hard Gate
- 必须返回非空 issues 列表
```

### 决策 2 — 三级 Progressive Disclosure

| 层级 | 内容 | 加载时机 |
|------|------|---------|
| Level 1: metadata | YAML frontmatter 全部字段 | 启动时 |
| Level 2: body | Markdown 主体 | `capability_match` 命中时 |
| Level 3: scripts/references | `scripts/` + `references/` | 调用时 |

### 决策 3 — 向后兼容

- 现有 `examples/skill_porting/skills/` 的 SKILL.md 无 frontmatter → 自动补 `name` (文件名) + `description` (第一段) + `category: axis1-process`
- 不破坏现有 39 个 Skill 的执行

### 决策 4 — Scripts 目录允许 C++ 二进制

```
skills/code-review/
├── SKILL.md
├── scripts/
│   └── scan.sh       # Shell 脚本
├── references/
│   └── security.md   # 引用文档
└── lib/              # 可选：依赖的 .agent.md 或 SKILL.md
```

`scripts/` 中的可执行文件由 SkillInterpreter 通过 `safe_exec` 调用（ADR-0055）。

## 实施

- 文件: `include/agenticdsl/skills/skill_parser.h/.cpp`
- 测试: `tests/test_skill_parser.cpp`
- 工作量: 1 week
- 优先级: P0

## 参考

- Anthropic Skills: https://www.anthropic.com/news/skills
- LangChain Skills: https://www.langchain.com/blog/langchain-skills
- Cline Skills: https://docs.cline.bot/customization/skills
- `docs/proposals/skill-system/04-skill-compiler-design.md`
- [ADR-0052](./adr-0052-agent-plugin-manifest.md), [ADR-0053](./adr-0053-agent-descriptor-interface.md), [ADR-0055](./adr-0055-skill-isolation.md), [ADR-0058](./adr-0058-tool-schema-validation.md)