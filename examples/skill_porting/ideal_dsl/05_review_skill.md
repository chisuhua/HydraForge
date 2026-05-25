# Ideal DSL Extension 05: review_skill

**文件**: `05_review_skill.md`
**状态**: 提案
**解决的问题**: 审查/质量类技能（轴3）的 DSL 模式标准化

---

## 动机

轴3（审查/质量）技能的核心是**门禁**模式：

```
输入 → 审查维度1 → 审查维度2 → ... → 审查维度N → 通过/不通过
                 ↓               ↓
              [fork]          [join]
```

每个审查维度可能并行（review_work 的 5 路审查），最后汇聚判断。

关键特征：
1. **并行审查** — 多个维度同时审查
2. **结果汇聚** — 合并多路审查结果
3. **门禁判断** — 必须全部通过（或按阈值）
4. **反馈循环** — 不通过时返回修改

---

## 现有审查类技能的共同结构

### review_work
```
start → load_context → collect_artifacts → init_review_state
    ↓
fork（5路并行）:
  - review_goal_constraint (Oracle)
  - review_code_quality (Oracle)
  - review_security (Oracle)
  - review_qa_execution (unspecified-high)
  - review_context_mining (unspecified-high)
    ↓
join_reviews → judge_reviews
    ↓
(if not all passed) collect_feedback → present_feedback
    ↓
(revision) → fork again
    ↓
(if all passed) generate_report → end
```

### receiving_code_review
```
start → load_feedback → parse_feedback
    ↓
fork（每条反馈验证）:
  - validate_item_1
  - validate_item_2
  - validate_item_3
    ↓
merge_validations → classify_actions
    ↓
route_to_actions（每条反馈的处理方式）
    ↓
(agree_and_fix) → apply_fix
(disagree) → discuss_disagreement
(out_of_scope) → explain_scope
    ↓
generate_response_message → finalize → end
```

### verification_before_completion
```
start → collect_artifacts → check_claims
    ↓
fork（验证每个声明）:
  - verify_build
  - verify_tests
  - verify_coverage
  - verify_diagnostics
    ↓
merge_verification → judge_verdict
    ↓
(if failed) report_gaps
(if passed) present_success → end
```

### 共同模式识别

1. **并行审查** — fork 分支，每个分支一个审查维度
2. **结果汇聚** — 所有分支结果汇聚判断
3. **门禁** — 判断是否全部通过
4. **反馈收集** — 不通过时收集反馈
5. **循环** — 可以返回修改后重新审查

---

## 提案：审查类技能标准 DSL 模式

### 核心语法

```markdown
## /review/start
type: review_skill
skill: "implementation_review"

# 审查维度定义
dimensions:
  - name: goal_constraint
    type: oracle
    prompt: "验证实现满足原始需求..."
    weight: 1.0

  - name: code_quality
    type: oracle
    prompt: "验证代码质量..."
    weight: 1.0

  - name: security
    type: oracle
    prompt: "验证安全性..."
    weight: 1.0

  - name: qa_execution
    type: agent
    task: run_tests
    weight: 1.0

  - name: context_mining
    type: agent
    task: check_related_docs
    weight: 0.5

# 汇聚配置
aggregation:
  mode: all_pass|majority|weighted_average
  pass_threshold: 1.0  # all_pass 模式

# 反馈配置
feedback:
  on_fail:
    collect: true
    present: true
    allow_revision: true
    max_rounds: 3

# 输出
output:
  verdict: passed|failed|needs_revision
  report: review_report
  issues: issue_list
```

### 语义

| 字段 | 含义 |
|------|------|
| `dimensions` | 审查维度列表 |
| `dimensions[].type` | 审查类型（oracle/agent/tool） |
| `dimensions[].weight` | 权重（用于加权平均汇聚） |
| `aggregation.mode` | 汇聚模式 |
| `feedback` | 不通过时的反馈配置 |

---

## 审查门禁节点

### 语法

```markdown
## /review/gate
type: review_gate
name: implementation_review
dimensions:
  - /review/dim_goal
  - /review/dim_quality
  - /review/dim_security
  - /review/dim_qa
  - /review/dim_context
aggregation:
  mode: all_pass
  pass_threshold: 1.0
on_pass:
  next: /merge/approve
on_fail:
  next: /review/collect_feedback
  output: failed_dimensions
```

### 执行流程

```
review_gate
    ↓
并行执行所有 dimensions
    ↓
等待所有分支完成
    ↓
汇聚结果（mode: all_pass）
    ↓
判断是否通过
    ↓
路由到 on_pass 或 on_fail
```

---

## 审查结果汇聚

### 1. all_pass 模式

```markdown
aggregation:
  mode: all_pass
  # 所有维度都必须通过
```

```cpp
bool all_pass(const std::vector<DimensionResult>& results) {
    return std::all_of(results.begin(), results.end(),
                       [](const auto& r) { return r.passed; });
}
```

### 2. majority 模式

```markdown
aggregation:
  mode: majority
  threshold: 0.6
  # 60% 以上通过即可
```

### 3. weighted_average 模式

```markdown
aggregation:
  mode: weighted_average
  pass_threshold: 0.8
  # 加权平均 >= 0.8 通过
```

