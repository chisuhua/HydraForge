// examples/slice_01_tool_call/main.cpp
// 文件头注释
// 功能描述：Track 0.2 slice_01 端到端示例（--mock 模式完整版）。
//          流程：DSLEngine::from_markdown()（自动 MockLLMProvider）
//              → 注册 echo 工具
//              → 配置 MockLLMProvider 响应
//              → 创建 SimpleCognitiveOrchestrator
//              → 调用 process() 触发单轮 ReAct
//              → 打印 ToolResult JSON
// 设计依据：plan §10
// 作者：AgenticDSL Phase 0 / Track B
// 最后修改日期：2026-06-08

#include "core/engine.h"
#include "modules/cognitive/simple_orchestrator.h"
#include "core/types/tool_result.h"
#include "common/llm/mock_provider.h"
#include "common/llm/llm_types.h"

#include <iostream>
#include <string>

namespace {

const std::string kDemoDsl = R"(
### AgenticDSL `/main`
```yaml
# --- BEGIN AgenticDSL ---
graph_type: subgraph
nodes:
  - id: start
    type: start
    next: ["/main/end"]
  - id: end
    type: end
# --- END AgenticDSL ---
```
)";

const char* kMockResponse =
    R"({"tool":"echo","args":{"message":"hello from mock"}})";

} // namespace

int main(int argc, char** argv) {
  bool mock_mode = (argc > 1 && std::string(argv[1]) == "--mock");
  if (!mock_mode) {
    std::cerr << "Usage: " << argv[0] << " --mock\n";
    return 1;
  }

  // 1) 创建 engine（自动使用 MockLLMProvider）
  auto engine = agenticdsl::DSLEngine::from_markdown(kDemoDsl);

  // 2) 注册 echo 工具（覆盖默认 web_search/get_weather/calculate）
  engine->register_tool(
      "echo",
      [](const std::unordered_map<std::string, std::string>& args)
          -> nlohmann::json {
        auto it = args.find("message");
        std::string msg = (it != args.end()) ? it->second : "";
        return nlohmann::json{{"echoed", msg}};
      });

  // 3) 获取 MockLLMProvider 并配置响应
  auto* mock = dynamic_cast<agenticdsl::MockLLMProvider*>(
      engine->get_llm_provider());
  if (mock) {
    mock->set_fixed_response(kMockResponse);
  } else {
    std::cerr << "WARN: engine is not using MockLLMProvider (got: "
              << typeid(*engine->get_llm_provider()).name() << ")\n";
  }

  // 4) 创建 orchestrator
  agenticdsl::SimpleCognitiveOrchestrator orch(
      &engine->get_tool_registry(), engine->get_llm_provider());

  // 5) 执行并打印结果
  bool done = false;
  orch.process("demo-session", [&](agenticdsl::ToolResult r) {
    std::cout << "ToolResult: " << r.to_json().dump(2) << "\n";
    done = true;
  });

  return done ? 0 : 1;
}