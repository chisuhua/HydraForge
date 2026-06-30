# Proposal: 迁移 examples 到 MockLLMProvider (Examples MockLLM Migration)

> **STATUS: PROPOSAL** — 2026-06-30 全项目审计 D-2 (17 天已知未跟踪债务)

## Why

`examples/agent_simple/` 和 `examples/agent_loop/` 两个示例程序从 2026-06-13 审计发现 4 个编译错误以来已经 17 天未修复，AGENTS.md line 46 明确承认这是已知债务但未创建 OpenSpec change 跟踪。

**具体问题**:
1. `examples/agent_simple/` 和 `examples/agent_loop/` 整个目录**无 CMakeLists.txt** — 不可构建
2. `examples/agent_simple/simple.cpp` 编译错误:
   - 引用不存在的 `common/utils.h` (实际在 `common/utils/parser_utils.h`)
   - 错误调用 `from_markdown(aggregated_dsl_content, agent_context)` 2 参数签名
3. `examples/agent_loop/agent_loop.cpp` 编译错误:
   - 引用已删除的 `agenticdsl::PromptBuilder::inject_libraries_into_prompt()`
   - 调用已删除的 `DSLEngine::get_llm_adapter()`

**影响**:
- 任何新贡献者参考 `examples/` 时立即编译失败 → onboarding 障碍
- `docs/superpowers/plans/` 引用 `examples/agent_basic/` 作为 reference，但 `agent_simple` + `agent_loop` 是用户友好的入门示例
- 7+ 月审计报告的债，新债未 ship → 文档说"已修复"但实际未修复

## What Changes

### 1. 重新启用 `examples/agent_simple/`
- 创建 `examples/agent_simple/CMakeLists.txt` (参考 `slice_01_tool_call` 模板)
- 重写 `simple.cpp` 使用 MockLLMProvider 模式
- 移除 `LlamaAdapter` 引用，改用 `MockLLMProvider`
- 修复 `from_markdown()` 2 参数调用
- 验证可编译运行

### 2. 重新启用 `examples/agent_loop/`
- 创建 `examples/agent_loop/CMakeLists.txt`
- 重写 `agent_loop.cpp` 移除 `PromptBuilder` 依赖
- 用 MockLLMProvider 替代 `get_llm_adapter()` 链式调用
- 实现新的 `build_prompt()` 工具函数 (in-file helper)
- 验证可编译运行

### 3. 更新 AGENTS.md
- 移除 line 46 的审计债注释 (now resolved)
- 在 `examples/` 表中标记两个新启用的 example

## Capabilities

### New Capabilities
无新增能力。

### Modified Capabilities
无现有 spec 变更 (这是 examples 修复而非 spec 级别)。

## Impact

| 维度 | 影响 |
|------|------|
| 源代码变更 | 2 个 .cpp 重写 + 2 个 CMakeLists.txt 新建 (~150 行总) |
| 测试变更 | examples 编译即通过 (无需 ctest，examples 独立二进制) |
| 行为变更 | 无 (examples 仅演示) |
| API 变更 | 无 (DSLEngine API 不变) |
| 文档更新 | AGENTS.md line 46 移除 + `examples/` 表更新 |

## Non-goals

- **不重写 `examples/agent_basic/`** — 已 ship 并可编译，不在本 change 范围
- **不迁移到真 LLM 后端** — 演示用 MockLLMProvider 已足够
- **不添加新 example** — 仅修复现有 2 个
- **不重命名文件** — 保持 `agent_simple` / `agent_loop` 命名

## Estimated Effort

- CMakeLists.txt 模板 (2 个): 0.5 小时
- simple.cpp 重写: 0.5 天 (MockLLMProvider 集成 + 修复 4 个编译错误)
- agent_loop.cpp 重写: 1 天 (删除 PromptBuilder + 新 build_prompt + 修复 get_llm_adapter)
- 验证编译 + 文档更新: 0.25 天

**总计**: ~1.5-2 天 (1 个 Sprint 工作日)

## Test Strategy

- `cmake --preset debug -DAGENTICDSL_BUILD_EXAMPLES=ON` 通过
- 编译 2 个 example 二进制: 0 错误
- 启动 + 简单输出验证 (无真实 LLM 调用，使用 mock 数据)
- 无需 ctest (examples 是独立二进制)
- 完整 ctest 48/48 PASS 不变