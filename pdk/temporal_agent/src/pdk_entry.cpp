// pdk/temporal_agent/src/pdk_entry.cpp
// 功能描述：Temporal Agent Plugin 入口 - 注册 5 个工作流编排工具。
//          export extern "C" pdk_register_tools(IToolRegistry&),
//          遵循 PDK Plugin 契约 (ADR-0021, ADR-0022, 参照 pdk/session_agent 范式)。
//          工具清单:
//            temporal/start_workflow - 阻塞启动 + 轮询直到完成
//            temporal/start_async    - 异步启动 + 立即返回
//            temporal/poll           - 轮询工作流状态
//            temporal/signal         - 发送信号
//            temporal/query          - 查询只读元数据
// 设计依据：openspec/changes/pkgm-temporal-agent/tasks.md §3
// 作者：pkgm-temporal-agent change
// 最后修改日期：2026-07-27

#include <stdexcept>
#include <string>
#include <unordered_map>
#include <utility>

#include <nlohmann/json.hpp>

#include <agenticdsl/contract/itool_registry.h>
#include <agenticdsl/plugin/plugin_info.h>

#include "temporal_client.h"

namespace {

// === 参数解析 helper (参照 pdk/session_agent/src/pdk_entry.cpp) ===

inline std::string str_arg(const std::unordered_map<std::string, std::string>& args,
                           const std::string& key, const std::string& default_val = "") {
  auto it = args.find(key);
  return (it != args.end()) ? it->second : default_val;
}

inline long long llong_arg(const std::unordered_map<std::string, std::string>& args,
                           const std::string& key, long long default_val = 0) {
  auto it = args.find(key);
  if (it == args.end()) return default_val;
  try { return std::stoll(it->second); } catch (...) { return default_val; }
}

// 将 WorkflowResult 序列化为 JSON
inline nlohmann::json result_to_json(const pdk_temporal_agent::WorkflowResult& r) {
  nlohmann::json j;
  j["workflow_id"] = r.workflow_id;
  j["run_id"] = r.run_id;
  j["status"] = pdk_temporal_agent::workflow_status_str(r.status);
  if (!r.result.is_null()) {
    j["result"] = r.result;
  }
  if (!r.failure_reason.empty()) {
    j["failure_reason"] = r.failure_reason;
  }
  j["history_size_bytes"] = r.history_size_bytes;
  j["event_count"] = r.event_count;
  return j;
}

// GrpcError -> 错误消息前缀 (用于异常捕获后 JSON error)
inline const char* grpc_error_prefix(pdk_temporal_agent::GrpcError code) {
  switch (code) {
    case pdk_temporal_agent::GrpcError::NotFound:         return "WORKFLOW_NOT_FOUND";
    case pdk_temporal_agent::GrpcError::AlreadyExists:    return "ALREADY_EXISTS";
    case pdk_temporal_agent::GrpcError::DeadlineExceeded: return "TIMEOUT";
    case pdk_temporal_agent::GrpcError::Unavailable:      return "UNAVAILABLE";
    case pdk_temporal_agent::GrpcError::PermissionDenied: return "PERMISSION_DENIED";
    case pdk_temporal_agent::GrpcError::InvalidArgument:  return "INVALID_ARGUMENT";
    default:                                              return "INTERNAL";
  }
}

}  // namespace

// ============================================================================
// pdk_plugin_info - PluginLoader dlopen 后零代码执行读取
// ============================================================================

extern "C" const hydraforge::PluginInfo pdk_plugin_info = {
  hydraforge::CURRENT_ABI_VERSION,                  // abi_version = 2
  "infra.temporal",                                 // name[64]
  0, 1, 0,                                          // semver 0.1.0
  "Temporal Agent - workflow orchestration (start/poll/signal/query)",  // description[256]
  "temporal,workflow_orchestration,grpc,durable_execution",             // capabilities[512]
  ""                                                // dependencies[256] (无依赖)
};

// ============================================================================
// pdk_register_tools - 注册 5 个工具
// ============================================================================

