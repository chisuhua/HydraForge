// pdk/g1_coding_assistant/src/g1_agent.cpp
// 功能描述：G1 Coding Assistant 核心 — DEFINE_AGENT + 2-step ReAct handler + manifest (Phase 6 W1)
//          Step 1: 调用 G3 knowledge_base/query 检索知识
//          Step 2: MockLLMProvider (via DSLEngine::set_llm_provider) 合成审查评论
//          Tool manifest: 发现 knowledge_base/query (通过 IToolRegistry::has_tool)
// 设计依据：openspec/changes/phase6-service-ification-v1/
//          tasks.md §3.2-3.7, specs/coding-assistant-agent/spec.md
// 参考范式：pdk/g3_knowledge_base/src/g3_query.cpp (Phase 6 W1)
//          pdk/g3_knowledge_base/src/g3_entry.cpp (entry point pattern)
// 作者：Phase 6 W1 (Sisyphus-Junior)
// 最后修改日期：2026-07-15

#include "g1_state.h"

#include "agenticdsl/contract/itool_registry.h"
#include "agenticdsl/pdk/agent_macros.h"
#include "common/llm/llm_types.h"
#include "common/policy/execution_policy.h"
#include "core/engine.h"

#include <nlohmann/json.hpp>

#include <memory>
#include <string>
#include <unordered_map>

using json = nlohmann::json;

namespace agenticdsl::pdk::g1 {

// ============================================================================
// 单例访问
// ============================================================================

G1State& g1_state() {
  static G1State s;
  return s;
}

// ============================================================================
// Tool manifest 访问器 (测试用)
// ============================================================================

const std::vector<std::string>& g1_tool_manifest() {
  return g1_state().tool_manifest;
}

// ============================================================================
// Step 1: 调用 G3 knowledge_base/query 检索知识
// ============================================================================

static json step1_invoke_g3(const std::string& question,
                            const std::string& session_id) {
  auto* registry = g1_state().registry;
  if (!registry) {
    return {{"success", false}, {"error", "G1: IToolRegistry not available"}};
  }

  if (!registry->has_tool("knowledge_base/query")) {
    return {{"success", false},
            {"error", "G1: knowledge_base/query tool not found in registry"}};
  }

  return registry->call_tool("knowledge_base/query",
                             {{"question", question}, {"session_id", session_id}});
}

// ============================================================================
// Step 2: 通过 DSLEngine + MockLLMProvider 合成最终审查评论
// ============================================================================

static json step2_synthesize(const std::string& request,
                             const std::string& g3_answer,
                             const std::string& code) {
  // 构建合成 prompt
  std::string prompt = "Synthesize a code review comment based on:\n";
  prompt += "Request: " + request + "\n";
  prompt += "Knowledge Base Answer: " + g3_answer + "\n";
  prompt += "Code snippet: " + code.substr(0, 200) + "\n";
  prompt += "Review:";

  // 创建 DSLEngine 并注入 MockLLMProvider (Sprint 19 模式, tasks.md §3.6)
  auto mock = std::make_unique<::agenticdsl::MockLLMProvider>();
  std::string review =
      "[G1 Review] " + g3_answer + " (code reviewed: " + code.substr(0, 30) + "...)";
  mock->enqueue_response(review);

  auto& state = g1_state();
  {
    std::lock_guard lock(state.mtx);
    state.mock_provider = mock.get();  // 捕获裸指针用于测试验证

    state.engine = agenticdsl::DSLEngine::from_markdown("# minimal\n");
    state.engine->set_llm_provider(std::move(mock));
  }

  // 通过 Decorator 链调用 MockLLMProvider.generate() (验证 wiring)
  ::agenticdsl::GenerationRequest gen_req(prompt);
  auto result =
      state.engine->get_llm_provider()->generate(gen_req, std::stop_token{});

  if (!result.has_value()) {
    return {{"success", false},
            {"error", "G1: synthesis LLM call failed"}};
  }

  return {{"success", true}, {"answer", result.value().text}};
}

// ============================================================================
// coding_assistant/review 工具处理函数 (≤30 lines)
// ============================================================================

json handle_coding_assistant_review(
    const std::unordered_map<std::string, std::string>& args) {
  auto r_it = args.find("request");
  auto c_it = args.find("code");
  if (r_it == args.end() || c_it == args.end()) {
    return {{"success", false},
            {"error", "Missing required args: request and code"}};
  }

  std::string request   = r_it->second;
  std::string code      = c_it->second;
  std::string session_id =
      args.find("session_id") != args.end() ? args.find("session_id")->second
                                            : "g1_default_session";

  // Step 1: 调用 G3 knowledge_base/query
  json g3_resp = step1_invoke_g3(request, session_id);
  if (!g3_resp.value("success", false))
    return {{"success", false}, {"error", "G1: G3 query failed"}};

  std::string g3_answer = g3_resp.value("answer", "(no answer)");

  // Step 2: MockLLMProvider 合成最终审查评论
  return step2_synthesize(request, g3_answer, code);
}

// ============================================================================
// 工具注册 (ADR-0051 §Decision 3: register_tool_function, NOT DECLARE_TOOL)
// ============================================================================

void register_g1_tools(::agenticdsl::IToolRegistry& registry) {
  auto& state = g1_state();
  {
    std::lock_guard lock(state.mtx);
    state.registry = &registry;

    // 发现 G3 的 knowledge_base/query 工具 (§3.3: exactly 1 entry)
    if (registry.has_tool("knowledge_base/query")) {
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

// ============================================================================
// DEFINE_AGENT — spec 合规 (§3.2, §3.7: exact 2-parameter form)
// 定义 CodingAssistantAgent class, React 循环类型
// ============================================================================

DEFINE_AGENT(CodingAssistant, ::hydraforge::pdk::AgentLoopType::React)

} // namespace agenticdsl::pdk::g1

// ============================================================================
// Test helpers (extern "C" — dlsym'd by test_g1_coding_assistant)
// ============================================================================

extern "C" {

const std::vector<std::string>* g1_get_tool_manifest() {
  return &agenticdsl::pdk::g1::g1_tool_manifest();
}

::agenticdsl::MockLLMProvider* g1_get_mock_provider() {
  return agenticdsl::pdk::g1::g1_state().mock_provider;
}

void g1_reset_state() {
  auto& state = agenticdsl::pdk::g1::g1_state();
  std::lock_guard lock(state.mtx);
  state.tool_manifest.clear();
  state.registry = nullptr;
  state.mock_provider = nullptr;
  state.engine.reset();
}

} // extern "C"