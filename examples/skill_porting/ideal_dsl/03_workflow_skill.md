# Ideal DSL Extension 03: workflow_skill

**文件**: `03_workflow_skill.md`
**状态**: 提案
**解决的问题**: 流程类技能（轴1）的 DSL 模式标准化

---

## 动机

轴1（流程/方法论）技能有一个共同模式：

```
start → explore → validate → propose → design → end
```

每个流程类技能都是这个模式的变体，但当前每个技能都要自己定义图结构。

理想情况：流程类技能可以直接用声明式的方式定义。

---

## 现有流程类技能的共同结构

### brainstorming
```
start → load_context → init_state → ask_intent → analyze_intent → check_clarity
    → (if not clear) ask_clarification → update_intent → loop
    → propose_approaches → present_approaches → build_design → save_design
    → confirm_design → (if not approved) revise_design → loop
    → skill_transition → end
```

### systematic_debugging
```
start → collect_symptoms → classify_problem → init_debug_context → gather_environment
    → select_diagnostic → run_diagnostics (fork) → merge_diagnostics
    → form_hypothesis → verify_hypothesis → judge_hypothesis
    → (if not confirmed) refine_hypothesis → loop
    → implement_fix → verify_fix → judge_fix
    → (if failed) revert_and_iterate → loop
    → document_fix → end
```

### test_driven_development
```
start → setup_context → phase_red (write failing test)
    → save_test → verify_red → check_red → (if fail) fix_red → loop
    → phase_green (write minimal implementation)
    → save_impl → verify_green → check_green → (if fail) fix_green → loop
    → phase_refactor → refactor_code → verify_refactor → check_refactor
    → (if not done) continue_refactor → loop
    → next_feature → (if more) loop
    → end
```

### 共同模式识别

1. **阶段（Phase）** — 每个技能都有明确的阶段
2. **门禁（Gate）** — 阶段之间有判断条件
3. **循环（Loop）** — 某些阶段需要迭代
4. **分支（Branch）** — 某些阶段可以并行

---

## 提案：流程类技能标准 DSL 模式

### 核心语法

```markdown
## /<skill>/start
type: workflow_skill
skill: "brainstorming"
phases:
  - name: intent_gathering
    steps:
      - load_context
      - init_state
      - ask_intent
    gate:
      type: clarity_check
      on_fail: ask_clarification
      loop: true

  - name: design_proposal
    steps:
      - propose_approaches
      - present_approaches
      - build_design
      - save_design
    gate:
      type: user_approval
      on_fail: revise_design

  - name: transition
    steps:
      - skill_transition
    end: true
```

### 语义

| 字段 | 含义 |
|------|------|
| `workflow_skill` | 声明这是一个流程类技能工作流 |
| `phases` | 阶段列表 |
| `phases[].steps` | 该阶段的步骤 |
| `phases[].gate` | 进入下一阶段前的要求 |
| `phases[].loop` | 是否循环直到 gate 通过 |
| `phases[].end` | 是否是最后一个阶段 |

### 简化示例

用 `workflow_skill` 重写 brainstorming：

```markdown
## /brainstorm/start
type: workflow_skill
skill: "brainstorming"

phases:
  - name: intent_gathering
    loop: true
    gate:
      type: dsl_call
      llm_tool: gpt-4
      prompt: "检查意图是否清晰"
      on_fail: ask_clarification
    steps:
      - /brainstorm/load_context
      - /brainstorm/init_state
      - /brainstorm/ask_intent
      - /brainstorm/analyze_intent

  - name: design_proposal
    steps:
      - /brainstorm/propose_approaches
      - /brainstorm/present_approaches
    gate:
      type: user_approval
      on_fail: /brainstorm/revise_design

  - name: design_creation
    steps:
      - /brainstorm/build_design
      - /brainstorm/save_design

  - name: transition
    steps:
      - /brainstorm/skill_transition
    end: true
```

---

## 通用阶段类型

### 1. 循环阶段（Loop Phase）

