# LS-003: 标准库扩展清单

**ID**: LS-003
**日期**: 2026-05-20
**状态**: 已批准（Oracle 确认，2026-05-22）
**关联**: ADR-006
**调研依据**: Oracle 架构评估（bg_d2fa1c41）

---

## 现状

当前标准库（`lib/`）：

```
lib/
├── auth/verify_session.md
├── human/confirm_action.md
├── human/clarify_input.md
├── math/add.md
└── utils/noop.md
```

共 5 个文件，4 个目录，覆盖 4 个领域。

## 扩展方向

### 第 1 批：基础增强（短期）

| 目录 | 文件 | 说明 | 优先级 | Oracle 裁定 |
|------|------|------|--------|------------|
| `lib/flow/` | `if.md` | 条件分支包装 | P2（延后） | 做，但延后到推理标准库完成后。DSL 没有原生条件 next，if 子图可降低 LLM 出错率 |
| `lib/flow/` | `while.md` | 循环包装 | P2（延后） | 同上 |
| `lib/flow/` | `try_catch.md` | 错误处理包装 | P2（延后） | 低优先级。可用 assert + `{success, error, data}` 约定模拟 |
| `lib/flow/` | `pipeline.md` | 链式处理 | ❌ 不做 | 纯语法糖，必要性弱 |
| `lib/text/` | `join.md` | 字符串拼接 | P3 | 通用工具 |
| `lib/text/` | `split.md` | 字符串分割 | P3 | 通用工具 |
| `lib/text/` | `template.md` | 模板渲染 | P3 | 通用工具 |
| `lib/validation/` | `assert_type.md` | 类型断言 | P2 | 辅助类型系统 |
| `lib/validation/` | `assert_range.md` | 范围断言 | P2 | 辅助类型系统 |
| `lib/validation/` | `assert_schema.md` | 结构校验 | P1 | 推理标准库输入校验需要 |

### 第 2 批：推理标准库（自举相关）——分 3 阶段实施

| 阶段 | 文件 | 说明 | 技术依赖 | 时间 |
|------|------|------|---------|------|
| **2a（立即）** | `engine.md` | 推理引擎生命周期 | 纯 tool_call | 现在 |
| **2a（立即）** | `model.md` | 模型管理 | 纯 tool_call | 现在 |
| **2a（立即）** | `session.md` | 推理会话聚合 | dsl_call 聚合 | 现在 |
| **2b（1 周后）** | `prefix_cache.md` | Prefix-cache | json scope nesting | 1 周后 |
| **2b（1 周后）** | `kv_cache.md` | KV-cache 策略 | json scope nesting | 1 周后 |
| **2b（1 周后）** | `decoding.md` | 解码策略 | json scope nesting | 1 周后 |
| **2c（2 周后）** | `batching.md` | 动态 batching | queue 管理 | 2 周后 |
| ~~`orchestrate.md`~~ | — | 自适应编排 | ❌ 不做 | — |

### 第 3 批：网络与数据（中长期）

| 目录 | 文件 | 说明 |
|------|------|------|
| `lib/net/` | `http_client.md` | HTTP 请求 |
| `lib/net/` | `http_server.md` | HTTP 服务 |
| `lib/net/` | `websocket.md` | WebSocket |
| `lib/db/` | `query.md` | 数据库查询 |
| `lib/db/` | `transaction.md` | 事务管理 |
| `lib/data/` | `map.md` | Map 转换 |
| `lib/data/` | `filter.md` | Filter |
| `lib/data/` | `reduce.md` | Reduce |

## 标准库编写规范

每个标准库子图应遵循：

```yaml
### AgenticDSL `<path>`
# --- BEGIN AgenticDSL ---
graph_type: subgraph
signature: "(input1: type, input2: type) -> (output1: type, output2: type)"
permissions: []
nodes:
  - id: node_1
    type: tool_call
    tool: some_tool
    arguments:
      arg1: "{{inputs.var}}"
    output_keys: ["result"]
    next: ["/next_node"]
# --- END AgenticDSL ---
```

规范要求：

1. **签名声明** — 必须声明输入输出类型
2. **权限声明** — 如果调用需要权限的工具（如网络），在 permissions 中声明（见 docs/specs/dsl.md 7.2）
3. **错误处理** — 关键节点应该有 assert 或 on_failure 路径（assert 节点类型已支持 on_failure）
4. **单元测试** — 每个子图至少有一个测试用例
5. **注释** — 每个节点应该有简要中文注释说明

> **Oracle 确认**：`permissions` 和 `on_failure` 已在 DSL 规范（docs/specs/dsl.md）中定义，不需要新增语言规范。当前缺口是 C++ 运行时的实现完整度（`permissions` 只做工具存在性检查，`on_failure` 只在 AssertNode 中实现）。

## 标准库与运行时

标准库子图在运行时通过 `StandardLibraryLoader` 加载：

```cpp
// StandardLibraryLoader 当前逻辑（不变）
void load_from_directory(const std::string& lib_dir);

// 新目录被识别为新的标准库路径
// 不需要修改加载器代码，只需把 .md 文件放到正确的目录
```

## 实施优先级总览

```
P0（立即）: engine.md, model.md, session.md
P1（短期）: assert_schema.md
P2（中期）: if.md, while.md, try_catch.md, assert_type.md, assert_range.md
P3（长期）: join.md, split.md, template.md, http_client.md, ...
❌ 不做: pipeline.md, orchestrate.md
```

---

## 关联文档

| 文档 | 关系 |
|------|------|
| [01-type-system.md](01-type-system.md) | 标准库子图的 signature 类型校验 — 类型系统在标准库中的应用 |
| [02-module-namespace.md](02-module-namespace.md) | 标准库通过模块命名空间（standard::math::add）引用 |
| [docs/specs/stdlib-v3.10.md](../../specs/stdlib-v3.10.md) | 当前标准库规范（v3.10，合并自 dsl-lib + stdlib）— 本文的扩展基线 |
| [docs/archive/specs/phase2-standard-library-v1.0.md](../../archive/specs/phase2-standard-library-v1.0.md) | 已归档的 Phase 2 标准库规划（memory/context 相关），与本文互补（已退役） |
| [docs/adr/adr-0009-dsl-standard-library.md](../../adr/adr-0009-dsl-standard-library.md) | 标准库架构决策 — 目录结构、权限、版本策略 |
