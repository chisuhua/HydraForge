// examples/agent_loop/agent_loop.cpp
// 文件头注释
// 功能描述：多轮 LLM 调用循环演示 (MockLLMProvider 模式，无真实模型依赖)
//          流程：DSLEngine::from_file() 加载 initial.md
//              → 配置 MockLLMProvider 队列响应（每轮一个预设 DSL 块）
//              → 循环 run()，每次 llm_call 节点消耗一个队列响应
//              → 演示 continue_with_generated_dsl() 合并运行时生成的 DSL
//              → 打印 ExecutionResult 与累计 session cost
// 设计依据：openspec/changes/examples-mockllm-migration/proposal.md §2
//          Decision 3: 重写 build_prompt() helper (替代已删除的 PromptBuilder)
// 作者：AgenticDSL Sprint 19
// 最后修改日期：2026-06-30

#include "core/engine.h"
#include "common/llm/mock_provider.h"
#include "common/llm/llm_types.h"
#include "core/types/context.h"

#include <iostream>
#include <memory>
#include <string>

namespace {

// 构建包含历史与上下文的 prompt (替代已删除的 PromptBuilder::inject_libraries_into_prompt)
// 简化版：序列化 Context JSON + 拼接 base + 历史
std::string build_prompt(const std::string& base, const agenticdsl::Context& ctx) {
    std::string result = base;
    result += "\n\n=== Current Context ===\n";
    result += ctx.dump(2);
    if (ctx.contains("history") && ctx["history"].is_array()) {
        result += "\n\n=== History ===\n";
        for (const auto& entry : ctx["history"]) {
            result += "- " + entry.dump() + "\n";
        }
    }
    return result;
}

} // namespace

int main(int argc, char** argv) {
    std::cout << "[INFO] agent_loop: MockLLMProvider mode (no model weights required)\n";

    const std::string dsl_path = (argc > 1) ? argv[1] : "initial.md";

    try {
        // 1) 加载初始 DSL（必须包含 /main 子图）
        auto engine = agenticdsl::DSLEngine::from_file(dsl_path);
        if (!engine) {
            std::cerr << "[ERROR] DSLEngine::from_file returned null\n";
            return 1;
        }

        // 2) 配置 MockLLMProvider 队列响应 — 每次 llm_call 消耗一个
        auto* mock = dynamic_cast<agenticdsl::MockLLMProvider*>(
            engine->get_llm_provider());
        if (!mock) {
            std::cerr << "[ERROR] engine is not using MockLLMProvider\n";
            return 1;
        }
        // 轮次 1: 演示 "15 + 27" 计算
        mock->enqueue_response(
            std::string("The answer to 15 + 27 is 42."));
        // 轮次 2: 演示后续查询
        mock->enqueue_response(
            std::string("Beijing weather: clear skies, 22C."));
        // 兜底 fixed response
        mock->set_fixed_response(
            std::string("(mock fallback) LLM response"));

        // 3) 准备初始上下文
        agenticdsl::Context ctx;
        ctx["user_input"] = std::string("Calculate 15 + 27 and then get the weather in Beijing.");
        ctx["history"] = nlohmann::json::array();

        const std::string base_prompt =
            "You are an AI agent that generates AgenticDSL code to continue the workflow.";

        // 4) 循环执行
        const int max_steps = 3;
        int step = 0;
        bool completed = false;

        for (step = 0; step < max_steps; ++step) {
            std::cout << "\n=== Agent Step " << (step + 1) << " ===\n";

            // 4a) 执行当前 DAG
            auto result = engine->run(ctx);
            ctx = result.final_context;

            if (!result.success) {
                std::cerr << "[FAIL] Execution failed: " << result.message << "\n";
                break;
            }

            std::cout << "[OK] Step " << (step + 1) << " completed.\n";

            // 4b) 演示 build_prompt + MockLLMProvider 调用 (替代 PromptBuilder + get_llm_adapter)
            agenticdsl::GenerationRequest req;
            req.prompt = build_prompt(base_prompt, ctx);
            // stop_token 默认构造表示不取消
            auto llm_result = engine->get_llm_provider()->generate(req, std::stop_token{});

            if (llm_result.has_value()) {
                std::cout << "[LLM] Response: " << llm_result.value().text << "\n";
                nlohmann::json entry;
                entry["step"] = step + 1;
                entry["prompt_preview"] = req.prompt.substr(0, 80) + "...";
                entry["response"] = llm_result.value().text;
                ctx["history"].push_back(entry);

                // 4c) 演示 continue_with_generated_dsl: 让引擎继续解析生成内容
                // 在 MockLLMProvider 模式下，生成的 DSL 仅作为示例记录，不强制要求继续触发新的 llm_call
                // 真实 ReAct 循环中，此处将 LLM 输出解析为 ParsedGraph 后 append_graphs()
                engine->continue_with_generated_dsl(llm_result.value().text);
            } else {
                std::cerr << "[LLM] Error: " << llm_result.error().message << "\n";
                break;
            }

            // 4d) 检查是否完成 — 演示目的，max_steps 后退出
            if (step + 1 >= max_steps) {
                completed = true;
                std::cout << "\n[INFO] Reached max_steps (" << max_steps << "). Terminating loop.\n";
            }
        }

        // 5) 总结
        std::cout << "\n=== Summary ===\n";
        std::cout << "Steps executed: " << (step) << "\n";
        std::cout << "MockLLMProvider call count: " << mock->call_count() << "\n";
        std::cout << "Session cost (USD): " << engine->get_session_cost() << "\n";
        std::cout << "Final context keys: ";
        for (auto it = ctx.begin(); it != ctx.end(); ++it) {
            std::cout << it.key() << " ";
        }
        std::cout << "\n";

        return completed ? 0 : 1;

    } catch (const std::exception& e) {
        std::cerr << "[FATAL] " << e.what() << "\n";
        return 1;
    }
}