```markdown
- name: investigation
  loop: true
  loop_condition: "{{hypothesis.confirmed}} == false && attempts < max"
  max_iterations: 10
  steps:
    - /debug/refine_hypothesis
    - /debug/verify_hypothesis
    - /debug/judge_hypothesis
  gate:
    type: assert
    condition: "{{verdict.confirmed}} == true"
    on_fail: continue_loop
```

### 2. 并行阶段（Parallel Phase）

```markdown
- name: diagnostic
  parallel: true
  branches:
    - /debug/run_tool_1
    - /debug/run_tool_2
  merge:
    type: dsl_call
    llm_tool: gpt-4
    prompt: "合并诊断结果"
  steps:
    - /debug/merge_diagnostics
```

### 3. 用户交互阶段（User Interaction Phase）

```markdown
- name: clarification
  user_input:
    prompt: "{{clarity_check.question}}"
    variable: clarification
    type: text
  on_success: /brainstorm/update_intent
  on_skip: /brainstorm/skip_clarification
```

### 4. 门禁阶段（Gate Phase）

```markdown
- name: pre_merge_check
  gate:
    type: and
    checks:
      - type: file_exists
        path: "{{test_file}}"
      - type: build_passes
        command: "make build"
      - type: test_passes
        command: "make test"
    on_fail: /review/fix_issues
```

---

## 预定义流程模板

### 模板 1：调查-假设验证循环（Debugging）

```markdown
type: workflow_skill
template: hypothesis_validation_loop
config:
  max_iterations: 10
  on_max_iterations: abort
phases:
  - name: collect
    steps: [collect_symptoms, classify_problem]
  - name: investigate
    loop: true
    steps: [form_hypothesis, verify_hypothesis, judge_hypothesis]
  - name: fix
    steps: [implement_fix, verify_fix]
```

### 模板 2：TDD 红绿重构

```markdown
type: workflow_skill
template: tdd_cycle
config:
  phases:
    - name: red
      description: "Write failing test"
      steps: [write_test, verify_fails]
    - name: green
      description: "Write minimal implementation"
      steps: [write_impl, verify_passes]
    - name: refactor
      description: "Improve code without changing behavior"
      loop: true
      steps: [refactor_code, verify_tests]
```

### 模板 3：审查-修复循环

```markdown
type: workflow_skill
template: review_fix_loop
config:
  max_review_rounds: 3
phases:
  - name: review
    parallel: true
    branches: [review_1, review_2, review_3]
    merge: merge_reviews
  - name: fix
    steps: [apply_fixes, verify]
  - name: gate
    gate:
      type: all_reviews_passed
      on_fail: review_again
```

---

## 实现要求

### 1. 流程技能解析器

```cpp
class WorkflowSkillParser {
public:
    // 解析 workflow_skill 节点
    ParsedWorkflow parse(const YAML::Node& node);

    // 验证阶段转换
    void validate_transitions(const ParsedWorkflow& workflow);

    // 生成执行计划
    ExecutionPlan plan(const ParsedWorkflow& workflow);
};
```

### 2. 阶段状态机

```cpp
class PhaseStateMachine {
public:
    void enter_phase(const std::string& phase_name);
    bool check_gate(const Gate& gate);
    void execute_step(const std::string& step);
    bool should_loop();
    void advance();
};
```

### 3. 模板引擎

```cpp
class WorkflowTemplate {
public:
    // 加载内置模板
    static std::map<std::string, WorkflowTemplate> built_in_templates();

    // 应用模板到具体技能
    ParsedWorkflow apply(const std::string& template_name, const Config& config);
};
```

---

## 与 skill_invoke 的关系

```
workflow_skill 是技能定义
skill_invoke 是技能调用

skill_invoke + workflow_skill 模板 = 完整的技能执行
```

```markdown
## /use_brainstorming
type: skill_invoke
skill: "brainstorming"
# 内部使用 workflow_skill template: brainstorming_standard
```

---

## 优先级

**中** — 可以先通过 skill_invoke 组合实现，再演进到 workflow_skill。

---

## 验证方式

1. 将 3 个以上的流程类技能（brainstorming, systematic_debugging, test_driven_development）用 workflow_skill 重写
2. 验证行为一致
3. 验证模板复用性