// pdk/temporal_agent/pdk_entry.cpp
// 功能描述：Temporal Agent PDK Plugin 入口 (Task 3)。
//          注册 5 个 temporal/* 工具到 IToolRegistry:
//            1. temporal/start_workflow -> start_workflow_blocking
//            2. temporal/start_async    -> start_workflow_async
//            3. temporal/poll           -> poll
//            4. temporal/signal         -> signal
//            5. temporal/query          -> query
//          导出 extern "C" pdk_register_tools(IToolRegistry&)
//          + pdk_plugin_info 数据符号 (PluginLoader ABI v2)。
//          遵循 PDK Plugin 契约 (ADR-0021, ADR-0022, ADR-0034 C7 范式)。
// 设计依据：.rddf/plans/pkm-temporal-demo-scaffold.md Task 3 Step 3
//           参考范式: pdk/g3_knowledge_base/src/g3_query.cpp (register_tool_function)
//           注: 不使用 DECLARE_TOOL (handler 签名不匹配 ToolFunc;
//               ADR-0051 §Decision 3 推荐 register_tool_function)
// 作者：pkm-temporal-demo-scaffold Task 3
// 最后修改日期：2026-07-28

#include "mock_client.h"

#include "agenticdsl/contract/itool_registry.h"
#include "agenticdsl/plugin/plugin_info.h"
#include "common/policy/execution_policy.h"

#include <nlohmann/json.hpp>

#include <memory>
#include <string>
#include <unordered_map>

using json = nlohmann::json;

