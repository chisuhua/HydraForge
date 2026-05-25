# Skill: cpp_debug

**分类**: 轴2-领域/工具
**触发词**: "crash", "segfault", "SIGSEGV", "deadlock", "memory leak", "段错误", "内存泄漏"

## When to Use

在以下 C++ 运行时问题中激活：
- 程序崩溃（段错误、总线错误、SIGSEGV）
- 死锁或活锁
- 内存泄漏、越界访问
- 悬挂指针、双重释放
- 未定义行为

## What It Does

提供 C++ 运行时故障排查的专业知识：
- 选择合适的诊断工具（gdb/valgrind/asan）
- 分析 core dump
- 解读内存错误报告
- 定位竞态条件

## Core Tools

| 问题类型 | 首选工具 | 备选工具 |
|---------|---------|---------|
| 崩溃/段错误 | gdb + core dump | address sanitizer |
| 内存泄漏 | valgrind --leak-check | asan |
| 越界访问 | valgrind | asan |
| 死锁 | gdb (thread apply all bt) | strace |
| 性能 | perf | gprof |
| 竞态 | helgrind | tsan |

## Common Patterns

### 分析段错误
```bash
# 1. 检查 core 文件
ulimit -c unlimited
gdb ./program core

# 2. 在 gdb 中
thread apply all bt
frame <n>
print variable_name
```

### 内存泄漏检测
```bash
valgrind --leak-check=full --show-leak-kinds=all ./program
```

## AgenticDSL Example

**对应文件**: `../../agenticdsl/axis2_domain/cpp_debug.agent.md`

该文件展示了如何用 AgenticDSL 实现 cpp_debug 技能，包含：
- `tool_call` — 执行 gdb/valgrind 命令
- `dsl_call` — 分析诊断输出
- `state` — 维护调试上下文
- `fork` — 并行运行多个诊断工具

## Ideal DSL Extension

**参考**: `../../ideal_dsl/04_domain_skill.md`

领域/工具类技能的理想 DSL 扩展提案：
- `type: debugger_session` — 托管 gdb 会话
- `type: memory_analysis` — 内存错误分析节点
- `type: stack_trace` — 结构化堆栈跟踪解析
