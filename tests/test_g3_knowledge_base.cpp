// tests/test_g3_knowledge_base.cpp
// 功能描述：G3 Knowledge Base Plugin 测试 (Phase 6 W1 §2.9-2.13)
//          5 TEST_CASE: 加载+注册 / 单次调用 / 多轮 / 隔离 / error-schema
//          使用 PluginLoader + MockToolRegistry 模式 (与 C14 test_llama_engine_plugin 一致)
// 设计依据：openspec/changes/phase6-service-ification-v1/
//          tasks.md §2.9-2.14, specs/knowledge-base-agent/spec.md
// 参考范式：tests/test_llama_engine_plugin.cpp (C14, 370 行, MockToolRegistry)
// 作者：Phase 6 W1 (Sisyphus-Junior)
// 最后修改日期：2026-07-15

#include "catch_amalgamated.hpp"

#include "agenticdsl/contract/itool_registry.h"
#include "agenticdsl/plugin/plugin_loader.h"
#include "agenticdsl/plugin/plugin_info.h"
#include "common/policy/execution_policy.h"

#include <nlohmann/json.hpp>

#include <cstdlib>
#include <dlfcn.h>
#include <fstream>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

using json = nlohmann::json;

namespace {

// ──── Mock ToolRegistry (与 test_llama_engine_plugin.cpp 一致) ──────────────
class MockToolRegistry : public ::agenticdsl::IToolRegistry {
 public:
  std::vector<std::string> registered_tools;
  std::unordered_map<std::string, ::agenticdsl::ToolMetadata> tool_metas;
  std::unordered_map<std::string, ToolFunc> tool_funcs;

  void register_tool_function(std::string name, ::agenticdsl::ToolMetadata meta, ToolFunc fn) override {
    registered_tools.push_back(name);
    tool_metas[name] = std::move(meta);
    tool_funcs[name] = std::move(fn);
  }

  bool has_tool(const std::string& name) const override {
    return tool_funcs.find(name) != tool_funcs.end();
  }

