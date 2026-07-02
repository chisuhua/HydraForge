// examples/agent_simple/simple.cpp
// 文件头注释
// 功能描述：单轮 ReAct 演示 (MockLLMProvider 模式，无真实模型依赖)
//          流程：DSLEngine::from_markdown() 自动使用 MockLLMProvider
//              → 注册 calculate 工具
//              → 配置 MockLLMProvider 响应 (llm_call 节点)
//              → run() 执行 start → prepare → compute (tool_call) → ask_llm (llm_call) → end
//              → 打印 ExecutionResult
// 设计依据：openspec/changes/examples-mockllm-migration/proposal.md §1
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

// 1) 演示 DSL (单轮 DAG): start → prepare → compute → ask_llm → end
const std::string kDemoDsl = R"(
### AgenticDSL `/main`
```yaml
# --- BEGIN AgenticDSL ---
graph_type: subgraph
nodes:
  - id: start
    type: start
    next: [prepare]
  - id: prepare
    type: assign
    assign:
      num1: "15"
      num2: "27"
    next: [compute]
  - id: compute
    type: tool_call
    tool: calculate
    arguments:
      a: "{{ num1 }}"
      b: "{{ num2 }}"
      op: "+"
    output_keys: "result"
    next: [ask_llm]
  - id: ask_llm
    type: llm_call
    prompt_template: "The calculation result is {{ result.result }}. Please explain it."
    output_keys: "explanation"
    next: [end]
  - id: end
    type: end
# --- END AgenticDSL ---
```
)";

// 2) MockLLMProvider 对 llm_call 节点的固定响应
const std::string kMockLlmResponse = "The sum 15 + 27 = 42. Forty-two is a nice round number.";

} // namespace

int main() {
    std::cout << "[INFO] agent_simple: MockLLMProvider mode (no model weights required)\n";

    try {
        // 1) 创建 engine（自动使用 MockLLMProvider）
        auto engine = agenticdsl::DSLEngine::from_markdown(kDemoDsl);
        if (!engine) {
            std::cerr << "[ERROR] DSLEngine::from_markdown returned null\n";
            return 1;
        }

        // 2) 注册 calculate 工具
        engine->register_tool(
            "calculate",
            agenticdsl::ToolMetadata{"calculate", "Calculate arithmetic expression", "example",
                agenticdsl::ToolCategory::ReadOnly, agenticdsl::LayerProfile::Workflow},
            [](const std::unordered_map<std::string, std::string>& args) -> nlohmann::json {
                auto a_it = args.find("a");
                auto b_it = args.find("b");
                auto op_it = args.find("op");
                if (a_it == args.end() || b_it == args.end()) {
                    return nlohmann::json{{"error", "missing a/b argument"}};
                }
                int a = std::stoi(a_it->second);
                int b = std::stoi(b_it->second);
                std::string op = (op_it != args.end()) ? op_it->second : "+";

                int result = 0;
                if (op == "+") result = a + b;
                else if (op == "-") result = a - b;
                else if (op == "*") result = a * b;
                else if (op == "/" && b != 0) result = a / b;
                else return nlohmann::json{{"error", "unsupported op: " + op}};

                return nlohmann::json{{"result", result}};
            });

        // 3) 获取 MockLLMProvider 并配置响应 (拦截 llm_call)
        auto* mock = dynamic_cast<agenticdsl::MockLLMProvider*>(
            engine->get_llm_provider());
        if (!mock) {
            std::cerr << "[ERROR] engine is not using MockLLMProvider\n";
            return 1;
        }
        mock->set_fixed_response(kMockLlmResponse);

        // 4) 准备初始 Context
        agenticdsl::Context ctx;
        ctx["task"] = std::string("Calculate 15 + 27 and explain the result.");

        // 5) 执行 DAG
        auto result = engine->run(ctx);

        // 6) 打印结果
        if (result.success) {
            std::cout << "[OK] Engine ready. Execution succeeded.\n";
            std::cout << "Final context:\n" << result.final_context.dump(2) << "\n";
        } else {
            std::cerr << "[FAIL] " << result.message << "\n";
            return 1;
        }

    } catch (const std::exception& e) {
        std::cerr << "[FATAL] " << e.what() << "\n";
        return 1;
    }

    return 0;
}