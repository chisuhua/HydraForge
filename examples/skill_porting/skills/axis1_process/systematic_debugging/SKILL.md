# Skill: systematic_debugging

**分类**: 轴1-流程/方法论
**触发词**: "error", "crash", "broken", "bug", "调试", "修复"

## When to Use

遇到任何 bug、测试失败或异常行为时，**必须**在提出修复方案之前使用此技能。

## What It Does

提供结构化的调试方法论：
1. **复现** — 稳定复现问题
2. **定位** — 缩小范围，定位根因
3. **假设** — 形成可验证假设
4. **验证** — 验证假设，排除其他可能
5. **修复** — 应用最小化修复

## How It Works

```
问题报告
    ↓
复现阶段（收集环境、症状、时间线）
    ↓
归类阶段（崩溃/逻辑错误/性能/内存/并发...）
    ↓
定位阶段（排除法、二分法、trace分析）
    ↓
假设形成
    ↓
验证实验
    ↓
修复 + 回归测试
```

## Core Principles

- **先诊断，后修复** — 不猜测，不 shotgun debug
- **最小化修复** — 只改问题本身，不重构周围代码
- **证据驱动** — 每个结论必须有证据支持
- **可重现** — 修复必须能稳定重现通过

## Anti-Patterns (Blocked)

- ❌ 删除失败的测试来"通过"
- ❌ shotgun debugging（随机改动期望碰巧解决）
- ❌ 压制类型错误 (`as any`, `@ts-ignore`)
- ❌ 空 catch 块 (`catch(e) {}`)

## AgenticDSL Example

**对应文件**: `../../agenticdsl/axis1_process/systematic_debugging.agent.md`

该文件展示了如何用 AgenticDSL 实现结构化调试工作流，包含：
- `state` — 维护调试上下文
- `dsl_call` — LLM 分析症状和定位方向
- `tool_call` — 执行诊断命令（gdb/valgrind/trace）
- `assert` — 验证假设是否成立

## Ideal DSL Extension

**参考**: `../../ideal_dsl/03_workflow_skill.md`

流程类技能的理想 DSL 扩展提案：
- `type: skill_invoke` — 调用 debugging 技能
- `hypothesis` 节点类型 — 结构化假设验证
- `diagnostic_tool` 节点类型 — 内置调试工具集成
