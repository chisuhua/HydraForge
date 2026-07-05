# SPEC-COMPILED: 编译技能产物格式 v1

**状态**: 已决 (2026-05-25)
**关联**: SPEC-SSL-IR, SPEC-BOOTSTRAP, ADR-0019

---

## 1. 概述

编译产物是编译器输出的 AgenticDSL 计算图，是标准 `.agent.md` 文件。可被 `MarkdownParser` 直接解析，被 `TopoScheduler` 直接执行。

## 2. 产物结构

```
/__meta__              — 版本、入口、预算、资源声明
/__meta__/resources    — 工具权限声明
/main/start → /main/init
/main/route            — dsl_call 路由（仅暴露 name+description+tags）
/main/check_match      — assert
/main/load_body        — tool_call: fs.read 读取 SKILL.md
/main/parse_body       — assign: 提取 instructions + tools
/main/phase{N}_{name}  — assign: phase_index + current_phase_prompt
/main/execute_phase{N} — dsl_call: 执行
/main/check_refs{N}    — regex.extract [LOAD_REF]
/main/load_ref_{N}_{M} — tool_call: fs.read
/main/join_refs{N}     — join: 合并 refs
/main/finalize → /main/end
/main/fallback         — dsl_call: 通用回答
```

## 3. 关键节点规范

### 3.1 `/__meta__`

```yaml
### AgenticDSL `/__meta__`
version: "3.10"
mode: dev
entry_point: "/main/start"
execution_budget:
  max_nodes: 30
  max_subgraph_depth: 3
```

### 3.2 入口序列

```yaml
### AgenticDSL `/main/start`
type: start
next: "/main/init"

### AgenticDSL `/main/init`
type: assign
assign:
  working.data.user_input: "{{ input.user_query }}"
  working.data.skill_registry: "{{ system.skill_registry }}"
  working.data.phase_index: 0
  working.data.loaded_refs: {}
next: "/main/route"
```

### 3.3 路由与加载

```yaml
### AgenticDSL `/main/route`
type: dsl_call
llm_tool_name: "{{ target_model }}"
llm_params:
  temperature: 0.1
  max_tokens: 128
prompt_template: |
  用户输入：{{ working.data.user_input }}
  可用 Skill：
  {% for skill in working.data.skill_registry.skills %}
  - {{ skill.name }}: {{ skill.description }}
  {% endfor %}
  返回最匹配的 Skill name，无匹配返回 NONE。
output_keys: ["working.data.matched_skill_name"]
next: "/main/check_match"
```

### 3.4 Phase 分段

由 SSL `structural.stages` 映射生成。每个 stage 产生一个 assign + dsl_call：

```yaml
### AgenticDSL `/main/phase{N}_{name}`
type: assign
assign:
  working.data.phase_index: N
  working.data.current_phase_prompt: |
    【阶段{N}：{stage.type | type_label}】
    {stage.instruction}
    可用工具：{stage.tools | join(', ')}
next: "/main/execute_phase{N}"

### AgenticDSL `/main/execute_phase{N}`
type: dsl_call
llm_tool_name: "{{ target_model }}"
prompt_template: |
  {{ working.data.current_phase_prompt }}
  用户请求：{{ working.data.user_input }}
  上一阶段输出：{{ working.data.phase_output | default('（无）') }}
output_keys: ["working.data.phase_output", "working.data.ref_requests"]
next: "/main/check_refs{N}"
```

### 3.5 Refs 子图（needs_refs=true 时）

```yaml
### AgenticDSL `/main/load_refs_fork_{N}`
type: fork
fork:
  branches:
    {% for ref in stage.refs %}
    - "/main/load_ref_{N}_{{ loop.index }}"
    {% endfor %}
join:
  wait_for: all
  merge_strategy: deep_merge
next: "/main/phase{N}_{name}"
```

### 3.6 收尾

```yaml
### AgenticDSL `/main/finalize`
type: assign
assign:
  output.result: "{{ working.data.phase_output }}"
next: "/main/end"

### AgenticDSL `/main/end`
type: end
termination_mode: soft
```

## 4. JIT 产物 vs AOT 产物

| 维度 | JIT | AOT |
|------|-----|-----|
| 路径 | `/dynamic/compiled/{name}` | `/lib/skills/compiled/{name}.agent.md` |
| 生命周期 | 一次执行 | 持久化 |
| 注册 | 不注册 | registry priority=50/100 |

## 5. 验证规则

- 必须有 `/__meta__` 和 `entry_point`
- 所有 `next` 指针有效
- `fork` 有对应 `join`
- 路径唯一
- 命名空间合规（不写 `/lib/`）
