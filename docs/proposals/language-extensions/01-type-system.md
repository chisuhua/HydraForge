# LS-001: 类型系统扩展提案

**ID**: LS-001
**日期**: 2026-05-20
**状态**: 已批准（轻量级方案，2026-05-22）
**关联**: ADR-007
**调研依据**: 需求评估（当前阶段类型系统价值有限，签名标注足够）

---

## 现状

当前 AgenticDSL 的类型模型：

```
所有数据 = nlohmann::json（运行时无类型）
节点签名 = 字符串（仅文档用途，不强制校验）
变量引用 = "{{variable}}" Inja 模板（运行时报错如果不存在）
```

## 问题

1. `{{user.name}}` 在 `user` 不包含 `name` 时运行时报错
2. `dsl_call` 的输出结构在 DSL 中无法静态验证
3. Agent 生成的 DSL 经常因字段名拼写错误而失败

**评估**：这些问题在**当前阶段**（阶段 1：DSL 控制参数）影响有限：
- 参数是简单值（string/int/float），拼写错误概率低
- 执行失败成本低（快速迭代）
- Agent 生成问题应通过 prompt 和示例解决，而非类型系统

## 提案：仅签名类型标注（文档用途）

### 核心设计

保留 `signature` 字段，但**仅用于文档生成和字段名提取**，不做类型校验。

```yaml
### AgenticDSL `/lib/inference/engine`
signature: "(device: string, gpu_layers: int, memory_limit_mb: int) -> (engine_id: string, status: string)"
```

### 设计原则

1. **零运行时影响** — 类型信息不改变执行逻辑
2. **纯文档用途** — 用于生成 API 文档、IDE 提示、字段名索引
3. **路径兼容** — 旧版运行时忽略类型信息
4. **未来扩展** — 为阶段 2 的静态校验预留空间

### 解析器行为

```yaml
# 当前
signature: "(device: string, gpu_layers: int) -> (engine_id: string, status: string)"

# 解析器仅做：
# 1. 提取字段名：inputs = [device, gpu_layers, memory_limit_mb]
#                  outputs = [engine_id, status]
# 2. 生成文档：参数列表、返回值列表
# 3. 不做类型检查、不做静态校验
```

### 不做的（当前阶段）

| 功能 | 决策 | 理由 |
|------|------|------|
| 静态类型校验 | ❌ 不做 | 阶段 1 不需要，增加解析器复杂度 |
| 结构化类型定义（`types:` 块） | ❌ 不做 | 过度设计，当前无复杂数据结构 |
| Sum type / 泛型 | ❌ 不做 | 阶段 2-3 再考虑 |
| 类型推断 | ❌ 不做 | 无标注时保持当前 JSON 行为 |
| 运行时类型检查 | ❌ 不做 | 用 `assert` 节点替代 |

## 与自举目标的关系

| 自举阶段 | 类型系统需求 | 当前决策 |
|---------|------------|---------|
| 阶段 0（硬编码参数） | 无 | ✅ 不适用 |
| 阶段 1（DSL 控制参数） | 低 — 简单值 | ✅ 签名标注足够 |
| 阶段 2（Agent 编排） | 中 — 验证子图结构 | ⏳ 未来扩展静态校验 |
| 阶段 3（持续自进化） | 高 — 自动生成代码 | ⏳ 未来引入完整类型系统 |

---

## 关联文档

| 文档 | 关系 |
|------|------|
| [02-module-namespace.md](02-module-namespace.md) | 类型系统的模块/命名空间上下文 — 类型在模块范围内定义 |
| [03-standard-library.md](03-standard-library.md) | 类型系统为标准库提供输入/输出字段校验 |
| [docs/specs/dsl.md](../../specs/dsl.md) | 当前 DSL v3.10 无类型规范 — 本文的渐进式采用确保向后兼容 |
| [docs/specs/architecture.md](../../specs/architecture.md) | 架构的五层定义 — 类型信息在 parser（L0）和运行时校验 |
