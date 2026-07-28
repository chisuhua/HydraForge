// examples/pkm_temporal_demo/main.cpp
// 功能描述：PKM Temporal Demo CLI 入口 (Task 4 Step 3)。
//          解析 --mock/--real/--scenario 参数, 创建 MockTemporalClient,
//          注册 5 个 temporal/* 工具, 加载 scenario .agent.md 并执行。
// 设计依据：.rddf/plans/pkm-temporal-demo-scaffold.md Task 4 Step 3
// 作者：pkm-temporal-demo-scaffold Task 4
// 最后修改日期：2026-07-28

#include "demo_args.h"

#include "agenticdsl/contract/itool_registry.h"
#include "common/tools/registry.h"
#include "core/engine.h"
#include "mock_client.h"

#include <iostream>
#include <memory>
#include <string>

namespace temporal_agent = agenticdsl::pdk::temporal_agent;
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

  auto client = std::make_unique<agenticdsl::pdk::MockTemporalClient>();
  temporal_agent::set_client(std::move(client));

  std::string scenario_path =
      "examples/pkm_temporal_demo/scenario-" + args.scenario + ".agent.md";

  auto engine = agenticdsl::DSLEngine::from_file(scenario_path);
  if (!engine) {
    std::cerr << "Error: failed to load scenario: " << scenario_path << "\n";
    return 3;
  }

  // 在引擎已有的 ToolRegistry 上注册 5 个 temporal/* 工具
  temporal_agent::register_tools(&engine->get_tool_registry());

  std::cout << "=== PKM Temporal Demo ===\n"
            << "Scenario: " << args.scenario << "\n"
            << "Mode: " << (args.mode == DemoMode::Mock ? "Mock" : "Real") << "\n"
            << "Running...\n";

  agenticdsl::Context ctx;
  ctx["user_input"] = "hello";
  auto result = engine->run(ctx);

  std::cout << "Result: " << (result.success ? "SUCCESS" : "FAILED") << "\n";
  return result.success ? 0 : 4;
}
