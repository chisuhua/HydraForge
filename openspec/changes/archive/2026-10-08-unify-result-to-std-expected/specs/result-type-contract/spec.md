# result-type-contract Specification

## Purpose

定义项目内"成功值 + 错误码"返回类型的统一契约。锁定 `std::expected<T, E>` 为唯一允许的错误处理返回类型,禁止再定义本地 `Result<T, E>` 模板。覆盖 **3 个已 ship 的使用场景**:

1. **LLM provider 链** (`src/common/llm/llm_types.h` `agenticdsl::Result<T,E>`): `ILLMProvider::generate` 虚函数 / `MockLLMProvider` / `CloudAdapter` / `FinetuneProvider` / `OrchestrationILLMProvider` / `ILLMProviderDecorator` 共 99 处
2. **Genome registry 抽象** (`include/agenticdsl/genome/genome.h` `agenticdsl::genome::Result<T,E>`): `IGenomeRegistry` 6 个虚函数 / `FilesystemGenomeRegistry` / `walk_ancestors` default impl 共 30 处
3. **Harness mutation** (`include/agenticdsl/evolution/harness_rsi.h:73` `agenticdsl::Result<AppliedMutation, MutationError>`): `apply_harness_mutation` 虚函数 ~14 处 (复用 `agenticdsl::Result` 模板, 已被场景 1 覆盖)

**显式不在本 change 范围**:
- `AgentResult<T>` (`include/agenticdsl/contract/iagent_composition.h:28`): 第 4 个 Result 风格模板 (bool + T + optional + message), 与 `std::expected` 语义部分重叠但非纯错误容器, 留独立 follow-up change
- `ToolResult::success(...)` / `ToolResult::failure(...)` (46 处): 完全不同类型, 不可替换
- `SecureToolRegistry::Result` (bool + json + SecurityError 三态) + `ContextEngine::Result` (Context + optional 合并结果): 非错误容器, 用户决策不重构 (per proposal.md)

## ADDED Requirements

### Requirement: 错误处理返回类型必须是 `std::expected<T, E>`

项目内任何返回"成功值或错误码"的函数 MUST 使用 `std::expected<T, E>` (C++23, 来自 `<expected>` 头)。MUST NOT 再定义本地 `Result<T, E>` 模板。`T` 是成功值的类型, `E` 是错误码的类型(通常是 `enum class` 或简单 struct)。

#### Scenario: 成功值返回路径
- **WHEN** 函数返回成功值
- **THEN** MUST 使用 `std::expected<T, E>{std::in_place, std::move(v)}` 构造(显式 in-place 避免与 `std::unexpected` 歧义,尤其当 T 和 E 同类型时)

#### Scenario: 错误码返回路径
- **WHEN** 函数返回错误码
- **THEN** MUST 使用 `std::unexpected(std::move(e))` 构造(idiomatic C++23 失败工厂)

#### Scenario: 拒绝新增本地 Result 模板
- **WHEN** 任何新代码尝试定义 `template <typename T, typename E> class Result { ... }` 或类似别名
- **THEN** MUST NOT 通过 code review(per AGENTS.md 治理纪律)

#### Scenario: 接受 `using Foo = std::expected<...>` 项目内类型别名(可选)
- **WHEN** 某模块内 `std::expected<X, Y>` 类型签名重复出现 5+ 次
- **THEN** MAY 定义 `using FooResult = std::expected<X, Y>;` 项目内别名
- **AND** MUST NOT 在公共 header 暴露(避免传染)

---

### Requirement: 访问方法必须是 `has_value()` / `value()` / `error()`

所有使用 `std::expected` 的代码 MUST 通过 `has_value()` / `value()` / `error()` 三个核心方法访问。`has_value()` 返回 `bool`, `value()` 返回成功值引用(失败时抛 `std::bad_expected_access<E>`), `error()` 返回错误引用。

#### Scenario: 安全访问模式(两段式)
- **WHEN** 调用方需要处理成功和失败两种情况
- **THEN** MUST 先调用 `r.has_value()` 判断
- **AND** 若为 true 则调用 `r.value()` 访问成功值
- **AND** 若为 false 则调用 `r.error()` 访问错误

#### Scenario: 禁止直接 `r.value()` 无 check
- **WHEN** 调用方未先调用 `r.has_value()` 而直接调用 `r.value()`
- **THEN** 在 `has_value() == false` 路径上 MUST 抛 `std::bad_expected_access<E>` 异常
- **AND** 这不是 UB,但 MUST 在 code review 中标注(行为变化:旧本地 Result 在失败时 UB)

