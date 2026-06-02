# RF-001: Skill Taxonomy 全量映射表

**ID**: RF-001
**日期**: 2026-05-20
**状态**: 草案
**关联**: ADR-001

---

## 说明

本表列举本次调查中出现的全部 39 个去重技能，映射到 5 维度分类体系。
跨轴技能标注为 `主轴/副轴`。

---

## 轴1：流程/方法论（12 个）

| 技能 | 触发词 | 核心 DSL 特性 | 编译模板 |
|------|--------|-------------|---------|
| brainstorming | "我想实现...", "设计一下..." | user_input, dsl_call, state, assert | 顺序流水线 + 分支循环 |
| test_driven_development | "测试", "TDD", "red-green" | assert, tool_call, loop | 循环流水线 |
| systematic_debugging | "error", "crash", "bug" | state, dsl_call, tool_call, assert | 分支流水线 |
| regression_bisect | "回归", "bisect" | tool_call, state, assert | 二分搜索流水线 |
| planning_with_files | "规划", "复杂任务" | tool_call, state | 顺序流水线 |
| writing_plans | "计划", "spec" | dsl_call, state | 生成流水线 |
| executing_plans | "执行计划" | assert, generate_subgraph | 验证流水线 |
| subagent_driven_development | "并行执行", "子 agent" | fork, join, generate_subgraph | Fork-Join 并行 |
| dispatching_parallel_agents | "并行调查" | fork, join, state | Fork-Join 并行 |
| finishing_development_branch | "完成", "merge" | tool_call, user_input, state | 顺序流水线 |
| using_git_worktrees | "隔离工作区" | tool_call, state, assert | 工具序列 |
| writing_skills | "创建技能" | state/领域(跨轴) | 顺序流水线 |

---

## 轴2：领域/工具（11 个）

| 技能 | 触发词 | 核心 DSL 特性 | 编译模板 |
|------|--------|-------------|---------|
| cmake | "CMakeLists.txt", "cmake configure" | tool_call, state | 工具序列 |
| cmake_manage | "依赖", "跨平台" | tool_call, state | 工具序列 |
| cpp_architecture | "架构", "模块依赖" | dsl_call, tool_call | 分析 + 工具 |
| cpp_debug | "crash", "segfault", "死锁" | tool_call, dsl_call, fork | 诊断分支 |
| cpp_modernize | "现代化", "智能指针" | tool_call, dsl_call | 工具 + 生成 |
| cuda | "CUDA", "GPU", "kernel" | tool_call, state | 工具序列 |
| obsidian_markdown | "wiki", "callout" | tool_call | 单工具 |
| tavily_search | "搜索" | tool_call | 单工具 |
| web_search | "搜索" | tool_call | 单工具 |
| find_skills | "找技能" | tool_call | 单工具 |
| playwright | "浏览器" | tool_call | 单工具 |
| git_master | "commit", "rebase", "blame" | 领域/流程(跨轴) | 工具序列 |

---

## 轴3：审查/质量（4 个）

| 技能 | 触发词 | 核心 DSL 特性 | 编译模板 |
|------|--------|-------------|---------|
| review_work | "review", "审查" | fork, join, dsl_call, assert | Fork-Join 5路并行 |
| requesting_code_review | "发起 review" | fork, join, dsl_call | Fork-Join 并行 |
| receiving_code_review | "review 反馈" | 审查/流程(跨轴), state | 顺序处理 |
| state_modification_audit | "状态审计" | tool_call, state | 工具序列 |
| ai_slop_remover | "AI 味道", "清理" | tool_call | 顺序处理 |
| verification_before_completion | "完成前验证" | 审查/流程(跨轴), assert | 验证流水线 |

---

## 轴4：UI/前端（1 个）

| 技能 | 触发词 | 核心 DSL 特性 | 编译模板 |
|------|--------|-------------|---------|
| frontend_ui_ux | "UI", "界面", "CSS" | dsl_call, user_input | LLM 生成流水线 |

---

## 轴5：项目专用（5 个）

| 技能 | 触发词 | 生效范围 | 编译模板 |
|------|--------|---------|---------|
| openspec_apply_change | "implement" | 本项目 | 工具命令序列 |
| openspec_archive_change | "归档" | 本项目 | 工具命令序列 |
| openspec_explore | "explore" | 本项目 | 分析流水线 |
| openspec_propose | "propose" | 本项目 | 生成流水线 |
| customize_opencode | "配置 opencode" | 本项目/opencode | 配置流水线 |

---

## 元技能（1 个）

| 技能 | 分类 | 说明 |
|------|------|------|
| using_superpowers | 流程·元层级 | 描述"如何使用其他技能"的元方法论 |

---

## 统计

| 轴 | 计数 | 占比 |
|----|------|------|
| 轴1: 流程/方法论 | 12 | 31% |
| 轴2: 领域/工具 | 11 | 28% |
| 轴3: 审查/质量 | 6 | 15% |
| 轴4: UI/前端 | 1 | 3% |
| 轴5: 项目专用 | 5 | 13% |
| 跨轴 | 4 | 10% |
| **合计** | **39** | **100%** |

---

## 关联文档

| 文档 | 关系 |
|------|------|
| [01-taxonomy.md](01-taxonomy.md) | 本文的分类框架定义 — 映射表的理论基础 |
| [02-invoke-compose.md](02-invoke-compose.md) | invoke/compose 语法设计，映射表用于分类配置 |
| [04-skill-compiler-design.md](04-skill-compiler-design.md) | 编译模板列的对应实现 — 每轴在编译器中有专用 DAG 模板 |
| [examples/skill_porting/skills/](../../../examples/skill_porting/skills/) | 当前 6 技能的原始 SKILL.md 文件 — 数据源（规划 39） |
