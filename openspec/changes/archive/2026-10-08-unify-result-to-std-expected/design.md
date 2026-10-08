# unify-result-to-std-expected Design

## Context

`be0600e` (2026-10-08) 升级 C++20 → C++23 + gcc-14,`std::expected<T, E>` (P0323R12) 已标准化为 C++23 标准库头 `<expected>`,libstdc++ 14.2 已完整支持。

但项目仍维持 2 套本地 `Result<T, E>` 模板:

- `src/common/llm/llm_types.h:83-110` — `agenticdsl::Result<T, E>`, 99 处调用点, 1 套 mutable accessors
- `include/agenticdsl/genome/genome.h:32-56` — `agenticdsl::genome::Result<T, E>`, 30 处调用点, 1 套 const-only accessors

两套命名一致但 namespace 不同,API 形态完全相同 (`has_value()` / `value()` / `error()` / 静态工厂 `success()` / `failure()`)。

直接证据 — `include/agenticdsl/policy/path_policy.h:144` 注释:

> "不使用 std::expected(C++23 才标准化)——改用 (error, message) 对"

C++23 切换消除了这一历史限制。

**约束**:
- C++23 + libstdc++ 14.2 是基础 (`be0600e` 已 ship)
- 公开 ABI 变化 (公开 header 暴露 std::expected 而非本地类型)
- 6 个 PDK `.so` (`loop_agent` / `provider_agent` / `chat_session` / `budget_agent` / `g1_coding_assistant` / `g3_knowledge_base`) 需重编
- 至少 8 个 test binary 涉及 Result 用法
- 必须保持 99 + 30 = 129 处调用方零行为变化 (drop_ratio ≤ 5%, 实际应 0%)
- 不允许修改 `GenomeError` / `LLMError` enum 本身

**Stakeholders**: Solo-Dev (实施 + 自审, per AGENTS.md §SINGLE-DEVELOPER MODE) + Oracle (post-impl review) + 用户 (最终 caller of the public API)

## Goals / Non-Goals

