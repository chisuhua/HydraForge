# std::expected Monadic Ops 链式重构

**Change ID**: `2026-10-09-std-expected-monadic-ops`
**Status**: 🟡 **Partial-Shipped** (2026-10-10, scope 缩减: 10 处 → 1 refactor + 1 rename)
**决议**: **不 archive** (per AGENTS.md L1073 + tasks.md Phase 5 决议, 实际 ship 范围与 OpenSpec claim 显著不同, archive 会误导; 保留为未来 monadic ops 扩展载体)
**优先级**: 中（纯 refactor，零行为变化）
**依赖**: 无（std::expected 已 ship in `2026-10-08-unify-result-to-std-expected`）
**关联 commit**: `2451498` (unify result ship, 124 调用点) + `9709317` (follow-up mentioned in AGENTS.md) + `ccd819f` (cost_tracking_decorator refactor) + `7b2d5d0` (SnapshotResult rename)

## Why

`2026-10-08-unify-result-to-std-expected` ship 时预留的非目标（per AGENTS.md:970）：
> "monadic ops (`.and_then/.or_else/.transform`) 留独立 follow-up change"

当前 std::expected 在 140 处使用，但绝大多数仍是 `if (res.has_value()) { ... res.value() ... } else { ... res.error() ... }` 三段式写法。这些写法：

1. **重复样板**：每处都写 has_value() + value() 双重检查，~10 个核心调用点共 ~80 行样板
2. **易遗漏错误处理**：单步 `res.value()` 不带 `if` 守卫时会抛 `std::bad_expected_access`，grep audit 已识别 12 处潜在风险点
3. **链式意图不显式**：error propagation 需要 `if (!x.has_value()) return error;` 显式串联，lambda 链式版本一行表达
4. **C++23 完整化**：std::expected 的 monadic ops 是 C++23 才标准化（libstdc++ 14.2 已支持），C++20 切换前无法应用

## What Changes

应用 std::expected 的 3 个 monadic ops：

| Monadic op | 签名 | 用途 |
|-----------|------|------|
| `.transform(fn)` | `T -> U`，错误透传 | 成功值转换（纯函数式） |
| `.and_then(fn)` | `T -> expected<U, E>`，错误透传 | 链式返回 expected 的操作 |
| `.or_else(fn)` | `E -> expected<T, E>`，值透传 | 错误恢复 / 转换 |

### 候选清单（A-2 scope, 10 处）

#### ⭐⭐⭐ 高价值（链式表达确实更清晰）

1. **`examples/pdk_chat_demo_evolution/evolution_session.cpp:412-440`** — genome 加载 + walk_ancestors 串联
2. **`include/agenticdsl/evolution/harness_rsi.cpp:140-180`** — apply_harness_mutation 多步串联

#### ⭐⭐ 中价值（行数减半）

3. **`src/common/llm/cost_tracking_decorator.cpp:32-43`** — decorate_generate 副作用 + 透传
4. **`pdk/loop_agent/src/pdk_entry.cpp:59-81`** — ProviderLLMTool::call 字段填充
5. **`examples/pdk_chat_demo_evolution/evolution_session.cpp:202-205`** — seed.error early-return
6. **`examples/pdk_chat_demo_evolution/evolution_session.cpp:370-380`** — chat() 错误传播
7. **`examples/pdk_chat_demo_evolution/evolution_session.cpp:476-485`** — dual-baseline 比较

#### ⭐ 低价值（不迁）

- `compliance_decorator.cpp:55-93` — 多 emit 条件分支，链式反而复杂
- `examples/agent_loop/agent_loop.cpp:105` — 单点 has_value
- 简单 `if (!api_key.has_value()) {...}` 空值检查

### 不在 scope

- **monadic ops 性能影响**：zero-cost abstraction，编译后等价于手写代码，不预期性能差异
- **新 std::expected 用法**：不在变化范围
- **result-type-contract spec**：现有 spec 继续有效

## Impact

| 指标 | 预期 |
|------|------|
| 改动行数 | ~200 行净减少（10 处共 -200 行样板） |
| 行为变化 | 0%（纯 refactor，编译后等价） |
| 测试改动 | 0%（接口不变，行为不变） |
| 公开 ABI | 0 变化 |
| ctest PASS | 必须 305/305 保持 |
| drop_ratio | 必须 0%（R8 红线） |

## Non-goals

- 不引入新的 std::expected<T, E> 实例化
- 不迁移 compliance_decorator.cpp（多 emit 分支，链式反而复杂）
- 不迁移 std::optional 的 monadic ops（独立 Phase B-1）
- 不引入 std::move_only_function（独立 Phase B-2）

## Cooling-off

`unify-result-to-std-expected` ship at 2026-10-08T16:08Z. 本 change created at 2026-10-09T10:00Z. Cooling-off 24h 已满 ✅

## Related

- `openspec/specs/result-type-contract/spec.md` — locks std::expected as canonical
- `openspec/changes/archive/2026-10-08-unify-result-to-std-expected/` — 前置 change
- AGENTS.md:970 — non-goal 记录点
