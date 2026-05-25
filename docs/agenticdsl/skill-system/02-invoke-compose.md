# ADR-007: Skill Invoke 与 Skill Compose

**ID**: ADR-007
**日期**: 2026-05-20
**状态**: 草案
**关联**: ADR-001, LS-001

---

## 上下文

当前 AgenticDSL 通过 `dsl_call` 节点模拟技能调用：

```yaml
## /workflow/brainstorm
type: dsl_call
llm_tool: gpt-4
prompt: |
  作为 brainstorming 技能，帮助用户...
output_keys: ["result"]
```

这种方式的问题：
1. 每次都要写 prompt（技能的行为无法复用）
2. 无类型安全（输入输出都是字符串）
3. 无法组合（技能之间无法声明式组合）

---

## 方案 A: skill_invoke

### 语法

```yaml
## /brainstorm/start
type: skill_invoke
skill: "brainstorming"
input:
  user_intent: "{{user_input}}"
  project_context: "{{project_files}}"
output:
  design_doc: design_doc
  next_skill: next_skill
```

### 语义

| 字段 | 含义 |
|------|------|
| `skill` | 技能名称（引用 SKILL.md 中定义的技能） |
| `input` | 输入变量映射（传递给技能的参数） |
| `output` | 输出变量映射（技能返回的结果） |

### 执行流程

```
skill_invoke 节点
    ↓
加载 skill: brainstorming/SKILL.md
    ↓
验证 input 变量存在
    ↓
使用技能定义的默认 prompt + 输入变量渲染
    ↓
执行 LLM 调用
    ↓
将 output 映射到当前图变量
```

### 与 dsl_call 的对比

| 特性 | dsl_call | skill_invoke |
|------|----------|-------------|
| Prompt 来源 | 内联 prompt: | SKILL.md 定义 |
| 输入类型 | 字符串模板 | 结构化 input: |
| 输出类型 | 字符串 | 结构化 output: |
| 可复用性 | 每次内联 | 声明一次，多次引用 |
| 技能组合 | 不支持 | 支持 skill_compose |

---

## 方案 B: skill_compose

### 语法

```yaml
## /workflow/main
type: skill_compose
skills:
  - skill: brainstorming
    input:
      user_intent: "{{user_idea}}"
    output_alias: "brainstorm"

  - skill: writing_plans
    input:
      spec: "{{brainstorm.design_doc}}"
    output_alias: "planner"

  - skill: subagent_driven_development
    input:
      plan: "{{planner.plan}}"
    output_alias: "executor"

options:
  on_error: rollback|continue
output:
  final: "{{executor.result}}"
```

### 数据流

```
skill_compose
    ↓
skill[0] (brainstorming) 执行
    ↓ 输出存入 context.brainstorm
skill[1] (writing_plans) 通过 {{brainstorm.design_doc}} 引用
    ↓ 输出存入 context.planner
skill[2] (subagent_driven_development) 通过 {{planner.plan}} 引用
    ↓
output.final = executor.result
```

### 并行优化

```yaml
## /parallel_review
type: skill_compose
skills:
  - skill: review.code_quality       # 并行
    output_alias: "cq"
    mode: parallel
  - skill: review.security           # 并行
    output_alias: "sec"
    mode: parallel
  - skill: review.merge              # 汇总（等前两个完成）
    input:
      reports: ["{{cq.report}}", "{{sec.report}}"]
    output_alias: "final"
```

---

## SKILL.md 的接口声明

skill_invoke 的执行依赖于 SKILL.md 中的接口声明：

```markdown
# Skill: brainstorming

## Skill Interface
input:
  user_intent:
    type: string
    required: true
    description: "用户想要实现的功能描述"
  project_context:
    type: json
    required: false
    description: "项目文件结构"

output:
  design_doc:
    type: string
    description: "完整的设计文档"
  next_skill:
    type: string
    description: "下一步推荐的技能名称"

default_prompt: |
  你是一个 brainstorming 助手...
```

---

## 实施考虑

### 技能注册表

```cpp
class SkillRegistry {
public:
    void register_skill(const std::string& name, const SkillDef& skill);
    SkillDef get_skill(const std::string& name) const;

    // 从 SKILL.md 文件加载
    void load_from_directory(const std::string& path);

    // 运行时注册（用于 Agent 自举）
    void register_dynamic_skill(const std::string& name, const SkillDef& skill);
};
```

### 类型验证

```cpp
void validate_skill_invoke(const SkillInvokeNode& node, const SkillRegistry& registry) {
    auto skill = registry.get_skill(node.skill_name);

    // 检查 input 变量
    for (auto& [key, value] : node.input_mapping) {
        if (!skill.has_input(key)) {
            throw SkillInvokeError("Unknown input: " + key);
        }
    }
}
```

---

## 与现有功能的关系

### 编译路线（采纳 Oracle 建议后的定位）

**skill_invoke 和 skill_compose 不是新增节点类型，而是语法糖。**
它们在解析阶段编译（展开）为现有节点类型：

```
skill_invoke { skill: "brainstorming", ... }
    ↓ 编译展开
dsl_call { subgraph: "/skills/brainstorming", input: ..., output_keys: [...] }

skill_compose { A → B → [C, D 并行] → E }
    ↓ 编译展开
FORK → dsl_call(A) → JOIN
     → dsl_call(B) → FORK → dsl_call(C) + dsl_call(D) → JOIN → dsl_call(E)
```

### 为什么这样设计？

| 考虑 | dsl_call 展开 | 新增节点类型 |
|------|-------------|-------------|
| Parser 复杂度 | 0（复用现有代码） | + 新 NodeType 枚举 + dispatch switch |
| Executor 复杂度 | 0（复用现有调度） | + 新 execute 方法 |
| 序列化 | 0 | + 新 node 序列化/反序列化 |
| 向后兼容 | ✅ 现有图不受影响 | ✅ 也不影响，但增加了表面积 |
| 语义清晰度 | YAML 仍是 skill_invoke | 也是 skill_invoke |

结论：YAML 语法层面保持 `skill_invoke`（给程序员看），内部立即展开为 `dsl_call`（给运行时看）。

### 编译器的角色

实际由 **SkillCompiler**（[04-skill-compiler-design.md](04-skill-compiler-design.md)）完成：
1. 读取 SKILL.md → 按轴选择 DAG 模板 → 生成 .agent.md
2. 编译后的 .agent.md 注册到 StandardLibraryLoader
3. `skill_invoke { skill: "brainstorming" }` → `dsl_call { subgraph: "/skills/brainstorming" }`
4. `skill_compose { ... }` → FORK/JOIN + dsl_call 的组合

---

## 关联文档

| 文档 | 关系 |
|------|------|
| [01-taxonomy.md](01-taxonomy.md) | 5 维度分类框架 — skill_invoke/skill_compose 的分类理论基础 |
| [03-taxonomy-mapping.md](03-taxonomy-mapping.md) | 39 技能映射表，为 invoke/compose 提供注册元数据 |
| [docs/specs/dsl.md](../../specs/dsl.md) | 当前 dsl_call 节点定义 — skill_invoke 从 dsl_call 扩展而来 |
| [docs/adr/adr-0009-dsl-standard-library.md](../../adr/adr-0009-dsl-standard-library.md) | 当前标准库调度规范，指导 skill_compose 组合规则 |
