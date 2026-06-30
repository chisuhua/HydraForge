// src/modules/cognitive/simple_orchestrator.cpp
// 文件头注释
// 功能描述：SimpleCognitiveOrchestrator 完整实现 — B 轨道（Track 0.2）单轮 ReAct。
//          流程：构造 prompt → 调用 LLM → 解析 JSON tool_call → 校验/调用工具 → 包装为 ToolResult。
//          错误通过 ToolResult::error() 统一表达，不抛异常至调用方。
//          Phase 1 P1.T2 (2026-06-18): 构造参数类型 ToolRegistry* → IToolRegistry* (依赖倒置, .cpp 无逻辑变化)
// 设计依据：ADR-0015（IPER 闭环）+ ADR-0019（IInteractionBus）+ plan §8-9 + openspec/.../T2 v3
// 作者：AgenticDSL Phase 0 / Track B + Phase 1 P1
// 最后修改日期：2026-06-18 [Phase 1 P1.T2: 构造参数 IToolRegistry*]

#include "agenticdsl/cognitive/simple_orchestrator.h"

#include "common/llm/llm_types.h"

#include <nlohmann/json.hpp>
#include <exception>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <utility>

namespace agenticdsl {

namespace {

ErrorCode llm_error_to_error_code(LLMError::Code code) {
  switch (code) {
    case LLMError::Code::NetworkError:
    case LLMError::Code::RateLimited:
    case LLMError::Code::ServerError:
      return ErrorCode::Retry;
    case LLMError::Code::AuthenticationError:
      return ErrorCode::PermissionDenied;
    case LLMError::Code::ContextOverflow:
      return ErrorCode::ResourceExhausted;
    case LLMError::Code::Cancelled:
    case LLMError::Code::InvalidRequest:
    case LLMError::Code::Unknown:
      return ErrorCode::Unknown;
  }
  return ErrorCode::Unknown;
}

// 将 nlohmann::json args（仅 string/number/bool/null/对象/数组）压平成
// std::unordered_map<string,string>（工具函数签名约定）。
// 对于复杂值，使用 dump() 后作为字符串传递；这是 MVP 简化策略。
void flatten_args(const nlohmann::json& in, std::unordered_map<std::string, std::string>& out) {
  if (!in.is_object()) return;
  for (auto it = in.begin(); it != in.end(); ++it) {
    const std::string& key = it.key();
    const nlohmann::json& v = it.value();
    if (v.is_string()) {
      out[key] = v.get<std::string>();
    } else if (v.is_number_integer()) {
      out[key] = std::to_string(v.get<long long>());
    } else if (v.is_number_float()) {
      out[key] = std::to_string(v.get<double>());
    } else if (v.is_boolean()) {
      out[key] = v.get<bool>() ? "true" : "false";
    } else if (v.is_null()) {
      out[key] = "";
    } else {
      // 对象/数组 -> JSON 字符串
      out[key] = v.dump();
    }
  }
}

} // namespace

SimpleCognitiveOrchestrator::SimpleCognitiveOrchestrator(
    IToolRegistry* registry,  // P1.T2: ToolRegistry* → IToolRegistry* (依赖倒置)
    ILLMProvider* llm)
    : registry_(registry), llm_(llm) {}

void SimpleCognitiveOrchestrator::process(
    const std::string& session_id,
    std::function<void(ToolResult)> on_complete) {
  // 1) 前置检查：依赖缺失
  if (!registry_ || !llm_) {
    if (on_complete) {
      on_complete(ToolResult::error(
          ErrorCode::Unknown,
          "Missing dependencies (registry or llm)"));
    }
    return;
  }

  // 2) 调用 react_once 并捕获所有异常
  try {
    ToolResult result = react_once(session_id);
    if (on_complete) on_complete(result);
  } catch (const std::exception& e) {
    if (on_complete) {
      on_complete(ToolResult::error(
          ErrorCode::Unknown, e.what()));
    }
  } catch (...) {
    if (on_complete) {
      on_complete(ToolResult::error(
          ErrorCode::Unknown,
          "non-std exception thrown"));
    }
  }
}

ToolResult SimpleCognitiveOrchestrator::react_once(const std::string& user_prompt) {
  // 1) 构造 prompt（MVP 硬编码）
  // TODO(mvp): 多轮 + 真实 prompt 模板留待 Phase 1
  const std::string tool_list = "echo"; // MVP：仅 echo 工具
  const std::string prompt =
      "You have access to tools: [" + tool_list + "]\n"
      "Respond with JSON: {\"tool\": \"name\", \"args\": {...}}";

  // 2) 调用 LLM
  GenerationRequest req;
  req.prompt = prompt + "\n[user] " + user_prompt;
  auto result = llm_->generate(req, {});
  if (!result.has_value()) {
    return ToolResult::error(
        llm_error_to_error_code(result.error().code),
        result.error().message);
  }

  // 3) 解析 JSON
  nlohmann::json tool_call;
  try {
    tool_call = nlohmann::json::parse(result.value().text);
  } catch (const std::exception& e) {
    return ToolResult::error(ErrorCode::Unknown, e.what());
  }

  // 4) 验证 tool 字段
  if (!tool_call.is_object() ||
      !tool_call.contains("tool") ||
      !tool_call["tool"].is_string()) {
    return ToolResult::error(
        ErrorCode::Unknown, "Missing 'tool' field");
  }
  std::string tool_name = tool_call["tool"].get<std::string>();

  // 5) 检查工具存在
  if (!registry_->has_tool(tool_name)) {
    return ToolResult::error(
        ErrorCode::ToolNotRegistered, "Tool not registered: " + tool_name);
  }

  // 6) 调用工具（将 json args 扁平化）
  nlohmann::json args_obj =
      tool_call.contains("args") && tool_call["args"].is_object()
          ? tool_call["args"]
          : nlohmann::json::object();
  std::unordered_map<std::string, std::string> flat_args;
  flatten_args(args_obj, flat_args);

  nlohmann::json tool_result;
  try {
    tool_result = registry_->call_tool(tool_name, flat_args);
  } catch (const std::exception& e) {
    return ToolResult::error(
        ErrorCode::Unknown,
        std::string("Tool execution failed: ") + e.what());
  }

  // 7) 工具结果中若含有 "error" 字段，视为工具级失败
  if (tool_result.is_object() && tool_result.contains("error") &&
      tool_result["error"].is_string()) {
    ToolResult r;
    r.ok = false;
    r.data = tool_result;
    r.meta["tool_name"] = tool_name;
    r.meta["error_code"] = std::string("ERR_TOOL.RUNTIME");
    r.meta["error_message"] = tool_result["error"].get<std::string>();
    return r;
  }

  // 8) 包装为成功 ToolResult
  ToolResult r;
  r.ok = true;
  r.data = tool_result;
  r.meta["tool_name"] = tool_name;
  return r;
}

} // namespace agenticdsl