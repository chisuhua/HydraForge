// src/common/policy/mode_switch_dialog.cpp
// 功能描述：ModeSwitchDialog 实现
// 作者：AgenticDSL Phase3 / Sprint 13 C3 ship
// 最后修改日期：2026-07-31
#include "common/policy/mode_switch_dialog.h"

#include <iostream>
#include <string>

namespace agenticdsl {

bool read_user_input_stdin(const std::string& prompt) {
  std::cout << prompt << std::flush;
  std::string line;
  if (!std::getline(std::cin, line)) {
    return false;  // EOF 拒绝
  }
  return (line == "y" || line == "Y" || line == "yes" || line == "YES");
}

bool requires_yolo_confirmation(const std::string& from_mode,
                                const std::string& to_mode) {
  // 任一模式为 yolo 即触发确认 (defense-in-depth)
  return from_mode == "yolo" || to_mode == "yolo";
}

bool confirm_yolo_switch(const std::string& from_mode, UserInputFn input) {
  // 检查是否需要确认 (Plan ↔ Agent 静默切换)
  if (!requires_yolo_confirmation(from_mode, "yolo")) {
    return true;  // 非 YOLO 切换自动放行
  }

  if (!input) {
    input = read_user_input_stdin;
  }

  std::string prompt = "\n⚠️  WARNING: YOLO MODE SWITCH\n"
                       "From: " + from_mode + " → To: yolo\n"
                       "YOLO mode auto-approves ALL tools except force_approval_always.\n"
                       "Are you sure? [y/N]: ";

  return input(prompt);
}

}  // namespace agenticdsl