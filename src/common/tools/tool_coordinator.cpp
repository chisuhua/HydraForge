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
#include <optional>
#include <sstream>
#include <stdexcept>
#include <thread>
#include <vector>

#include "agenticdsl/contract/event_builder.h"
#include "agenticdsl/policy/path_policy.h"
#include "agenticdsl/tools/tool_schema_validator.h"
#include "common/policy/dangerous_patterns.h"
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

// 发射 tool.execution.start / tool.execution.end 生命周期事件
// (ADR-0068 §决策 3: 5 个幻影主题中 2 个由 ToolCoordinator 真实发射)
void emit_tool_execution_event(const std::shared_ptr<IInteractionBus>& bus,
                               const std::string& topic,
                               const std::string& tool_name,
                               const std::string& layer,
                               const std::string& request_id,
                               const std::string& session_id,
                               bool ok,
                               std::int64_t duration_ms,
                               const std::optional<std::string>& error_code = std::nullopt) {
  if (!bus) return;
  nlohmann::json args = {
      {"tool", tool_name},
      {"layer", layer},
      {"ok", ok},
      {"duration_ms", duration_ms}};
  if (error_code) args["error_code"] = *error_code;
  nlohmann::json meta = {
      {"request_id", request_id},
      {"session_id", session_id}};
  bus->emit(EventBuilder(topic).args(args).meta(meta).build());
}

// ──── ADR-0073 D3: 4 步 pipeline helpers ────────────────────────────────

std::string stage_name(ToolCoordinator::ValidationStage stage) {
  using S = ToolCoordinator::ValidationStage;
  switch (stage) {
    case S::SchemaValidate: return "schema_validate";
    case S::Coercion:       return "coercion";
    case S::RequiredField:  return "required_field";
    case S::BusinessRules:  return "business_rules";
  }
  return "unknown";
}

// schema "type" 值 vs json 实例类型 (与 tool_schema_validator.cpp 同一套语义)
bool json_type_matches(const std::string& expected, const nlohmann::json& v) {
  if (expected == "object") return v.is_object();
  if (expected == "array") return v.is_array();
  if (expected == "string") return v.is_string();
  if (expected == "integer") return v.is_number_integer() || v.is_number_unsigned();
  if (expected == "number") return v.is_number();
  if (expected == "boolean") return v.is_boolean();
  if (expected == "null") return v.is_null();
  return true;  // 未知 type 名称 → 不约束 (schema 作者责任)
}

// args 指纹: std::hash hex (确定性, 仅用于审计关联; 项目无 vendored SHA-256
// 且本 change 禁止引入新外部依赖, 故不实现密码学哈希)
std::string args_fingerprint(const nlohmann::json& args) {
  std::ostringstream oss;
  oss << std::hex << std::hash<std::string>{}(args.dump());
  return oss.str();
}

}  // namespace

// ============================================================================
// ToolCoordinatorNestingGuard 实现 (ADR-0051 §Decision 5)
// ============================================================================

