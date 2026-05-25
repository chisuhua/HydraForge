# Skill Taxonomy — AgenticDSL Porting Examples

> 展示如何将技能（Skills）按 5 维度分类，并用 AgenticDSL 实现

## 概述

本目录展示 Skill 技能的分类体系，以及每种类型技能在 AgenticDSL 中的实现对照。

### 目录结构

```
examples/skill_porting/
├── README.md                    # 本文件：总览和分类矩阵
│
├── skills/                      # 技能示例（SKILL.md 标准格式）
│   ├── axis1_process/          # 轴1: 流程/方法论
│   ├── axis2_domain/           # 轴2: 领域/工具
│   ├── axis3_review/           # 轴3: 审查/质量
│   ├── axis4_frontend/         # 轴4: UI/前端
│   └── axis5_project/          # 轴5: 项目专用
│
├── agenticdsl/                  # AgenticDSL 实现对照
│   ├── axis1_process/
│   ├── axis2_domain/
│   ├── axis3_review/
│   ├── axis4_frontend/
│   └── axis5_project/
│
└── ideal_dsl/                   # 理想 DSL 扩展提案
    ├── 01_skill_invoke.md
    ├── 02_skill_compose.md
    ├── 03_workflow_skill.md
    ├── 04_domain_skill.md
    └── 05_review_skill.md
```

---

## 5 维度分类矩阵

| 轴 | 分类 | 核心特征 | 代表技能 |
|----|------|---------|---------|
| 轴1 | **流程/方法论** | 定义工作流顺序，强制约束 | brainstorming, systematic_debugging, test_driven_development |
| 轴2 | **领域/工具** | 提供特定技术栈能力 | cmake_workflow, cpp_debug, cuda |
| 轴3 | **审查/质量** | 门禁检查，质量验证 | review_work, receiving_code_review, verification_before_completion |
| 轴4 | **UI/前端** | 视觉呈现，交互设计 | frontend_ui_workflow, playwright |
| 轴5 | **项目专用** | 仅对当前项目有效 | openspec_propose, openspec_apply, customize_opencode |

---

## 技能示例清单

### 轴1: 流程/方法论

| 技能 | 分类 | 触发词 |
|------|------|--------|
| `brainstorming` | 流程 | "我想实现...", "设计一下..." |
| `systematic_debugging` | 流程 | "error", "crash", "bug", "调试" |

### 轴2: 领域/工具

| 技能 | 分类 | 触发词 |
|------|------|--------|
| `cmake_workflow` | 领域 | "CMakeLists.txt", "cmake configure" |
| `cpp_debug` | 领域 | "crash", "segfault", "内存泄漏" |

### 轴3: 审查/质量

| 技能 | 分类 | 触发词 |
|------|------|--------|
| `review_work` | 审查 | "review", "审查", "post-implementation" |
| `receiving_code_review` | 审查 | "code review feedback", "review 反馈" |

### 轴4: UI/前端

| 技能 | 分类 | 触发词 |
|------|------|--------|
| `frontend_ui_workflow` | 前端 | "UI", "界面", "CSS", "布局" |

### 轴5: 项目专用

| 技能 | 分类 | 触发词 |
|------|------|--------|
| `openspec_propose` | 项目 | "propose", "new feature", "我想改变" |
| `openspec_apply` | 项目 | "implement", "apply change", "execute plan" |

---

## 理想 DSL 扩展

当前 AgenticDSL 的节点类型不足以原生支持技能系统，需要以下扩展：

| # | 文件 | 核心提案 | 对应技能类型 |
|---|------|---------|------------|
| 01 | `01_skill_invoke.md` | `type: skill_invoke` — 原生技能调用 | 所有技能 |
| 02 | `02_skill_compose.md` | `skill_compose` — 技能链式组合 | 技能组合 |
| 03 | `03_workflow_skill.md` | `type: workflow_skill` — 流程类技能标准模式 | 轴1 |
| 04 | `04_domain_skill.md` | `type: domain_skill` — 领域类技能标准模式 | 轴2 |
| 05 | `05_review_skill.md` | `type: review_skill` — 审查类技能标准模式 | 轴3 |

### 优先级

```
P0 (基础):
  - 01_skill_invoke.md      # 技能调用基础

P1 (核心):
  - 02_skill_compose.md     # 技能组合
  - 03_workflow_skill.md    # 流程技能模式

P2 (扩展):
  - 04_domain_skill.md      # 领域技能模式
  - 05_review_skill.md      # 审查技能模式
```

---

## 技能对照关系

```
skills/axis1_process/brainstorming/SKILL.md
    ↕ 对照
agenticdsl/axis1_process/brainstorming.agent.md
    ↕ 理想 DSL 扩展
ideal_dsl/03_workflow_skill.md
```

每个技能目录包含：
1. **SKILL.md** — 技能定义（标准格式）
2. **对应 .agent.md** — AgenticDSL 实现
3. **scripts/reference/** — （如需要）参考资料

---

## 使用说明

### 查看技能示例

```bash
# 查看轴1流程技能
cat skills/axis1_process/brainstorming/SKILL.md
cat agenticdsl/axis1_process/brainstorming.agent.md

# 查看理想 DSL 扩展
cat ideal_dsl/01_skill_invoke.md
```

### 理解分类

如果想理解某个技能属于哪个轴：
1. 查看 `skills/<axis>/<skill>/SKILL.md`
2. 检查触发词和 When to Use 部分

### 实现新技能

1. 确定技能属于哪个轴
2. 参考同轴的 SKILL.md 格式
3. 参考对应 .agent.md 实现方式
4. 如需 DSL 扩展，参考 ideal_dsl/ 目录