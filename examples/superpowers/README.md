# Superpowers Skills — AgenticDSL 重写

> 本目录使用**完整增强路线图实现后**的 AgenticDSL 重写所有 Superpowers 技能。
> 每个 `.agent.md` 文件是一个可执行的 DSL 工作流，对应一个 Superpowers 技能。

## 用途

1. **示例**：展示 AgenticDSL 的能力边界
2. **测试**：实现增强路线图后，运行这些文件验证功能完整性
3. **对标**：确认 AgenticDSL 是否达到 Superpowers 同等能力

## 技能列表

| # | 文件 | 对应 Superpowers 技能 | 关键 DSL 特性 |
|---|------|----------------------|-------------|
| 1 | `03_executing_plans.agent.md` | executing-plans | assert, tool_call, generate_subgraph |
| 2 | `04_subagent_driven_development.agent.md` | subagent-driven-development | **fork/join**, generate_subgraph |
| 3 | `05_dispatching_parallel_agents.agent.md` | dispatching-parallel-agents | **fork/join**, state |
| 4 | `06_finishing_development_branch.agent.md` | finishing-a-development-branch | tool_call, user_input, state |
| 5 | `07_systematic_debugging.agent.md` | systematic-debugging | state, dsl_call, tool_call |
| 6 | `08_verification_before_completion.agent.md` | verification-before-completion | assert, tool_call |
| 7 | `09_using_git_worktrees.agent.md` | using-git-worktrees | tool_call, state, assert |
| 8 | `10_test_driven_development.agent.md` | test-driven-development | assert, tool_call, loop |
| 9 | `11_writing_skills.agent.md` | writing-skills | state, generate_subgraph, user_input |
| 10 | `12_receiving_code_review.agent.md` | receiving-code-review | dsl_call, user_input, state |
| 11 | `13_requesting_code_review.agent.md` | requesting-code-review | fork/join, dsl_call |
| 12 | `14_using_superpowers.agent.md` | using-superpowers | **generate_subgraph**, state |

## 所需 AgenticDSL 增强特性

```yaml
所需特性:
  fork_join:            ✅ C++20 协程实现
  user_input:           ✅ 交互节点
  state_store:          ✅ 持久化状态
  llm_streaming:        ✅ 流式输出
  tool_extensions:      ✅ 文件/命令/Git/Web 工具
  standard_library:     ✅ 20+ 子图
  context_isolation:    ✅ Fork 分支隔离
```

## 运行方式

```bash
# 编译 AgenticDSL（含增强特性）
mkdir build && cd build && cmake .. -DAGENTICDSL_BUILD_EXAMPLES=ON && make

# 运行技能示例（任选一个保留的示例）
./build/examples/superpowers/run_skill examples/superpowers/03_executing_plans.agent.md

# 运行全部示例作为集成测试
ctest --test-dir build --output-on-failure
```

## 技能完成度矩阵

| 技能 | 顺序 | 并行 | LLM | 工具 | 交互 | 状态 | 动态 |
|------|------|------|-----|------|------|------|------|
| 03_executing_plans | ✅ | ❌ | ❌ | ✅ | ❌ | ❌ | ✅ |
| 04_subagent_driven_dev | ✅ | ✅ | ✅ | ✅ | ❌ | ✅ | ✅ |
| 05_dispatching_parallel | ✅ | ✅ | ❌ | ✅ | ❌ | ✅ | ❌ |
| 06_finishing_branch | ✅ | ❌ | ❌ | ✅ | ✅ | ✅ | ❌ |
| 07_systematic_debugging | ✅ | ❌ | ✅ | ✅ | ❌ | ✅ | ❌ |
| 08_verification | ✅ | ❌ | ❌ | ✅ | ❌ | ❌ | ❌ |
| 09_git_worktrees | ✅ | ❌ | ❌ | ✅ | ✅ | ✅ | ❌ |
| 10_tdd | ✅ | ❌ | ✅ | ✅ | ❌ | ❌ | ❌ |
| 11_writing_skills | ✅ | ❌ | ✅ | ✅ | ✅ | ✅ | ✅ |
| 12_receiving_review | ✅ | ❌ | ✅ | ✅ | ✅ | ✅ | ❌ |
| 13_requesting_review | ✅ | ✅ | ✅ | ✅ | ❌ | ✅ | ❌ |
| 14_using_superpowers | ✅ | ❌ | ✅ | ❌ | ❌ | ✅ | ✅ |

> **标记说明：** ✅ = 完整实现  ⚠️ = 部分实现  ❌ = 不需要此能力