namespace agenticdsl::pdk::temporal_agent {

// ============================================================================
// 全局 ITemporalClient 实例 (可通过 set_client 注入测试 mock)
// ============================================================================
static std::unique_ptr<ITemporalClient> g_client;

void set_client(std::unique_ptr<ITemporalClient> c) {
  g_client = std::move(c);
}

// ============================================================================
// 辅助: 从 args map 提取字段 (字符串值)
// ============================================================================
static std::string get_str(
    const std::unordered_map<std::string, std::string>& args,
    const std::string& key) {
  auto it = args.find(key);
  return it != args.end() ? it->second : "";
}

// ============================================================================
// 辅助: 从 args map 提取 JSON 值 (尝试 parse, 失败回退为字符串)
// ============================================================================
static json get_json(
    const std::unordered_map<std::string, std::string>& args,
    const std::string& key) {
  auto it = args.find(key);
  if (it == args.end()) return json::object();
  const std::string& v = it->second;
  if (v.empty()) return json::object();
  try {
    return json::parse(v);
  } catch (...) {
    return v;
  }
}

// ============================================================================
// 5 工具处理函数
// ============================================================================

// 1. temporal/start_workflow - 阻塞启动
static json handle_start_workflow(
    const std::unordered_map<std::string, std::string>& args) {
  if (!g_client) return {{"error", "no client set"}};
  std::string wf_id = get_str(args, "workflow_id");
  json wf_args = get_json(args, "args");
  return g_client->start_workflow_blocking(wf_id, wf_args);
}

// 2. temporal/start_async - 异步启动
static json handle_start_async(
    const std::unordered_map<std::string, std::string>& args) {
  if (!g_client) return {{"error", "no client set"}};
  std::string wf_id = get_str(args, "workflow_id");
  json wf_args = get_json(args, "args");
  return g_client->start_workflow_async(wf_id, wf_args);
}

// 3. temporal/poll - 轮询状态
static json handle_poll(
    const std::unordered_map<std::string, std::string>& args) {
  if (!g_client) return {{"error", "no client set"}};
  std::string wf_id = get_str(args, "workflow_id");
  return g_client->poll(wf_id);
}

// 4. temporal/signal - 发送 signal
static json handle_signal(
    const std::unordered_map<std::string, std::string>& args) {
  if (!g_client) return {{"error", "no client set"}};
  std::string wf_id = get_str(args, "workflow_id");
  std::string sig_name = get_str(args, "signal_name");
  json payload = get_json(args, "payload");
  return g_client->signal(wf_id, sig_name, payload);
}

// 5. temporal/query - 查询元数据
static json handle_query(
    const std::unordered_map<std::string, std::string>& args) {
  if (!g_client) return {{"error", "no client set"}};
  std::string wf_id = get_str(args, "workflow_id");
  std::string q_name = get_str(args, "query_name");
  return g_client->query(wf_id, q_name);
}

// ============================================================================
// 工具注册 - ToolMetadata V2 完整字段
// ============================================================================
void register_tools(IToolRegistry* registry) {
  // 审批策略: agent (plan+agent 审批, yolo 不审)
  ApprovalPolicy agent_approval{true, true, false, false};
  // 审批策略: yolo (仅 yolo 模式允许, plan/agent 不审 -> 实际仅 yolo 执行)
  ApprovalPolicy yolo_approval{false, false, true, false};

  // ---- 1. temporal/start_workflow ----
  {
    ToolMetadata meta{
      "temporal/start_workflow",
      "阻塞启动 Temporal workflow (同步等待完成)",
      "temporal",
      ToolCategory::Execute,
      LayerProfile::Workflow,
      agent_approval,
      {LayerProfile::Workflow},
      0.0,
      60000  // 60s timeout (阻塞可能较慢)
    };
    registry->register_tool_function(
        "temporal/start_workflow", meta, handle_start_workflow);
  }

  // ---- 2. temporal/start_async ----
  {
    ToolMetadata meta{
      "temporal/start_async",
      "异步启动 Temporal workflow (立即返回 RUNNING)",
      "temporal",
      ToolCategory::Execute,
      LayerProfile::Workflow,
      agent_approval,
      {LayerProfile::Workflow},
      0.0,
      5000
    };
    registry->register_tool_function(
        "temporal/start_async", meta, handle_start_async);
  }

  // ---- 3. temporal/poll ----
  {
    ToolMetadata meta{
      "temporal/poll",
      "轮询 Temporal workflow 当前状态",
      "temporal",
      ToolCategory::ReadOnly,
      LayerProfile::Workflow,
      yolo_approval,
      {LayerProfile::Workflow},
      0.0,
      5000
    };
    registry->register_tool_function(
        "temporal/poll", meta, handle_poll);
  }

  // ---- 4. temporal/signal ----
  {
    ToolMetadata meta{
      "temporal/signal",
      "向 Temporal workflow 发送 signal (触发分支/恢复)",
      "temporal",
      ToolCategory::Execute,
      LayerProfile::Workflow,
      agent_approval,
      {LayerProfile::Workflow},
      0.0,
      5000
    };
    registry->register_tool_function(
        "temporal/signal", meta, handle_signal);
  }

  // ---- 5. temporal/query ----
  {
    ToolMetadata meta{
      "temporal/query",
      "查询 Temporal workflow 只读元数据 (不修改状态)",
      "temporal",
      ToolCategory::ReadOnly,
      LayerProfile::Workflow,
      yolo_approval,
      {LayerProfile::Workflow},
      0.0,
      5000
    };
    registry->register_tool_function(
        "temporal/query", meta, handle_query);
  }
}

}  // namespace agenticdsl::pdk::temporal_agent

// ============================================================================
// Plugin 入口 (extern "C" - PluginLoader dlopen/dlsym)
// ============================================================================

extern "C" void pdk_register_tools(::agenticdsl::IToolRegistry& registry) {
  agenticdsl::pdk::temporal_agent::register_tools(&registry);
}

// ============================================================================
// Plugin 元数据 (PluginLoader 在 dlopen 后零代码执行读取)
// 数据符号格式 - 与 C7 model_router / C14 llama_engine / Phase6 g3 一致
// ============================================================================

extern "C" const hydraforge::PluginInfo pdk_plugin_info = {
  hydraforge::CURRENT_ABI_VERSION,                  // abi_version = 2
  "hydraforge_temporal_agent",                      // name
  0, 1, 0,                                          // semver 0.1.0
  "Temporal workflow agent plugin - 5 tools (start_workflow/start_async/poll/signal/query)", // description
  "temporal,workflow,orchestration",                // capabilities
  ""                                                // dependencies (无依赖, MockTemporalClient 自包含)
};
