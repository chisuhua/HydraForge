# std::expected Monadic Ops 链式重构 — Spec

**Change ID**: `2026-10-09-std-expected-monadic-ops`
**Status**: 🔍 Proposed
**对应 proposal**: `proposal.md`
**对应 design**: `design.md`

## 1. 范围

本 spec 锁定 std::expected 在 HydraForge 项目中的 monadic ops 使用约定：

## ADDED Requirements

### Requirement: std::expected 必须支持 C++23 monadic ops

`std::expected<T, E>` MUST 支持以下 monadic ops（C++23 标准化，P0323R12）：

| Op | 签名 | 语义 |
|----|------|------|
| `.transform(fn)` | `fn: T → U` | 成功值转换，错误透传 |
| `.and_then(fn)` | `fn: T → expected<U, E>` | 链式返回 expected，错误自动透传 |
| `.or_else(fn)` | `fn: E → expected<T, E2>` | 错误恢复 |

#### Scenario: and_then 在 success 路径调用 fn
- **WHEN** `expected` 当前 has_value() = true
- **THEN** `.and_then(fn)` MUST call fn with value
- **AND** MUST return fn's expected<U, E> result（链式）

#### Scenario: and_then 在 error 路径透传
- **WHEN** `expected` 当前 has_value() = false
- **THEN** `.and_then(fn)` MUST NOT call fn
- **AND** MUST return原 unexpected(E) 值

#### Scenario: transform 转换成功值类型
- **WHEN** `expected` 当前 has_value() = true
- **THEN** `.transform(fn)` MUST call fn with value and wrap result in expected<U, E>

---

### Requirement: monadic ops 使用判定标准

仅当以下条件 ≥1 项成立时 SHOULD 使用 monadic ops。下列规则 MUST 由 code review 强制执行：

1. ≥ 2 步 expected<T, E> 串联
2. 单纯 transform 语义（不改变 E 类型）
3. 错误可一次性透传到顶层

**反例**（保留传统 `if-res.has_value()` 写法）：
- 多分支条件 + 多种 emit
- 需要直接读 E 字段的复杂错误处理
- 单点 has_value 检查

#### Scenario: compound condition 保留传统写法
- **WHEN** 调用点使用 compound condition (`has_value() && other_check`)
- **THEN** SHOULD NOT 强行迁移到 monadic ops（chain 形式反而难读）

#### Scenario: 多分支 emit 保留传统写法
- **WHEN** 调用点需要根据 success/error 分支做不同副作用（如 emit 事件）
- **THEN** SHOULD NOT 强行迁移（monadic 单一 fn 难以表达多 emit 分支）

---

### Requirement: 传统 if-res.has_value() 写法兼容

传统 `if-res.has_value()` 写法 MUST 继续允许（不强制替换）：

```cpp
auto r = some_call();
if (r.has_value()) { use(r.value()); }
else { handle(r.error()); }
```

传统写法与 monadic 链式可在同一 codebase 共存。

#### Scenario: 同 codebase 共存
- **WHEN** 同一文件中既有 monadic 链式调用点又有传统 `if/has_value()` 调用点
- **THEN** 两者 MUST NOT 互相排斥
- **AND** MUST NOT 由 linter / formatter 强制一种风格

---

## MODIFIED Requirements

### Requirement: 公开 ABI 不变（修改自 `unify-result-to-std-expected` R1）

`unify-result-to-std-expected` (`2026-10-08`) 引入的公开 ABI MUST 保持不变（`ILLMProvider::generate` + `IGenomeRegistry` 6 签名）。

本 change MUST NOT 新增虚函数或修改公开类型签名。monadic ops 是 std::expected 的成员函数调用（内联），公开 ABI 字节布局 MUST 零变化。

#### Scenario: PDK .so 不需重编
- **WHEN** consumer PDK 仅使用 std::expected monadic ops（不修改公开类型签名）
- **THEN** linked .so MUST NOT 需要重编（vs `2026-10-08-unify-result-to-std-expected` ship 时 6 个 PDK .so 重编）

#### Scenario: result-type-contract delta
- **WHEN** `result-type-contract` spec 描述 std::expected monadic ops 语义
- **THEN** `.and_then(fn)` MUST call fn 仅在 has_value() 时，错误自动透传
- **AND** `.transform(fn)` MUST call fn 仅在 has_value() 时，错误自动透传

