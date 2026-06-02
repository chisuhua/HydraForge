# Ideal DSL Extension 01: skill_invoke

**文件**: `01_skill_invoke.md`
**状态**: 提案
**解决的问题**: 当前通过 `dsl_call + prompt` 模拟技能调用，缺乏原生支持

---

## 动机

当前 AgenticDSL 通过 `dsl_call` 节点 + 复杂 prompt 来模拟技能调用：
```markdown
## /skill/brainstorming
type: dsl_call
llm_tool: gpt-4
output_keys: ["skill_result"]
prompt: |
  作为 brainstorming 技能，帮助用户...
```

这种方式的问题：
1. **每次都要写 prompt** — 技能的行为无法复用
2. **无类型安全** — 输入输出都是字符串
3. **无法组合** — 技能之间无法声明式组合

---

## 提案：`type: skill_invoke`

### 语法

```markdown
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
|------|----------|--------------|
| Prompt 来源 | 内联 `prompt:` 字段 | 来自 SKILL.md |
| 输入类型 | 字符串模板 | 结构化 `input:` |
| 输出类型 | 字符串 | 结构化 `output:` |
| 可复用性 | 每次内联 | 声明一次，多次引用 |
| 技能组合 | 不支持 | 支持 |

---

## 使用场景

### 场景 1：技能链式调用

```markdown
## /workflow/start
type: skill_invoke
skill: "brainstorming"
input:
  user_intent: "{{user_idea}}"
output:
  design_doc: design_doc

## /workflow/plan
type: skill_invoke
skill: "writing_plans"
input:
  spec: "{{design_doc}}"
output:
  plan: plan_output

## /workflow/execute
type: skill_invoke
skill: "subagent_driven_development"
input:
  plan: "{{plan}}"
output:
  result: execution_result
```

### 场景 2：并行技能派发

```markdown
## /multi_review
type: fork
branches:
  - /review/code_quality  # skill_invoke: review.code_quality
  - /review/security       # skill_invoke: review.security
  - /review/performance    # skill_invoke: review.performance
context_isolation: deep_copy
next: /multi_review/join
```

### 场景 3：条件技能调用

```markdown
## /route_by_type
type: switch
input: "{{problem_type}}"
cases:
  crash: /invoke/cpp_debug_crash
  memory: /invoke/cpp_debug_memory
  deadlock: /invoke/cpp_debug_deadlock

## /invoke/cpp_debug_crash
type: skill_invoke
skill: "cpp_debug"
input:
  symptom: "{{symptom}}"
  problem_type: "crash"
output:
  root_cause: crash_cause
  fix: crash_fix
```

---

## 实现要求

### 1. 技能注册表

```cpp
class SkillRegistry {
public:
    void register_skill(const std::string& name, const Skill& skill);
    Skill get_skill(const std::string& name) const;

    // 加载 skills/ 目录下的所有 SKILL.md
    void load_from_directory(const std::string& path);
};
```

### 2. 技能定义（SKILL.md 解析）

SKILL.md 需要解析以下字段用于 `skill_invoke`：

```markdown
# Skill: brainstorming
...
## Skill Interface
input:
  - user_intent: string
  - project_context: string (optional)
output:
  - design_doc: string
  - next_skill: string (optional)
default_prompt: |
  你是一个 brainstorming 助手...
```

### 3. 类型检查

```cpp
// skill_invoke 节点
struct SkillInvokeNode {
    std::string skill_name;
    std::map<std::string, std::string> input_mapping;
    std::map<std::string, std::string> output_mapping;
};

// 编译时检查
void validate_skill_invoke(const SkillInvokeNode& node, const SkillRegistry& registry) {
    auto skill = registry.get_skill(node.skill_name);

    // 检查 input 变量
    for (auto& [key, value] : node.input_mapping) {
        if (!skill.has_input(key)) {
            throw SkillInvokeError("Unknown input: " + key);
        }
    }

    // 检查 output 映射
    for (auto& [key, value] : node.output_mapping) {
        if (!skill.has_output(key)) {
            throw SkillInvokeError("Unknown output: " + key);
        }
    }
}
```

---

## 向后兼容

`skill_invoke` 是**新增节点类型**，不影响现有 `dsl_call`、`tool_call` 等节点。

现有使用 `dsl_call` 模拟技能的工作流可以逐步迁移到 `skill_invoke`。

---

## 优先级

**高** — 这是最基础的功能，支持技能组合和复用。

---

## 验证方式

1. 选取任一含 `dsl_call` 的 `.agent.md` 示例（如 `examples/skill_porting/agenticdsl/axis1_process/systematic_debugging.agent.md`），将其中的 `dsl_call` 改写为 `skill_invoke`
2. 验证行为一致性
3. 运行测试确保功能不变