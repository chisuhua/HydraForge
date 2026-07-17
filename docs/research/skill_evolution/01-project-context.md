# 项目内部 Skill 系统设计文档整理

**日期**: 2026-07-16
**来源**: `docs/proposals/skill-system/` 全部 4 篇文档

---

## 一、文档体系概览

```
docs/proposals/skill-system/
├── 01-taxonomy.md            (5 维度分类, 92 行)
├── 02-invoke-compose.md      (skill_invoke + skill_compose, 259 行)
├── 03-taxonomy-mapping.md    (39 Skills 映射表, 117 行)
└── 04-skill-compiler-design.md (SKILL.md → .agent.md 编译器, 237 行)
```

---

## 二、01-taxonomy.md — 5 维度分类

### 核心问题

随着技能数量增长（当前 39 个），需要分类方法决定：
1. 新技能落在哪里
2. Agent 如何选择正确技能
3. 为 AgenticDSL 语言扩展提供设计依据

### 5 个维度

| 轴 | 分类 | 核心问题 | 约束力 | 技术绑定 |
|----|------|---------|--------|---------|
| 1 | 流程/方法论 | "先做 A 才能做 B"？ | 高 | 低 |
| 2 | 领域/工具 | "用什么工具/查什么文档"？ | 中 | 高 |
| 3 | 审查/质量 | "交付前必须过的检查"？ | 高 | 中 |
| 4 | UI/前端 | 涉及 UI/UX/视觉呈现？ | 中 | 高 |
| 5 | 项目专用 | 只在当前仓库有效？ | 高 | 高 |

### 分类判断流程

```
1. 只在本项目有效？        → 轴5-项目专用
2. 涉及视觉/UI？           → 轴4-UI/前端
3. 交付前必须过的检查？    → 轴3-审查/质量
4. 告诉你用什么技术/工具？ → 轴2-领域/工具
5. 否则 → 轴1-流程/方法论
```

### 跨轴技能处理

确定**主轴**：该技能的核心目的？拿掉它哪个轴受影响最严重？

例：`verification-before-completion` → 主轴=审查（本质是质量门禁）

---

## 三、02-invoke-compose.md — Skill Invoke 与 Compose

### 当前问题

AgenticDSL 用 `dsl_call` 节点模拟技能调用，每次都要内联 prompt：
- 行为无法复用
- 无类型安全
- 无法声明式组合

### 提案：skill_invoke 节点

```yaml
type: skill_invoke
skill: "brainstorming"
input:
  user_intent: "{{user_input}}"
  project_context: "{{project_files}}"
output:
  design_doc: design_doc
  next_skill: next_skill
```

### 提案：skill_compose 节点

支持顺序 + 并行：

```yaml
type: skill_compose
skills:
  - skill: brainstorming
    input: {user_intent: "{{user_idea}}"}
    output_alias: "brainstorm"
  
  - skill: writing_plans
    input: {spec: "{{brainstorm.design_doc}}"}
    output_alias: "planner"
  
  - skill: subagent_driven_development
    input: {plan: "{{planner.plan}}"}
    output_alias: "executor"

options:
  on_error: rollback|continue
```

并行优化示例：
```yaml
skills:
  - skill: review.code_quality       # 并行
    mode: parallel
    output_alias: "cq"
  - skill: review.security           # 并行
    mode: parallel
    output_alias: "sec"
  - skill: review.merge              # 汇总
    input: {reports: ["{{cq.report}}", "{{sec.report}}"]}
```

### SKILL.md 接口声明

```markdown
# Skill: brainstorming

## Skill Interface
input:
  user_intent:
    type: string
    required: true
  project_context:
    type: json
    required: false

output:
  design_doc:
    type: string
  next_skill:
    type: string

default_prompt: |
  你是一个 brainstorming 助手...
```

### 编译路线（重要决策）

**skill_invoke 和 skill_compose 不是新增节点类型，而是语法糖。** 它们在解析阶段编译（展开）为现有节点类型：

