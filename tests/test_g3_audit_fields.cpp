// tests/test_g3_audit_fields.cpp
// 功能描述：G3 审计字段验证 (Phase 6 W1 §5.2-5.3)
//          验证 knowledge_base/query 调用后 5 个审计字段正确填充
//          Audit fields: caller_session_id / callee_tool_name / args_keys_only
//                       / return_latency_ms / callee_internally_invoked_llm
// 设计依据：openspec/changes/phase6-service-ification-v1/
//          tasks.md §5.2-5.3, ADR-0051 §Decision 5
// 参考范式：tests/test_g3_knowledge_base.cpp (dlsym pattern)
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
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

using json = nlohmann::json;

namespace {

class MockToolRegistry : public ::agenticdsl::IToolRegistry {
 public:
  std::unordered_map<std::string, ToolFunc> tool_funcs;

  void register_tool_function(std::string name, ::agenticdsl::ToolMetadata, ToolFunc fn) override {
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
  std::vector<std::string> list_tools() const override { return {}; }
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

struct G3AuditPlugin {
  ::hydraforge::PluginLoader loader;
  MockToolRegistry registry;
  void* handle = nullptr;

  // existing exported functions
  using ResetFn  = void (*)();
  using EnqFn    = void (*)(const char*);
  using ClearFn  = void (*)();

  ResetFn reset_mock   = nullptr;
  EnqFn   enqueue     = nullptr;
  ClearFn clear_sessions = nullptr;

  // new audit field getters
  using StrFn    = const char* (*)();
  using IntFn    = int (*)();
  using Int64Fn  = int64_t (*)();
  using KeyFn    = const char* (*)(int);

  StrFn   audit_session_id = nullptr;
  StrFn   audit_tool_name  = nullptr;
  IntFn   audit_args_keys_count = nullptr;
  KeyFn   audit_args_key   = nullptr;
  Int64Fn audit_latency_ms = nullptr;
  IntFn   audit_llm_invoked = nullptr;

  bool load() {
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

    // audit field getters
    audit_session_id    = reinterpret_cast<StrFn>(dlsym(handle, "g3_kb_audit_session_id"));
    audit_tool_name     = reinterpret_cast<StrFn>(dlsym(handle, "g3_kb_audit_tool_name"));
    audit_args_keys_count = reinterpret_cast<IntFn>(dlsym(handle, "g3_kb_audit_args_keys_count"));
    audit_args_key      = reinterpret_cast<KeyFn>(dlsym(handle, "g3_kb_audit_args_key"));
    audit_latency_ms    = reinterpret_cast<Int64Fn>(dlsym(handle, "g3_kb_audit_latency_ms"));
    audit_llm_invoked   = reinterpret_cast<IntFn>(dlsym(handle, "g3_kb_audit_llm_invoked"));

    return true;
  }

  ~G3AuditPlugin() { if (handle) dlclose(handle); }

  json query(const std::string& question, const std::string& session_id) {
    auto it = registry.tool_funcs.find("knowledge_base/query");
    REQUIRE(it != registry.tool_funcs.end());
    return it->second({{"question", question}, {"session_id", session_id}});
  }
};

} // namespace

// ============================================================================
// Test: 5 audit fields present after 1 invocation
// ============================================================================
TEST_CASE("g3_kb: audit fields populated after single query",
          "[g3][knowledge_base][audit]") {
  G3AuditPlugin plugin;
  if (!plugin.load()) return;

  if (plugin.reset_mock) plugin.reset_mock();
  if (plugin.clear_sessions) plugin.clear_sessions();
  if (plugin.enqueue) plugin.enqueue("HydraForge is an execution engine.");

  // Verify all 6 audit getters are available
  REQUIRE(plugin.audit_session_id != nullptr);
  REQUIRE(plugin.audit_tool_name != nullptr);
  REQUIRE(plugin.audit_args_keys_count != nullptr);
  REQUIRE(plugin.audit_args_key != nullptr);
  REQUIRE(plugin.audit_latency_ms != nullptr);
  REQUIRE(plugin.audit_llm_invoked != nullptr);

  // Invoke once
  auto result = plugin.query("What is HydraForge?", "audit_session_1");
  REQUIRE(result["success"] == true);

  // Verify field 1: caller_session_id
  const char* sid = plugin.audit_session_id();
  REQUIRE(sid != nullptr);
  REQUIRE(std::string(sid) == "audit_session_1");

  // Verify field 2: callee_tool_name
  const char* tname = plugin.audit_tool_name();
  REQUIRE(tname != nullptr);
  REQUIRE(std::string(tname) == "knowledge_base/query");

  // Verify field 3: args_keys_only (keys present, no values)
  int key_count = plugin.audit_args_keys_count();
  REQUIRE(key_count == 2);  // question, session_id
  bool has_question = false, has_session_id = false;
  for (int i = 0; i < key_count; ++i) {
    const char* key = plugin.audit_args_key(i);
    REQUIRE(key != nullptr);
    std::string ks(key);
    if (ks == "question") has_question = true;
    if (ks == "session_id") has_session_id = true;
  }
  REQUIRE(has_question);
  REQUIRE(has_session_id);

  // Verify field 4: return_latency_ms (positive)
  REQUIRE(plugin.audit_latency_ms() >= 0);

  // Verify field 5: callee_internally_invoked_llm (true after query)
  REQUIRE(plugin.audit_llm_invoked() == 1);
}

// ============================================================================
// Test: args_keys_only does NOT leak values (defense-in-depth)
// ============================================================================
TEST_CASE("g3_kb: audit args_keys_only excludes values",
          "[g3][knowledge_base][audit][security]") {
  G3AuditPlugin plugin;
  if (!plugin.load()) return;

  if (plugin.reset_mock) plugin.reset_mock();
  if (plugin.clear_sessions) plugin.clear_sessions();
  if (plugin.enqueue) plugin.enqueue("test answer");

  auto result = plugin.query("secret question", "sensitive_session_id");
  REQUIRE(result["success"] == true);

  int key_count = plugin.audit_args_keys_count();
  for (int i = 0; i < key_count; ++i) {
    const char* key = plugin.audit_args_key(i);
    std::string ks(key);
    // Keys must be present in the key set
    REQUIRE((ks == "question" || ks == "session_id"));
    // Values must NOT appear as keys (defense-in-depth)
    REQUIRE(ks != "secret question");
    REQUIRE(ks != "sensitive_session_id");
  }
}
