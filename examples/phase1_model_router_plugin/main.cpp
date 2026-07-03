// examples/phase1_model_router_plugin/main.cpp
// 功能描述：ModelRouter Plugin 加载演示 (C7 Phase 2 升级)。
//          使用 PluginLoader (Sprint 5) 加载 3 个策略 .so + ModelRegistry .so,
//          演示 call_tool("model_router/cost/quality/latency") 路由决策。
// 模式:
//   --mock    使用 MockLLMProvider + set_available_models() 注入测试模型, 演示 3 策略
//   --list    调用 model_router/registry 打印所有可用模型
// 设计依据：openspec/changes/2026-06-26-adr-0034-model-router-plugin/
//          specs/model-router-plugin/spec.md — model-router-plugin-entry requirement
// 作者：C7 Phase 2

#include "agenticdsl/plugin/plugin_loader.h"
#include "agenticdsl/plugin/plugin_info.h"
#include "agenticdsl/contract/itool_registry.h"
#include "common/llm/mock_provider.h"
#include "common/tools/registry.h"

#include <cstdlib>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

using agenticdsl::MockLLMProvider;
using agenticdsl::ILLMProvider;
using hydraforge::PluginLoader;
using hydraforge::PluginInfo;
using agenticdsl::ToolRegistry;

namespace {

// PluginLoader 路径白名单拒绝相对路径, 必须用绝对路径
// CMake 注入 BINARY_DIR 宏, 运行时拼出 .so 绝对路径
#ifndef AGENTICDSL_EXAMPLE_BINARY_DIR
#define AGENTICDSL_EXAMPLE_BINARY_DIR "."
#endif

std::string build_plugin_path(const char* relative) {
  return std::string(AGENTICDSL_EXAMPLE_BINARY_DIR) + "/" + relative;
}

const std::vector<std::pair<std::string, std::string>> kPlugins = {
  {"hydraforge_model_router_cost",    "pdk/model_router/cost_strategy/libhydraforge_model_router_cost.so"},
  {"hydraforge_model_router_quality", "pdk/model_router/quality_strategy/libhydraforge_model_router_quality.so"},
  {"hydraforge_model_router_latency", "pdk/model_router/latency_strategy/libhydraforge_model_router_latency.so"},
  {"hydraforge_model_registry",       "pdk/model_router/libhydraforge_model_registry.so"},
};

void print_registered(const ToolRegistry& registry) {
  for (const auto& name : registry.list_tools()) {
    std::cout << "    * " << name << "\n";
  }
}

} // namespace

