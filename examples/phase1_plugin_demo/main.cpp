// examples/phase1_plugin_demo/main.cpp
// 文件头注释
// 功能描述：Phase 1 端到端 demo (Plugin Stub + ToolResult 信封)
//          Sprint 0: 模拟 phase1_model_router_plugin Plugin 决策 + MockLLMProvider 调用
//          Sprint 5 PluginLoader 真实 .so 加载时, 此 demo 扩展为 dlopen 路径
//          Sprint 1a (S1a.T5) 扩展: 演示 ToolResult P2-P4 字段 (error_code/latency_ms/trace_id)
// 设计依据：phase1-execution.md §Sprint 0 + openspec REQ-TR-001..004
// 作者：AgenticDSL Phase 1 / Sprint 0 + Sprint 1a
// 最后修改日期：2026-06-16

#include "common/llm/llm_types.h"
#include "common/llm/mock_provider.h"
#include "core/types/tool_result.h"

#include <chrono>
#include <iostream>
#include <stop_token>
#include <string>
#include <thread>

int main(int argc, char** argv) {
  const bool mock_mode = (argc > 1 && std::string(argv[1]) == "--mock");
  if (!mock_mode) {
    std::cerr << "Usage: " << argv[0] << " --mock\n"
              << "  Phase 1 demo 仅支持 --mock 模式\n";
    return 1;
  }

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

  // === Sprint 1a (S1a.T5) 扩展: ToolResult 信封演示 ===
  std::cout << "\n[phase1_plugin_demo] Sprint 1a ToolResult 信封演示\n";

  // 4. 成功路径: success + metadata + trace_id
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

  // 5. 错误路径: ErrorCode::Retry (REQ-TR-001)
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

  // 6. 错误路径: ErrorCode::Abort (REQ-TR-001 Scenario)
  auto abort_result = agenticdsl::ToolResult::error(
      agenticdsl::ErrorCode::Abort, "unrecoverable");
  std::cout << "  - ToolResult::error(Abort):\n";
  std::cout << "    ok=" << abort_result.ok
            << ", error_code=" << static_cast<int>(abort_result.error_code.value())
            << " (Abort) → NodeExecutor MUST 抛出异常终止整个 Graph\n";

  // 7. 错误路径: ErrorCode::Skip
  auto skip_result = agenticdsl::ToolResult::error(
      agenticdsl::ErrorCode::Skip, "preconditions not met");
  std::cout << "  - ToolResult::error(Skip):\n";
  std::cout << "    ok=" << skip_result.ok
            << ", error_code=" << static_cast<int>(skip_result.error_code.value())
            << " (Skip) → NodeExecutor 返回原 context 不抛异常\n";

  // 8. JSON 往返演示
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
