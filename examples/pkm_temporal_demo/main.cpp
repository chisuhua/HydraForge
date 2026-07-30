// examples/pkm_temporal_demo/main.cpp
// 功能描述：PKM Temporal Demo CLI 入口 (Task 4 Step 3)。
//          解析 --mock/--real/--scenario 参数, 配置 TemporalClient (InMemory 后端),
//          注册 5 个 temporal/* 工具, 加载 scenario .agent.md 并执行。
// 设计依据：.rddf/plans/pkm-temporal-demo-scaffold.md Task 4 Step 3
//          + pdk/temporal_agent 实际 API (TemporalClient 单例 + InMemoryTemporalBackend)
// 作者：pkm-temporal-demo-scaffold Task 4
// 最后修改日期：2026-07-30 (修复 mock_client.h 不存在问题, 改用 InMemoryTemporalBackend)

#include "demo_args.h"

#include "agenticdsl/contract/itool_registry.h"
#include "core/engine.h"
#include "temporal_client.h"

#include <iostream>
#include <memory>
#include <string>

extern "C" void pdk_register_tools(::agenticdsl::IToolRegistry& registry);

using agenticdsl::pdk::demo::DemoMode;
using agenticdsl::pdk::demo::parse_args;

int main(int argc, char** argv) {
  auto args = parse_args(argc, argv);

  if (args.scenario.empty()) {
    std::cerr << "Usage: pkm_temporal_demo --mock --scenario <name>\n"
              << "  Scenarios: blocking, async-poll, signal, idempotent\n";
    return 1;
  }

  if (args.mode == DemoMode::Real) {
    std::cerr << "Error: --real mode not yet implemented (requires pkgm-temporal-agent)\n";
    return 2;
  }

  // 配置 TemporalClient (单例) 使用 InMemory 后端 (零 gRPC 依赖, Mock 模式)
  auto& client = pdk_temporal_agent::TemporalClient::instance();
  client.set_backend(std::make_unique<pdk_temporal_agent::InMemoryTemporalBackend>());
  client.connect("in-memory");

  std::string scenario_path =
      "examples/pkm_temporal_demo/scenario-" + args.scenario + ".agent.md";

  auto engine = agenticdsl::DSLEngine::from_file(scenario_path);
  if (!engine) {
    std::cerr << "Error: failed to load scenario: " << scenario_path << "\n";
    return 3;
  }

  // 在引擎已有的 ToolRegistry 上注册 5 个 temporal/* 工具
  pdk_register_tools(engine->get_tool_registry());

  std::cout << "=== PKM Temporal Demo ===\n"
            << "Scenario: " << args.scenario << "\n"
            << "Mode: " << (args.mode == DemoMode::Mock ? "Mock" : "Real") << "\n"
            << "Running...\n";

  auto result = engine->run();

  std::cout << "Result: " << (result.success ? "SUCCESS" : "FAILED") << "\n";

  client.shutdown();
  return result.success ? 0 : 4;
}