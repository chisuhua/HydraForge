# Skill: review_work

**分类**: 轴3-审查/质量
**触发词**: "review", "审查", "检查代码", "verify implementation", "post-implementation"

## When to Use

在以下情况激活此技能：
- 完成重大功能实现后
- 发起 pull request 前
- 合并到主分支前
- 需要系统化验证工作质量时

## What It Does

启动 5 路并行审查子 agent，全部通过才算评审通过：

```
review_work
    ├── Oracle (goal/constraint verification)     ← 验证目标和约束
    ├── Oracle (code quality)                      ← 验证代码质量
    ├── Oracle (security)                          ← 验证安全性
    ├── unspecified-high (hands-on QA execution)   ← 动手 QA 执行
    └── unspecified-high (context mining)          ← 上下文挖掘
```

## The Five Dimensions

| # | 审查维度 | 目标 |
|---|---------|------|
| 1 | Goal/Constraint 验证 | 实现是否满足原始需求和约束 |
| 2 | Code Quality 验证 | 代码质量、可读性、设计模式 |
| 3 | Security 验证 | 安全漏洞、输入验证、权限控制 |
| 4 | Hands-on QA | 实际运行测试，验证功能 |
| 5 | Context Mining | 从 git/Slack/Notion 挖掘相关上下文 |

## Pass Criteria

**所有 5 路审查必须全部通过**才算 review 通过。
任一路失败 → 汇总反馈 → 返回修改。

## Hard Gate

在所有 5 路审查完成并通过之前，**不得**：
- 合并代码
- 关闭 issue
- 标记任务完成

## AgenticDSL Example

**对应文件**: `../../agenticdsl/axis3_review/review_work.agent.md`

该文件展示了如何用 AgenticDSL 实现 5 路并行评审，包含：
- `fork` — 并行派发 5 路审查
- `dsl_call` — LLM 作为审查者
- `join` — 汇聚审查结果
- `assert` — 质量门禁判断

## Ideal DSL Extension

**参考**: `../../ideal_dsl/05_review_skill.md`

审查/质量类技能的理想 DSL 扩展提案：
- `type: review_gate` — 审查门禁节点
- `type: parallel_review` — 并行审查节点
- `review_criteria` — 标准化审查标准定义
