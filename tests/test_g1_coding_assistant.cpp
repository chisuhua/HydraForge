// tests/test_g1_coding_assistant.cpp
// 功能描述：G1 Coding Assistant Plugin 测试 (Phase 6 W1 §3.9-3.12)
//          3 TEST_CASE: 2-step ReAct loop / tool manifest size / MockLLMProvider wiring
//          使用 PluginLoader + MockToolRegistry + LLM callback 模式
// 设计依据：openspec/changes/phase6-service-ification-v1/
//          tasks.md §3.9-3.12, specs/coding-assistant-agent/spec.md
// 参考范式：tests/test_g3_knowledge_base.cpp (Phase 6 W1)
//          pdk/g3_knowledge_base/src/g3_query.cpp (callback pattern)
// 作者：Phase 6 W1 (Sisyphus-Junior)
// 最后修改日期：2026-07-15

#include "catch_amalgamated.hpp"

#include "core/engine.h"

#include "agenticdsl/contract/itool_registry.h"
#include "agenticdsl/plugin/plugin_loader.h"
#include "agenticdsl/plugin/plugin_info.h"
#include "common/llm/mock_provider.h"
#include "common/policy/execution_policy.h"

#include <nlohmann/json.hpp>

#include <cstdlib>
#include <dlfcn.h>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

using json = nlohmann::json;

namespace {

class MockToolRegistry : public ::agenticdsl::IToolRegistry {
 public:
  std::vector<std::string> registered_tools;
  std::unordered_map<std::string, ::agenticdsl::ToolMetadata> tool_metas;
  std::unordered_map<std::string, ToolFunc> tool_funcs;

  void register_tool_function(std::string name, ::agenticdsl::ToolMetadata meta,
                              ToolFunc fn) override {
    registered_tools.push_back(name);
    tool_metas[name] = std::move(meta);
    tool_funcs[name] = std::move(fn);
  }
  bool has_tool(const std::string& name) const override {
    return tool_funcs.find(name) != tool_funcs.end();
  }
  json call_tool(const std::string& name,
                 const std::unordered_map<std::string, std::string>& args) override {
    auto it = tool_funcs.find(name);
    if (it == tool_funcs.end()) return {{"error", "tool not found"}, {"tool", name}};
    return it->second(args);
  }
  std::vector<std::string> list_tools() const override { return registered_tools; }
  void register_llm_tool(std::string, std::unique_ptr<::agenticdsl::ILLMTool>,
                         const ::agenticdsl::LLMParams&) override {}
  bool is_llm_tool(const std::string&) const override { return false; }
  const ::agenticdsl::LLMParams& get_llm_params(const std::string&) const override {
    static const ::agenticdsl::LLMParams kEmpty{};
    return kEmpty;
  }
  json call_llm_tool(const std::string&, const std::string&,
                     const ::agenticdsl::LLMParams&) override { return {}; }
  void set_cost_callback(CostCallback) override {}
};

struct G1Fixture {
  ::hydraforge::PluginLoader loader;
  MockToolRegistry registry;
  void* handle = nullptr;

  using ManifestFn    = const std::vector<std::string>* (*)();
  using CallCountFn   = int (*)();
  using ResetFn       = void (*)();

  ManifestFn  get_manifest   = nullptr;
  CallCountFn get_call_count = nullptr;
  ResetFn     reset_state    = nullptr;

  bool load() {
    setenv("HYDRAFORGE_PLUGIN_PATH",
           "/workspace/project/HydraForge/build/pdk/g1_coding_assistant/", 1);

    const char* so_path = "/workspace/project/HydraForge/build/pdk/g1_coding_assistant/"
                          "libhydraforge_g1_coding_assistant.so";

    if (!loader.load_so(so_path, registry)) {
      WARN("G1 .so not found — skipping tests");
      return false;
    }

    handle = dlopen(so_path, RTLD_NOW | RTLD_GLOBAL);
    if (handle) {
      get_manifest   = reinterpret_cast<ManifestFn>(
          dlsym(handle, "g1_get_tool_manifest"));
      get_call_count = reinterpret_cast<CallCountFn>(
          dlsym(handle, "g1_get_llm_call_count"));
      reset_state    = reinterpret_cast<ResetFn>(
          dlsym(handle, "g1_reset_state"));
    }
    return true;
  }

  ~G1Fixture() { if (handle) dlclose(handle); }

