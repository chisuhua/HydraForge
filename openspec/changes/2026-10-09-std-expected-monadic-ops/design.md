# Design: std::expected Monadic Ops 链式重构

**Change ID**: `2026-10-09-std-expected-monadic-ops`
**Status**: 🔍 Proposed
**对应 proposal**: `proposal.md`

## 1. 背景

`std::expected<T, E>` 是 C++23 标准化的错误处理类型（P0323R12）。libstdc++ 14.2 实现完整（`<expected>` header + 全部 monadic ops）。当前项目已在 140 处使用。

C++23 完整化了 3 个 monadic ops，用于函数式链式错误处理：

| Op | 签名 | 何时调用 fn |
|----|------|------------|
| `.transform(F)` | `F: T → U` | 仅当 has_value()，产出 `expected<U, E>` |
| `.and_then(F)` | `F: T → expected<U, E>` | 仅当 has_value()，错误自动透传 |
| `.or_else(F)` | `F: E → expected<T, E>` | 仅当 !has_value()，产出 `expected<T, E2>` |

## 2. 设计决策（D1-D6）

### D1：scope = A-2（中等推荐范围）

**选项**：
- A-1（保守）：5 处，仅 core 业务流（evolution_session + harness_rsi）
- **A-2（推荐）**：10 处，+ 装饰器 + ProviderLLMTool
- A-3（激进）：124+ 处，全代码库系统迁移

**决策**：A-2。

**理由**：
- A-1 收益太小（5 处共 -50 行），不值得 OpenSpec change 流程成本
- A-3 风险太高：124+ 处 mechanical 替换，回滚成本大；`compliance_decorator` 等多分支场景链式反而难读
- A-2 覆盖 ~80% 价值 / ~20% 风险，符合 incremental refactor 原则
- 留给未来 change 迭代（如发现 A-2 收益显著，可启动 A-3 follow-up）

### D2：链式 vs 传统写法并存

不强制迁移全部 std::expected 调用点。允许传统 `if-res.has_value()` 与 monadic 链式共存。

**判定标准**（仅当以下 ≥1 项成立时迁移）：
1. ≥ 2 步串联（每步都是 expected<T,E>）
2. 单纯 transform 语义（仅转换 T → U，不改变 E）
3. 链中错误可一次性透传到顶层

**反例**（保留传统写法）：
- 多分支 emit + 多种条件
- 需要读 E 的复杂错误处理
- 单点 has_value 检查

### D3：保留 `*` / `.error()` 解包语法

```cpp
auto e = result.error();  // 保留 (monadic 不适用读 E)
auto t = *result;          // 保留 (等价于 result.value())
auto t = result.value();   // 保留
```

**理由**：monadic ops 是新增能力，不是替换。错误处理代码（读 E）必须保留直接访问。

### D4：测试覆盖策略

- **新增单测**：每个迁移点增加 1 个 monadic chain TEST_CASE，验证 chain 等价于原 if-else 版本
- **不删原测试**：原 if-else 测试保留（验证行为不变）
- **integration test**：跑现有 ctest 套件，验证 drop_ratio = 0%

**反例**：不引入"统一 monadic ops helper"（如 `auto chain_monadic(...)`），保持每个调用点显式。

### D5：commit 粒度

按 AGENTS.md Pattern #4（atomic commits）：
- 每个迁移点 1 commit
- 1 commit 包含该点的所有 helper / 单测改动
- 不 amend，按顺序堆叠

例：
```
2451498 (已 ship, baseline)
8210b9a (已 ship, archive)
dc867f1 (已 ship, docs)

# 本 change
<commit-A1>: refactor(evolution_session): apply .and_then chain at L412-440
<commit-A2>: refactor(harness_rsi): apply .and_then at apply_harness_mutation
<commit-A3>: refactor(cost_tracking_decorator): apply .and_then side-effect
<commit-A4>: refactor(pdk_entry): ProviderLLMTool .transform chain
<commit-A5>: refactor(evolution_session): remaining 3 sites
<commit-A6>: archive + docs
```

### D6：公开 ABI 不变

所有 monadic ops 都是 std::expected 的成员函数调用（内联），不引入新虚函数或新类型。公开 ABI 字节布局零变化。PDK .so **不需重编**（vs 2026-10-08 ship 需重编）。

## 3. 候选迁移详细设计

### 迁移点 #1: evolution_session.cpp:412-440 (genome chain)

**当前**（伪代码）：
```cpp
auto genome_result = registry_->load(name, version);
if (!genome_result.has_value()) {
    return std::unexpected(genome_result.error());
}
const auto& genome = genome_result.value();

auto walk_result = registry_->walk_ancestors(name, version);
if (!walk_result.has_value()) {
    return std::unexpected(walk_result.error());
}
// ... 5 more similar steps ...
```

