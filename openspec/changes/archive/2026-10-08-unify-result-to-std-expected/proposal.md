# unify-result-to-std-expected

## Why

项目 C++20 时代由于 `std::expected` 尚未标准化 (P0323R12 在 C++23 落地),被迫自实现两套 `Result<T, E>` 模板 (`include/agenticdsl/genome/genome.h:32` 与 `src/common/llm/llm_types.h:83`),并在 `include/agenticdsl/policy/path_policy.h:144` 明确写下"不使用 std::expected(C++23 才标准化)——改用 (error, message) 对"。

`be0600e` (2026-10-08) 升级到 C++23 + gcc-14 后,这一历史限制消失。但两套本地 `Result<T, E>` 共 **129 处使用点** (llm 99 + genome 30) 仍存在,带来:

- 重复实现 (4 套错误类型, 实际是 2 套模板 + 2 套非模板结构体)
- 缺乏 monadic ops: 调用方必须 `if (r.has_value()) use(r.value()); else handle(r.error());`,而非 `r.and_then(...).or_else(...)`
- 公开 API 噪音: `ILLMProvider::generate()` 虚函数签名被自家类型占据,影响 PDK 插件与第三方实现
- 与 std::optional/std::variant 互操作需手动转换

Per AGENTS.md §C++23 切换评估 (2026-10-08) + Pattern #1 (系统性) + Pattern #4 (atomic ship),本 change 利用 C++23 新能力统一两处本地 `Result<T, E>` 为 `std::expected<T, E>`,为后续 monadic refactor 与 std::flat_map 迁移铺路。

## What Changes

- **替换 `agenticdsl::Result<T, E>`** (`src/common/llm/llm_types.h:83-110`): **BREAKING** 删除本地模板,所有签名 `Result<X, Y>` 替换为 `std::expected<X, Y>`。**99 处调用点全部更新** (含 `ILLMProvider::generate` 虚函数 / `ILLMProviderDecorator::decorate_generate` / `MockLLMProvider` / `CloudAdapter` / `FinetuneProvider` / `OrchestrationILLMProvider`)
- **替换 `agenticdsl::genome::Result<T, E>`** (`include/agenticdsl/genome/genome.h:32-56`): **BREAKING** 同上。**30 处调用点全部更新** (含 `IGenomeRegistry` 6 个虚函数 / `FilesystemGenomeRegistry` / `walk_ancestors` default impl)
- **更新工厂语法** `.success(v)` → `std::expected<T, E>{std::in_place, std::move(v)}` (大括号聚合初始化, 单一锁定形式);`.failure(e)` → `std::unexpected(std::move(e))` (C++23 失败工厂)
- **更新访问语法** `.has_value()` → `.has_value()` (不变) / `.value()` → `.value()` / `.error()` → `.error()` (不变) / `!r.has_value()` → `!r.has_value()` (不变)
- **更新 `policy/path_policy.h:144` 注释**: 删除"C++23 才标准化"原因说明,改写为 SecurityError 仍保持 `(error, message)` 对的 rationale
- **新增 spec capability** `result-type-contract`: 统一 `Result<T, E>` 语义契约,适用所有 namespace
- **不重构 `SecureToolRegistry::Result`** (bool+json+SecurityError 三态结构体,不是错误容器)
- **不重构 `ContextEngine::Result`** (Context+optional 合并结果,不是错误容器)
- **不引入兼容层**: 不保留 `using Result = std::expected<T,E>` 别名(决策: 完全替换,见决策点)
- **不批量改 monadic ops**: 本 change 仅做 1:1 替换,`.and_then()` / `.or_else()` / `.transform()` 留作后续 follow-up change

## Capabilities

### New Capabilities

- `result-type-contract`: 定义项目错误处理统一类型契约 — `std::expected<T, E>` 是项目所有"成功值 + 错误码"返回类型的规范选择;`.has_value()` / `.value()` / `.error()` 是必提供访问接口;不允许再定义本地 `Result<T, E>` 模板

### Modified Capabilities

- `genome-registry`: 6 个 `IGenomeRegistry` 虚函数签名从 `Result<X, GenomeError>` 改为 `std::expected<X, GenomeError>`(delta spec) — 现有 spec 文本 6 个 Requirement 中 5 个含 `Result::success/failure` 引用需更新

## Impact

- **代码影响 (估算)**:
  - `src/common/llm/llm_types.h`: 删除 28 行 Result 模板, 1 行 `#include <expected>`
  - `include/agenticdsl/genome/genome.h`: 删除 25 行 Result 模板, 1 行 `#include <expected>`
  - 99 + 30 = 129 处调用点更新 (`.success(` / `.failure(` / 类型别名, 约 4-6 行/处批量替换)
  - 1 行注释更新 (`path_policy.h:144`)
  - **预估 diff 规模**: 15-20 文件, +200/-300 行 (净减少)
- **API 破坏**:
  - 公开 header ABI 变化 (`llm_types.h` / `genome.h` 暴露的 std::expected)
  - 所有 PDK 插件 (`loop_agent` / `provider_agent` / `chat_session` 等) 需重编
  - 所有 `tests/test_*.cpp` 涉及 `Result<...>` 用法的 binary 需更新
- **依赖**:
  - libstdc++ 14+ (C++23 `std::expected` 支持, gcc-14.2 已 ship per `be0600e` 验证)
  - 无新增 vendored 依赖
- **测试影响**:
  - 至少 8 个 test binary 涉及 `Result<>` 用法 (per grep `tests/test_*.cpp`)
  - 全部 test 必须重编通过
  - drop_ratio 期望 ≤ 5% (零功能性变化, 纯类型替换)
- **冷却期 (Cooling-Off)**:
  - 触发 24h cooling-off 周期 (per AGENTS.md Pattern #4)
  - 起点: proposal.md 完成时
  - 到期: 24h 后可执行 Phase 2 (实施)
- **回滚策略**:
  - git revert single atomic commit (per AGENTS.md 模式 #4 atomicity)
  - 4 套 provider 临时还原 (但 PDK .so 已发布, 需协调 PDK rebuild)
- **Non-goals (明确范围)**:
  - 不引入 monadic ops (`.and_then` / `.or_else` / `.transform`) refactor — 留独立 follow-up change
  - 不替换 `SecureToolRegistry::Result` (2 处使用,非错误容器,语义不同)
  - 不替换 `ContextEngine::Result` (1 处使用,合并结果,非错误容器)
  - 不改变 `GenomeError` / `LLMError` enum 本身 (签名变化不影响错误码集合)
  - 不引入 `Result<T,E>` 类型别名 / 兼容层 (用户决策: 完全替换)
  - 不修改 `tools/registry.h` 的 `register_tool<Func>` 模板桥接
  - 不触及 std::optional / std::function / std::map (其他 C++23 优化机会,独立 follow-up)
