# Skill Evolution 调研总结

**日期**: 2026-07-16
**状态**: 🟡 调研中（librarian 后台调研 + 项目内部文档整理）

---

## 调研目标

为 HydraForge "Agent 进化管线" (SKILL → DSL → C++ → Wasm) 寻找 SOTA 理论与实践基础，并整理现有项目内部的相关设计。

## 调研范围

### 项目内部（已完成）

| 路径 | 内容 |
|------|------|
| `docs/proposals/skill-system/01-taxonomy.md` | 5 维度 Skill 分类（流程/领域/审查/UI/项目专用） |
| `docs/proposals/skill-system/02-invoke-compose.md` | skill_invoke + skill_compose DSL 语法 |
| `docs/proposals/skill-system/03-taxonomy-mapping.md` | 39 Skills 映射到 5 维度 + 编译模板 |
| `docs/proposals/skill-system/04-skill-compiler-design.md` | SKILL.md → .agent.md 编译器设计（SectionParser + 5 轴模板 + DAGBuilder） |

### 外部 SOTA（调研中）

librarian 后台调研 2024-2026 关于以下主题的论文与项目：
1. **Skill 进化**：自然语言 Skill → 结构化工作流的自动/半自动转写
2. **Skill 优化**：Agent 工作流热点识别与快实现替换
3. **Skill 编译**：Agent 定义编译为 native / Wasm
4. **Skill 等价验证**：演化前后行为一致性保障

## 关键发现（项目内部）

### 1. 已有 SkillCompiler 设计（非新概念）

`docs/proposals/skill-system/04-skill-compiler-design.md` 已经定义了完整的 SKILL.md → .agent.md 编译器：

```
SKILL.md
  → SectionParser (提取 ## Section)
  → AxisClassifier (按 metadata 判断轴)
  → TemplateEngine (按轴选模板 + 变量填充)
  → NodeGen (Section → Node list)
  → DAGBuilder (连接 + 验证)
  → .agent.md
```

5 种轴各有专用 DAG 模板：
- 轴1 流程/方法论 → 顺序流水线 + 分支循环
- 轴2 领域/工具 → 工具调用序列
- 轴3 审查/质量 → Fork-Join 并行
- 轴4 UI/前端 → LLM 生成流水线
- 轴5 项目专用 → 工具命令序列

### 2. 已识别 7 步实施路径

| Step | 内容 |
|------|------|
| 1 | 实现 SectionParser |
| 2-5 | 逐轴实现模板引擎 + NodeGen |
| 6 | SkillRegistry.compile_and_register 集成 |
| 7 | 验证覆盖率 > 90% |

### 3. 编译的 4 条原则

1. **模板驱动** — 编译是变量填充而非 NLP 理解
2. **确定性输出** — 同 SKILL.md 每次结果一致
3. **增量验证** — 每步骤独立验证
4. **可逆元数据** — 编译结果保留来源 SKILL.md 路径

## 与 ADR-0061 的关系

| 维度 | 现有设计 | ADR-0061 范围 |
|------|---------|--------------|
| SKILL.md → .agent.md | `skill-system/04` | ✅ 沿用，作为阶段 2 的核心机制 |
| .agent.md → C++ | 未涉及 | 🔵 ADR-0061 新增（性能化） |
| DSL/C++ → Wasm | 未涉及 | 🔵 ADR-0061 新增（可移植化） |
| 行为等价验证 | 未涉及 | 🔵 ADR-0061 新增（RegressionSuite） |
| 跨阶段统一抽象 | 未涉及 | 🔵 ADR-0061 整合 4 阶段为单一管线 |

## 调研文档清单

| 文档 | 状态 |
|------|------|
| `01-project-context.md` | ✅ 已完成 |
| `02-sota-survey.md` | 🟡 调研中（等待 librarian） |
| `03-evolution-pipeline-recommendations.md` | ⏳ 待 SOTA 完成后 |

## 下一步

1. 等待 librarian 完成 SOTA 论文调研（主题 1-4）
2. 综合项目内部设计 + SOTA 论文，形成"Skill Evolution Pipeline" 推荐
3. 更新 ADR-0061（Agent 进化与固化）的实施细节