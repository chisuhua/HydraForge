// src/common/policy/approval_callbacks.cpp
// 功能描述：3 种 ApprovalCallback 工厂实现
// 作者：AgenticDSL Phase3 / Sprint 13 C3 ship
// 最后修改日期：2026-07-31
#include "common/policy/approval_callbacks.h"

#include <atomic>
#include <chrono>
#include <iostream>
#include <mutex>
#include <string>
#include <thread>

#include "agenticdsl/contract/iinteraction_bus.h"

namespace agenticdsl {

namespace {

std::string generate_request_id() {
  static std::atomic<std::uint64_t> counter{0};
  auto now = std::chrono::steady_clock::now().time_since_epoch().count();
  return "req-" + std::to_string(now) + "-" + std::to_string(counter++);
}

}  // namespace

ApprovalCallback make_tui_stdin_callback() {
  return [](const ApprovalRequest& req, int timeout_ms) -> bool {
    std::cout << "\n[APPROVAL REQUIRED]\n"
              << "Tool: " << req.tool_name << "\n"
              << "Request ID: " << req.request_id << "\n"
              << "Command: " << req.preview.command_line << "\n"
              << "Risk: " << req.preview.risk_summary << "\n"
              << "Approve? [y/N] (timeout " << timeout_ms << "ms): "
              << std::flush;

    std::string line;
    if (!std::getline(std::cin, line)) {
      return false;
    }
    return (line == "y" || line == "Y" || line == "yes");
  };
}

ApprovalCallback make_event_bus_callback(std::shared_ptr<IInteractionBus> bus) {
  if (!bus) {
    return [](const ApprovalRequest&, int) { return false; };
  }
  return [bus](const ApprovalRequest& req, int timeout_ms) -> bool {
    std::string payload =
        "request_id=" + req.request_id +
        ", tool=" + req.tool_name +
        ", command=" + req.preview.command_line +
        ", risk=" + req.preview.risk_summary +
        ", timeout_ms=" + std::to_string(timeout_ms);
    bus->emit("policy.approval.requested", payload);

    return true;
  };
}

ApprovalCallback make_test_auto_callback(bool decision) {
  return [decision](const ApprovalRequest&, int) -> bool {
    return decision;
  };
}

}  // namespace agenticdsl