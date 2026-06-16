// examples/phase1_plugin_demo/main.cpp
// 文件头注释
// 功能描述：Phase 1 Sprint 0 端到端 demo (Plugin Stub 触发)
//          模拟 phase1_model_router_plugin Plugin 决策 + MockLLMProvider 调用
//          Sprint 5 PluginLoader 真实 .so 加载时, 此 demo 扩展为 dlopen 路径
// 设计依据：phase1-execution.md §Sprint 0
// 作者：AgenticDSL Phase 1 / Sprint 0
// 最后修改日期：2026-06-16

#include "common/llm/llm_types.h"
#include "common/llm/mock_provider.h"

#include <iostream>
#include <stop_token>
#include <string>

int main(int argc, char** argv) {
  const bool mock_mode = (argc > 1 && std::string(argv[1]) == "--mock");
  if (!mock_mode) {
    std::cerr << "Usage: " << argv[0] << " --mock\n"
              << "  Sprint 0 Plugin Stub demo 仅支持 --mock 模式\n";
    return 1;
  }

  std::cout << "[phase1_plugin_demo] Sprint 0 Plugin Stub 端到端验证\n";

  // 1. Plugin 决策: 调用 MockLLMProvider::available_models() 拿到模型列表
  agenticdsl::MockLLMProvider provider;
  const auto models = provider.available_models();
  std::cout << "  - available models: " << models.size() << "\n";
  for (const auto& m : models) {
    std::cout << "    * " << m.name << " (provider=" << m.provider
              << ", ctx=" << m.context_window << ")\n";
  }

  if (models.empty()) {
    std::cerr << "FATAL: no models available, MockLLMProvider::available_models() returned empty\n";
    return 1;
  }

  // 2. Plugin Stub 模拟: 选定第一个 Chat-capable 模型 (与 ModelRouterPolicy.route() 一致)
  const auto& selected = models.front();
  std::cout << "  - Plugin decision: routed to " << selected.name << "\n";

  // 3. 调用 MockLLMProvider::generate() 验证集成
  provider.set_fixed_response("ModelRouter Plugin Stub 决策已执行");
  agenticdsl::GenerationRequest req("test prompt");
  std::stop_token token;
  auto result = provider.generate(req, token);

  if (result.has_value()) {
    std::cout << "  - generate() output: " << result.value().text << "\n";
  } else {
    std::cerr << "FATAL: generate() returned error code="
              << static_cast<int>(result.error().code) << "\n";
    return 1;
  }

  std::cout << "  - demo complete (Plugin Stub 验证通过, Sprint 5 将扩展为真实 .so 加载)\n";
  return 0;
}
