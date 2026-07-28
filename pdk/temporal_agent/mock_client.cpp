// pdk/temporal_agent/mock_client.cpp
// 功能描述：MockTemporalClient 实现 - in-memory 状态机 (Task 2)。
//          状态转换逻辑:
//            start_workflow_async: 幂等检查 -> 插入 RUNNING record -> 返回 JSON
//            poll: maybe_transition (延迟到期 -> COMPLETED/FAILED) -> 返回状态
//            signal: 追加 signal -> 返回 ack
//            query: 返回只读元数据 (status/signals)
//            start_workflow_blocking: 循环 poll 直到 COMPLETED/FAILED
//          虚拟时钟: current_time_ 通过 advance_time 推进 (避免真实 sleep)
// 设计依据：.rddf/plans/pkm-temporal-demo-scaffold.md Task 2 Step 3
// 作者：pkm-temporal-demo-scaffold Task 2
// 最后修改日期：2026-07-28

#include "mock_client.h"

#include <nlohmann/json.hpp>

#include <chrono>
#include <stdexcept>
#include <string>

namespace agenticdsl {
namespace pdk {

namespace {

// WorkflowState -> string (用于 JSON 输出)
const char* state_to_string(WorkflowState s) {
  switch (s) {
    case WorkflowState::CREATED:   return "CREATED";
    case WorkflowState::RUNNING:   return "RUNNING";
    case WorkflowState::COMPLETED: return "COMPLETED";
    case WorkflowState::FAILED:    return "FAILED";
  }
  return "UNKNOWN";
}

}  // namespace

// ============================================================================
// 构造: 初始化虚拟时钟为 steady_clock 起点
// ============================================================================
MockTemporalClient::MockTemporalClient()
    : current_time_(std::chrono::steady_clock::now()) {
}

// ============================================================================
// start_workflow_async - 异步启动 (幂等)
// ============================================================================
nlohmann::json MockTemporalClient::start_workflow_async(
    const std::string& workflow_id,
    const nlohmann::json& args) {
  std::lock_guard<std::mutex> lock(mu_);
  return create_workflow(workflow_id, args);
}

// ============================================================================
// create_workflow - 内部: 创建 workflow record (锁已持有)
// ============================================================================
nlohmann::json MockTemporalClient::create_workflow(
    const std::string& workflow_id,
    const nlohmann::json& args) {
  auto it = workflows_.find(workflow_id);
  if (it != workflows_.end()) {
    // 幂等重放: 返回现有 record, 标记 idempotent_replay
    nlohmann::json j = make_status_json(workflow_id, it->second);
    j["idempotent_replay"] = true;
    j["original_workflow_id"] = workflow_id;
    return j;
  }

  // 创建新 record
  WorkflowRecord rec;
  rec.state = WorkflowState::RUNNING;
  rec.args = args;
  rec.created_at = current_time_;
  rec.latency = default_latency_;
  rec.idempotent_replay = false;

  workflows_.emplace(workflow_id, std::move(rec));
  it = workflows_.find(workflow_id);

  return make_status_json(workflow_id, it->second);
}

// ============================================================================
// start_workflow_blocking - 阻塞直到 COMPLETED/FAILED
// ============================================================================
nlohmann::json MockTemporalClient::start_workflow_blocking(
    const std::string& workflow_id,
    const nlohmann::json& args) {
  {
    std::lock_guard<std::mutex> lock(mu_);
    create_workflow(workflow_id, args);
  }

  // 轮询直到 COMPLETED/FAILED
  while (true) {
    auto state = poll(workflow_id);
    std::string s = state.value("state", "");
    if (s == "COMPLETED" || s == "FAILED") {
      return state;
    }
    // 推进虚拟时钟 (blocking 模式自动推进时间)
    advance_time(std::chrono::milliseconds(10));
  }
}

// ============================================================================
// poll - 轮询状态 (触发延迟到期转换)
// ============================================================================
nlohmann::json MockTemporalClient::poll(const std::string& workflow_id) {
  std::lock_guard<std::mutex> lock(mu_);
  maybe_transition(current_time_);

  auto it = workflows_.find(workflow_id);
  if (it == workflows_.end()) {
    return {
      {"workflow_id", workflow_id},
      {"error", "workflow_not_found"}
    };
  }
  return make_status_json(workflow_id, it->second);
}

// ============================================================================
// signal - 向 workflow 发送 signal
// ============================================================================
nlohmann::json MockTemporalClient::signal(
    const std::string& workflow_id,
    const std::string& signal_name,
    const nlohmann::json& payload) {
  std::lock_guard<std::mutex> lock(mu_);
  maybe_transition(current_time_);

  auto it = workflows_.find(workflow_id);
  if (it == workflows_.end()) {
    return {
      {"workflow_id", workflow_id},
      {"error", "workflow_not_found"}
    };
  }

  it->second.signals.push_back({signal_name, payload});

  return {
    {"workflow_id", workflow_id},
    {"signal_name", signal_name},
    {"ack", true},
    {"state", state_to_string(it->second.state)}
  };
}

// ============================================================================
// query - 只读元数据查询
// ============================================================================
nlohmann::json MockTemporalClient::query(
    const std::string& workflow_id,
    const std::string& query_name) {
  std::lock_guard<std::mutex> lock(mu_);
  maybe_transition(current_time_);

  auto it = workflows_.find(workflow_id);
  if (it == workflows_.end()) {
    return {
      {"workflow_id", workflow_id},
      {"error", "workflow_not_found"}
    };
  }

  if (query_name == "status") {
    nlohmann::json j = make_status_json(workflow_id, it->second);
    j["args"] = it->second.args;
    return j;
  }

  if (query_name == "signals") {
    nlohmann::json sigs = nlohmann::json::array();
    for (const auto& sig : it->second.signals) {
      sigs.push_back({
        {"name", sig.name},
        {"payload", sig.payload}
      });
    }
    return {
      {"workflow_id", workflow_id},
      {"count", it->second.signals.size()},
      {"signals", sigs}
    };
  }

  // 默认: 返回 status
  return make_status_json(workflow_id, it->second);
}

// ============================================================================
// advance_time - 推进虚拟时钟
// ============================================================================
void MockTemporalClient::advance_time(std::chrono::milliseconds delta) {
  std::lock_guard<std::mutex> lock(mu_);
  current_time_ += delta;
}

// ============================================================================
// set_simulated_latency - 设置新 workflow 默认延迟
// ============================================================================
void MockTemporalClient::set_simulated_latency(std::chrono::milliseconds latency) {
  std::lock_guard<std::mutex> lock(mu_);
  default_latency_ = latency;
}

// ============================================================================
// maybe_transition - 延迟到期: RUNNING -> COMPLETED/FAILED
// ============================================================================
void MockTemporalClient::maybe_transition(
    std::chrono::steady_clock::time_point now) {
  for (auto& [id, rec] : workflows_) {
    if (rec.state != WorkflowState::RUNNING) {
      continue;
    }
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        now - rec.created_at);
    if (elapsed >= rec.latency) {
      // 检查 args 中 fail 标志
      if (rec.args.value("fail", false)) {
        rec.state = WorkflowState::FAILED;
      } else {
        rec.state = WorkflowState::COMPLETED;
      }
    }
  }
}

// ============================================================================
// make_status_json - 生成状态 JSON
// ============================================================================
nlohmann::json MockTemporalClient::make_status_json(
    const std::string& workflow_id,
    const WorkflowRecord& rec) const {
  return {
    {"workflow_id", workflow_id},
    {"state", state_to_string(rec.state)}
  };
}

}  // namespace pdk
}  // namespace agenticdsl
