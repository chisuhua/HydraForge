# ADR-0015: IPER 闭环推理
> 📋 **Phase 3 规划: 推理能力** (规划于 2026-05/06, 2026-06-09 整理归档) — 见 `implementation-roadmap.md`
## 状态

**❌ 未实施** (2026-05-13, 2026-06-09 标注废弃)

## 背景

HydraForge Phase 2 需要"意图-计划-执行-反思"（IPER）闭环推理机制，用于鲁棒的任务执行：

- **Intent**：理解用户意图
- **Plan**：将意图分解为可执行计划
- **Execute**：执行计划
- **Reflect**：反思执行结果，失败时重试

**优势**：
- 复杂任务自动分解 + 执行
- 执行失败时自动重试 + 反思
- 最终成功返回结果 或 失败返回归因报告

---

## 决策

### 1. 子图定义

#### `/lib/reasoning/iper_loop@v1`

```yaml
AgenticDSL `/lib/reasoning/iper_loop@v1`
signature:
  inputs:
    - name: agent_id
      type: string
      required: true
    - name: user_id
      type: string
      required: true
    - name: user_intent
      type: string
      required: true
      description: "原始用户请求或任务目标"
    - name: planner_path
      type: string
      required: true
      description: "生成执行计划的子图路径（如 /lib/dslgraph/generate@v1）"
    - name: max_reflections
      type: integer
      default: 3
      minimum: 1
      maximum: 5
      description: "最大反思/重试次数"
  outputs:
    - name: final_result
      type: object
      required: true
      description: "最终成功结果或归因报告"
    - name: status
      type: string
      enum: [success, failed, max_iterations]
    - name: reflection_count
      type: integer
    - name: last_error
      type: string
      required: false
version: "1.0"
stability: stable
permissions:
  - generate_subgraph: { max_depth: 2 }
```

### 2. 执行逻辑

```
1. 接收 user_intent
2. 调用 planner_path 生成 /dynamic/plan_v1
3. 执行该计划
4. 若成功 → 返回 final_result (status: success)
5. 若失败 → 进入反思：
   a. 调用 planner_path 注入错误上下文生成修复计划
   b. reflection_count++
   c. 若 reflection_count >= max_reflections → 返回归因 (status: max_iterations)
   d. 否则重复步骤 3-5
6. 失败 → 返回归因报告 (status: failed)
```

> **实现说明**：IPER 循环在 ADR-0030 中由 async_simple `Lazy<T>` 协程状态机实现，
> 支持 `co_await` 挂起等待（如用户审批）和 `co_yield` 流式推送（如计划生成过程）。
> 参见 [ADR-0030 第 3 节](../archive/adr/adr-0030-async-runtime-dual-layer.md) IPER 循环示例。
>
> **当前实现对应** (2026-08-01): Plan → Execute → Verify 三阶段循环
> (`include/agenticdsl/pdk/agent_loops/plan_execute_loop.h`, Sprint 20, ADR-0021 §3.2)
> 实现了 IPER 中 Plan→Execute→Reflect 的核心逻辑，以简化三状态机
> (Planning→Executing→Verifying→Done/Retry) 替代原始五阶段 (Intent→Plan→Execute→Reflect→Retry)。
> 关键差异: 当前实现省略了独立的 "Intent" 阶段（意图隐含在 goal prompt 中），
> Verify 阶段等价于原 "Reflect" 阶段。重试机制保持（max_retries=3），但无 reflection_count 累积。详见
> [`include/agenticdsl/pdk/agent_loops/plan_execute_loop.h`](../../../include/agenticdsl/pdk/agent_loops/plan_execute_loop.h)。

### 3. Trace 输出

```json
{
  "iper_loop": {
    "status": "success | failed | max_iterations",
    "reflection_count": 2,
    "final_result": { ... },
    "last_error": "...",
    "plans_generated": ["/dynamic/plan_1", "/dynamic/repair_1"]
  }
}
```

### 4. 与 generate_subgraph 的关系

- IPER 依赖 `generate_subgraph` 生成计划子图
- 生成的子图权限受 `permissions.generate_subgraph.max_depth` 限制
- 计划子图写入 `/dynamic/**` 命名空间

---

## 参考

- [ADR-0009: DSL 标准库规划](./adr-0009-dsl-standard-library.md)
- [ADR-0030: AsyncRuntime 双层异步架构](../archive/adr/adr-0030-async-runtime-dual-layer.md) — IPER 循环的协程状态机实现（`Lazy<T>`）