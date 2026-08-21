// src/modules/plugin/agent_lifecycle_emitter.h
// 功能描述：agent.* 生命周期事件 emit helper（P2 emit-agent-lifecycle-events）
//          集中 emit 函数，便于扩展
// 关联：ADR-0057（agent.* 主题定义）+ ADR-0068（EventBuilder V2）
// 设计依据：openspec/changes/emit-agent-lifecycle-events (P2)
// 作者：HydraForge Sprint 22 P2 ship
// 最后修改日期：2026-08-20

#pragma once

#include "agenticdsl/contract/event_builder.h"
#include "agenticdsl/contract/iinteraction_bus.h"

#include <atomic>
#include <cstdint>
#include <string>

namespace agenticdsl {

// 状态枚举
enum class AgentLifecycleState {
  kSpawned,
  kTerminated,
  kError,
  kHeartbeat
};

// 集中 emit 函数：agent.* 生命周期事件
// 符合 ADR-0068 EventBuilder V2 + ADR-0057 payload schema
inline void emit_agent_lifecycle_event(
    IInteractionBus* bus,
    AgentLifecycleState state,
    const std::string& agent_id,
    const std::string& plugin_name = "",
    const std::string& version = "",
    const std::string& error_code = "",
    const std::string& error_message = "") {
  if (!bus) return;

  std::string topic;
  nlohmann::json args;

  switch (state) {
    case AgentLifecycleState::kSpawned:
      topic = "agent.spawned";
      args = {{"agent_id", agent_id},
              {"plugin_name", plugin_name},
              {"version", version},
              {"timestamp_ms", std::chrono::duration_cast<std::chrono::milliseconds>(
                   std::chrono::system_clock::now().time_since_epoch()).count()}};
      break;
    case AgentLifecycleState::kTerminated:
      topic = "agent.terminated";
      args = {{"agent_id", agent_id},
              {"plugin_name", plugin_name},
              {"clean_shutdown", error_code.empty()},
              {"reason", error_message.empty() ? "normal" : error_message}};
      break;
    case AgentLifecycleState::kError:
      topic = "agent.error";
      args = {{"agent_id", agent_id},
              {"plugin_name", plugin_name},
              {"error_code", error_code.empty() ? "UNKNOWN" : error_code},
              {"error_message", error_message}};
      break;
    case AgentLifecycleState::kHeartbeat:
      topic = "agent.heartbeat";
      args = {{"agent_id", agent_id},
              {"plugin_name", plugin_name},
              {"state", "active"},
              {"uptime_ms", 0}};
      break;
  }

  try {
    bus->emit(EventBuilder(topic).args(args).build());
  } catch (...) {
    // emit 失败不应影响调用方
  }
}

}  // namespace agenticdsl