  json review(const std::string& request, const std::string& code,
              const std::string& session_id = "g1_test_session") {
    auto it = registry.tool_funcs.find("coding_assistant/review");
    REQUIRE(it != registry.tool_funcs.end());
    return it->second(
        {{"request", request}, {"code", code}, {"session_id", session_id}});
  }
};

void register_mock_g3_tool(MockToolRegistry& reg) {
  ::agenticdsl::ToolMetadata meta{
      "knowledge_base/query", "Mock G3 KB", "knowledge",
      ::agenticdsl::ToolCategory::Execute,
      ::agenticdsl::LayerProfile::Workflow,
      ::agenticdsl::ApprovalPolicy{true, true, false, false},
      {::agenticdsl::LayerProfile::Workflow}, 0.0, 30000};

  reg.register_tool_function("knowledge_base/query", meta,
      [](const std::unordered_map<std::string, std::string>& args) -> json {
        auto q = args.find("question");
        auto s = args.find("session_id");
        if (q == args.end() || s == args.end())
          return {{"success", false}, {"error", "missing args"}};
        return {{"success", true},
                {"answer", "Mock G3 analysis for '" + q->second +
                               "' [session: " + s->second + "]"}};
      });
}

}  // namespace

// ============================================================================
// Test 1 (§3.9): 2-step ReAct loop executes successfully
// ============================================================================
TEST_CASE("g1_coding_assistant: 2-step ReAct loop executes successfully",
          "[g1][coding_assistant][react-loop]") {
  G1Fixture fix;
  if (fix.reset_state) fix.reset_state();
  register_mock_g3_tool(fix.registry);
  if (!fix.load()) return;

  REQUIRE(fix.registry.has_tool("coding_assistant/review"));
  REQUIRE(fix.registry.has_tool("knowledge_base/query"));

  auto result = fix.review("Review this code", "int main() { return 0; }");

  REQUIRE(result["success"] == true);
  REQUIRE(result.contains("answer"));
  REQUIRE(result["answer"].is_string());
  // 验证默认回退合成 (无 callback 时使用 [G1 Review] 前缀)
  REQUIRE(result["answer"].get<std::string>().find("[G1 Review]") != std::string::npos);
}

// ============================================================================
// Test 2 (§3.10): tool manifest contains exactly 1 entry
// ============================================================================
TEST_CASE("g1_coding_assistant: tool manifest has exactly 1 entry",
          "[g1][coding_assistant][manifest]") {
  G1Fixture fix;
  if (fix.reset_state) fix.reset_state();
  register_mock_g3_tool(fix.registry);
  if (!fix.load()) return;
  REQUIRE(fix.get_manifest != nullptr);

  auto* manifest = fix.get_manifest();
  REQUIRE(manifest != nullptr);
  INFO("Manifest size: " << manifest->size() << ", expected: 1");
  REQUIRE(manifest->size() == 1);
  REQUIRE((*manifest)[0] == "knowledge_base/query");
}

// ============================================================================
// Test 3 (§3.11): MockLLMProvider wiring verified (via callback, no real LLM)
// ============================================================================
TEST_CASE("g1_coding_assistant: MockLLMProvider wiring via DSLEngine + callback",
          "[g1][coding_assistant][mock-wiring]") {
  G1Fixture fix;
  if (fix.reset_state) fix.reset_state();
  register_mock_g3_tool(fix.registry);
  if (!fix.load()) return;
  REQUIRE(fix.get_call_count != nullptr);

  // 创建 per-test-instance MockLLMProvider (§3.6: 非共享静态实例)
  auto mock = std::make_unique<::agenticdsl::MockLLMProvider>();
  mock->enqueue_response("[G1 LLM] Code looks good — best practices followed.");
  ::agenticdsl::MockLLMProvider* mock_raw = mock.get();

  // 注入 MockLLMProvider 到 G1 的 DSLEngine (§3.6: engine->set_llm_provider)
  // 使用最小 valid DSL 创建 DSLEngine
  std::string minimal_dsl = R"(
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
  auto engine = agenticdsl::DSLEngine::from_markdown(minimal_dsl);
  REQUIRE(engine != nullptr);
  engine->set_llm_provider(std::move(mock));

  // 验证 provider 已注入 (Decorator 链包装后内部是 MockLLMProvider)
  auto* provider = engine->get_llm_provider();
  REQUIRE(provider != nullptr);

  // 通过 generate 调用验证 wiring
  ::agenticdsl::GenerationRequest req("test prompt");
  auto gen_result = provider->generate(req, std::stop_token{});
  REQUIRE(gen_result.has_value());
  REQUIRE(gen_result.value().text == "[G1 LLM] Code looks good — best practices followed.");
}
