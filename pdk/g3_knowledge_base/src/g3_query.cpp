// pdk/g3_knowledge_base/src/g3_query.cpp
// 功能描述：knowledge_base/query 工具处理函数 + 内部 LLM 回调
//          注册工具到 IToolRegistry, 使用 SessionStore 管理多轮会话。
//          Hardcoded retrieval (3-5 snippets) + pluggable LLM callback
//          避免依赖 agenticdsl_core (MockLLMProvider vtable/fPIC 冲突)
// 设计依据：openspec/changes/phase6-service-ification-v1/
//          tasks.md §2.3-2.7, specs/knowledge-base-agent/spec.md
// 参考范式：pdk/llama_engine/src/llama_engine.cpp (C14 tool registration)
// 作者：Phase 6 W1 (Sisyphus-Junior)
// 最后修改日期：2026-07-15

#include "g3_state.h"

#include "agenticdsl/contract/itool_registry.h"
#include "common/policy/execution_policy.h"

#include <nlohmann/json.hpp>

#include <chrono>
#include <functional>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

using json = nlohmann::json;

namespace agenticdsl::pdk::g3 {

// ============================================================================
// Internal LLM callback system (避免 MockLLMProvider vtable/fPIC 冲突)
// ============================================================================

/// LLM 回调签名: prompt → answer
using LLMCallback = std::function<std::string(const std::string& prompt)>;

namespace {
  std::mutex g_cb_mutex;
  LLMCallback g_llm_cb;
  std::vector<std::string> g_call_prompts;
  std::vector<std::string> g_queued_responses;
}

void g3_set_llm_callback(LLMCallback cb) {
  std::lock_guard lock(g_cb_mutex);
  g_llm_cb = std::move(cb);
}

void g3_reset_mock() {
  std::lock_guard lock(g_cb_mutex);
  g_call_prompts.clear();
  g_queued_responses.clear();
}

void g3_enqueue_response(const std::string& text) {
  std::lock_guard lock(g_cb_mutex);
  g_queued_responses.push_back(text);
}

const std::vector<std::string>& g3_call_history() {
  std::lock_guard lock(g_cb_mutex);
  return g_call_prompts;
}

/// 内部 LLM 调用: 消费队列响应, 记录 prompt
static std::string g3_internal_llm(const std::string& prompt) {
  std::lock_guard lock(g_cb_mutex);
  if (g_llm_cb) return g_llm_cb(prompt);

  g_call_prompts.push_back(prompt);
  if (!g_queued_responses.empty()) {
    std::string r = g_queued_responses.front();
    g_queued_responses.erase(g_queued_responses.begin());
    return r;
  }
  return "G3: no response queued";
}

// ============================================================================
// SessionStore (static singleton)
// ============================================================================

static SessionStore& g3_sessions() {
  static SessionStore store;
  return store;
}

void g3_clear_sessions() { g3_sessions().clear(); }

// ============================================================================
// Audit record (plugin-internal, ADR-0051 §Decision 5)
// ============================================================================

struct G3AuditRecord {
  std::string caller_session_id;
  std::string callee_tool_name;
  std::vector<std::string> args_keys_only;
  int64_t return_latency_ms = 0;
  bool callee_internally_invoked_llm = false;
};

static G3AuditRecord g_last_audit;

static std::vector<std::string> collect_arg_keys(
    const std::unordered_map<std::string, std::string>& m) {
  std::vector<std::string> keys; keys.reserve(m.size());
  for (const auto& [k, v] : m) keys.push_back(k);
  return keys;
}

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
  auto t0 = std::chrono::steady_clock::now();
  std::string answer = g3_internal_llm(context + "Q: " + question + "\nA:");
  auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
      std::chrono::steady_clock::now() - t0).count();
  g_last_audit = {session_id, "knowledge_base/query", collect_arg_keys(args_map), ms, true};
  if (answer.empty())
    return {{"success", false}, {"error", "LLM returned empty response"}};
  store.append(session_id, question, answer);
  return {{"success", true}, {"answer", answer}};
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

// ============================================================================
// Test helpers (extern "C" — dlsym'd by test_g3_knowledge_base)
// ============================================================================

extern "C" {

void g3_kb_reset_mock() { agenticdsl::pdk::g3::g3_reset_mock(); }
void g3_kb_enqueue_response(const char* text) { agenticdsl::pdk::g3::g3_enqueue_response(text); }
void g3_kb_clear_sessions() { agenticdsl::pdk::g3::g3_clear_sessions(); }

const char* g3_kb_audit_session_id() {
  return agenticdsl::pdk::g3::g_last_audit.caller_session_id.c_str();
}
const char* g3_kb_audit_tool_name() {
  return agenticdsl::pdk::g3::g_last_audit.callee_tool_name.c_str();
}
int g3_kb_audit_args_keys_count() {
  return static_cast<int>(agenticdsl::pdk::g3::g_last_audit.args_keys_only.size());
}
const char* g3_kb_audit_args_key(int idx) {
  auto& keys = agenticdsl::pdk::g3::g_last_audit.args_keys_only;
  if (idx < 0 || static_cast<size_t>(idx) >= keys.size()) return "";
  return keys[idx].c_str();
}
int64_t g3_kb_audit_latency_ms() {
  return agenticdsl::pdk::g3::g_last_audit.return_latency_ms;
}
int g3_kb_audit_llm_invoked() {
  return agenticdsl::pdk::g3::g_last_audit.callee_internally_invoked_llm ? 1 : 0;
}

} // extern "C"