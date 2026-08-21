// include/agenticdsl/contract/iagent_composition.h
// 功能描述：Agent 编排模式接口（ADR-0060 决策 4 表格）
//          3 模式实际可用 + 1 占位（stream Phase 2）
//          同步 call / 异步 call_async / 委派 delegate / 流式 stream(占位)
// 设计依据：openspec/changes/adr-0060-p2-p3-patterns (P8)
// 作者：HydraForge Sprint 22 P8 ship
// 最后修改日期：2026-08-20

#pragma once

#include "core/types/tool_result.h"  // ErrorCode

#include <chrono>
#include <functional>
#include <future>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <unordered_map>

namespace agenticdsl {

class TestDoubleAgentRegistry;
using AgentRegistryPtr = std::shared_ptr<TestDoubleAgentRegistry>;

template <typename T>
struct AgentResult {
  bool ok = false;
  T value{};
  std::optional<ErrorCode> error_code;
  std::string message;
};

struct TaskHandle {
  std::string task_id;
  std::function<void()> cancel;
};

struct StreamHandle {
  std::string stream_id;
};

class IAgentComposition {
 public:
  virtual ~IAgentComposition() = default;

  virtual AgentResult<std::string> call(
      const std::string& agent_id,
      const std::string& args,
      std::chrono::milliseconds timeout = std::chrono::seconds(30)) = 0;

  virtual std::future<AgentResult<std::string>> call_async(
      const std::string& agent_id,
      const std::string& args,
      std::function<void(AgentResult<std::string>)> callback = nullptr,
      std::chrono::milliseconds timeout = std::chrono::seconds(30)) = 0;

  virtual TaskHandle delegate(
      const std::string& agent_id,
      const std::string& task,
      const std::string& priority = "normal") = 0;

  virtual StreamHandle stream(
      const std::string& agent_id,
      const std::string& args) {
    throw std::logic_error("Phase 2 - stream not yet implemented");
  }
};

std::unique_ptr<IAgentComposition> make_agent_composition(
    AgentRegistryPtr registry);

}  // namespace agenticdsl