# Ideal DSL Extension 02: skill_compose

**文件**: `02_skill_compose.md`
**状态**: 提案
**解决的问题**: 技能之间如何声明式组合、数据如何传递

---

## 动机

在复杂工作流中，技能之间往往需要组合：

```
brainstorming → writing_plans → subagent_driven_development
         ↓              ↓                ↓
    design_doc    plan_output      execution_result
```

当前需要手动连接：
```markdown
## /workflow/after_brainstorm
type: dsl_call
prompt: |
  设计文档: {{design_doc}}
  作为 writing_plans 技能...
```

这种方式的问题：
1. **手动传递** — 每一步都要写 prompt 来接收上一步输出
2. **类型丢失** — 结构化输出变成字符串再解析
3. **顺序固定** — 无法声明"这些技能可以并行"

---

## 提案：`type: skill_compose`

### 语法

```markdown
## /workflow/main
type: skill_compose
skills:
  - skill: brainstorming
    input:
      user_intent: "{{user_idea}}"
    output_alias: "brainstorm"
  - skill: writing_plans
    input:
      spec: "{{brainstorm.design_doc}}"  # 引用前一个技能的输出
    output_alias: "planner"
  - skill: subagent_driven_development
    input:
      plan: "{{planner.plan}}"
    mode: parallel
options:
  on_error: rollback|continue
  timeout: 300
output:
  final_result: "{{planner.result}}"
```

### 语义

| 字段 | 含义 |
|------|------|
| `skills` | 技能列表，按顺序执行 |
| `input` | 技能输入，引用前序技能的输出（如 `{{brainstorm.design_doc}}`） |
| `output_alias` | 给这个技能输出起的别名，用于后续引用 |
| `mode: parallel` | 标记这个技能可以与下一个并行执行 |
| `options.on_error` | 错误处理策略 |

### 执行流程

```
skill_compose
    ↓
按顺序执行 skills[0] (brainstorming)
    ↓
将输出存入 context.brainstorm
    ↓
skills[1] (writing_plans) 输入引用 {{brainstorm.design_doc}}
    ↓
...
```

### 并行优化

```markdown
## /workflow/multi_review
type: skill_compose
skills:
  - skill: review.code_quality
    output_alias: "cq"
  - skill: review.security
    output_alias: "sec"
  - skill: review.performance
    output_alias: "perf"
  - skill: review.merge_results
    input:
      code_quality: "{{cq.result}}"
      security: "{{sec.result}}"
      performance: "{{perf.result}}"
```

所有 review 技能可以并行执行，最后汇总。

---

## 与 skill_invoke 的关系

```
skill_compose 组合多个 skill_invoke

skill_invoke  = 单个技能调用
skill_compose = 技能组合 + 数据流
```

可以看作语法糖，但语义更强：

```markdown
## /equiv_compose
type: skill_compose
skills:
  - skill: brainstorming
    output_alias: "b"
```

等价于：

```markdown
## /equiv_invoke
type: skill_invoke
skill: brainstorming
output:
  result: b_result
```

---

## 高级特性

### 1. 条件执行

```markdown
## /conditional_review
type: skill_compose
skills:
  - skill: detect_issue_type
    output_alias: "detector"
  - skill: cpp_debug_crash
    condition: "{{detector.type}} == 'crash'"
    output_alias: "crash_result"
  - skill: cpp_debug_memory
    condition: "{{detector.type}} == 'memory'"
    output_alias: "memory_result"
```

### 2. 超时和重试

```markdown
## /resilient_call
type: skill_compose
skills:
  - skill: unreliable_service
    options:
      timeout: 30
      retry: 3
      backoff: exponential
    output_alias: "service_result"
```

### 3. 回滚策略

```markdown
## /with_rollback
type: skill_compose
skills:
  - skill: risky_operation
    output_alias: "op_result"
options:
  on_error: rollback
  rollback_skills:
    - skill: cleanup_operation
      input:
        state: "{{op_result.partial_state}}"
```

---

## 实现要求

### 1. 技能组合器

```cpp
class SkillComposer {
public:
    // 解析 skill_compose 节点
    void parse(const SkillComposeNode& node);

    // 执行组合
    ExecutionResult execute(Context& ctx);

    // 优化并行执行
    std::vector<std::vector<SkillInvocation>> optimize_parallel();
};
```

### 2. 数据流验证

```cpp
void validate_data_flow(const SkillComposeNode& node) {
    // 检查所有引用的变量是否存在
    for (auto& skill : node.skills) {
        for (auto& [key, value] : skill.input) {
            // 解析 {{source.sink}} 格式
            auto [source, sink] = parse_reference(value);
            if (!context.has_output(source, sink)) {
                throw DataFlowError("Cannot find: " + value);
            }
        }
    }
}
```

### 3. 并行优化器

```cpp
std::vector<std::vector<SkillInvocation>>
optimize_parallel(const SkillComposeNode& node) {
    // 构建依赖图
    // 找出入度为 0 的节点（可并行）
    // 拓扑排序分组
}
```

---

## 使用示例

### 示例 1：完整工作流

```markdown
## /feature_flow
type: skill_compose
skills:
  - skill: brainstorming
    input:
      user_intent: "{{feature_request}}"
    output_alias: "design"
  - skill: writing_plans
    input:
      spec: "{{design.design_doc}}"
    output_alias: "plan"
  - skill: verification_before_completion
    input:
      implementation: "{{plan.implementation}}"
      spec: "{{design.design_doc}}"
    output_alias: "verification"
options:
  on_error: continue
output:
  final: "{{verification.result}}"
```

### 示例 2：并行审查

```markdown
## /parallel_audit
type: skill_compose
skills:
  - skill: review.code_quality
    output_alias: "cq"
    mode: parallel
  - skill: review.security
    output_alias: "sec"
    mode: parallel
  - skill: review.performance
    output_alias: "perf"
    mode: parallel
  - skill: review.merge
    input:
      reports: ["{{cq.report}}", "{{sec.report}}", "{{perf.report}}"]
    output_alias: "final"
```

---

## 向后兼容

`s kill_compose` 是**新增节点类型**，不影响现有节点。

---

## 优先级

**高** — 技能组合是规模化使用技能的基础。

---

## 验证方式

1. 选取任一含链式 `dsl_call` 的 `.agent.md` 示例（如 `examples/skill_porting/agenticdsl/axis1_process/systematic_debugging.agent.md`），将其中的链式 dsl_call 改写为 skill_compose
2. 验证数据流正确性
3. 对比执行结果一致性