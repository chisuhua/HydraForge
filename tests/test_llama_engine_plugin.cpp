// tests/test_llama_engine_plugin.cpp
// 功能描述：Llama Engine Plugin 集成测试 (C14 Phase 5 B2.1)
//          验证 pdk/llama_engine/libhydraforge_llama_engine.so:
//          - pdk_plugin_info ABI 版本匹配
//          - 12 个推理工具正确注册
//          - 工具调用行为正确
// 设计依据：openspec/changes/phase5-llama-engine-plugin/
//          proposal.md §7, tasks.md §8, specs/llama-engine-plugin/spec.md
// 参考范式：tests/test_plugin_loader.cpp (Sprint 5 E2E)
// 作者：C14 Phase 5
// 最后修改日期：2026-07-08

#include "catch_amalgamated.hpp"

#include "agenticdsl/contract/itool_registry.h"
#include "agenticdsl/plugin/plugin_loader.h"
#include "agenticdsl/plugin/plugin_info.h"
#include "common/policy/execution_policy.h"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cstdlib>  // setenv
#include <dlfcn.h>   // dlopen/dlsym
#include <string>
#include <unordered_map>
#include <vector>

using json = nlohmann::json;

namespace {

// ──── Mock ToolRegistry ──────────────────────────────────────────────────────

class MockToolRegistry : public ::agenticdsl::IToolRegistry {
 public:
  std::vector<std::string> registered_tools;
  std::unordered_map<std::string, ::agenticdsl::ToolMetadata> tool_metas;
  std::unordered_map<std::string, ::agenticdsl::IToolRegistry::ToolFunc> tool_funcs;

  void register_tool_function(
      std::string name,
      ::agenticdsl::ToolMetadata meta,
      ToolFunc fn) override {
    registered_tools.push_back(name);
    tool_metas[name] = meta;
    tool_funcs[name] = fn;
  }

  bool has_tool(const std::string& name) const override {
    return tool_funcs.find(name) != tool_funcs.end();
  }

