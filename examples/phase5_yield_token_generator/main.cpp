// examples/phase5_yield_token_generator/main.cpp
// Phase 5 Stage 1 Step 2: YIELD/STREAM 端到端示例
// --mock: 用 MockLLMProvider 注入 N 个 token, 验证 CONTINUE 模式拉取全流程

#include "core/engine.h"
#include "common/llm/mock_provider.h"
#include "core/types/context.h"

#include <iostream>
#include <string>
#include <vector>

namespace {

const std::string kYieldDsl = R"(
### AgenticDSL `/main`
```yaml
# --- BEGIN AgenticDSL ---
graph_type: subgraph
nodes:
  - id: start
    type: start
    next: ["/main/yield_node"]
  - id: yield_node
    type: yield
    yield_value: "Generate tokens for prompt"
    mode: continue
    next: ["/main/end_node"]
  - id: end_node
    type: end
# --- END AgenticDSL ---
```
)";

void print_usage(const char* prog) {
  std::cerr << "Usage: " << prog << " --mock [--tokens N]\n"
            << "  --mock    Required: enable mock LLM provider mode\n"
            << "  --tokens N  Number of tokens to generate (default 5)\n";
}

}  // namespace

int main(int argc, char** argv) {
  bool mock_mode = false;
  int token_count = 5;
  for (int i = 1; i < argc; ++i) {
    std::string arg = argv[i];
    if (arg == "--mock") {
      mock_mode = true;
    } else if (arg == "--tokens" && i + 1 < argc) {
      token_count = std::stoi(argv[++i]);
    } else if (arg == "--help" || arg == "-h") {
      print_usage(argv[0]);
      return 0;
    }
  }
  if (!mock_mode) {
    print_usage(argv[0]);
    return 1;
  }

  auto engine = agenticdsl::DSLEngine::from_markdown(kYieldDsl);

  auto* mock = dynamic_cast<agenticdsl::MockLLMProvider*>(engine->get_llm_provider());
  if (!mock) {
    std::cerr << "FAIL: expected MockLLMProvider\n";
    return 1;
  }

  std::vector<std::string> tokens;
  for (int i = 0; i < token_count; ++i) {
    tokens.push_back("token-" + std::to_string(i));
  }
  mock->set_stream_tokens(tokens);

  agenticdsl::LayeredContext initial_ctx;
  agenticdsl::ExecutionResult result = engine->run(initial_ctx);

  if (!result.success) {
    std::cerr << "FAIL: execution unsuccessful: " << result.message << "\n";
    return 1;
  }

  std::string expected_concat;
  for (const auto& t : tokens) expected_concat += t;

  const auto& ctx = result.final_context;
  if (!ctx.contains("__yield__") || !ctx.contains("__yield_mode__")) {
    std::cerr << "FAIL: missing __yield__/__yield_mode__ in final context\n";
    return 1;
  }

  std::string got = ctx["__yield__"];
  std::string mode = ctx["__yield_mode__"];

  if (mode != "CONTINUE") {
    std::cerr << "FAIL: expected mode CONTINUE, got " << mode << "\n";
    return 1;
  }

  if (got != expected_concat) {
    std::cerr << "FAIL: token concat mismatch\n"
              << "  expected: " << expected_concat << "\n"
              << "  got:      " << got << "\n";
    return 1;
  }

  std::cout << "OK phase5_yield_token_generator: " << token_count
            << " tokens, mode=" << mode << ", concat=" << got << "\n";
  return 0;
}