#### Scenario: 单分支访问模式
- **WHEN** 调用方只关心成功值(失败视为不可恢复)
- **THEN** MAY 使用 `r.value()` 直接访问(隐式接受异常传播)
- **AND** MUST 在函数头注释说明"失败时抛 std::bad_expected_access"

---

### Requirement: `LLMResult` 虚函数契约 (`llm-types`)

`ILLMProvider::generate()` 虚函数 MUST 签名:
```cpp
virtual std::expected<GenerationResult, LLMError>
    generate(const GenerationRequest& req, std::stop_token token) = 0;
```

`ILLMProviderDecorator::decorate_generate()` MUST 签名:
```cpp
virtual std::expected<GenerationResult, LLMError>
    decorate_generate(const GenerationRequest& req,
                      std::expected<GenerationResult, LLMError> inner_result) = 0;
```

具体 provider 实现(`MockLLMProvider` / `CloudAdapter` / `FinetuneProvider` / `OrchestrationILLMProvider`)MUST override 这两个虚函数,使用 `std::expected` 工厂语法。

#### Scenario: CloudAdapter::generate 成功路径
- **WHEN** `CloudAdapter::generate()` 成功获取 LLM response
- **THEN** MUST 返回 `std::expected<GenerationResult, LLMError>{std::in_place, result}`
- **AND** `result.text` / `result.prompt_tokens` / `result.completion_tokens` 字段填充

#### Scenario: CloudAdapter::generate 失败路径
- **WHEN** `CloudAdapter::generate()` 检测到 HTTP 402 / 401 / network error
- **THEN** MUST 返回 `std::unexpected(LLMError{LLMError::Code::AuthenticationError, "HTTP 401 unauthorized"})` 或对应 LLMError 变体
- **AND** MUST NOT 抛异常(契约要求)
- **AND** LLMError 实际有 8 个 variant: `NetworkError` / `RateLimited` / `AuthenticationError` / `Cancelled` / `InvalidRequest` / `ServerError` / `ContextOverflow` / `Unknown`(per `src/common/llm/llm_types.h:27-36`)

#### Scenario: Decorator 链式包装
- **WHEN** `CostTrackingDecorator::decorate_generate()` 包裹 inner_result
- **THEN** MUST 检查 `inner_result.has_value()`,若有 value 则累加 cost
- **AND** 累加失败 MUST 返回 `std::unexpected(LLMError{LLMError::Code::RateLimited, "cost budget exceeded"})`(per `cost-tracking-decorator-realllm` spec, 注意 LLMError 是 struct 不是 enum, 必须用 `LLMError{Code::..., msg}` 双参构造)
- **AND** 累加成功 MUST 透传 `inner_result`

---

### Requirement: `GenomeResult` 虚函数契约 (`genome-types`)

`IGenomeRegistry` 6 个虚函数 MUST 签名:
```cpp
virtual std::expected<Genome, GenomeError>
    load(const std::string& name, uint64_t version) = 0;

virtual std::expected<CommitResult, GenomeError>
    commit(const Genome& genome) = 0;

virtual std::expected<CommitResult, GenomeError>
    fork(const std::string& name, uint64_t parent_version,
         const GenomeSpec& mutations) = 0;

virtual std::expected<std::vector<uint64_t>, GenomeError>
    list_versions(const std::string& name) = 0;

virtual std::expected<GenomeDiff, GenomeError>
    diff(const std::string& name, uint64_t v1, uint64_t v2) = 0;

virtual std::expected<LineageWalk, GenomeError>
    walk_ancestors(const std::string& name, uint64_t from_version,
                   std::optional<uint64_t> to_version = std::nullopt) {
    (void)name; (void)from_version; (void)to_version;
    return std::unexpected(GenomeError::NotImplemented);
}
```

具体实现 `FilesystemGenomeRegistry` MUST override 这 6 个虚函数,使用 `std::expected` 工厂语法。

#### Scenario: FilesystemGenomeRegistry::load 成功
- **WHEN** `load(name@version)` 找到 existing version 且 HMAC 校验通过
- **THEN** MUST 返回 `std::expected<Genome, GenomeError>{std::in_place, std::move(g)}`

#### Scenario: FilesystemGenomeRegistry::load 失败
- **WHEN** `load(name@version)` 检测到 NotFound / SchemaViolation / IntegrityViolation / BrokenLineage / IOError
- **THEN** MUST 返回 `std::unexpected(GenomeError::NotFound)` 或对应错误
- **AND** MUST NOT 抛异常(契约要求)

#### Scenario: walk_ancestors 默认实现
- **WHEN** concrete override 不存在或调用 default impl
- **THEN** MUST 返回 `std::unexpected(GenomeError::NotImplemented)`(per `ig-genome-registry-walk-ancestors` spec 默认实现)
