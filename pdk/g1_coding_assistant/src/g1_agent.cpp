// pdk/g1_coding_assistant/src/g1_agent.cpp
// 功能描述：G1 Coding Assistant 核心 — DEFINE_AGENT + 2-step ReAct handler + manifest (Phase 6 W1)
//          Step 1: 调用 G3 knowledge_base/query 检索知识
//          Step 2: 通过 LLM callback 合成审查评论 (避免 .so 链接 agenticdsl_common)
//          Tool manifest: 发现 knowledge_base/query (通过 IToolRegistry::has_tool)
//          LLM callback 模式与 G3 的 g3_set_llm_callback 一致
// 设计依据：openspec/changes/phase6-service-ification-v1/
//          tasks.md §3.2-3.7, specs/coding-assistant-agent/spec.md
// 参考范式：pdk/g3_knowledge_base/src/g3_query.cpp (Phase 6 W1)
// 作者：Phase 6 W1 (Sisyphus-Junior)
// 最后修改日期：2026-07-15

#include "g1_state.h"

#include "agenticdsl/contract/itool_registry.h"
#include "agenticdsl/pdk/agent_macros.h"
#include "common/policy/execution_policy.h"

#include <nlohmann/json.hpp>

#include <functional>
#include <string>
#include <unordered_map>

using json = nlohmann::json;

namespace agenticdsl::pdk::g1 {

G1State& g1_state() {
  static G1State s;
  return s;
}

const std::vector<std::string>& g1_tool_manifest() {
  return g1_state().tool_manifest;
}

void g1_set_llm_callback(
    std::function<std::string(const std::string& prompt)> cb) {
  auto& state = g1_state();
  std::lock_guard lock(state.mtx);
  state.llm_callback = std::move(cb);
}

// Step 1: 调用 G3 knowledge_base/query 检索知识
static json step1_invoke_g3(const std::string& question,
                            const std::string& session_id) {
  auto* registry = g1_state().registry;
  if (!registry)
    return {{"success", false}, {"error", "G1: IToolRegistry not available"}};

  if (!registry->has_tool("knowledge_base/query"))
    return {{"success", false},
            {"error", "G1: knowledge_base/query tool not found"}};

  return registry->call_tool("knowledge_base/query",
                             {{"question", question}, {"session_id", session_id}});
}

// Step 2: 通过 LLM callback 合成审查评论
static json step2_synthesize(const std::string& request,
                             const std::string& g3_answer,
                             const std::string& code) {
  auto& state = g1_state();
  std::string review;

  {
    std::lock_guard lock(state.mtx);
    if (state.llm_callback) {
      std::string prompt = "Synthesize review from: Request='" + request +
                           "', KB_Answer='" + g3_answer +
                           "', Code='" + code.substr(0, 200) + "'";
      review = state.llm_callback(prompt);
      state.llm_call_count++;
    }
  }

  if (review.empty()) {
    review = "[G1 Review] " + g3_answer +
             " (code reviewed: " + code.substr(0, 30) + "...)";
  }

  return {{"success", true}, {"answer", review}};
}

json handle_coding_assistant_review(
    const std::unordered_map<std::string, std::string>& args) {
  auto r_it = args.find("request");
  auto c_it = args.find("code");
  if (r_it == args.end() || c_it == args.end())
    return {{"success", false}, {"error", "Missing args: request and code"}};

  std::string request = r_it->second;
  std::string code = c_it->second;
  std::string session_id =
      args.find("session_id") != args.end() ? args.find("session_id")->second
                                            : "g1_default_session";

  json g3_resp = step1_invoke_g3(request, session_id);
  if (!g3_resp.value("success", false))
    return {{"success", false}, {"error", "G1: G3 query failed"}};

  return step2_synthesize(request, g3_resp.value("answer", ""), code);
}

void register_g1_tools(::agenticdsl::IToolRegistry& registry) {
  auto& state = g1_state();
  {
    std::lock_guard lock(state.mtx);
    state.registry = &registry;
    if (registry.has_tool("knowledge_base/query") &&
        state.tool_manifest.empty()) {
      state.tool_manifest.push_back("knowledge_base/query");
    }
  }

  ::agenticdsl::ToolMetadata meta{
      "coding_assistant/review",
      "G1 Coding Assistant — 2-step ReAct code review with G3 KB",
      "coding",
      ::agenticdsl::ToolCategory::Execute,
      ::agenticdsl::LayerProfile::Workflow,
      ::agenticdsl::ApprovalPolicy{true, true, false, false},
      {::agenticdsl::LayerProfile::Workflow},
      0.0,
      30000};

  registry.register_tool_function("coding_assistant/review", meta,
                                  handle_coding_assistant_review);
}

DEFINE_AGENT(CodingAssistant, ::hydraforge::pdk::AgentLoopType::React)

} // namespace agenticdsl::pdk::g1

extern "C" {

const std::vector<std::string>* g1_get_tool_manifest() {
  return &agenticdsl::pdk::g1::g1_tool_manifest();
}

int g1_get_llm_call_count() {
  return agenticdsl::pdk::g1::g1_state().llm_call_count;
}

void g1_reset_state() {
  auto& state = agenticdsl::pdk::g1::g1_state();
  std::lock_guard lock(state.mtx);
  state.tool_manifest.clear();
  state.registry = nullptr;
  state.llm_callback = nullptr;
  state.llm_call_count = 0;
}

} // extern "C"
