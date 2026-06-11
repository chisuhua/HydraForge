# ADR-0016: 异常自动快照回溯
> ⛔ **已废弃 (2026-06-09)** — 代码侧 0 命中,仅作设计历史保留。详见 OpenSpec change `tech-debt-and-doc-cleanup`
## 状态

**❌ 未实施** (2026-05-13, 2026-06-09 标注废弃)

## 背景

HydraForge Phase 2 需要 try-catch-fallback 模式，提升 DSL 执行的可靠性：

- **自动快照**：在 try 执行前创建上下文快照
- **失败回溯**：try 失败时自动恢复上下文并执行 catch
- **错误处理**：提供详细的错误信息用于调试

---

## 决策

### 1. 子图定义

#### `/lib/reasoning/try_catch@v1`

```yaml
AgenticDSL `/lib/reasoning/try_catch@v1`
signature:
  inputs:
    - name: agent_id
      type: string
      required: true
    - name: user_id
      type: string
      required: true
    - name: try_block
      type: string
      required: true
      description: "可能失败的子图路径"
    - name: catch_block
      type: string
      required: true
      description: "失败时执行的 fallback 子图路径"
    - name: snapshot_on_try
      type: boolean
      default: true
      description: "是否在 try 前创建上下文快照"
  outputs:
    - name: success
      type: boolean
    - name: error
      type: string
      required: false
      description: "try_block 失败时的错误信息"
    - name: executed_path
      type: string
      enum: [try, catch, none]
version: "1.0"
stability: stable
permissions:
  - memory: state_read
  - memory: state_write
```

### 2. 执行逻辑

```
1. 若 snapshot_on_try=true，在入口触发上下文快照
2. 执行 try_block
3. 若成功 → 返回 (success: true, executed_path: try)
4. 若失败：
   a. 恢复上下文快照（回到 try 执行前的状态）
   b. 执行 catch_block
   c. 返回 (success: true/false, executed_path: catch, error: ...)
```

### 3. Trace 输出

```json
{
  "try_catch": {
    "executed_path": "try | catch",
    "success": true,
    "error": "...",
    "snapshot_created": true
  }
}
```

### 4. 与上下文合并策略的关系

- `snapshot` 操作依赖于 LayeredContext 的 `working.data`
- 回溯时恢复 `working.data` 到快照点
- 不影响 L1 System（永不回溯）和 L2/L3（由 ContextCompressor 管理）

---

## 参考

- [ADR-0008: 结构化 Context](./adr-0008-structured-context.md)
- [ADR-0010: 记忆系统标准接口](./archive/adr-0010-memory-system.md)