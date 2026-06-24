// examples/phase1_plugin_demo/main.cpp
// 文件头注释
// 功能描述：Phase 1 端到端 demo (Plugin Stub + ToolResult 信封)
//          Sprint 0: 模拟 phase1_model_router_plugin Plugin 决策 + MockLLMProvider 调用
//          Sprint 5 PluginLoader 真实 .so 加载时, 此 demo 扩展为 dlopen 路径 (S5.T3)
//          Sprint 1a (S1a.T5) 扩展: 演示 ToolResult P2-P4 字段 (error_code/latency_ms/trace_id)
// 设计依据：phase1-execution.md §Sprint 0 + openspec REQ-TR-001..004 + openspec/changes/2026-07-14-plugin-loader S5.T3
// 作者：AgenticDSL Phase 1 / Sprint 0 + Sprint 1a + Sprint 5 S5.T3
// 最后修改日期：2026-06-24

#include "common/llm/llm_types.h"
#include "common/llm/mock_provider.h"
#include "common/tools/registry.h"
#include "core/types/tool_result.h"
#include "agenticdsl/plugin/plugin_loader.h"
#include "agenticdsl/plugin/plugin_info.h"

#include <chrono>
#include <iostream>
#include <optional>
#include <stop_token>
#include <string>

// --- Sprint 5 S5.T3: 3 mode CLI 解析 ---
namespace {

struct CliArgs {
  bool mock = true;  // Sprint 0 fallback default
  std::optional<std::string> load_plugin;
  std::optional<std::string> plugin_path;
};

constexpr const char* kUsage =
    "Usage: phase1_plugin_demo [--mock | --load-plugin=<path> | --plugin-path=<dir>]\n"
    "  --mock                   Sprint 0 fallback (default)\n"
    "  --load-plugin=<path>     Load single .so plugin\n"
    "  --plugin-path=<dir>      Scan dir for .so plugins\n"
    "  --mock 与 --load-plugin/--plugin-path 二选一, 互斥";

CliArgs parse_args(int argc, char** argv) {
  CliArgs args;
  for (int i = 1; i < argc; ++i) {
    std::string a = argv[i];
    if (a == "--mock") {
      args.mock = true;
      args.load_plugin.reset();
      args.plugin_path.reset();
    } else if (a.rfind("--load-plugin=", 0) == 0) {
      args.mock = false;
      args.load_plugin = a.substr(14);
      args.plugin_path.reset();
    } else if (a.rfind("--plugin-path=", 0) == 0) {
      args.mock = false;
      args.load_plugin.reset();
      args.plugin_path = a.substr(14);
    } else {
      throw std::runtime_error(std::string("Unknown arg: ") + a + "\n  " + kUsage);
    }
  }
  // 互斥校验
  if (!args.mock &&
      (args.load_plugin.has_value() == args.plugin_path.has_value())) {
    throw std::runtime_error(
        "--mock and --load-plugin/--plugin-path are mutually exclusive\n  " +
        std::string(kUsage));
  }
  return args;
}

// --- Sprint 0 mock 模式 (保留原始行为) ---
int run_mock_mode() {
  std::cout << "[phase1_plugin_demo] Sprint 0 Plugin Stub + Sprint 1a ToolResult 端到端验证\n";

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

  // 2. Plugin Stub 模拟: 选定第一个 Chat-capable 模型
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

  // === Sprint 1a ToolResult 信封演示 ===
  std::cout << "\n[phase1_plugin_demo] Sprint 1a ToolResult 信封演示\n";

  auto success_result = agenticdsl::ToolResult::success(
      nlohmann::json{{"echoed", "hello"}, {"count", 3}},
      nlohmann::json{{"meta_k", "meta_v"}});
  success_result.latency_ms = 12;
  success_result.trace_id = "trace-demo-success-001";
  success_result.metadata = {{"caller", "phase1_plugin_demo"}, {"attempt", 1}};
  std::cout << "  - ToolResult::success:\n";
  std::cout << "    ok=" << success_result.ok
            << ", latency_ms=" << success_result.latency_ms.value_or(0)
            << ", trace_id=" << success_result.trace_id.value_or("(none)") << "\n";
  std::cout << "    meta=" << success_result.meta.dump()
            << ", metadata=" << success_result.metadata.value().dump() << "\n";

  auto retry_result = agenticdsl::ToolResult::error(
      agenticdsl::ErrorCode::Retry, "network blip");
  retry_result.latency_ms = 150;
  retry_result.trace_id = "trace-demo-retry-002";
  std::cout << "  - ToolResult::error(Retry):\n";
  std::cout << "    ok=" << retry_result.ok
            << ", error_code=" << static_cast<int>(retry_result.error_code.value())
            << " (Retry)\n";
  std::cout << "    latency_ms=" << retry_result.latency_ms.value_or(0)
            << ", trace_id=" << retry_result.trace_id.value_or("(none)") << "\n";
  std::cout << "    meta=" << retry_result.meta.dump() << "\n";

  auto abort_result = agenticdsl::ToolResult::error(
      agenticdsl::ErrorCode::Abort, "unrecoverable");
  std::cout << "  - ToolResult::error(Abort):\n";
  std::cout << "    ok=" << abort_result.ok
            << ", error_code=" << static_cast<int>(abort_result.error_code.value())
            << " (Abort) → NodeExecutor MUST 抛出异常终止整个 Graph\n";

  auto skip_result = agenticdsl::ToolResult::error(
      agenticdsl::ErrorCode::Skip, "preconditions not met");
  std::cout << "  - ToolResult::error(Skip):\n";
  std::cout << "    ok=" << skip_result.ok
            << ", error_code=" << static_cast<int>(skip_result.error_code.value())
            << " (Skip) → NodeExecutor 返回原 context 不抛异常\n";

  auto envelope_json = retry_result.to_json();
  std::cout << "  - ToolResult::to_json():\n    " << envelope_json.dump() << "\n";
  auto roundtrip = agenticdsl::ToolResult::from_json(envelope_json);
  std::cout << "  - ToolResult::from_json() roundtrip:\n";
  std::cout << "    ok=" << roundtrip.ok
            << ", error_code=" << static_cast<int>(roundtrip.error_code.value_or(agenticdsl::ErrorCode::Unknown))
            << ", latency_ms=" << roundtrip.latency_ms.value_or(0)
            << ", trace_id=" << roundtrip.trace_id.value_or("(none)") << "\n";

  std::cout << "  - demo complete (Plugin Stub + ToolResult 验证通过)\n";
  return 0;
}

// --- Sprint 5 S5.T3 模式 2/3: PluginLoader 真实 dlopen 路径 ---
int run_load_plugin_mode(const std::string& path) {
#ifndef __linux__
  std::cerr << "[phase1_plugin_demo] --load-plugin 仅支持 Linux (dlopen) 平台\n";
  return 1;
#else
  std::cout << "[phase1_plugin_demo] loading single plugin: " << path << "\n";
  hydraforge::PluginLoader loader;
  agenticdsl::ToolRegistry registry;
  bool ok = loader.load_so(path, registry);
  if (!ok) {
    std::cerr << "  - PluginLoader::load_so failed (path whitelist or ABI mismatch)\n";
    return 1;
  }
  auto loaded = loader.list_loaded();
  std::cout << "  - loaded plugins: " << loaded.size() << "\n";
  for (const auto& info : loaded) {
    std::cout << "    * " << info.name << " v" << info.major_version
              << "." << info.minor_version << "." << info.patch_version
              << " (abi=" << info.abi_version << ")\n";
  }
  std::cout << "  - registered tools: " << registry.list_tools().size() << "\n";
  std::cout << "  - demo complete (real .so E2E 验证通过)\n";
  return 0;
#endif
}

int run_plugin_path_mode(const std::string& dir) {
#ifndef __linux__
  std::cerr << "[phase1_plugin_demo] --plugin-path 仅支持 Linux (dlopen) 平台\n";
  return 1;
#else
  std::cout << "[phase1_plugin_demo] scanning plugin path: " << dir << "\n";
  hydraforge::PluginLoader loader;
  agenticdsl::ToolRegistry registry;
  // 设置环境变量模拟 plugin path (PluginLoader::get_search_paths 通过 env 优先)
  setenv("HYDRAFORGE_PLUGIN_PATH", dir.c_str(), 1);
  std::size_t count = loader.load_all(registry);
  std::cout << "  - load_all scanned, loaded count: " << count << "\n";
  auto loaded = loader.list_loaded();
  std::cout << "  - listed loaded plugins: " << loaded.size() << "\n";
  for (const auto& info : loaded) {
    std::cout << "    * " << info.name << " v" << info.major_version
              << "." << info.minor_version << "." << info.patch_version
              << " (abi=" << info.abi_version << ")\n";
  }
  std::cout << "  - registered tools: " << registry.list_tools().size() << "\n";
  if (count == 0) {
    std::cout << "  - note: 0 plugins loaded (expected if dir empty or path whitelist)\n";
  }
  std::cout << "  - demo complete (plugin-path E2E 验证完成)\n";
  return 0;
#endif
}

}  // namespace

int main(int argc, char** argv) {
  CliArgs args;
  try {
    args = parse_args(argc, argv);
  } catch (const std::exception& e) {
    std::cerr << "[phase1_plugin_demo] " << e.what() << std::endl;
    return 1;
  }

  // 模式 1: --mock (Sprint 0 fallback)
  if (args.mock) {
    return run_mock_mode();
  }

  // 模式 2: --load-plugin=<path>
  if (args.load_plugin.has_value()) {
    return run_load_plugin_mode(*args.load_plugin);
  }

  // 模式 3: --plugin-path=<dir>
  if (args.plugin_path.has_value()) {
    return run_plugin_path_mode(*args.plugin_path);
  }

  std::cerr << "[phase1_plugin_demo] no mode specified, use --mock\n";
  return 1;
}