```
skill_invoke { skill: "brainstorming", ... }
    ↓ 编译展开
dsl_call { subgraph: "/skills/brainstorming", input: ..., output_keys: [...] }

skill_compose { A → B → [C, D 并行] → E }
    ↓ 编译展开
FORK → dsl_call(A) → JOIN → dsl_call(B) → FORK → dsl_call(C) + dsl_call(D) → JOIN → dsl_call(E)
```

| 考虑 | 编译展开 | 新增节点类型 |
|------|---------|------------|
| Parser 复杂度 | 0 | + 新 NodeType + dispatch |
| Executor 复杂度 | 0 | + 新 execute 方法 |
| 向后兼容 | ✅ | ✅ |
| 语义清晰度 | YAML 仍是 skill_invoke | 同 |

**结论**：YAML 语法层面保持 `skill_invoke`，内部立即展开为 `dsl_call`。

### SkillRegistry

```cpp
class SkillRegistry {
public:
    void register_skill(const std::string& name, const SkillDef& skill);
    SkillDef get_skill(const std::string& name) const;
    void load_from_directory(const std::string& path);
    void register_dynamic_skill(const std::string& name, const SkillDef& skill);
};
```

---

## 四、03-taxonomy-mapping.md — 39 Skills 全量映射

### 统计

| 轴 | 计数 | 占比 |
|----|------|------|
| 轴1: 流程/方法论 | 12 | 31% |
| 轴2: 领域/工具 | 11 | 28% |
| 轴3: 审查/质量 | 6 | 15% |
| 轴4: UI/前端 | 1 | 3% |
| 轴5: 项目专用 | 5 | 13% |
| 跨轴 | 4 | 10% |
| **合计** | **39** | **100%** |

### 轴1 流程/方法论（12 个）

| 技能 | 触发词 | 编译模板 |
|------|--------|---------|
| brainstorming | "我想实现...", "设计一下..." | 顺序流水线 + 分支循环 |
| test_driven_development | "测试", "TDD", "red-green" | 循环流水线 |
| systematic_debugging | "error", "crash", "bug" | 分支流水线 |
| regression_bisect | "回归", "bisect" | 二分搜索流水线 |
| planning_with_files | "规划", "复杂任务" | 顺序流水线 |
| writing_plans | "计划", "spec" | 生成流水线 |
| executing_plans | "执行计划" | 验证流水线 |
| **subagent_driven_development** | "并行执行", "子 agent" | **Fork-Join 并行** |
| dispatching_parallel_agents | "并行调查" | Fork-Join 并行 |
| finishing_development_branch | "完成", "merge" | 顺序流水线 |
| using_git_worktrees | "隔离工作区" | 工具序列 |
| writing_skills | "创建技能" | 顺序流水线 |

### 轴2 领域/工具（11 个）

| 技能 | 触发词 | 编译模板 |
|------|--------|---------|
| cmake / cmake_manage | "CMakeLists.txt", "依赖" | 工具序列 |
| cpp_architecture | "架构", "模块依赖" | 分析 + 工具 |
| cpp_debug | "crash", "segfault" | 诊断分支 |
| cpp_modernize | "现代化", "智能指针" | 工具 + 生成 |
| cuda | "CUDA", "GPU", "kernel" | 工具序列 |
| obsidian_markdown | "wiki", "callout" | 单工具 |
| tavily_search / web_search | "搜索" | 单工具 |
| find_skills | "找技能" | 单工具 |
| playwright | "浏览器" | 单工具 |
| git_master | "commit", "rebase" | 工具序列（跨轴） |

### 轴3 审查/质量（6 个）

| 技能 | 触发词 | 编译模板 |
|------|--------|---------|
| **review_work** | "review", "审查" | **Fork-Join 5路并行** |
| requesting_code_review | "发起 review" | Fork-Join 并行 |
| receiving_code_review | "review 反馈" | 顺序处理 |
| state_modification_audit | "状态审计" | 工具序列 |
| ai_slop_remover | "AI 味道", "清理" | 顺序处理 |
| verification_before_completion | "完成前验证" | 验证流水线（跨轴） |

### 轴4 UI/前端（1 个）

`frontend_ui_ux` → LLM 生成流水线

### 轴5 项目专用（5 个）

