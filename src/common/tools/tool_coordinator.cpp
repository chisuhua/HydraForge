// src/common/tools/tool_coordinator.cpp
// 功能描述：ToolCoordinator 实现 (layer check + approval + audit log)
// 作者：AgenticDSL Phase3 / Sprint 14 C4 ship
// 最后修改日期：2026-06-29
#include "common/tools/tool_coordinator.h"

#include <atomic>
#include <chrono>
#include <cstdint>
#include <ctime>
#include <iomanip>
#include <sstream>
#include <stdexcept>

#include "common/policy/layer_profile.h"

namespace agenticdsl {

namespace {

// ISO 8601 timestamp helper
std::string now_iso8601() {
  auto now = std::chrono::system_clock::now();
  auto time_t_now = std::chrono::system_clock::to_time_t(now);
  std::tm tm_buf{};
  gmtime_r(&time_t_now, &tm_buf);
  std::ostringstream oss;
  oss << std::put_time(&tm_buf, "%Y-%m-%dT%H:%M:%SZ");
  return oss.str();
}

// request_id 生成器 (与 ApprovalHandler 内部生成器一致, 简单递增)
std::string generate_request_id() {
  static std::atomic<std::uint64_t> counter{0};
  auto now = std::chrono::steady_clock::now().time_since_epoch().count();
  return "tc-" + std::to_string(now) + "-" + std::to_string(counter++);
}

// 构建 audit event meta (去重模板)
nlohmann::json audit_meta(const std::string& event,
                           const std::string& request_id,
                           const std::string& tool_name,
                           const std::string& category,
                           const std::string& caller_layer,
                           const std::string& session_id) {
  return {
    {"event", event},
    {"request_id", request_id},
    {"tool_name", tool_name},
    {"category", category},
    {"caller_layer", caller_layer},
    {"session_id", session_id},
    {"timestamp_iso8601", now_iso8601()}
  };
}

}  // namespace

ToolCoordinator::ToolCoordinator(IToolRegistry& registry,
                                 std::shared_ptr<IExecutionPolicy> policy,
                                 ApprovalCallback callback,
                                 std::shared_ptr<IInteractionBus> bus,
                                 int default_timeout_ms)
    : registry_(registry),
      policy_(std::move(policy)),
      approval_handler_(std::make_unique<ApprovalHandler>(
          policy_, std::move(callback), default_timeout_ms)),
      bus_(std::move(bus)) {}

ToolResult ToolCoordinator::execute(
    const ToolMetadata& meta,
    const ToolCallContext& ctx,
    const std::unordered_map<std::string, std::string>& args) {
  const std::string request_id = generate_request_id();
  const auto start = std::chrono::steady_clock::now();

  const std::string cat_str = to_string(meta.category);
  const std::string tool_name = meta.name;

  // ===== Step 1: Layer check (ADR-0004 §8 矩阵 via layer_profile.h) =====
  try {
    LayerProfile caller_layer = parse_layer(ctx.caller_layer);
    if (!check_layer_permission(caller_layer, meta.category)) {
      if (bus_) {
        bus_->emit("tool.audit.denied",
            ToolResult::error(ErrorCode::PermissionDenied,
                "Layer '" + ctx.caller_layer + "' cannot call category '" +
                    cat_str + "'",
                audit_meta("tool.audit.denied", request_id, tool_name,
                           cat_str, ctx.caller_layer, ctx.session_id)));
      }
      return ToolResult::error(
          ErrorCode::PermissionDenied,
          "Layer permission denied: '" + ctx.caller_layer +
              "' cannot call category '" + cat_str + "'");
    }
  } catch (const std::invalid_argument& e) {
    return ToolResult::error(ErrorCode::Unknown, e.what());
  }

  // ===== Step 2: Approval check (委托 ApprovalHandler) =====
  ToolPreview preview;
  preview.command_line = meta.name + "(" + std::to_string(args.size()) + " args)";
  preview.risk_summary = "Tool category: " + cat_str;

  if (!approval_handler_->process_request(meta, ctx, preview)) {
    if (bus_) {
      bus_->emit("tool.audit.denied",
          ToolResult::error(ErrorCode::PermissionDenied,
              "Approval denied by policy",
              audit_meta("tool.audit.denied", request_id, tool_name,
                         cat_str, ctx.caller_layer, ctx.session_id)));
    }
    return ToolResult::error(ErrorCode::PermissionDenied,
                            "Approval denied by policy for tool: " + meta.name);
  }

  // ===== Step 3: emit tool.audit.invoked =====
  if (bus_) {
    nlohmann::json invoked_meta = audit_meta(
        "tool.audit.invoked", request_id, tool_name,
        cat_str, ctx.caller_layer, ctx.session_id);
    invoked_meta["args_keys_count"] = std::to_string(args.size());
    bus_->emit("tool.audit.invoked", ToolResult::success({}, std::move(invoked_meta)));
  }

  // ===== Step 4: 实际调用 registry (返回 nlohmann::json) =====
  nlohmann::json raw_result = registry_.call_tool(meta.name, args);

  // 将 json 结果转换为 ToolResult 信封
  // ToolRegistry 约定: error 时返回 {"error": "..."}
  // SecureToolRegistry 约定: error 时返回 {"success": false, ...}
  ToolResult result;
  auto err_it = raw_result.find("error");
  auto success_it = raw_result.find("success");
  if (err_it != raw_result.end() && err_it->is_string()) {
    result = ToolResult::error(ErrorCode::Unknown,
                               err_it->get<std::string>());
  } else if (success_it != raw_result.end() &&
             success_it->is_boolean() && !success_it->get<bool>()) {
    auto msg_it = raw_result.find("error_message");
    std::string msg = (msg_it != raw_result.end() && msg_it->is_string())
                          ? msg_it->get<std::string>()
                          : "Unknown error";
    result = ToolResult::error(ErrorCode::Unknown, std::move(msg));
  } else {
    result = ToolResult::success(std::move(raw_result));
  }

  // ===== Step 5: emit tool.audit.completed =====
  const auto end = std::chrono::steady_clock::now();
  const auto duration_ms =
      std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

  if (bus_) {
    nlohmann::json completed_meta = audit_meta(
        "tool.audit.completed", request_id, tool_name,
        cat_str, ctx.caller_layer, ctx.session_id);
    completed_meta["duration_ms"] = std::to_string(duration_ms);
    completed_meta["ok"] = result.ok ? "true" : "false";
    completed_meta["error_code"] = result.ok ? "" : "tool.error";
    bus_->emit("tool.audit.completed", ToolResult::success({}, std::move(completed_meta)));
  }

  return result;
}

}  // namespace agenticdsl