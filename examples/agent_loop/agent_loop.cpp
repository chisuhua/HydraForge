#include "core/engine.h"
#include <iostream>
#include <fstream>
#include <sstream>

// 加载初始 DSL 文件
std::string load_file(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        throw std::runtime_error("Cannot open file: " + path);
    }
    std::stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

// ⚠️ ACTUAL STATE NOTE (2026-06-13, OpenSpec change docs-code-drift-audit-2026-06):
// 本文件使用 API 的实际状态（基于 git history 审计）：
//   - `agenticdsl::PromptBuilder`: DELETED，但删除时间早于 DEPRECATED 注释声称的时间
//     - 真删 commit: `9a619f3` (2025-11-05，"update spec to v3.7")
//     - 误归 commit: `ac9e684` (2026-06-09) 只删了 `src/modules/prompts.yaml` 数据文件
//       + 一个引用已不存在 header 的 stub test (该 commit message 自己承认
//       "tests/test_prompt_builder.cpp (8-line stub for non-existent header)")
//   - `engine->get_llm_adapter()`: DELETED — DSLEngine 公共 API 现在是
//     `get_llm_provider()` 返回 `ILLMProvider*`（src/core/engine.h:67）；
//     原 `LlamaAdapter` 类仍存在但 DSLEngine 不再直接暴露它。
//
// 实际编译错误（g++ -c 验证）：
//   1. `examples/agent_loop/agent_loop.cpp:58` `agenticdsl::PromptBuilder` 未声明
//   2. `examples/agent_loop/agent_loop.cpp:95` `DSLEngine::get_llm_adapter` 不存在
//      → 应改 `get_llm_provider()` + `dynamic_cast<MockLLMProvider*>` 模式
//
// 迁移路径：未来 OpenSpec change（独立于本 audit change）将本 example 迁移到
// `MockLLMProvider` + `ILLMProvider` 模式（参考 examples/slice_01_tool_call/main.cpp）。
// 完整修复需重新实现 `build_prompt`（PromptBuilder 已无可替代类）+ 重写 `main()` 的
// LLM 调用部分。范围超出本 audit change（设计决策：保留作为 design history）。

// 构建包含历史和可用库的 prompt
std::string build_prompt(const agenticdsl::Context& ctx, const std::string& paused_at) {
    // 使用 Inja 模板构建更结构化的 prompt
    std::string base_prompt = R"(
You are an AI agent that generates AgenticDSL code to continue the workflow.

Current context:
{{ context | dump(2) }}

You are paused at node: {{ paused_at }}

Available standard libraries:
{% for lib in available_subgraphs %}
- Path: {{ lib.path }}
  {% if lib.signature %}Signature: {{ lib.signature }}{% endif %}
  Permissions: {{ lib.permissions | join(", ") or "none" }}
{% endfor %}

Generate ONLY the next AgenticDSL block(s) in the exact format below. Do NOT explain.

### AgenticDSL `/main/stepX`
```yaml
# --- BEGIN AgenticDSL ---
type: ...
...
# --- END AgenticDSL ---
```
)";

    agenticdsl::Context prompt_ctx;
    prompt_ctx["context"] = ctx;
    prompt_ctx["paused_at"] = paused_at;

    return agenticdsl::PromptBuilder::inject_libraries_into_prompt(base_prompt, prompt_ctx);
}

int main() {
    try {
        // 1. 加载初始 DSL（必须包含 /main 子图）
        auto engine = agenticdsl::DSLEngine::from_file("initial.md");

        // 2. 初始化上下文
        agenticdsl::Context ctx;
        ctx["user_input"] = "Calculate 15 + 27 and then get the weather in Beijing.";
        ctx["history"] = nlohmann::json::array();

        int max_steps = 5; // 防止无限循环
        int step = 0;

        while (step < max_steps) {
            std::cout << "\n--- Agent Step " << (step + 1) << " ---\n";

            // 3. 执行当前 DAG
            auto result = engine->run(ctx);
            ctx = result.final_context;

            if (!result.success) {
                std::cerr << "❌ Execution failed: " << result.message << "\n";
                break;
            }

            // 4. 检查是否暂停（LLM_CALL 节点）
            if (result.paused_at) {
                std::cout << "⏸️  Paused at: " << *result.paused_at << "\n";

                // 5. 构建 prompt（含可用标准库）
                std::string prompt = build_prompt(ctx, *result.paused_at);
                std::cout << "📝 Prompt:\n" << prompt << "\n";

                // 6. 调用 LLM 生成新 DSL
                std::string new_dsl = engine->get_llm_adapter()->generate(prompt);
                std::cout << "🤖 LLM Generated DSL:\n" << new_dsl << "\n";

                // 7. 记录历史
                nlohmann::json history_entry;
                history_entry["prompt"] = prompt;
                history_entry["generated_dsl"] = new_dsl;
                ctx["history"].push_back(history_entry);

                // 8. 【关键】使用 continue_with_generated_dsl 解析并合并
                engine->continue_with_generated_dsl(new_dsl);

                std::cout << "✅ Appended new blocks. Continuing...\n";
                step++;
            } else {
                std::cout << "✅ Workflow completed successfully.\n";
                std::cout << "Final context:\n" << ctx.dump(2) << "\n";
                break;
            }
        }

        if (step >= max_steps) {
            std::cout << "⚠️  Max steps reached. Terminating.\n";
        }

    } catch (const std::exception& e) {
        std::cerr << "💥 Fatal error: " << e.what() << "\n";
        return 1;
    }

    return 0;
}