extern "C" void pdk_register_tools(::agenticdsl::IToolRegistry& registry) {
  auto& client = pdk_temporal_agent::TemporalClient::instance();

  // === 1. temporal/start_workflow ===
  // 阻塞启动: StartWorkflowExecution + 轮询 DescribeWorkflowExecution 直到完成
  registry.register_tool_function(
    "temporal/start_workflow",
    ::agenticdsl::ToolMetadata{
      .name = "temporal/start_workflow",
      .description = "Start Temporal workflow and block until completion",
      .domain = "temporal",
      .category = ::agenticdsl::ToolCategory::Execute,
      .min_layer = ::agenticdsl::LayerProfile::Workflow,
      .approval = ::agenticdsl::ApprovalPolicy{
        .requires_approval_in_plan = true,
        .requires_approval_in_agent = true,
        .requires_approval_in_yolo = false,
        .force_approval_always = false
      },
      .allowed_layers = {::agenticdsl::LayerProfile::Workflow},
      .cost_estimate = 0.0,
      .timeout_ms = 30000
    },
    [&client](const std::unordered_map<std::string, std::string>& args) -> nlohmann::json {
      std::string workflow_type = str_arg(args, "workflow_type");
      std::string task_queue = str_arg(args, "task_queue");
      std::string input_json = str_arg(args, "input_json", "{}");
      std::string workflow_id = str_arg(args, "workflow_id");
      long long timeout_ms = llong_arg(args, "timeout_ms", 30000);

      if (workflow_type.empty()) {
        throw std::runtime_error("workflow_type is required");
      }
      if (task_queue.empty()) {
        throw std::runtime_error("task_queue is required");
      }

      try {
        auto result = client.start_workflow_blocking(
            workflow_type, task_queue, input_json, workflow_id, timeout_ms);
        return result_to_json(result);
      } catch (const pdk_temporal_agent::TemporalError& e) {
        nlohmann::json err;
        err["error"] = e.what();
        err["code"] = grpc_error_prefix(e.code);
        return err;
      }
    }
  );

  // === 2. temporal/start_async ===
  // 异步启动: StartWorkflowExecution + 立即返回
  registry.register_tool_function(
    "temporal/start_async",
    ::agenticdsl::ToolMetadata{
      .name = "temporal/start_async",
      .description = "Start Temporal workflow asynchronously, return immediately",
      .domain = "temporal",
      .category = ::agenticdsl::ToolCategory::Execute,
      .min_layer = ::agenticdsl::LayerProfile::Workflow,
      .approval = ::agenticdsl::ApprovalPolicy{
        .requires_approval_in_plan = true,
        .requires_approval_in_agent = true,
        .requires_approval_in_yolo = false,
        .force_approval_always = false
      },
      .allowed_layers = {::agenticdsl::LayerProfile::Workflow},
      .cost_estimate = 0.0,
      .timeout_ms = 10000
    },
    [&client](const std::unordered_map<std::string, std::string>& args) -> nlohmann::json {
      std::string workflow_type = str_arg(args, "workflow_type");
      std::string task_queue = str_arg(args, "task_queue");
      std::string input_json = str_arg(args, "input_json", "{}");
      std::string workflow_id = str_arg(args, "workflow_id");

      if (workflow_type.empty()) {
        throw std::runtime_error("workflow_type is required");
      }
      if (task_queue.empty()) {
        throw std::runtime_error("task_queue is required");
      }

      try {
        auto result = client.start_workflow_async(
            workflow_type, task_queue, input_json, workflow_id);
        return result_to_json(result);
      } catch (const pdk_temporal_agent::TemporalError& e) {
        nlohmann::json err;
        err["error"] = e.what();
        err["code"] = grpc_error_prefix(e.code);
        return err;
      }
    }
  );

  // === 3. temporal/poll ===
  // 轮询: DescribeWorkflowExecution
  registry.register_tool_function(
    "temporal/poll",
    ::agenticdsl::ToolMetadata{
      .name = "temporal/poll",
      .description = "Poll Temporal workflow execution status",
      .domain = "temporal",
      .category = ::agenticdsl::ToolCategory::ReadOnly,
      .min_layer = ::agenticdsl::LayerProfile::Workflow,
      .approval = ::agenticdsl::ApprovalPolicy{
        .requires_approval_in_plan = false,
        .requires_approval_in_agent = false,
        .requires_approval_in_yolo = false,
        .force_approval_always = false
      },
      .allowed_layers = {::agenticdsl::LayerProfile::Workflow},
      .cost_estimate = 0.0,
      .timeout_ms = 5000
    },
    [&client](const std::unordered_map<std::string, std::string>& args) -> nlohmann::json {
      std::string workflow_id = str_arg(args, "workflow_id");
      long long timeout_ms = llong_arg(args, "timeout_seconds", 0);
      // timeout_seconds -> ms (兼容两种参数名)
      if (timeout_ms == 0) {
        timeout_ms = llong_arg(args, "timeout_ms", 5000);
      } else {
        timeout_ms *= 1000;
      }

      if (workflow_id.empty()) {
        throw std::runtime_error("workflow_id is required");
      }

      try {
        auto result = client.poll(workflow_id, timeout_ms);
        return result_to_json(result);
      } catch (const pdk_temporal_agent::TemporalError& e) {
        nlohmann::json err;
        err["error"] = e.what();
        err["code"] = grpc_error_prefix(e.code);
        return err;
      }
    }
  );

  // === 4. temporal/signal ===
  // 发送信号: SignalWorkflowExecution
  registry.register_tool_function(
    "temporal/signal",
    ::agenticdsl::ToolMetadata{
      .name = "temporal/signal",
      .description = "Send signal to a running Temporal workflow",
      .domain = "temporal",
      .category = ::agenticdsl::ToolCategory::Execute,
      .min_layer = ::agenticdsl::LayerProfile::Workflow,
      .approval = ::agenticdsl::ApprovalPolicy{
        .requires_approval_in_plan = true,
        .requires_approval_in_agent = true,
        .requires_approval_in_yolo = false,
        .force_approval_always = false
      },
      .allowed_layers = {::agenticdsl::LayerProfile::Workflow},
      .cost_estimate = 0.0,
      .timeout_ms = 10000
    },
    [&client](const std::unordered_map<std::string, std::string>& args) -> nlohmann::json {
      std::string workflow_id = str_arg(args, "workflow_id");
      std::string signal_name = str_arg(args, "signal_name");
      std::string input_json = str_arg(args, "input_json", "{}");

      if (workflow_id.empty()) {
        throw std::runtime_error("workflow_id is required");
      }
      if (signal_name.empty()) {
        throw std::runtime_error("signal_name is required");
      }

      try {
        bool ok = client.signal(workflow_id, signal_name, input_json);
        return {{"ok", ok}, {"workflow_id", workflow_id}, {"signal_name", signal_name}};
      } catch (const pdk_temporal_agent::TemporalError& e) {
        nlohmann::json err;
        err["error"] = e.what();
        err["code"] = grpc_error_prefix(e.code);
        return err;
      }
    }
  );

  // === 5. temporal/query ===
  // 查询只读元数据: DescribeWorkflowExecution
  registry.register_tool_function(
    "temporal/query",
    ::agenticdsl::ToolMetadata{
      .name = "temporal/query",
      .description = "Query Temporal workflow metadata (read-only)",
      .domain = "temporal",
      .category = ::agenticdsl::ToolCategory::ReadOnly,
      .min_layer = ::agenticdsl::LayerProfile::Workflow,
      .approval = ::agenticdsl::ApprovalPolicy{
        .requires_approval_in_plan = false,
        .requires_approval_in_agent = false,
        .requires_approval_in_yolo = false,
        .force_approval_always = false
      },
      .allowed_layers = {::agenticdsl::LayerProfile::Workflow},
      .cost_estimate = 0.0,
      .timeout_ms = 5000
    },
    [&client](const std::unordered_map<std::string, std::string>& args) -> nlohmann::json {
      std::string workflow_id = str_arg(args, "workflow_id");

      if (workflow_id.empty()) {
        throw std::runtime_error("workflow_id is required");
      }

      try {
        auto result = client.query(workflow_id);
        return result_to_json(result);
      } catch (const pdk_temporal_agent::TemporalError& e) {
        nlohmann::json err;
        err["error"] = e.what();
        err["code"] = grpc_error_prefix(e.code);
        return err;
      }
    }
  );
}

// ============================================================================
// 任务 3.7: pdk_register_agent
// 注: 当前 HydraForge PluginLoader 仅识别 pdk_plugin_info + pdk_register_tools
//     两个符号, 没有 AgentDescriptor 类型或 pdk_register_agent 符号约定。
//     此入口遵循未来 Agent 注册契约的预留接口, Phase 2 主机端 AgentDescriptor
//     基础设施落地后激活。当前为 deferred (tasks.md §3.7 标记 deferred)。
// ============================================================================
// extern "C" void pdk_register_agent(AgentDescriptor& desc) {
//   desc.name = "temporal_agent";
//   desc.entry_tool = "temporal/start_workflow";
//   desc.capabilities = {"temporal", "workflow_orchestration"};
// }
