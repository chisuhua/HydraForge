# unify-result-to-std-expected — Audit Baseline

**Date**: 2026-10-08T16:30Z (UTC)
**Verifier**: Implementation phase setup
**Status**: Baseline established — implementation may proceed

## Raw counts (实测 grep)

| 类别 | 数量 | 说明 |
|---|---|---|
| `Result<X,Y>::success` (all forms, all namespaces) | **25** | LLM + Genome + tests combined |
| `Result<X,Y>::failure` (all forms, all namespaces) | **85** | LLM + Genome + tests combined |
| `Result<X,Y>::{success,failure}` 合计 (含 namespace 前缀) | **110** | 必须替换 |
| `ToolResult::success` | 16 | **MUST NOT replace** (不同类型, 完全不相关) |
| `ToolResult::failure` | 0 | **MUST NOT replace** |
| `AgentResult<T>` 模板实例化 | 11 | **out of scope** (iagent_composition.h) |
| 简写形式 `Result::success` (无模板实参) | 0 | 全工作区 = 0 |
| 简写形式 `Result::failure` (无模板实参) | 1 | 在 `src/common/llm/rate_limit_decorator.h:32` 注释中 (非代码) |

**注**: proposal.md 估计 "129 处" 实测 110 处,差额来自：(a) genome::Result<X,Y>::success 实际 = 0 (Genome 实现用 inline factory)；(b) 文档/注释不算 grep 实际命中。

## 拆分（按 namespace）

| namespace | ::success | ::failure | 合计 |
|---|---|---|---|
| `agenticdsl::Result` (or bare `Result<>` in agenticdsl) | 25 | 67 | 92 |
| `agenticdsl::genome::Result` (含 `::agenticdsl::genome::Result`) | 0 | 18 | 18 |
| **Total** | 25 | 85 | 110 |

## 拆分（按位置）

| 区域 | ::success | ::failure | 备注 |
|---|---|---|---|
| `src/common/llm/` (LLM provider 链) | 7 | 13 | `cloud_adapter` / `http_adapter` / `finetune_provider` / `mock_provider` / `llama_adapter_provider` / `illmprovider_decorator` |
| `src/common/governance/cross_cutting/decorator_pattern.cpp` | 1 | 0 | 单一 success |
| `src/core/genome/registry_filesystem.cpp` | 5 | 0 | 全部 success, failure 走 enum 直接返回 |
| `src/evolution/harness_rsi.cpp` | 1 | 0 | `Result<AppliedMutation, MutationError>::success` |
| `tests/` (涉及 Result 用法 binary) | 13 | 18 | 16 binary, 详见 tasks.md 4.1 列表 |
| **Total** | 25 | 85 | 110 |

## 关键 MUST-NOT 误伤清单

| 标识 | 数量 | 误伤风险 | 措施 |
|---|---|---|---|
| `ToolResult::success` | 16 | sed `\.success\(` 会全命中 | 用 `Result<...>::success` 锚定模式 (前缀 `Result<`) |
| `AgentResult<T>` 模板实例化 | 11 | 不同类型, sed 不命中 | grep 模式 `Result<` 锚定, `AgentResult<` 不会被匹配 (因为 `Result<` 前缀缺失) |
| `Result::failure` 简写形式 (1 处注释) | 1 | 注释非代码, 但应清理 | Phase 6 文档审计处理 |

## sed 模式验证 (锁定)

```bash
# Success 替换 (Result<...>::success 必须锚定 Result< 前缀)
sed -i 's/Result<\([^>]*\), \([^>]*\)>::success(/std::expected<\1, \2>{std::in_place, std::move(/g' <file>

# Failure 替换
sed -i 's/Result<\([^>]*\), \([^>]*\)>::failure(/std::unexpected(std::move(/g' <file>

# 处理闭合: 每个 success 替换后原尾部 `)` 需手动调整
# 例: Result<X, Y>::success(value);  → std::expected<X, Y>{std::in_place, std::move(value)});
#     注意原 `);` 改成 `)});` (多一层 `)` 因 `std::move(...)` 增加)
```

## Baseline ctest 验证

```
ctest --test-dir build -LE must_realllm --output-on-failure
263/263 tests passed, 0 tests failed
Total Test time: 64.74s
```

**drop_ratio baseline = 0%** (target for post-ship ctest 同样 263/263)