**Goals:**
- 完全替换 2 处本地 `Result<T, E>` 为 `std::expected<T, E>`,无兼容层
- 保留 6 个公开虚函数签名语义 (callers 零行为变化, 仅类型名不同)
- 一次 atomic commit ship (per AGENTS.md 模式 #4)
- 全量 ctest 零回归 (`ctest -LE must_realllm` 100% PASS, drop_ratio = 0%)
- PDK 6 个 `.so` 全部重编成功 + 全量 build 无 error
- 更新 `policy/path_policy.h:144` 注释, 删除过时的 C++23 限制说明
- 新增 `result-type-contract` spec 锁定未来契约

**Non-Goals:**
- 不引入 monadic ops (`.and_then()` / `.or_else()` / `.transform()`) 重构 — 留独立 follow-up change,避免范围蔓延
- 不替换 `SecureToolRegistry::Result` (非错误容器, 用户决策 ack)
- 不替换 `ContextEngine::Result` (非错误容器, 用户决策 ack)
- 不引入 `using Result = std::expected<T, E>` 别名 (用户决策 ack: 完全替换)
- 不修改 `GenomeError` / `LLMError` enum 内容 (只换外壳)
- 不触及 std::optional / std::function / std::map 的 C++23 优化机会 (独立 change 候选)
- 不引入 `<expected>` 之外的 C++23 新依赖 (如 std::flat_map / std::mdspan / std::generator)

## Decisions

### D1: 完全替换 vs 类型别名 vs 双套并存

**决策**: 完全替换 (无 `using` 别名,无 `[[deprecated]]` 兼容层)

**理由**:
- 用户明确决策: 完全替换
- 类型别名 `using Result = std::expected<T, E>` 仅节省重命名工作量, 仍要求调用方改写工厂语法 (`.success(v)` → `std::in_place, v`)
- 双套并存留 1 release cycle 维护成本, 但本项目是 single-dev 模式, 没有外部 PDK consumer 受 ABI 约束
- 完全替换 1 次性消除技术债, 与 AGENTS.md "少留半完成工作" 治理原则一致

**替代方案**:
- 类型别名 (rejected): 节省 30 分钟重命名, 长期留 2 套类型
- 双套并存 (rejected): 1 release cycle 维护 2 套, 单人开发成本高

### D2: 工厂语法风格

**决策**: 锁定单一统一形式 — 成功路径 `std::expected<T, E>{std::in_place, std::move(v)}` (大括号聚合初始化), 失败路径 `std::unexpected(std::move(e))` (C++23 失败工厂)

**理由**:
- `std::expected<T, E>::success(v)` 不存在(标准未提供), 必须用 in-place 构造或移动
- 成功路径选大括号聚合初始化 `{std::in_place, v}` 而非圆括号 `(std::in_place, v)`:
  - 大括号是 C++23 标准文档 idiom, 与 libstdc++ 示例一致
  - 圆括号 `(std::in_place, v)` 容易与函数调用风格混淆, 且必须显式给模板参数 (std::expected **无 deduction guide**, 不能用 `std::expected{std::in_place, v}` 简写)
  - `{std::in_place, v}` 的 in_place 显式 tag 避免与 `std::unexpected` 歧义 (尤其当 T 和 E 同类型时)
- 失败路径用 `std::unexpected(e)`: `std::unexpected` 是 std::expected 的失败工厂, 直接返回是 idiomatic C++23 错误处理

**具体语法** (锁定为项目内唯一允许形式):
```cpp
// 成功 (大括号聚合初始化)
return std::expected<T, E>{std::in_place, std::move(v)};

// 失败 (C++23 失败工厂)
return std::unexpected(std::move(e));
```

**禁止形式** (本 change 范围外, 留 monadic ops follow-up):
- `r.and_then(...)` / `r.or_else(...)` / `r.transform(...)` — C++23 monadic ops, 留独立 follow-up change
- `using Foo = std::expected<X, Y>` 项目内别名 — 公开 header 禁止 (避免传染, 详见 D5)

**理由补充**: 锁定单一大括号形式是因为 std::expected 无 CTAD (无 deduction guide), 任何省略显式模板参数的写法都不编译。AI 实施时只能照此模板, 不存在二选一歧义。

### D3: 访问方法兼容性

**决策**: `has_value()` / `value()` / `error()` 全部沿用 std::expected 原生方法, 不提供包装

**理由**:
- std::expected 的 `has_value()` / `operator*()` / `value()` / `error()` 与本地 Result 完全同名同语义
- 99 + 30 = 129 处调用点的访问代码 (`if (r.has_value()) r.value();`) 零修改
- 唯一变化是 `value()` 在 std::expected 中是 `[[nodiscard]]` 且失败时抛 `std::bad_expected_access<E>` — 这是行为变化!

**风险点**: `value()` 抛异常 (vs 本地 Result 的 UB)。**缓解**: 现有调用方都先 `has_value()` 检查再 `value()` (per grep 模式), 实际抛异常路径不触发。但需在 tasks.md 写明这一行为差异, 配 audit 验证所有 129 处访问路径。

### D4: 错误类型包装 (GenomeError / LLMError)

**决策**: `GenomeError` / `LLMError` enum 保持原状, 包装为 `std::unexpected<E>` 即可

**理由**:
- 错误类型本身是 enum class, 无附加数据
- `std::unexpected<GenomeError>` 透明包装, 调用方 `.error()` 直接拿到 enum
- 不需要适配层, 不增加内存 (std::unexpected 是 empty type)

### D5: 命名空间处理

**决策**: 不再使用 `agenticdsl::Result` 或 `agenticdsl::genome::Result` 命名空间前缀, 直接 `std::expected<T, E>`

**理由**:
- std::expected 是标准库类型, 暴露在 std namespace 是 idiomatic
- 调用方代码可读性: `std::expected<GenerationResult, LLMError>` 明确表达 "标准库的 success-or-error"
- 避免项目内 `Result` 名字污染

**代价**: 类型签名变长, 但 std::expected 在 C++23 社区已是共识, 不引入混淆

### D6: 头文件包含

**决策**: 在 2 个公开 header (llm_types.h + genome.h) 加 `#include <expected>`

**理由**:
- std::expected 头轻量, 无二进制开销
- 2 处公开头都已 ship, 增量 1 行 include
- 避免 transitive include 问题 (调用方直接看到 std::expected 来源)

### D7: PDK 兼容性策略

**决策**: PDK 6 个 `.so` 不需 ABI 兼容层, 直接重编 (per AGENTS.md 模式 #4 atomic ship + full rebuild 模式)

**理由**:
- 项目是 monorepo, PDK 与 core 同 tree 重编
- PDK 是 `.so` 而非 `.a`, 不存在静态链接的二进制兼容问题
- 全量 rebuild 已在 `be0600e` ship 时验证 (215/215 ctest PASS)

**替代方案**:
- 提供旧 Result 兼容 shim (rejected): PDK 是 6 个 .so, 维护 shim 成本高于重编
- 强制 PDK consumer 升级 (rejected): 项目内 6 个 PDK 都是项目自有, 无外部 consumer

### D8: Test 回归策略

**决策**: 全量 ctest 重跑, 重点审计 8 个涉及 Result 的 test binary

**理由**:
- 8 个 test binary 涉及 Result: `test_provider_factory*` / `test_loop_agent*` / `test_harness_rsi*` / `test_genome*` / `test_provider_llm_tool*` 等
- 全量 `ctest -LE must_realllm` 必须 100% PASS
- drop_ratio ≤ 5% (R8.1 红线, 实际期望 0% 因纯类型替换)

## Risks / Trade-offs

### R1: `value()` / `error()` 抛异常 vs 旧 Result 静默默认值

[Risk] `std::expected::value()` 在 has_value()==false 时抛 `std::bad_expected_access<E>`, `error()` 在 has_value()==true 时是 **UB** (无 guard). 旧 `Result::value()` / `error()` 在错误分支对 class-type 返回**默认构造对象** (定义行为, 但常被忽略导致逻辑 bug), 对 scalar-type 是 indeterminate value (UB).
[Impact] **方向变安全**: value() 从"静默返回默认值" → throw, 让忽略 has_value() 检查的隐藏 bug 立即暴露 (而非悄无声息地读垃圾值). error() 方向相反 (从"静默返回默认 E" → UB), 但 E 在本项目多为 enum class, 旧 default 实际无意义, 实际风险有限. **任何调用方跳过 has_value() 检查都会从"静默错误"变为"明确 throw 或 UB"**, 暴露既有 bug.
[Mitigation]
- 函数级 audit (per tasks 4.3): 验证所有 99 + 30 = 129 处 .value() / ~30 处 .error() 调用点控制流路径上 has_value() 已保证, 不只行级 grep "前一行有 check"
- 重点 audit `harness_rsi.cpp:218` 多行间隔模式 (Gate 3 顶部 has_value() 后函数中段 .value())
- 若发现裸 .value() / .error() 无 check, 补 if-check (首选) 或加函数注释说明"失败时抛 std::bad_expected_access" (per result-type-contract spec)
- tasks.md Phase 2 写明 audit 步骤

### R2: PDK 6 个 `.so` 同步重编失败

[Risk] 任一 PDK 链接失败会阻塞全量 build.
[Impact] build red → ctest 无法跑 → ship 失败.
[Mitigation]
- 先核心 tree 重编验证 (`cmake --build build`), 再 PDK tree 重编 (`cmake --build build/pdk`)
- tasks.md Phase 1 拆为 4 sub-step: core types → core impl → tests → PDK
- 任意 phase 失败立即停, 不进入下一步

### R3: 类型签名变长影响可读性

[Risk] `std::expected<GenerationResult, LLMError>` 长度约 50 字符, 虚函数签名变拥挤.
[Impact] 虚函数声明可读性下降.
[Mitigation]
- 可考虑 `template alias using LLMResult = std::expected<GenerationResult, LLMError>;` 但**不**引入 (避免重蹈本地 Result 覆辙)
- 当前决策: 接受签名变长, 标准化优于语法糖

### R4: GCC 14.2 std::expected 实现细节

[Risk] libstdc++ 14.2 std::expected 是 P0323R12 实现, 与 C++23 标准化版本有微小差异 (如 `error()` 在某些边界 case 行为).
[Impact] 罕见路径行为偏差.
[Mitigation]
- 仅使用 std::expected 的核心 API: `has_value()` / `value()` / `error()` / `operator bool` / 工厂构造
- 不使用边缘 API (如 `transform_or`, `and_then` 等 monadic, 留 follow-up)
- C++23 切换 commit `be0600e` 已用 gcc-14.2 全量 ctest PASS, 基础已验证

### R5: ABI 公开头破坏 (AGENTS.md §Single-Dev 模式显式接受)

[Risk] 公开 header ABI 变化 → 第三方 PDK consumer (理论) 需重编.
[Impact] 项目目前无外部 PDK consumer (AGENTS.md §SINGLE-DEVELOPER MODE 自承); 内部 6 个 PDK .so 重编成本 < 1 min.
[Mitigation] 接受风险, 在 proposal.md Impact 段显式 ack. 若未来有外部 PDK consumer, 需独立兼容层 change.

### R6: 24h cooling-off 期间 (AGENTS.md Pattern #4)

[Risk] 24h cooling-off 期间用户可能基于部分信息质疑设计.
[Impact] 实施延后, 但符合治理纪律.
[Mitigation] 接受, 不绕过 cooling-off. 期间可做: tasks.md 细化 + Oracle pre-impl 预审.

## Migration Plan

### Phase 0: 准备 (cooling-off 期间)
- 写完 proposal.md / design.md / specs/* (本 change 当前阶段)
- 触发 24h cooling-off (起点: proposal.md 完成时)
- 期间可细化 tasks.md + 跑 Oracle pre-impl 审查 (可选)

### Phase 1: 核心类型替换 (核心 tree)
- **Step 1.1**: `src/common/llm/llm_types.h` 删除 Result 模板, 加 `#include <expected>`
- **Step 1.2**: 99 处 `agenticdsl::Result<X, Y>::success/failure` 调用点批量替换
- **Step 1.3**: 编译 `cmake --build build` 验证核心 tree 零 error

### Phase 2: Genome 替换
- **Step 2.1**: `include/agenticdsl/genome/genome.h` 删除 Result 模板
- **Step 2.2**: 30 处 `agenticdsl::genome::Result<X, Y>::success/failure` 调用点批量替换
- **Step 2.3**: 编译 `cmake --build build` 验证核心 tree + genome_registry_filesystem.so 零 error

### Phase 3: Test 回归
- **Step 3.1**: 8 个涉及 Result 的 test binary 全量重编
- **Step 3.2**: 全量 ctest `-LE must_realllm` 验证 100% PASS
- **Step 3.3**: drop_ratio 验证 = 0% (纯类型替换, 无行为变化)
- **Step 3.4**: 8 个 test binary 单独跑 + 重点 audit 是否有直接 .value() 无 check

### Phase 4: PDK 集成
- **Step 4.1**: 6 个 PDK .so 链接核心 tree 零 error
- **Step 4.2**: PDK 自身 ctest 验证

### Phase 5: 注释 + Spec 收尾
- **Step 5.1**: 更新 `policy/path_policy.h:144` 注释, 删除过时 C++23 限制说明
- **Step 5.2**: 更新 proposal.md / design.md 状态 (✅ ship)
- **Step 5.3**: atomic commit (per AGENTS.md 模式 #4)
- **Step 5.4**: Oracle post-impl SHIP-with-fixes 审查 (per AGENTS.md 模式 #4)

### 回滚策略

- **失败信号**: 任一 phase 编译失败 / 任一 ctest FAIL / drop_ratio > 5%
- **回滚操作**: `git revert <commit-hash>` (single atomic commit, 一键回滚)
- **回滚后状态**: 恢复本地 Result, 4 套 provider 重新调用本地类型, 公开 ABI 还原
- **回滚成本**: 1 分钟 git revert, 但若 PDK 已发布需协调 (项目内 monorepo 无此问题)

## Open Questions

- **OQ1**: 是否需要在 `src/common/llm/llm_types.h` 暴露 `using LLMResult<T> = std::expected<T, LLMError>` 类型别名?
  - **当前倾向**: 不暴露 (决策 D5 一致性), 但若实施时发现某 .cpp 文件签名重复 5+ 次, 局部加 alias 可接受
  - **决策时机**: Phase 1.2 实施时按需

- **OQ2**: `std::bad_expected_access<E>` 异常是否需要在某层捕获 + 转译回 `LLMError` enum?
  - **当前倾向**: 不需要 (per R1 mitigation, 所有调用方都先 has_value() 检查)
  - **验证**: Phase 3.4 audit 时确认
  - **若发现**: 配独立 follow-up change (本 change 范围外)

- **OQ3**: 是否同步引入 std::optional 的 monadic ops (`and_then` / `or_else` / `transform`)?
  - **当前倾向**: 不引入 (用户决策, 留 follow-up)
  - **理由**: 范围蔓延 + audit 复杂度高 + 与 std::expected 配合需重新设计链式 API