#### Scenario: genome-registry delta
- **WHEN** callers 在 `IGenomeRegistry` 返回值上使用 `.and_then(...)` 串联多步
- **THEN** chain MUST 保持 Error type 不变（不允许 narrowing）
- **AND** 错误 MUST 自动透传到顶层 caller

## REMOVED Requirements

无。

## 2. 公开 spec 同步

### `result-type-contract` (existing)

```diff
 ## ADDED Requirements

 ### The `expected` Type Semantics
 - `std::expected<T, E>` MUST use T = success value type, E = error type
 - The unexpected value MUST be constructible via `std::unexpected(E{...})`
 - `has_value()` MUST return true iff success
+- Monadic ops (transform/and_then/or_else) MUST follow C++23 semantics
+- `.and_then(fn)` MUST call fn only if has_value(), propagate error otherwise
+- `.transform(fn)` MUST call fn only if has_value(), propagate error otherwise
```

### `genome-registry` (existing)

```diff
 ## ADDED Requirements

 ### Error Propagation
 - `IGenomeRegistry::commit/fork/walk_ancestors` MUST return `std::expected<T, GenomeError>`
 - Errors MUST be specific `GenomeError` enum values
+- Callers MAY chain via `.and_then(...)` for multi-step operations
+- Error type MUST remain `GenomeError` through the chain (no error narrowing)
```

## 3. 实现约定

### 3.1 命名空间

所有 std::expected 调用位于 `namespace agenticdsl` / `agenticdsl::genome` 等项目 namespace，std::expected 本身在 `namespace std`。

### 3.2 包含头文件

```cpp
#include <expected>  // std::expected + std::unexpected
```

不再 include 已删除的本地 `Result<T, E>`（per `unify-result-to-std-expected` 已删）。

### 3.3 构造方式

```cpp
// 成功值
return std::expected<T, E>{std::in_place, std::move(value)};

// 错误值
return std::unexpected(std::move(error));
```

**禁止**：`Result::success(v)` / `Result::failure(e)` — 本地 factory 已删除。

### 3.4 解包方式

```cpp
// 1. 直接解包（仅在 has_value() 守卫后）
if (r.has_value()) { use(r.value()); }

// 2. 解引用
if (r.has_value()) { use(*r); }

// 3. monadic chain
auto final = r.and_then([](auto v){ return transform(v); });
```

### 3.5 error() 访问

```cpp
auto e = r.error();  // 仅在 !has_value() 守卫后使用
```

**禁止**：在 has_value() 路径中访问 `.error()`（UB）。

## 4. 测试约定

### 4.1 monadic chain 等价测试

每个迁移点必须新增 1 个测试验证：

```cpp
TEST_CASE("evolution_session genome chain equivalence") {
  // 1. 用原 if-else 写法构造 expected<T, E>
  // 2. 用新 monadic chain 写法构造 expected<T, E>
  // 3. REQUIRE(两个结果完全等价 — has_value / value / error 一致)
}
```

### 4.2 集成回归

跑全量 ctest 验证 drop_ratio = 0%。

## 5. 失败模式（CI gate）

| 失败模式 | 触发条件 | 治理动作 |
|---------|---------|---------|
| drop_ratio > 5% | monadic 链式引入行为变化 | 立即回滚 + Oracle 复评 |
| TSan data race | lambda 闭包未捕获正确 | 修捕获语义 |
| 公开 ABI 变化 | 误改签名 | 立即回滚 |

## 6. 治理链

- OpenSpec change 4 文件：proposal/design/tasks/specs ✅
- AGENTS.md Recent Changes entry（commit message 含 [Reverse Indicator]）
- Oracle post-impl review (Pattern #4 SHIP-with-fixes)
- Day-5 archive: `git mv` 到 archive/（per AGENTS.md 2026-09-15 Day 5 lesson）
- 24h cooling-off（per AGENTS.md Pattern #4）

## 7. 历史

- **2026-10-08** `unify-result-to-std-expected` ship — 引入 std::expected，删本地 Result
- **2026-10-09** `2026-10-09-std-expected-monadic-ops` (本 change) — 引入 monadic ops 链式约定
