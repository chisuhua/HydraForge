// pdk/g3_knowledge_base/src/g3_query.cpp
// 功能描述：knowledge_base/query 工具处理函数 + MockLLMProvider 集成
//          注册工具到 IToolRegistry, 内部使用 SessionStore 管理多轮会话。
//          Hardcoded retrieval (3-5 snippets) + MockLLMProvider generate()
// 设计依据：openspec/changes/phase6-service-ification-v1/
//          tasks.md §2.3-2.7, specs/knowledge-base-agent/spec.md
// 参考范式：pdk/llama_engine/src/llama_engine.cpp (C14 tool registration)
// 作者：Phase 6 W1 (Sisyphus-Junior)
// 最后修改日期：2026-07-15

#include "g3_state.h"

#include "agenticdsl/contract/itool_registry.h"
#include "common/llm/mock_provider.h"
#include "common/policy/execution_policy.h"

#include <nlohmann/json.hpp>

#include <string>
#include <unordered_map>

using json = nlohmann::json;

namespace agenticdsl::pdk::g3 {

// ============================================================================
// MockLLMProvider (per Metis H5: 单线程, 每个 test 独立实例)
// ============================================================================

static ::agenticdsl::MockLLMProvider& g3_mock_provider() {
  static ::agenticdsl::MockLLMProvider instance;
  return instance;
}

void g3_reset_mock() { g3_mock_provider().reset(); }
void g3_enqueue_response(const std::string& text) { g3_mock_provider().enqueue_response(text); }
const std::vector<::agenticdsl::GenerationRequest>& g3_call_history() { return g3_mock_provider().call_history(); }

// ============================================================================
// SessionStore (static singleton)
// ============================================================================

static SessionStore& g3_sessions() {
  static SessionStore store;
  return store;
}

void g3_clear_sessions() { g3_sessions().clear(); }

// ============================================================================
// knowledge_base/query 工具处理函数 (≤30 lines)
// ============================================================================

json handle_knowledge_base_query(const std::unordered_map<std::string, std::string>& args_map) {
  auto q_it = args_map.find("question");
  auto s_it = args_map.find("session_id");
  if (q_it == args_map.end() || s_it == args_map.end())
    return {{"success", false}, {"error", "Missing required args: question and session_id"}};
  std::string question = q_it->second;
  std::string session_id = s_it->second;
  auto& store = g3_sessions();
  store.get_or_create(session_id);
  std::string context = store.build_context(session_id);
  ::agenticdsl::GenerationRequest req{context + "Q: " + question + "\nA:"};
  auto result = g3_mock_provider().generate(req, std::stop_token{});
  if (!result.has_value())
    return {{"success", false}, {"error", result.error().message}};
  store.append(session_id, question, result.value().text);
  return {{"success", true}, {"answer", result.value().text}};
}

// ============================================================================
// 工具注册 (ADR-0051 §Decision 3: register_tool_function, NOT DECLARE_TOOL)
// ============================================================================

void register_g3_tools(::agenticdsl::IToolRegistry& registry) {
  ::agenticdsl::ToolMetadata meta{
    "knowledge_base/query",                                       // (1) name
    "G3 Knowledge Base 检索 + LLM 问答 (multi-turn session)",     // (2) description
    "knowledge",                                                  // (3) domain
    ::agenticdsl::ToolCategory::Execute,                          // (4) category
    ::agenticdsl::LayerProfile::Workflow,                         // (5) min_layer
    ::agenticdsl::ApprovalPolicy{true, true, false, false},       // (6) plan+agent 审批
    {::agenticdsl::LayerProfile::Workflow},                       // (7) allowed_layers
    0.0,                                                          // (8) cost_estimate
    30000                                                         // (9) timeout_ms
  };

  registry.register_tool_function("knowledge_base/query", meta, handle_knowledge_base_query);
}

} // namespace agenticdsl::pdk::g3