OpenSpec 系列（apply/archive/explore/propose）+ customize_opencode

---

## 五、04-skill-compiler-design.md — Skill 编译器设计

### 动机

当前 39+ 技能以 SKILL.md 格式存在，但 AgenticDSL 运行时只识别 `.agent.md` 子图格式。需要 SKILL.md → .agent.md 的"技能编译器"，实现：
1. 可移植 — 现有技能无需手动重写为 DAG
2. 可进化 — Agent 生成新 SKILL.md → 自动编译 → 注册
3. 一致性 — 编译结果遵循统一结构

### 编译原则

1. **模板驱动** — 每种轴有专用 DAG 模板，编译是变量填充而非 NLP 理解
2. **确定性输出** — 同 SKILL.md 每次编译结果一致
3. **增量验证** — 每步骤独立验证
4. **可逆元数据** — 编译结果保留来源 SKILL.md 路径，支持溯源

### SKILL.md → .agent.md 映射总览

```
SKILL.md                          .agent.md
自然语言 + 章节结构     编译器     结构化 YAML + 节点 DAG
                               →
- 分类与触发词                       /__meta__ (execution_budget)
- How It Works (流程图)             控制流节点 (start → ... → end)
- Core Patterns (代码片段)           tool_call 节点
- Hard Gate (约束)                  assert 节点
- Pass Criteria (门禁条件)          fork/join (并行审核)
```

### 各轴编译模板

#### 轴1：流程/方法论 → 顺序流水线模板

```
SKILL.md 章节           →     .agent.md 节点
When to Use (条件)      →     [可选] assert 前置条件
How It Works (流程图)   →     DAG 结构骨架
  Step 1                          user_input / tool_call
  Step 2                          dsl_call (LLM 处理)
  Step 3 (分支)                    assert + 路由
  Step 4 (循环澄清)                 loop back via state
Hard Gate (约束)        →     assert 在关键节点前
Core Principles         不编译（保留为注释）
```

控制流：`start → [前置条件?] → step1 → step2 → [分支?] → step3 → end`，带循环回路。

#### 轴2：领域/工具 → 工具调用序列模板

```
When to Use (场景)      →     [可选] user_input 收集需求
Core Patterns (代码)    →     tool_call + assign
  ✅ 推荐模式                      工具调用的标准参数
  ❌ 反模式                        assert 参数校验
How It Works            →     工具调用链
```

控制流：`start → user_input → dsl_call(分析) → tool_call(step1) → tool_call(step2) → dsl_call(生成结果) → end`

#### 轴3：审查/质量 → Fork-Join 并行模板

```
The N Dimensions (维度) →     fork(branches=[...])
  维度1                                dsl_call (审查者1)
  维度2                                dsl_call (审查者2)
  ...
Pass Criteria (门禁)    →     assert (全部通过?)
Hard Gate               →     前置 assert
```

控制流：
```
start → fork → [dim1, dim2, ..., dimN]
       → join (等待全部)
       → assert (全部通过?)
       → [通过] end
       → [失败] 汇总报告 → user_input(修改) → 重新审查
```

#### 轴4：UI/前端 → LLM 生成模板

```
start → user_input → dsl_call(分析需求) → dsl_call(生成UI)
     → tool_call(应用修改) → assert(验证) → end
```

#### 轴5：项目专用 → 工具命令模板

```
start → tool_call(命令1) → dsl_call(解析结果)
     → tool_call(命令2) → assert(验证) → end
```

### 编译器架构

```
                     SkillCompiler
                     ────────────
                                                    ┌─────────────┐
  SKILL.md ───→ SectionParser ───→ Section[] ────→ │ Template     │
                                                    │ Engine       │
                                                    │              │
                     AxisClassifier ───────────────→ │ Selects      │
                       (从分类元数据判断轴)           │ Template per │
                                                    │ Axis         │
                                                    └──────┬──────┘
                                                           │
                                                           ▼
                                                    ┌─────────────┐
                                                    │ NodeGen      │
                                                    │ Section →    │
                                                    │ Node list    │
                                                    └──────┬──────┘
                                                           │
                                                           ▼
                                                    ┌─────────────┐
                                                    │ DAGBuilder   │
                                                    │ 连接 next    │
                                                    │ 验证完整性   │
                                                    └──────┬──────┘
                                                           │
                                                           ▼
                                                    .agent.md
```

