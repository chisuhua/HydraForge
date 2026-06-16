// examples/phase1_model_router_plugin/main.cpp
// 文件头注释
// 功能描述：Phase 1 Sprint 0 ModelRouter Plugin Stub 二进制 (K1 决策)
//          Plugin 层独立可执行, 非 .so 加载 (避免循环依赖 Sprint 4/5)
//          演示 ModelRouterPolicy 路由决策: 第一个 Chat-capable 模型
// 设计依据：phase1-execution.md §Sprint 0 (K1)
// 作者：AgenticDSL Phase 1 / Sprint 0
// 最后修改日期：2026-06-16

#include "common/llm/llm_types.h"
#include "common/llm/mock_provider.h"

#include <algorithm>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace agenticdsl {
namespace plugin_stub {

// ModelRouterPolicy: 第一个 Chat-capable 模型路由
// Sprint 5 PluginLoader 实现后, 此 Policy 逻辑可迁移到 PDK Plugin
class ModelRouterPolicy {
 public:
  /// 路由决策: 选择第一个支持 Chat 的 ModelInfo
  /// @throws std::runtime_error 当 available_models() 为空或无 Chat-capable 模型
  static ILLMProvider::ModelInfo route(const ILLMProvider& provider) {
    const auto models = provider.available_models();
    if (models.empty()) {
      throw std::runtime_error("no models available from provider");
    }

    auto it = std::find_if(models.begin(), models.end(),
                           [](const ILLMProvider::ModelInfo& m) {
                             return std::any_of(
                                 m.capabilities.begin(),
                                 m.capabilities.end(),
                                 [](ILLMProvider::ModelCapability c) {
                                   return c == ILLMProvider::ModelCapability::Chat;
                                 });
                           });
    if (it == models.end()) {
      throw std::runtime_error("no Chat-capable model available");
    }
    return *it;
  }
};

}  // namespace plugin_stub
}  // namespace agenticdsl

int main(int argc, char** argv) {
  // --mock 参数检查
  const bool mock_mode = (argc > 1 && std::string(argv[1]) == "--mock");
  if (!mock_mode) {
    std::cerr << "Usage: " << argv[0] << " --mock\n"
              << "  Sprint 0 Plugin Stub 仅支持 --mock 模式 (MockLLMProvider)\n";
    return 1;
  }

  // Plugin Stub 触发: 创建 MockLLMProvider, 调用 available_models(),
  // 应用 ModelRouterPolicy.route() 决策
  agenticdsl::MockLLMProvider provider;

  try {
    auto selected = agenticdsl::plugin_stub::ModelRouterPolicy::route(provider);

    // 打印路由决策
    std::cout << "[phase1_model_router_plugin] ModelRouter Stub 验证\n"
              << "  - total models: " << provider.available_models().size() << "\n"
              << "  - selected: " << selected.name << "\n"
              << "  - provider: " << selected.provider << "\n"
              << "  - context_window: " << selected.context_window << "\n"
              << "  - capabilities: ";
    for (size_t i = 0; i < selected.capabilities.size(); ++i) {
      if (i > 0) std::cout << ", ";
      switch (selected.capabilities[i]) {
        case agenticdsl::ILLMProvider::ModelCapability::Chat:
          std::cout << "Chat"; break;
        case agenticdsl::ILLMProvider::ModelCapability::Completion:
          std::cout << "Completion"; break;
        case agenticdsl::ILLMProvider::ModelCapability::Embedding:
          std::cout << "Embedding"; break;
        case agenticdsl::ILLMProvider::ModelCapability::ToolUse:
          std::cout << "ToolUse"; break;
        case agenticdsl::ILLMProvider::ModelCapability::Vision:
          std::cout << "Vision"; break;
      }
    }
    std::cout << "\n  - routed to model: " << selected.name << "\n";

    return 0;
  } catch (const std::exception& e) {
    std::cerr << "ModelRouterPolicy failed: " << e.what() << "\n";
    return 1;
  }
}