ToolCoordinatorNestingGuard::ToolCoordinatorNestingGuard(
    const std::string& tool_name,
    const std::shared_ptr<IInteractionBus>& bus)
    : name_(tool_name) {
  // ----- cycle check FIRST (more specific than depth, per ADR-0051 §Decision 6) -----
  auto it = std::find(tls_active_call_stack.begin(),
                      tls_active_call_stack.end(), name_);
  if (it != tls_active_call_stack.end()) {
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
      bus->emit(agenticdsl::EventBuilder("tool.coordinator.cycle_detected")
          .args(nlohmann::json{
              {"caller", *it},
              {"callee", name_},
              {"nesting_depth", tls_nesting_depth}
          })
          .meta(nlohmann::json{
              {"call_stack", std::move(stack)},
              {"thread_id", std::hash<std::thread::id>{}(std::this_thread::get_id())},
              {"timestamp", now_iso8601()}
          })
          .build());
    }
    throw std::runtime_error(
        "ToolCoordinator cycle detected: " + name_ +
        " already in call_stack | stack: " + format_call_stack());
  }

  // ----- depth check -----
  if (tls_nesting_depth >= 2) {
    throw std::runtime_error(
        "ToolCoordinator nesting depth > 2 (depth=" +
        std::to_string(tls_nesting_depth + 1) + "): " + name_ +
        " | call_stack: " + format_call_stack());
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

// ============================================================================
// ADR-0073 D3: 4 步 sanitization pipeline 实现
// ============================================================================
//
// 与 plan 原文的偏差 (Batch 2 实施时确认的项目实际):
//  - ValidationMode 实际为 Strict/Warn/Ignore (非 Strict/Coerce/Off):
//    Strict = 类型不匹配即拒绝; Warn = 自动类型转换并 stderr 警告; Ignore = 跳过.
//  - ToolCategory 无 Dangerous 枚举值 → 业务规则锚定 ToolCategory::Execute
//    (shell/exec 类工具的自然分类).
//  - meta.input_schema 为 std::optional<nlohmann::json>, has_value()==false
//    表示 V2 legacy 工具: 跳过 step 1-3, step 4 业务规则仍然生效.
//  - ErrorCode::InvalidParams 为本 change 新增枚举值 (JSON-RPC -32602).

ErrorCode ToolCoordinator::map_stage_to_error(ValidationStage stage) {
  // 4 步全部映射到 InvalidParams 语义 (JSON-RPC -32602)
  (void)stage;
  return ErrorCode::InvalidParams;
}

int ToolCoordinator::map_to_jsonrpc(ErrorCode code) {
  switch (code) {
    case ErrorCode::InvalidParams: return -32602;
    case ErrorCode::Unknown:
    default:                       return -32603;
  }
}

bool ToolCoordinator::check_type(const nlohmann::json& schema,
                                 const std::string& key,
                                 const nlohmann::json& val) {
  if (!schema.is_object()) return true;
  auto props_it = schema.find("properties");
  if (props_it == schema.end() || !props_it->is_object()) return true;
  auto field_it = props_it->find(key);
  if (field_it == props_it->end() || !field_it->is_object()) return true;
  auto type_it = field_it->find("type");
  if (type_it == field_it->end()) return true;
  if (type_it->is_string()) {
    return json_type_matches(type_it->get<std::string>(), val);
  }
  if (type_it->is_array()) {
    for (const auto& t : *type_it) {
      if (t.is_string() && json_type_matches(t.get<std::string>(), val)) return true;
    }
    return false;
  }
  return true;
}

std::vector<std::string> ToolCoordinator::get_required_fields(
    const nlohmann::json& schema) {
  std::vector<std::string> out;
  if (!schema.is_object()) return out;
  auto it = schema.find("required");
  if (it == schema.end() || !it->is_array()) return out;
  for (const auto& f : *it) {
    if (f.is_string()) out.push_back(f.get<std::string>());
  }
  return out;
}

nlohmann::json ToolCoordinator::coerce_args(const nlohmann::json& schema,
                                            const nlohmann::json& args) {
  nlohmann::json out = args;
  if (!schema.is_object()) return out;
  auto props_it = schema.find("properties");
  if (props_it == schema.end() || !props_it->is_object()) return out;
  for (auto& [key, val] : out.items()) {
    auto field_it = props_it->find(key);
    if (field_it == props_it->end() || !field_it->is_object()) continue;
    auto type_it = field_it->find("type");
    if (type_it == field_it->end() || !type_it->is_string()) continue;
    const std::string expected = type_it->get<std::string>();
    if (!val.is_string()) continue;  // 仅从 string 源做转换 (map 入参全为 string)
    const std::string s = val.get<std::string>();
    try {
      if (expected == "integer") {
        size_t pos = 0;
        long long v = std::stoll(s, &pos);
        if (pos == s.size()) val = v;
      } else if (expected == "number") {
        size_t pos = 0;
        double v = std::stod(s, &pos);
        if (pos == s.size()) val = v;
      } else if (expected == "boolean") {
        if (s == "true") val = true;
        else if (s == "false") val = false;
      }
    } catch (const std::exception&) {
      // 转换失败保持原值; Strict 模式下的拒绝由 check_type 负责
    }
  }
  return out;
}

void ToolCoordinator::emit_audit_denied(ValidationStage stage,
                                        const std::string& tool_name,
                                        const std::string& reason) {
  if (!bus_) return;
  bus_->emit(EventBuilder("tool.audit.denied",
      ToolResult::error(map_stage_to_error(stage), reason,
          audit_meta("tool.audit.denied", generate_request_id(), tool_name,
                     "validation", "coordinator", "")))
      .args(nlohmann::json{
          {"tool", tool_name},
          {"reason", reason},
          {"validation_stage", stage_name(stage)}})
      .build());
}

ToolResult ToolCoordinator::execute(
    const ToolMetadata& meta,
    const ToolCallContext& ctx,
    const std::unordered_map<std::string, std::string>& args,
    std::stop_token token) {
  const std::string tool_name = meta.name;

  // Step 0: RAII nesting guard (ADR-0051 §Decision 5)
  //         检测 depth>2 / cycle → HARD KILL (throw)
  ToolCoordinatorNestingGuard nesting_guard(tool_name, bus_);

  // Phase B Step 3: 收到取消请求时提前返回并发射 audit denied 事件
  if (token.stop_requested()) {
    if (bus_) {
      nlohmann::json denied_meta = audit_meta(
          "tool.audit.denied", generate_request_id(), tool_name,
          to_string(meta.category), ctx.caller_layer, ctx.session_id);
      bus_->emit(EventBuilder("tool.audit.denied",
          ToolResult::error(ErrorCode::PermissionDenied, "cancelled", std::move(denied_meta)))
          .args(nlohmann::json{{"tool", tool_name}, {"reason", "cancelled"}})
          .build());
    }
    return ToolResult::error(ErrorCode::PermissionDenied, "cancelled");
  }

  const std::string request_id = generate_request_id();
  const auto start = std::chrono::steady_clock::now();

  const std::string cat_str = to_string(meta.category);

  std::unordered_map<std::string, std::string> effective_args = args;
  std::vector<std::string> hook_warnings;

  // ===== Step A: pre-hooks (ADR-0069) =====
  if (hook_registry_) {
    PreHookResult pre = hook_registry_->apply_pre_hooks(
        meta, ctx, effective_args, hook_warnings);

    if (pre.action == PreHookResult::Deny) {
      if (bus_) {
        nlohmann::json denied_meta = audit_meta(
            "tool.audit.denied", request_id, tool_name,
            cat_str, ctx.caller_layer, ctx.session_id);
        denied_meta["reason"] = pre.deny_reason;
        if (!hook_warnings.empty()) denied_meta["hook_warnings"] = hook_warnings;
        bus_->emit(EventBuilder("tool.audit.denied",
            ToolResult::error(ErrorCode::PermissionDenied,
                pre.deny_reason, std::move(denied_meta)))
            .build());
      }
      return ToolResult::error(ErrorCode::PermissionDenied, pre.deny_reason);
    }

    if (pre.action == PreHookResult::ModifyArgs) {
      effective_args = std::move(pre.modified_args);
    }
  }

  // ===== ADR-0073 D3: 4 步 sanitization pipeline =====
  // 插入点: pre-hooks 之后, tool.execution.start 之前 —
  // 校验拒绝的调用从未真正 "start", 不应产生 execution.start 事件.
  // V2 legacy (input_schema 无值): 跳过 step 1-3, step 4 业务规则仍然生效.
  {
    nlohmann::json args_json = nlohmann::json::object();
    for (const auto& [k, v] : effective_args) args_json[k] = v;

    if (meta.input_schema.has_value()) {
      const nlohmann::json& schema = *meta.input_schema;

      // Step 1: SchemaValidate — 结构级校验 (enum/嵌套等),
      //         type-mismatch 与 required-missing 延后到 step 2/3 归因
      {
        tools::ToolSchemaValidator validator(schema.dump());
        auto vr = validator.validate(args_json);
        if (!vr.ok) {
          nlohmann::json deferred = nlohmann::json::array();
          nlohmann::json fatal = nlohmann::json::array();
          for (const auto& err : vr.errors) {
            const std::string msg = err.value("message", "");
            if (msg.rfind("type mismatch", 0) == 0 ||
                msg == "required field missing") {
              deferred.push_back(err);
            } else {
              fatal.push_back(err);
            }
          }
          if (!fatal.empty()) {
            nlohmann::json m;
            m["validation_stage"] = stage_name(ValidationStage::SchemaValidate);
            m["errors"] = std::move(fatal);
            emit_audit_denied(ValidationStage::SchemaValidate, tool_name,
                              "schema validation failed");
            return ToolResult::error(ErrorCode::InvalidParams,
                                     "schema validation failed for tool: " + tool_name,
                                     std::move(m));
          }
        }
      }

      // Step 2: Coercion — Strict 拒绝类型不匹配 / Warn 自动转换 / Ignore 跳过
      if (meta.validation_mode == ToolMetadata::ValidationMode::Strict) {
        for (auto& [key, val] : args_json.items()) {
          if (!check_type(schema, key, val)) {
            nlohmann::json m;
            m["validation_stage"] = stage_name(ValidationStage::Coercion);
            m["field_path"] = key;
            emit_audit_denied(ValidationStage::Coercion, tool_name,
                              "strict type check failed on field: " + key);
            return ToolResult::error(ErrorCode::InvalidParams,
                                     "type mismatch on field '" + key + "' for tool: " + tool_name,
                                     std::move(m));
          }
        }
      } else if (meta.validation_mode == ToolMetadata::ValidationMode::Warn) {
        nlohmann::json coerced = coerce_args(schema, args_json);
        if (coerced != args_json) {
          std::fprintf(stderr,
              "[tool_coordinator] Warn mode: coerced args for tool '%s'\n",
              tool_name.c_str());
          args_json = std::move(coerced);
        }
      }
      // Ignore: 跳过 step 2

      // Step 3: RequiredField — schema.required[] 全部存在
      for (const auto& field : get_required_fields(schema)) {
        if (!args_json.contains(field)) {
          nlohmann::json m;
          m["validation_stage"] = stage_name(ValidationStage::RequiredField);
          m["field_path"] = field;
          emit_audit_denied(ValidationStage::RequiredField, tool_name,
                            "required field missing: " + field);
          return ToolResult::error(ErrorCode::InvalidParams,
                                   "required field '" + field + "' missing for tool: " + tool_name,
                                   std::move(m));
        }
      }
    }

    // Step 4: BusinessRules — Execute 类工具强制危险模式 + 路径策略检查
    // (锚定 Execute: ToolCategory 无 Dangerous 枚举值, 见上方偏差说明)
    if (meta.category == ToolCategory::Execute) {
      if (meta.input_schema.has_value() &&
          meta.input_schema->contains("properties") &&
          (*meta.input_schema)["properties"].contains("path") &&
          args_json.contains("path") && args_json["path"].is_string()) {
        PathPolicy path_policy;
        auto check = path_policy.check(args_json["path"].get<std::string>());
        if (!check.allowed) {
          nlohmann::json m;
          m["validation_stage"] = stage_name(ValidationStage::BusinessRules);
          m["reason"] = "path_policy_violation";
          m["args_hash"] = args_fingerprint(args_json);
          emit_audit_denied(ValidationStage::BusinessRules, tool_name,
                            "path policy violation: " + check.reason);
          return ToolResult::error(ErrorCode::InvalidParams,
                                   "path policy violation for tool: " + tool_name,
                                   std::move(m));
        }
      }
      if (args_json.contains("cmd") && args_json["cmd"].is_string()) {
        const std::string cmd = args_json["cmd"].get<std::string>();
        if (policy::DangerousPatterns::contains_dangerous(cmd)) {
          nlohmann::json m;
          m["validation_stage"] = stage_name(ValidationStage::BusinessRules);
          m["reason"] = "dangerous_pattern_detected";
          m["matched_pattern"] = policy::DangerousPatterns::first_match(cmd);
          m["args_hash"] = args_fingerprint(args_json);
          // 不记录 raw cmd (defense-in-depth, 审计日志不落敏感命令)
          emit_audit_denied(ValidationStage::BusinessRules, tool_name,
                            "dangerous pattern detected: " +
                                policy::DangerousPatterns::first_match(cmd));
          return ToolResult::error(ErrorCode::InvalidParams,
                                   "dangerous pattern detected for tool: " + tool_name,
                                   std::move(m));
        }
      }
    }

    // 校验通过: 将 (可能被 Warn 模式转换过的) args 写回 string map
    for (auto& [k, v] : args_json.items()) {
      effective_args[k] = v.is_string() ? v.get<std::string>() : v.dump();
    }
  }

  // 发射 tool.execution.start (ADR-0068 §决策 3, 在 layer/approval 决策前先记录意图)
  emit_tool_execution_event(bus_, "tool.execution.start", tool_name,
                            ctx.caller_layer, request_id, ctx.session_id,
                            true, 0);

  // ===== Step 1: Layer check (ADR-0004 §8 矩阵 via layer_profile.h) =====
  try {
    LayerProfile caller_layer = parse_layer(ctx.caller_layer);
    if (!check_layer_permission(caller_layer, meta.category)) {
      if (bus_) {
        // ADR-0068 §决策 7: operation-result event (ok=false + PermissionDenied) 通过 EventBuilder
        // (promote-event-builder-fulltoolresult-support 2026-08-03 V2 扩展)
        bus_->emit(EventBuilder("tool.audit.denied",
            ToolResult::error(ErrorCode::PermissionDenied,
                "Layer '" + ctx.caller_layer + "' cannot call category '" +
                    cat_str + "'",
                audit_meta("tool.audit.denied", request_id, tool_name,
                           cat_str, ctx.caller_layer, ctx.session_id)))
            .build());
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
  preview.command_line = meta.name + "(" + std::to_string(effective_args.size()) + " args)";
  preview.risk_summary = "Tool category: " + cat_str;
  preview.metadata_json = metadata_to_json(meta);  // C6: Attach metadata JSON for TUI approval display

  if (!approval_handler_->process_request(meta, ctx, preview)) {
    if (bus_) {
      bus_->emit(EventBuilder("tool.audit.denied",
          ToolResult::error(ErrorCode::PermissionDenied,
              "Approval denied by policy",
              audit_meta("tool.audit.denied", request_id, tool_name,
                         cat_str, ctx.caller_layer, ctx.session_id)))
          .build());
    }
    return ToolResult::error(ErrorCode::PermissionDenied,
                            "Approval denied by policy for tool: " + meta.name);
  }

  // ===== Step 3: emit tool.audit.invoked =====
  if (bus_) {
    nlohmann::json invoked_meta = audit_meta(
        "tool.audit.invoked", request_id, tool_name,
        cat_str, ctx.caller_layer, ctx.session_id);
    invoked_meta["args_keys_count"] = std::to_string(effective_args.size());
    bus_->emit(agenticdsl::EventBuilder("tool.audit.invoked")
        .args(nlohmann::json{
            {"tool_name", tool_name},
            {"category", cat_str}
        })
        .meta(std::move(invoked_meta))
        .build());
  }

  // ===== Step 4: 实际调用 registry (返回 nlohmann::json) =====
  nlohmann::json raw_result = registry_.call_tool(meta.name, effective_args);

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

  // ===== Step E: post-hooks (ADR-0069) =====
  if (hook_registry_) {
    result = hook_registry_->apply_post_hooks(
        meta, ctx, std::move(result), hook_warnings);

    if (!result.ok) {
      if (bus_) {
        nlohmann::json denied_meta = audit_meta(
            "tool.audit.denied", request_id, tool_name,
            cat_str, ctx.caller_layer, ctx.session_id);
        std::string reason = result.meta.contains("error_message")
            ? result.meta["error_message"].get<std::string>()
            : "post-hook fail-closed";
        denied_meta["reason"] = reason;
        if (!hook_warnings.empty()) denied_meta["hook_warnings"] = hook_warnings;
        bus_->emit(EventBuilder("tool.audit.denied",
            ToolResult::error(result.error_code.value_or(ErrorCode::Unknown),
                reason, std::move(denied_meta)))
            .build());
      }
      return result;
    }
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
    if (!hook_warnings.empty()) completed_meta["hook_warnings"] = hook_warnings;
    bus_->emit(agenticdsl::EventBuilder("tool.audit.completed")
        .args(nlohmann::json{
            {"tool_name", tool_name},
            {"category", cat_str},
            {"ok", result.ok ? std::string{"true"} : std::string{"false"}},
            {"duration_ms", std::to_string(duration_ms)},
            {"error_code", result.ok ? std::string{} : std::string{"tool.error"}}
        })
        .meta(std::move(completed_meta))
        .build());
  }

  // 发射 tool.execution.end (ADR-0068 §决策 3, success 路径)
  emit_tool_execution_event(bus_, "tool.execution.end", tool_name,
                            ctx.caller_layer, request_id, ctx.session_id,
                            result.ok,
                            static_cast<std::int64_t>(duration_ms),
                            result.ok ? std::nullopt
                                      : std::optional<std::string>("tool.error"));

  return result;
}

}  // namespace agenticdsl