### 组件职责

| 组件 | 职责 | 实现位置 |
|------|------|---------|
| **SectionParser** | 读取 SKILL.md，提取 `## Section` 标题和内容 | 新文件 |
| **AxisClassifier** | 从 `**分类**:` 元数据判断轴 | 现有技能元数据 |
| **TemplateEngine** | 每种轴持有一个 DAG 模板，填充变量 | 新文件 |
| **NodeGen** | 将 Section 内容转为节点定义 | 每个节点类型一个生成器 |
| **DAGBuilder** | 连接节点 `next` 指针，验证无悬挂边 | 复用现有验证逻辑 |

### 模板格式示例

模板是带占位符的 .agent.md 片段：

```yaml
## /{{skill_name}}/start
type: start
next: ["/{{skill_name}}/step_1"]

## /{{skill_name}}/step_1
type: user_input
prompt: "{{section_content}}"
input_variable: "{{step_id}}_input"
next: ["/{{skill_name}}/step_2"]

## /{{skill_name}}/step_2
type: dsl_call
llm_tool: gpt-4
output_keys: ["{{step_id}}_result"]
prompt: "{{section_content}}"
next: ["/{{skill_name}}/end"]

## /{{skill_name}}/end
type: end
```

### SkillRegistry 集成

```cpp
class SkillRegistry {
  void compile_and_register(const std::string& skill_path) {
      // 1. 读取 SKILL.md
      // 2. 解析章节
      // 3. 按轴选择模板
      // 4. 生成 .agent.md
      // 5. 注册到 StandardLibraryLoader
  }

  void load_all_from_directory(const std::string& dir) {
      for (auto& entry : fs::directory_iterator(dir)) {
          if (entry.path().extension() == ".md" &&
              !is_agent_dsl(entry.path())) {
              compile_and_register(entry.path());
          }
      }
  }
};
```

### 7 步实施路径

| Step | 内容 | 产出 |
|------|------|------|
| 1 | 实现 SectionParser | `src/common/skills/section_parser.h` |
| 2 | 流程轴模板 + NodeGen | 10+ 流程技能可编译 |
| 3 | 领域轴模板 + NodeGen | 11+ 领域技能可编译 |
| 4 | 审查轴 Fork-Join 模板 | 6+ 审查技能可编译 |
| 5 | UI + 项目轴模板 | 6+ 技能可编译 |
| 6 | SkillRegistry.compile_and_register 集成 | 启动时自动编译 |
| 7 | 验证覆盖率 > 90% | 与手写版本对比 |

---

## 六、关键交叉发现

### 1. SkillCompiler 概念已存在，可直接复用

`skill-system/04` 已定义完整的 SKILL.md → .agent.md 编译器，这是 ADR-0061 阶段 2（Solidification）的现成基础。

### 2. skill_compose 已支持并行 + 顺序组合

ADR-0060 的"五种协作模式"中：
- ① 同步 RPC = skill_invoke 展开为 dsl_call
- ⑤ fork/join = skill_compose 的 `mode: parallel` + `output_alias` 链式组合

这意味着 ADR-0061 的性能化和可移植化可以基于 skill_compose 的组合结构优化。

### 3. 模板驱动策略确认

`04-skill-compiler-design.md` 明确"编译是变量填充而非 NLP 理解"——这与 SOTA "Neuro-symbolic orchestration"（Loom DSL）一致。

### 4. 与 ADR-0061 的边界

| 范围 | 在哪里定义 |
|------|-----------|
| SKILL.md 章节结构 / 5 轴分类 / 模板 / 编译展开 | `skill-system/01-04`（已存在） |
| 跨阶段统一管线（4 阶段 API） / 行为等价 / 性能化 / Wasm 编译 | ADR-0061（待整合） |

**建议**：ADR-0061 不重复 `04` 的细节，引用其设计并扩展跨阶段编排。