```cpp
double weighted_average(const std::vector<DimensionResult>& results) {
    double total_weight = 0;
    double weighted_sum = 0;
    for (auto& r : results) {
        weighted_sum += r.score * r.weight;
        total_weight += r.weight;
    }
    return weighted_sum / total_weight;
}
```

---

## 反馈收集节点

### 语法

```markdown
## /review/collect_feedback
type: feedback_collector
dimensions: "{{failed_dimensions}}"
merge:
  type: dsl_call
  llm_tool: gpt-4
  prompt: "合并 {{count}} 个审查维度的反馈为统一报告"
output:
  feedback_report: feedback_doc
  action_plan: action_items
on_feedback_complete:
  present_to_user: true
  allow_revision: true
```

---

## 实现要求

### 1. 审查技能解析器

```cpp
class ReviewSkillParser {
public:
    ParsedReviewSkill parse(const YAML::Node& node);

    // 验证维度覆盖
    void validate_dimensions(const ParsedReviewSkill& skill);

    // 生成并行执行计划
    std::vector<std::vector<ReviewTask>> plan_parallel(
        const ParsedReviewSkill& skill);
};
```

### 2. 审查维度执行器

```cpp
class ReviewDimensionExecutor {
public:
    // 执行单个审查维度
    DimensionResult execute(const ReviewDimension& dim,
                            const Context& ctx);

    // Oracle 维度（LLM 审查）
    OracleResult execute_oracle(const OracleDimension& dim,
                               const Context& ctx);

    // Agent 维度（子 agent 审查）
    AgentResult execute_agent(const AgentDimension& dim,
                              const Context& ctx);

    // Tool 维度（工具执行审查）
    ToolResult execute_tool(const ToolDimension& dim,
                           const Context& ctx);
};
```

### 3. 结果汇聚器

```cpp
class ReviewAggregator {
public:
    // 汇聚多维度结果
    AggregationResult aggregate(const std::vector<DimensionResult>& results,
                                const AggregationConfig& config);

    // 判断是否通过
    Verdict judge(const AggregationResult& result);

    // 生成审查报告
    std::string generate_report(const AggregationResult& result);
};
```

---

## 使用示例

### 示例 1：review_work 用 review_skill 重写

```markdown
## /review/start
type: review_skill
skill: "implementation_review"

dimensions:
  - name: goal_constraint
    type: oracle
    prompt: "验证实现满足原始需求和约束"
    weight: 1.0

  - name: code_quality
    type: oracle
    prompt: "验证代码可读性、设计模式、错误处理"
    weight: 1.0

  - name: security
    type: oracle
    prompt: "验证输入验证、权限控制、敏感信息处理"
    weight: 1.0

  - name: qa_execution
    type: tool
    tool: bash
    command: "make test"
    timeout: 120

  - name: context_mining
    type: oracle
    prompt: "从 git/issue/PR 挖掘相关上下文"
    weight: 0.8

aggregation:
  mode: all_pass

feedback:
  on_fail:
    collect: true
    present: true
    allow_revision: true
    max_rounds: 3

output:
  verdict: passed|failed|needs_revision
  report: review_report
  issues: "{{aggregation.issues}}"
```

### 示例 2：receiving_code_review 用 review_skill 重写

```markdown
## /rcr/start
type: review_skill
skill: "code_review_response"

dimensions:
  - name: feedback_validation
    type: oracle
    prompt: "验证每条反馈的技术正确性"
    for_each: "{{feedback_items}}"

  - name: scope_analysis
    type: oracle
    prompt: "分析反馈是否超出 PR scope"
    for_each: "{{feedback_items}}"

  - name: impact_assessment
    type: oracle
    prompt: "评估每条反馈的实施影响"
    for_each: "{{feedback_items}}"

aggregation:
  mode: weighted_average
  weights:
    feedback_validation: 0.4
    scope_analysis: 0.3
    impact_assessment: 0.3
  pass_threshold: 0.7

output:
  action_plan: "{{aggregation.action_plan}}"
  response_message: "{{aggregation.response_message}}"
```

---

## 与 skill_invoke 的关系

```
skill_invoke  = 调用技能
review_skill  = 技能内部使用审查模式的实现

skill_invoke 包装 review_skill 提供统一调用接口
```

---

## 预定义审查模板

### 模板 1：Code Review

```markdown
type: review_skill
template: code_review
dimensions:
  - correctness
  - style
  - performance
  - security
  - test_coverage
aggregation:
  mode: weighted_average
  weights: [0.3, 0.2, 0.2, 0.2, 0.1]
```

### 模板 2：Security Audit

```markdown
type: review_skill
template: security_audit
dimensions:
  - input_validation
  - authentication
  - authorization
  - data_protection
  - vulnerability_scan
aggregation:
  mode: all_pass  # 安全问题必须全部解决
```

### 模板 3：Performance Review

```markdown
type: review_skill
template: performance_review
dimensions:
  - cpu_usage
  - memory_usage
  - latency
  - throughput
  - scalability
aggregation:
  mode: majority
  thresholds:
    cpu_usage: 80  # 百分比
    memory_usage: 90
    latency: 100   # ms
```

---

## 优先级

**中** — 审查类技能有明确的模式，但可以先通过 skill_invoke + fork/join 实现。

---

## 验证方式

1. 将 review_work 和 receiving_code_review 用 review_skill 重写
2. 验证并行审查正确性
3. 验证汇聚判断正确性
4. 验证反馈收集流程