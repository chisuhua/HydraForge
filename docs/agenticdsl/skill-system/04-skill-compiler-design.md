# Skill Compiler 设计

**状态**: 草案
**关联**: 01-taxonomy.md, 02-invoke-compose.md, 03-taxonomy-mapping.md
**日期**: 2026-05-20

---

## 动机

当前 39+ 技能以 SKILL.md 格式存在（自然语言描述），但 AgenticDSL 运行时只识别 `.agent.md` 子图格式。
需要一套"技能编译器"将 SKILL.md 自动编译为 `.agent.md`，实现：

1. **可移植** — 现有技能无需手动重写为 DAG
2. **可进化** — Agent 生成新 SKILL.md → 自动编译 → 注册到运行时
3. **一致性** — 编译结果遵循统一结构，而非人工手写的差异

---

## 核心设计

```
SKILL.md                          .agent.md
─────────                         ─────────
自然语言 + 章节结构      编译器    结构化 YAML + 节点 DAG
                              →
- 分类与触发词                       /__meta__ (execution_budget)
- How It Works (流程图)             控制流节点 (start → ... → end)
- Core Patterns (代码片段)           tool_call 节点
- Hard Gate (约束)                  assert 节点
- Pass Criteria (门禁条件)          fork/join (并行审核)
```

### 编译原则

1. **模板驱动** — 每种轴有专用 DAG 模板，编译是变量填充而非 NLP 理解
2. **确定性输出** — 同 SKILL.md 每次编译结果一致
3. **增量验证** — 每个步骤生成的节点可独立验证
4. **可逆元数据** — 编译结果保留来源 SKILL.md 路径，支持溯源

---

## 各轴编译映射

### 轴1：流程/方法论 → 顺序流水线模板

```
SKILL.md 章节           →     .agent.md 节点
──────────────────────────────────────────────
When to Use (条件)      →     [可选] assert 前置条件
How It Works (流程图)   →     DAG 结构骨架
  Step 1                          user_input / tool_call
  Step 2                          dsl_call (LLM 处理)
  Step 3 (分支)                    assert + 路由
  Step 4 (循环澄清)                 loop back via state
Hard Gate (约束)        →     assert 在关键节点前
Core Principles         不编译（保留为注释）

控制流模式:
  start → [前置条件?] → step1 → step2 → [分支?] → step3 → end
                            ↑__________|  (循环回路)
```

### 轴2：领域/工具 → 工具调用序列模板

```
SKILL.md 章节           →     .agent.md 节点
──────────────────────────────────────────────
When to Use (场景)      →     [可选] user_input 收集需求
Core Patterns (代码)    →     tool_call + assign
  ✅ 推荐模式                      工具调用的标准参数
  ❌ 反模式                        assert 参数校验
How It Works (一个流程) →     工具调用链
怎么工作的实际上在
Core Patterns 里体现

控制流模式:
  start → user_input → dsl_call(分析) → tool_call(step1)
       → tool_call(step2) → dsl_call(生成结果) → end
```

### 轴3：审查/质量 → Fork-Join 并行模板

```
SKILL.md 章节           →     .agent.md 节点
──────────────────────────────────────────────
The N Dimensions (维度) →     fork(branches=[...])
  维度1                                dsl_call (审查者1)
  维度2                                dsl_call (审查者2)
  ...
Pass Criteria (门禁)    →     assert (全部通过?汇总报告)
Hard Gate               →     前置 assert

控制流模式:
  start → fork → [dim1, dim2, ..., dimN]
              → join (等待全部)
              → assert (全部通过?)
              → [通过] end
              → [失败] 汇总报告 → user_input(修改) → 重新审查
```

### 轴4：UI/前端 → LLM 生成模板

```
控制流模式:
  start → user_input → dsl_call(分析需求) → dsl_call(生成UI)
       → tool_call(应用修改) → assert(验证) → end
```

### 轴5：项目专用 → 工具命令模板

```
控制流模式:
  start → tool_call(命令1) → dsl_call(解析结果)
       → tool_call(命令2) → assert(验证) → end
```

---

## 编译器架构

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

---

## 与 SkillRegistry 的集成

编译结果注册到运行时：

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

---

## 实施步骤

| 步骤 | 内容 | 产出 |
|------|------|------|
| Step 1 | 实现 SectionParser（可解析任意 SKILL.md 章节结构） | `src/common/skills/section_parser.h` |
| Step 2 | 为轴1（流程）实现模板引擎 + NodeGen | 10+ 流程技能可编译 |
| Step 3 | 为轴2（领域）实现模板引擎 + NodeGen | 11+ 领域技能可编译 |
| Step 4 | 为轴3（审查）实现 Fork-Join 模板 | 6+ 审查技能可编译 |
| Step 5 | 为轴4（UI）轴5（项目）实现模板 | 6+ 技能可编译 |
| Step 6 | SkillRegistry.compile_and_register 集成 | 启动时自动编译所有技能 |
| Step 7 | 验证：编译全部 39 技能，与手写版本对比 | 覆盖率 > 90% |

---

## 与文档体系的关系

| 文档 | 更新内容 |
|------|---------|
| [02-invoke-compose.md](02-invoke-compose.md) | 添加"skill_invoke 是语法糖，编译为 dsl_call"的补充说明 |
| [03-taxonomy-mapping.md](03-taxonomy-mapping.md) | 添加"编译模板"列（每轴对应模板类型） |
| [implementation-roadmap/01-roadmap.md](../implementation-roadmap/01-roadmap.md) | 添加"Skill Compiler"作为 Step 7 |
