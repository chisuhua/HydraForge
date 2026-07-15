// src/common/tools/tool_coordinator.cpp
// 功能描述：ToolCoordinator 实现 (layer check + approval + audit log + RAII nesting guard)
// 作者：AgenticDSL Phase3 / Sprint 14 C4 ship + Phase 6 W1 escalation triggers
// 最后修改日期：2026-07-15
#include "common/tools/tool_coordinator.h"

#include <atomic>
#include <chrono>
#include <cstdint>
#include <ctime>
#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <thread>
#include <vector>

#include "common/policy/layer_profile.h"
#include "nlohmann/json.hpp"

namespace agenticdsl {

namespace {

// ──── thread_local nesting state (ADR-0051 §5.5) ─────────────────────────
// 已知限制 (v1): thread_local 变量绑定到 DomainWorkerPool 的 jthread worker
// 线程 (ADR-0020, Sprint 3)。跨线程 cycle (e.g. G1 on Worker A → G3 on Worker B
// → G1 on Worker A) 不可检测。这是 v1 接受限制, 记录在 ADR-0051 §不变量中。
static thread_local int tls_nesting_depth = 0;
static thread_local std::vector<std::string> tls_active_call_stack;

std::string format_call_stack() {
  std::string r;
  for (size_t i = 0; i < tls_active_call_stack.size(); i++) {
    if (i > 0) r += " → ";
    r += tls_active_call_stack[i];
  }
  return r;
}

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

// ============================================================================
// ToolCoordinatorNestingGuard 实现 (ADR-0051 §Decision 5)
// ============================================================================

ToolCoordinatorNestingGuard::ToolCoordinatorNestingGuard(
    const std::string& tool_name,
    const std::shared_ptr<IInteractionBus>& bus)
    : name_(tool_name) {
  // ----- depth check (BEFORE state modification) -----
  if (tls_nesting_depth >= 2) {
    throw std::runtime_error(
        "ToolCoordinator nesting depth > 2 (depth=" +
        std::to_string(tls_nesting_depth + 1) + "): " + name_ +
        " | call_stack: " + format_call_stack());
  }

  // ----- cycle check (同一工具名已在 stack 中?) -----
  auto it = std::find(tls_active_call_stack.begin(),
                      tls_active_call_stack.end(), name_);
  if (it != tls_active_call_stack.end()) {
    // 发射 cycle_detected_log audit event (before HARD KILL)
    if (bus) {
      nlohmann::json payload;
      nlohmann::json stack = nlohmann::json::array();
      for (auto& s : tls_active_call_stack) stack.push_back(s);
      payload["call_stack"] = stack;
      payload["caller"] = *it;
      payload["callee"] = name_;
      payload["thread_id"] =
          std::hash<std::thread::id>{}(std::this_thread::get_id());
      payload["nesting_depth"] = tls_nesting_depth;
      payload["timestamp"] = now_iso8601();
      bus->emit("tool.coordinator.cycle_detected",
                ToolResult::success({}, std::move(payload)));
    }
    throw std::runtime_error(
        "ToolCoordinator cycle detected: " + name_ +
        " already in call_stack | stack: " + format_call_stack());
  }

  // ----- push state (检查通过后才修改) -----
  tls_nesting_depth++;
  tls_active_call_stack.push_back(name_);
}

ToolCoordinatorNestingGuard::~ToolCoordinatorNestingGuard() {
  // 清理: 从 call stack 弹出, 递减深度
  if (!tls_active_call_stack.empty() &&
      tls_active_call_stack.back() == name_) {
    tls_active_call_stack.pop_back();
  }
  if (tls_nesting_depth > 0) tls_nesting_depth--;
}

// Helper to convert ToolMetadata to JSON string for ToolPreview.metadata_json
std::string ToolCoordinator::metadata_to_json(const ToolMetadata& meta) {
  nlohmann::json j;
  j["name"] = meta.name;
  j["category"] = static_cast<int>(meta.category);
  j["min_layer"] = static_cast<int>(meta.min_layer);
  nlohmann::json layers = nlohmann::json::array();
  for (auto& l : meta.allowed_layers) layers.push_back(static_cast<int>(l));
  j["allowed_layers"] = layers;
  j["cost_estimate"] = meta.cost_estimate;
  j["timeout_ms"] = meta.timeout_ms;
  j["approval"]["requires_approval_in_plan"] = meta.approval.requires_approval_in_plan;
  j["approval"]["requires_approval_in_agent"] = meta.approval.requires_approval_in_agent;
  j["approval"]["requires_approval_in_yolo"] = meta.approval.requires_approval_in_yolo;
  return j.dump();
}

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
  const std::string tool_name = meta.name;

  // Step 0: RAII nesting guard (ADR-0051 §Decision 5)
  //         检测 depth>2 / cycle → HARD KILL (throw)
  ToolCoordinatorNestingGuard nesting_guard(tool_name, bus_);

  const std::string request_id = generate_request_id();
  const auto start = std::chrono::steady_clock::now();

  const std::string cat_str = to_string(meta.category);

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
  preview.metadata_json = metadata_to_json(meta);  // C6: Attach metadata JSON for TUI approval display

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