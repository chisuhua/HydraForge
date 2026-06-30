# Design: 迁移 examples 到 MockLLMProvider

## Context

`examples/agent_simple/` 和 `examples/agent_loop/` 从 2026-06-13 docs-code-drift-audit 发现 4 个编译错误以来 17 天未修复。已 ship 的 `examples/slice_01_tool_call/` 和 `examples/phase1_*` 演示 MockLLMProvider 模式，可作为模板参考。

## Goals / Non-Goals

**Goals:**
- 修复 2 个 examples 编译错误
- 演示 MockLLMProvider 模式 (不依赖真实模型权重)
- 保持 example 简洁可读 (新贡献者入门用)
- AGENTS.md line 46 移除审计债注释

**Non-Goals:**
- 不引入真 LLM 依赖
- 不重写 `examples/agent_basic/`
- 不修改 DSLEngine / MockLLMProvider 公开 API
- 不添加新 example

## Decisions

### Decision 1: CMakeLists.txt 模板 — 复用 `slice_01_tool_call`
**选择**: 完全复制 `examples/slice_01_tool_call/CMakeLists.txt` 模板，仅修改 executable 名称和依赖模块。

```cmake
# examples/agent_simple/CMakeLists.txt
add_executable(agent_simple simple.cpp)
target_link_libraries(agent_simple
    agenticdsl_includes
    agenticdsl_core
    agenticdsl_modules_cognitive  # SimpleCognitiveOrchestrator
)
```

**替代方案**:
- 引入 `examples/CMakeLists.txt` 统一管理 — 范围扩大，需要修改根 CMakeLists.txt
- 使用 add_subdirectory 复用 slice_01 — 增加耦合，不利于独立演进

**结论**: 直接复制模板最简单，符合现有 examples 模式。

### Decision 2: `simple.cpp` 重写策略 — MockLLMProvider + 删除旧 API
**当前问题代码**:
- `agenticdsl::LlamaAdapter` (global namespace, 不在 agenticdsl 中)
- `agenticdsl::InjaTemplateRenderer::render()` 仍可工作
- `agenticdsl::extract_pathed_blocks()` 仍可工作
- `from_markdown(content, context)` 2 参数调用错误

**选择**:
```cpp
// 新 simple.cpp 模式 (基于 examples/slice_01_tool_call/main.cpp:53-69)
#include "common/llm/llm_types.h"
#include "common/llm/mock_provider.h"
#include "core/engine.h"
// ...

int main() {
    agenticdsl::DSLEngine engine;
    // MockLLMProvider 替代 LlamaAdapter
    auto mock_provider = std::make_shared<agenticdsl::MockLLMProvider>(...);
    engine.set_llm_provider(mock_provider);
    
    // 正确单参数调用
    auto graphs = agenticdsl::DSLEngine::from_markdown(aggregated_content);
    // ...
}
```

**关键 API 变化**:
- 移除 `agenticdsl::LlamaAdapter` (使用 MockLLMProvider)
- `from_markdown(content, context)` → `from_markdown(content)` (单参数)
- 工具注册通过 `engine.register_tool()` (Sprint 1b IToolRegistry 重构后)

### Decision 3: `agent_loop.cpp` 重写策略 — 删 PromptBuilder + 新 build_prompt helper
**当前问题代码**:
- `agenticdsl::PromptBuilder::inject_libraries_into_prompt()` 已删除
- `engine->get_llm_adapter()` 已删除，改用 `get_llm_provider()`
- 完整 PromptBuilder 功能无法恢复

**选择**:
```cpp
// 新 agent_loop.cpp 模式
namespace {
// 新 build_prompt helper (替代 PromptBuilder)
std::string build_prompt(const std::string& base, const Context& ctx) {
    // 简化版: 序列化 Context JSON + 拼接 base
    return base + "\n\nContext:\n" + ctx.dump(2);
}
}

int main() {
    agenticdsl::DSLEngine engine;
    // ...
    for (const auto& user_input : inputs) {
        // 旧: auto new_dsl = engine->get_llm_adapter()->generate(prompt);
        // 新:
        std::string prompt = build_prompt(base_prompt, current_context);
        auto response = engine.get_llm_provider()->generate({prompt, {}});
        // 解析 response 提取新 DSL...
    }
}
```

**关键 API 变化**:
- 移除 `agenticdsl::PromptBuilder` 引用
- `engine->get_llm_adapter()->generate(prompt)` → `engine.get_llm_provider()->generate({prompt, params})`
- 简化 `build_prompt()` 实现 (原 PromptBuilder 功能在 examples 上下文不必要)

### Decision 4: 不引入新依赖
**选择**: 仅使用 `agenticdsl_core` + `agenticdsl_modules_cognitive` (已存在)。

**结论**: 零新依赖。

## Risks / Trade-offs

| Risk | Mitigation |
|------|-----------|
| simple.cpp 重写丢失原示例教学价值 | 保留 tool call + DSL 解析 + 单轮 ReAct 核心流程 |
| agent_loop.cpp 新 build_prompt 比原 PromptBuilder 弱 | examples 上下文足够；生产用 PromptBuilder 重构超出 scope |
| MockLLMProvider 配置复杂 (model + responses) | 参考 `examples/slice_01_tool_call/main.cpp:53-69` 模板 |
| examples 启动后输出与 docs/superpowers/plans/ 描述不一致 | 更新 AGENTS.md 描述 + 启动输出 INFO 提示 |

## Migration Plan

### Step 1: simple.cpp CMakeLists + 重写
1. 创建 `examples/agent_simple/CMakeLists.txt`
2. 重写 `examples/agent_simple/simple.cpp` (基于 MockLLMProvider)
3. 编译验证
4. 启动 + 简单输出

### Step 2: agent_loop.cpp CMakeLists + 重写
1. 创建 `examples/agent_loop/CMakeLists.txt`
2. 重写 `examples/agent_loop/agent_loop.cpp` (删除 PromptBuilder)
3. 编译验证
4. 启动 + 简单输出

### Step 3: AGENTS.md 同步
1. 移除 line 46 审计债注释
2. 更新 `examples/` 描述表
3. 验证 `python3 tools/adr_lint.py` 仍然 exit 0

### Rollback
revert 2 个 commit 即可 (simple + agent_loop)。无生产代码影响。

## Open Questions

无 — 所有 API 决策基于 Sprint 17 已 ship 模式 (MockLLMProvider + IToolRegistry + DSLEngine 公开 API)。