int main(int argc, char** argv) {
  bool mock_mode = false;
  bool list_mode = false;
  for (int i = 1; i < argc; ++i) {
    std::string arg(argv[i]);
    if (arg == "--mock") mock_mode = true;
    else if (arg == "--list") list_mode = true;
    else {
      std::cerr << "Usage: " << argv[0] << " --mock | --list\n"
                << "  --mock    演示 3 策略路由 (cost/quality/latency)\n"
                << "  --list    列出可用模型\n";
      return 1;
    }
  }

  if (!mock_mode && !list_mode) {
    std::cerr << "Usage: " << argv[0] << " --mock | --list\n";
    return 1;
  }

  // PluginLoader 路径白名单仅接受 HYDRAFORGE_PLUGIN_PATH 下的路径,
  // 将 build/pdk/model_router/ 加入白名单 (覆盖所有 3 个 strategy + registry 子目录)
  std::string plugin_root = std::string(AGENTICDSL_EXAMPLE_BINARY_DIR) + "/pdk/model_router";
  setenv("HYDRAFORGE_PLUGIN_PATH", plugin_root.c_str(), 1);

  // 创建 ToolRegistry + PluginLoader
  // 注意: 声明顺序很重要 — PluginLoader 必须在 registry 之后声明,
  //       这样销毁时 registry 先析构 (lambdas 安全释放, 此时 .so 仍加载),
  //       然后 PluginLoader 才 dlclose 关闭 .so。颠倒顺序会导致 segfault。
  PluginLoader loader;
  ToolRegistry registry;

  std::cout << "[phase1_model_router_plugin] Loading plugins via PluginLoader...\n"
            << "  HYDRAFORGE_PLUGIN_PATH=" << plugin_root << "\n";
  for (const auto& [name, relpath] : kPlugins) {
    std::string abs_path = build_plugin_path(relpath.c_str());
    bool ok = loader.load_so(abs_path, registry);
    if (ok) {
      std::cout << "  - loaded: " << name << " (" << abs_path << ")\n";
    } else {
      std::cerr << "  - failed: " << name << " (" << abs_path << ")\n";
    }
  }

  std::cout << "  - registered tools:\n";
  print_registered(registry);

  // --list 模式
  if (list_mode) {
    std::cout << "\n[phase1_model_router_plugin] Available models (via model_router/registry):\n";
    auto result = registry.call_tool("model_router/registry", {});
    std::cout << "  - result: " << result.dump(2) << "\n";
    return 0;
  }

  // --mock 模式
  if (mock_mode) {
    MockLLMProvider provider;
    // 注入测试模型
    provider.set_available_models({
      ILLMProvider::ModelInfo("gpt-4",
          {ILLMProvider::ModelCapability::Chat, ILLMProvider::ModelCapability::ToolUse},
          8192, "openai"),
      ILLMProvider::ModelInfo("gpt-3.5-turbo",
          {ILLMProvider::ModelCapability::Chat},
          4096, "openai"),
      ILLMProvider::ModelInfo("claude-3-opus",
          {ILLMProvider::ModelCapability::Chat, ILLMProvider::ModelCapability::Vision},
          16384, "anthropic"),
    });

    // 构造 candidates JSON (PDK Plugin 侧 ModelCapability 格式)
    std::string candidates_json = R"([
      {"model_id":"gpt-4","model_name":"GPT-4","n_ctx":8192,"max_tokens":4096,
       "supports_streaming":true,"supports_function_call":true,
       "per_token_cost":0.03,"avg_latency_ms":500,
       "tags":["general","reasoning","code"]},
      {"model_id":"gpt-3.5-turbo","model_name":"GPT-3.5 Turbo","n_ctx":4096,"max_tokens":4096,
       "supports_streaming":true,"supports_function_call":false,
       "per_token_cost":0.002,"avg_latency_ms":200,
       "tags":["general","fast"]},
      {"model_id":"claude-3-opus","model_name":"Claude 3 Opus","n_ctx":16384,"max_tokens":4096,
       "supports_streaming":true,"supports_function_call":true,
       "per_token_cost":0.015,"avg_latency_ms":350,
       "tags":["general","reasoning","code","vision"]}
    ])";

    std::cout << "\n[phase1_model_router_plugin] Model Router Demo (3 strategies)\n"
              << "  candidates: 3 models (gpt-4, gpt-3.5-turbo, claude-3-opus)\n\n";

    // 策略 1: Cost (最低成本)
    std::cout << "--- CostRouter (tag=general) ---\n";
    auto cost_result = registry.call_tool("model_router/cost", {
      {"task_type", "completion"},
      {"required_tags", "[\"general\"]"},
      {"candidates", candidates_json}
    });
    std::cout << "  result: " << cost_result.dump(2) << "\n";
    std::cout << "  expected: gpt-3.5-turbo (@ $0.002/token)\n";

    // 策略 2: Quality (最高匹配度)
    std::cout << "\n--- QualityRouter (tag=general) ---\n";
    auto quality_result = registry.call_tool("model_router/quality", {
      {"task_type", "completion"},
      {"required_tags", "[\"general\"]"},
      {"candidates", candidates_json}
    });
    std::cout << "  result: " << quality_result.dump(2) << "\n";

    std::cout << "\n--- QualityRouter (tag=vision) ---\n";
    auto quality_vision = registry.call_tool("model_router/quality", {
      {"task_type", "vision_task"},
      {"required_tags", "[\"vision\"]"},
      {"candidates", candidates_json}
    });
    std::cout << "  result: " << quality_vision.dump(2) << "\n";

    // 策略 3: Latency (最低延迟, max_latency=300ms)
    std::cout << "\n--- LatencyRouter (tag=general, max_latency=300ms) ---\n";
    auto latency_result = registry.call_tool("model_router/latency", {
      {"task_type", "real_time"},
      {"required_tags", "[\"general\"]"},
      {"max_latency", "300"},
      {"candidates", candidates_json}
    });
    std::cout << "  result: " << latency_result.dump(2) << "\n";

    return 0;
  }

  return 1;
}