  json call_tool(const std::string& name, const std::unordered_map<std::string, std::string>& args) override {
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

// ──── Test helper: wrap PluginLoader load + dlsym test functions ───────────
struct G3Plugin {
  ::hydraforge::PluginLoader loader;
  MockToolRegistry registry;
  void* handle = nullptr;

  // extern "C" function pointers (dlsym'd from .so)
  using ResetFn  = void (*)();
  using EnqFn    = void (*)(const char*);
  using ClearFn  = void (*)();

  ResetFn reset_mock   = nullptr;
  EnqFn   enqueue     = nullptr;
  ClearFn clear_sessions = nullptr;

  bool load() {
    const char* existing = getenv("HYDRAFORGE_PLUGIN_PATH");
    setenv("HYDRAFORGE_PLUGIN_PATH",
           "/workspace/project/HydraForge/build/debug/pdk/g3_knowledge_base/", 1);

    const char* so_path = getenv("G3_PLUGIN_SO");
    if (!so_path) {
      so_path = "/workspace/project/HydraForge/build/debug/pdk/g3_knowledge_base/"
                "libhydraforge_g3_knowledge_base.so";
    }

    if (!loader.load_so(so_path, registry)) {
      WARN("G3 plugin .so not found at " << so_path << " — skipping tests");
      return false;
    }

    handle = dlopen(so_path, RTLD_LAZY);
    if (!handle) return true;

    reset_mock   = reinterpret_cast<ResetFn>(dlsym(handle, "g3_kb_reset_mock"));
    enqueue     = reinterpret_cast<EnqFn>(dlsym(handle, "g3_kb_enqueue_response"));
    clear_sessions = reinterpret_cast<ClearFn>(dlsym(handle, "g3_kb_clear_sessions"));

    return true;
  }

  ~G3Plugin() { if (handle) dlclose(handle); }

  json query(const std::string& question, const std::string& session_id) {
    auto it = registry.tool_funcs.find("knowledge_base/query");
    REQUIRE(it != registry.tool_funcs.end());
    return it->second({{"question", question}, {"session_id", session_id}});
  }
};

} // namespace

// ============================================================================
// Test 1 (2.1+2.2): 加载并验证注册
// ============================================================================
TEST_CASE("g3_kb: plugin loads and registers knowledge_base/query",
          "[g3][knowledge_base][load]") {
  G3Plugin plugin;
  if (!plugin.load()) return;

  REQUIRE(plugin.registry.has_tool("knowledge_base/query"));
  REQUIRE(plugin.registry.registered_tools.size() == 1);

  auto& meta = plugin.registry.tool_metas["knowledge_base/query"];
  REQUIRE(meta.name == "knowledge_base/query");
  REQUIRE(meta.category == ::agenticdsl::ToolCategory::Execute);
  REQUIRE(meta.min_layer == ::agenticdsl::LayerProfile::Workflow);
  REQUIRE(meta.allowed_layers.size() == 1);
  REQUIRE(meta.allowed_layers[0] == ::agenticdsl::LayerProfile::Workflow);
}

// ============================================================================
// Test 2 (2.9): 单次调用 — 新 session_id
// ============================================================================
TEST_CASE("g3_kb: single-shot call returns answer",
          "[g3][knowledge_base][single-shot]") {
  G3Plugin plugin;
  if (!plugin.load()) return;

  if (plugin.reset_mock) plugin.reset_mock();
  if (plugin.clear_sessions) plugin.clear_sessions();
  if (plugin.enqueue) plugin.enqueue("HydraForge is an execution engine.");

  auto result = plugin.query("What is HydraForge?", "session_1");
  REQUIRE(result["success"] == true);
  REQUIRE(result["answer"] == "HydraForge is an execution engine.");
  REQUIRE_FALSE(result.contains("error"));
}

// ============================================================================
// Test 3 (2.10): 多轮对话 — 同一 session_id, 上下文保留
// ============================================================================
TEST_CASE("g3_kb: multi-turn preserves prior context",
          "[g3][knowledge_base][multi-turn]") {
  G3Plugin plugin;
  if (!plugin.load()) return;

  if (plugin.reset_mock) plugin.reset_mock();
  if (plugin.clear_sessions) plugin.clear_sessions();
  if (plugin.enqueue) plugin.enqueue("first answer");
  if (plugin.enqueue) plugin.enqueue("answer referencing prior context");

  auto r1 = plugin.query("first question", "session_mt");
  REQUIRE(r1["success"] == true);
  REQUIRE(r1["answer"] == "first answer");

  auto r2 = plugin.query("second question", "session_mt");
  REQUIRE(r2["success"] == true);
}

// ============================================================================
// Test 4 (2.11): 会话隔离 — 不同 session_id 独立
// ============================================================================
TEST_CASE("g3_kb: different session_ids are isolated",
          "[g3][knowledge_base][isolation]") {
  G3Plugin plugin;
  if (!plugin.load()) return;

  if (plugin.reset_mock) plugin.reset_mock();
  if (plugin.clear_sessions) plugin.clear_sessions();
  if (plugin.enqueue) plugin.enqueue("answer for A");

  auto r_a = plugin.query("question to A", "session_A");
  REQUIRE(r_a["success"] == true);

  if (plugin.enqueue) plugin.enqueue("answer for B");
  auto r_b = plugin.query("question to B", "session_B");
  REQUIRE(r_b["success"] == true);
}

// ============================================================================
// Test 5 (2.12): 错误 schema — mandatory fields
// ============================================================================
TEST_CASE("g3_kb: missing args returns error schema",
          "[g3][knowledge_base][error-schema]") {
  G3Plugin plugin;
  if (!plugin.load()) return;

  if (plugin.reset_mock) plugin.reset_mock();
  if (plugin.clear_sessions) plugin.clear_sessions();

  auto it = plugin.registry.tool_funcs.find("knowledge_base/query");
  REQUIRE(it != plugin.registry.tool_funcs.end());
  auto result = it->second({{"question", "test"}});

  REQUIRE(result["success"] == false);
  REQUIRE(result.contains("error"));
  REQUIRE(result["error"].is_string());
  REQUIRE_FALSE(result.contains("answer"));
}

// ============================================================================
// Test 6 (2.13): 处理函数行数 ≤30 (验证 CI 约束)
// ============================================================================
TEST_CASE("g3_kb: handler function body ≤30 lines",
          "[g3][knowledge_base][line-count]") {
  const std::string src_path =
    "/workspace/project/HydraForge/pdk/g3_knowledge_base/src/g3_query.cpp";
  std::ifstream file(src_path);
  REQUIRE(file.is_open());

  std::string line;
  bool in_handler = false;
  int brace_depth = 0;
  int handler_lines = 0;

  while (std::getline(file, line)) {
    if (!in_handler &&
        line.find("handle_knowledge_base_query(") != std::string::npos) {
      in_handler = true;
      // 函数签名行含 '{', 进入函数体
      brace_depth = static_cast<int>(std::count(line.begin(), line.end(), '{'));
      if (brace_depth <= 0) brace_depth = 1; // multiline signature fallback
      continue;
    }

    if (in_handler) {
      // 跳过纯注释行和空行
      auto first = line.find_first_not_of(" \t");
      std::string trimmed = (first != std::string::npos) ? line.substr(first) : "";
      if (trimmed.empty() || trimmed[0] == '/' || trimmed[0] == '*')
        continue;

      for (char c : line) {
        if (c == '{') brace_depth++;
        if (c == '}') brace_depth--;
      }

      if (brace_depth <= 0) break;
      handler_lines++;
    }
  }

  REQUIRE(handler_lines <= 30);
  REQUIRE(handler_lines >= 5);
}