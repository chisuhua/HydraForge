# LS-002: 模块与命名空间提案

**ID**: LS-002
**日期**: 2026-05-20
**状态**: 已批准（轻量级方案，2026-05-22）
**关联**: ADR-007
**调研依据**: 需求评估（当前 8 个标准库文件，单项目单版本场景）

---

## 现状

当前 AgenticDSL 的模块组织：

```
图路径: "/main/step1", "/lib/inference/kv_cache"
引用方式: subgraph: "/lib/math/add"
标准库: lib/ 目录下 .md 文件
```

当前能力：
- ✅ 路径唯一性（文件系统天然约束）
- ✅ 层级表达（`lib/inference/` vs `lib/math/`）
- ❌ 无显式模块声明（仅靠路径推断）

**需求评估**：当前 8 个标准库文件（5 现有 + 3 新建），单项目单版本场景。完整包系统（package/version/import/exports）过度设计。

## 提案：轻量级命名空间

### 核心设计

仅引入 `module` 字段作为**文档和静态分析**用途，不影响运行时行为。

```yaml
## /__meta__
module: "inference::kv_cache"
```

### 设计原则

1. **零运行时影响** — `module` 字段不改变路径解析、不改变执行逻辑
2. **纯元数据** — 用于文档生成、IDE 提示、静态分析
3. **路径兼容** — 现有 `/lib/inference/kv_cache` 路径完全保留
4. **未来扩展** — 为完整包系统预留空间（后续可增加 package/version/exports）

### 使用示例

```yaml
### AgenticDSL `/lib/inference/engine`
## /__meta__
module: "inference::engine"

## /init
  type: tool_call
  tool: inference.engine_init
  ...
```

### 与路径的关系

```
路径（运行时）: /lib/inference/engine
模块（文档）:    inference::engine

两者共存，路径仍然是唯一标识符
```

### 不做的（当前阶段）

| 功能 | 决策 | 理由 |
|------|------|------|
| `package` 包声明 | ❌ 不做 | 单项目无多包需求 |
| `exports` 可见性控制 | ❌ 不做 | 当前无隐私需求，命名约定 `_private` 可临时解决 |
| `import` 导入声明 | ❌ 不做 | 路径引用足够显式 |
| `version` 版本化 | ❌ 不做 | 单版本场景，文件名后缀可临时解决 |
| 路径解析规则变更 | ❌ 不做 | 继续使用现有路径机制 |

## 与 StandardLibraryLoader 的关系

当前 `StandardLibraryLoader` 按文件路径加载。**不改变**。`module` 字段仅作为元数据附加到 ParsedGraph。

```
当前: /lib/math/add  → 路径前缀匹配（继续）
未来: module 字段 → 仅用于文档和静态分析

无迁移成本，无兼容期问题
```

## 实施方向

1. 在 ParsedGraph 中新增 `module` 字符串字段（可选）
2. parser 在加载 `/__meta__` 时解析 `module` 字段
3. 不影响路径解析、不影响执行逻辑
4. 文档生成器可使用 module 字段生成索引

## 何时升级到完整包系统

触发条件（满足任一）：
- 标准库文件数量 > 15 个
- 需要多版本共存（如 kv_cache@v1 和 kv_cache@v2）
- 引入第三方包（非项目内置的标准库）
- 需要可见性控制（隐藏内部节点）

当前不满足任何条件，保持轻量级。

---

## 关联文档

| 文档 | 关系 |
|------|------|
| [01-type-system.md](01-type-system.md) | 模块内类型定义 — 与模块/命名空间组合使用 |
| [03-standard-library.md](03-standard-library.md) | 标准库通过模块命名空间管理版本和可见性 |
| [docs/specs/dsl.md](../../specs/dsl.md) | 当前路径引用约定（`/lib/reasoning/react`），本文为 v3.10 增加命名空间层次 |
| [docs/specs/stdlib-v3.10.md](../../specs/stdlib-v3.10.md) | 当前标准库目录结构（v3.10），模块命名空间与 lib/ 路径相互映射 |
| [CPP-001: standard:: 命名空间](../../../specs/stdlib-v3.10.md) | 标准库 namespace 设计，模块命名空间的参考 |