**改写后**：
```cpp
return registry_->load(name, version)
    .and_then([&](Genome g) { return registry_->walk_ancestors(name, version)
        .transform([&](LineageWalk w) { /* merge */ }); })
    .and_then(...);
```

**行数**：22 行 → 8 行（-64%）

### 迁移点 #2: harness_rsi.cpp apply_harness_mutation

**当前**：3 步串联 validate → apply → persist，每步都检查 + 早返

**改写后**：链式 `.and_then(...).and_then(...)` 串联

**行数**：~40 行 → ~15 行

### 迁移点 #3: cost_tracking_decorator.cpp:32-43

**当前**：
```cpp
if (inner_result.has_value() && budget_ != nullptr) {
    const auto& result = inner_result.value();
    const int total_tokens = result.prompt_tokens + result.completion_tokens;
    if (total_tokens > 0) {
        budget_->record_llm_call(total_tokens, req.params.model);
    }
}
return inner_result;
```

**改写后**：
```cpp
if (budget_ != nullptr) {
    inner_result.and_then([&](const GenerationResult& r) -> std::expected<GenerationResult, LLMError> {
        const int total_tokens = r.prompt_tokens + r.completion_tokens;
        if (total_tokens > 0) {
            budget_->record_llm_call(total_tokens, req.params.model);
        }
        return inner_result;  // 透传
    });
}
return inner_result;
```

**行数**：11 行 → 9 行（-18%）。**注意**：这里 monadic ops 改善有限，主要目的是统一风格。

### 迁移点 #4: pdk_entry.cpp ProviderLLMTool (L59-81)

**当前**（22 行）：
```cpp
auto res = provider_.generate(req, cancellation_token_);
if (res.has_value()) {
    out.text = std::move(res).value().text;
    // ... empty check ...
    out.success = true;
    out.tokens_generated = res.value().completion_tokens;
} else {
    out.success = false;
    out.error = res.error().message;
}
return out;
```

**改写后**：
```cpp
return provider_.generate(req, cancellation_token_)
    .and_then([&](GenerationResult&& r) -> std::expected<GenerationResult, LLMError> {
        // 空 text 校验抛异常（不是 expected 错误，保持原行为）
        if (r.text.empty() || std::all_of(r.text.begin(), r.text.end(),
                [](unsigned char c){ return std::isspace(c); })) {
            throw std::runtime_error("...");
        }
        out.text = std::move(r).text;
        out.success = true;
        out.tokens_generated = r.completion_tokens;
        return std::unexpected(LLMError{});  // 标记走完（hack？）
    })
    .or_else([&](const LLMError& e) -> std::expected<GenerationResult, LLMError> {
        out.success = false;
        out.error = e.message;
        return std::unexpected(e);  // 透传
    })
    .transform_error([&](const LLMError&) -> LLMError { return LLMError{}; });  // 占位
```

**复杂度评估**：因为 `out` 是按引用写入的 + 异常分支存在，链式表达**反而复杂**。**决策**：此处保留传统写法。改写 ROI 低。

**调整 scope**：移除此候选，剩 9 处。

## 4. 风险与缓解

| 风险 | 概率 | 影响 | 缓解 |
|------|------|------|------|
| monadic lambda 闭包开销 | 低 | 性能 -1% | O2 编译后等价 |
| 调试时 stack trace 失真 | 中 | DX | 每个 chain 末端加注释指向迁移前位置 |
| lambda 捕获悬垂引用 | 低 | UB | 显式 `[&]` 标识捕获域；compile-time 检查 |
| 行为变化（refactor 引入 bug） | 中 | drop_ratio > 5% → block | TDD 5-step + dual review |

## 5. 验证策略

```bash
# 1. 编译验证 (gcc-14 + C++23)
cmake --build build -j$(nproc)

# 2. 单元测试 + 集成测试
ctest --test-dir build -LE must_realllm --output-on-failure
# 预期：305/305 PASS（与 2451498 baseline 一致）

# 3. drop_ratio 守卫 (R8 红线)
# 在 --release-metrics flag 验证 drop_ratio == 0.0

# 4. Real LLM 烟雾测试（可选，有 key 时）
ctest --test-dir build -L must_realllm --output-on-failure
```

## 6. 治理

- OpenSpec change 4 文件：proposal/design/tasks/specs ✅
- AGENTS.md Recent Changes entry：commit message 含 [Reverse Indicator] 段
- Oracle post-impl review (per AGENTS.md Pattern #4 SHIP-with-fixes)
- Day-5 archive：git mv 整个 openspec/changes/<name>/ 到 archive/

## 7. 非目标

- 不迁移 std::optional 的 monadic ops（独立 Phase B-1）
- 不迁移 std::function → std::move_only_function（独立 Phase B-2）
- 不引入 monadic ops 的辅助 helper（每个调用点显式）
- 不重命名现有 Result 类型（已 ship 的 std::expected 不动）