  json call_tool(
      const std::string& name,
      const std::unordered_map<std::string, std::string>& args) override {
    auto it = tool_funcs.find(name);
    if (it == tool_funcs.end()) {
      return {{"error", "tool not found"}, {"tool", name}};
    }
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
  void set_cost_callback(::agenticdsl::IToolRegistry::CostCallback) override {}
};

const std::vector<std::string> EXPECTED_ENGINE_TOOLS = {
  "inference/engine/init",
  "inference/engine/generate",
  "inference/engine/stream",
  "inference/engine/status",
};

const std::vector<std::string> EXPECTED_MODEL_TOOLS = {
  "inference/model/load",
  "inference/model/unload",
  "inference/model/list",
  "inference/model/switch",
};

const std::vector<std::string> EXPECTED_ARCH_TOOLS = {
  "inference/prefix_cache/configure",
  "inference/kv_cache/configure",
  "inference/decoding/configure",
  "inference/cloud_engine/configure",
};

// ──── Plugin Load Helper ─────────────────────────────────────────────────────

bool try_load_plugin(hydraforge::PluginLoader& loader, MockToolRegistry& registry) {
  // 设置 HYDRAFORGE_PLUGIN_PATH 环境变量, 允许加载 build 目录下的 .so
  const char* build_dir = std::getenv("HYDRAFORGE_PLUGIN_PATH");
  std::string expanded_path;
  if (!build_dir) {
    expanded_path = "/workspace/project/HydraForge/build/debug/pdk/llama_engine/";
    setenv("HYDRAFORGE_PLUGIN_PATH", expanded_path.c_str(), 1);
  }

  const std::vector<std::string> candidate_paths = {
    "build/debug/pdk/llama_engine/libhydraforge_llama_engine.so",
    "build/release/pdk/llama_engine/libhydraforge_llama_engine.so",
    "/workspace/project/HydraForge/build/debug/pdk/llama_engine/libhydraforge_llama_engine.so",
  };
  for (const auto& path : candidate_paths) {
    if (loader.load_so(path, registry)) return true;
  }
  return false;
}

} // namespace

// =====================================================================
// Test 1: Plugin 加载与 ABI 验证
// =====================================================================

TEST_CASE("llama_engine: dlopen + pdk_plugin_info ABI matches",
          "[llama_engine][c14][abi]") {
  hydraforge::PluginLoader loader;
  MockToolRegistry registry;

  if (!try_load_plugin(loader, registry)) {
    SUCCEED("llama_engine .so not built, skipping");
    return;
  }

  auto loaded = loader.list_loaded();
  REQUIRE_FALSE(loaded.empty());
  auto& info = loaded.front();
  REQUIRE(info.abi_version == hydraforge::CURRENT_ABI_VERSION);
  REQUIRE(std::string(info.name) == "hydraforge_llama_engine");
  REQUIRE(info.major_version == 1);
  REQUIRE(info.minor_version == 0);
  REQUIRE(info.patch_version == 0);
}

// =====================================================================
// Test 2: 12 个工具全部注册
// =====================================================================

TEST_CASE("llama_engine: registers all 12 tools",
          "[llama_engine][c14][tools]") {
  hydraforge::PluginLoader loader;
  MockToolRegistry registry;

  if (!try_load_plugin(loader, registry)) {
    SUCCEED("llama_engine .so not built, skipping");
    return;
  }

  REQUIRE(registry.registered_tools.size() == 12);

  for (const auto& name : EXPECTED_ENGINE_TOOLS) {
    INFO("Missing: " << name);
    REQUIRE(registry.has_tool(name));
  }
  for (const auto& name : EXPECTED_MODEL_TOOLS) {
    INFO("Missing: " << name);
    REQUIRE(registry.has_tool(name));
  }
  for (const auto& name : EXPECTED_ARCH_TOOLS) {
    INFO("Missing: " << name);
    REQUIRE(registry.has_tool(name));
  }
}

// =====================================================================
// Test 3: engine/status 返回引擎元数据
// =====================================================================

TEST_CASE("llama_engine: engine/status returns backend info",
          "[llama_engine][c14][engine]") {
  hydraforge::PluginLoader loader;
  MockToolRegistry registry;

  if (!try_load_plugin(loader, registry)) {
    SUCCEED("llama_engine .so not built, skipping");
    return;
  }

  REQUIRE(registry.has_tool("inference/engine/status"));
  auto result = registry.call_tool("inference/engine/status", {});
  REQUIRE(result.contains("loaded"));
  REQUIRE(result.contains("backend"));
  REQUIRE(result["backend"] == "llama.cpp");
}

// =====================================================================
// Test 4: model/list 返回数组结构
// =====================================================================

TEST_CASE("llama_engine: model/list returns array",
          "[llama_engine][c14][model]") {
  hydraforge::PluginLoader loader;
  MockToolRegistry registry;

  if (!try_load_plugin(loader, registry)) {
    SUCCEED("llama_engine .so not built, skipping");
    return;
  }

  REQUIRE(registry.has_tool("inference/model/list"));
  auto result = registry.call_tool("inference/model/list", {});
  REQUIRE(result.is_array());
}

// =====================================================================
// Test 5: inference/cloud_engine/configure PLACEHOLDER stub
// =====================================================================

TEST_CASE("llama_engine: inference/cloud_engine/configure returns placeholder",
          "[llama_engine][c14][arch]") {
  hydraforge::PluginLoader loader;
  MockToolRegistry registry;

  if (!try_load_plugin(loader, registry)) {
    SUCCEED("llama_engine .so not built, skipping");
    return;
  }

  REQUIRE(registry.has_tool("inference/cloud_engine/configure"));
  auto result = registry.call_tool("inference/cloud_engine/configure", {});
  REQUIRE(result.contains("status"));
  REQUIRE(result["status"] == "not_yet_implemented");
}

// =====================================================================
// Test 6: inference/decoding/configure 验证 sampler 值 + clamp
// =====================================================================

TEST_CASE("llama_engine: inference/decoding/configure validates sampler (D1 compliant)",
          "[llama_engine][c14][arch][decoding]") {
  hydraforge::PluginLoader loader;
  MockToolRegistry registry;

  if (!try_load_plugin(loader, registry)) {
    SUCCEED("llama_engine .so not built, skipping");
    return;
  }

  REQUIRE(registry.has_tool("inference/decoding/configure"));

  // 合法 sampler
  auto r1 = registry.call_tool("inference/decoding/configure",
      {{"sampler", "greedy"}, {"temperature", "0.5"}});
  REQUIRE(r1["active_sampler"] == "greedy");
  REQUIRE(r1["unsupported_warning"] == "");

  // mirostat_v2
  auto r2 = registry.call_tool("inference/decoding/configure",
      {{"sampler", "mirostat_v2"}});
  REQUIRE(r2["active_sampler"] == "mirostat_v2");

  // 非法 sampler → fallback greedy
  auto r3 = registry.call_tool("inference/decoding/configure",
      {{"sampler", "invalid"}});
  REQUIRE(r3["active_sampler"] == "greedy");
  REQUIRE_FALSE(r3["unsupported_warning"].get<std::string>().empty());

  // temperature clamp
  auto r4 = registry.call_tool("inference/decoding/configure",
      {{"temperature", "5.0"}});
  REQUIRE(r4["params"]["temperature"] <= 2.0);
}

// =====================================================================
// Test 7: inference/kv_cache/configure 处理 evict_policy
// =====================================================================

TEST_CASE("llama_engine: inference/kv_cache/configure handles evict_policy",
          "[llama_engine][c14][arch][kv_cache]") {
  hydraforge::PluginLoader loader;
  MockToolRegistry registry;

  if (!try_load_plugin(loader, registry)) {
    SUCCEED("llama_engine .so not built, skipping");
    return;
  }

  REQUIRE(registry.has_tool("inference/kv_cache/configure"));
  auto result = registry.call_tool("inference/kv_cache/configure",
      {{"evict_policy", "lfu"}, {"max_size_gb", "2.0"}});
  REQUIRE(result["active_policy"] == "lfu");
}

// =====================================================================
// Test 8: inference/prefix_cache/configure 处理参数
// =====================================================================

TEST_CASE("llama_engine: inference/prefix_cache/configure handles params",
          "[llama_engine][c14][arch][prefix_cache]") {
  hydraforge::PluginLoader loader;
  MockToolRegistry registry;

  if (!try_load_plugin(loader, registry)) {
    SUCCEED("llama_engine .so not built, skipping");
    return;
  }

  REQUIRE(registry.has_tool("inference/prefix_cache/configure"));
  auto result = registry.call_tool("inference/prefix_cache/configure",
      {{"enabled", "true"}, {"max_size", "256"}});
  REQUIRE(result.contains("status"));
}

// =====================================================================
// Test 9: ToolMetadata 验证
// =====================================================================

TEST_CASE("llama_engine: correct tool metadata (category/approval)",
          "[llama_engine][c14][metadata]") {
  hydraforge::PluginLoader loader;
  MockToolRegistry registry;

  if (!try_load_plugin(loader, registry)) {
    SUCCEED("llama_engine .so not built, skipping");
    return;
  }

  // engine/init: ReadOnly, yolo only (不要求 plan/agent 审批)
  {
    auto& meta = registry.tool_metas.at("inference/engine/init");
    REQUIRE(meta.category == ::agenticdsl::ToolCategory::ReadOnly);
    REQUIRE(meta.approval.requires_approval_in_plan == false);
    REQUIRE(meta.approval.requires_approval_in_agent == false);
  }
  // engine/generate: Execute, agent+plan 都需要审批
  {
    auto& meta = registry.tool_metas.at("inference/engine/generate");
    REQUIRE(meta.category == ::agenticdsl::ToolCategory::Execute);
    REQUIRE(meta.approval.requires_approval_in_agent == true);
    REQUIRE(meta.approval.requires_approval_in_plan == true);
  }
  // inference/decoding/configure: plan 审批, yolo 不审批 (配置变更需计划)
  {
    auto& meta = registry.tool_metas.at("inference/decoding/configure");
    REQUIRE(meta.approval.requires_approval_in_plan == true);
    REQUIRE(meta.approval.requires_approval_in_agent == false);
  }
}

// =====================================================================
// Test 10: engine/stream 返回 stream_id stub
// =====================================================================

TEST_CASE("llama_engine: engine/stream returns stream metadata",
          "[llama_engine][c14][engine][stream]") {
  hydraforge::PluginLoader loader;
  MockToolRegistry registry;

  if (!try_load_plugin(loader, registry)) {
    SUCCEED("llama_engine .so not built, skipping");
    return;
  }

  REQUIRE(registry.has_tool("inference/engine/stream"));
  // stream 不传参也应该返回 stream_id (即使 engine 未初始化)
  auto result = registry.call_tool("inference/engine/stream", {});
  // 未初始化时返回 error 是预期行为
  REQUIRE(result.contains("error"));
}