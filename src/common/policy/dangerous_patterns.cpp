// src/common/policy/dangerous_patterns.cpp
// 功能描述：ADR-0073 D3 — dangerous pattern 检测实现 (5 条 OWASP 命令注入 pattern)
// 作者：from-roadmap-phase-6c-schema-complete change (Batch 1 Task 2)
// 最后修改日期：2026-08-18
#include "common/policy/dangerous_patterns.h"

#include <array>
#include <cctype>

namespace agenticdsl::policy {

namespace {

std::string to_lower(std::string_view s) {
  std::string out;
  out.reserve(s.size());
  for (char c : s) {
    out.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
  }
  return out;
}

// OWASP 命令注入黑名单 (5 条)
const std::array<std::string_view, 5> kDangerousPatterns = {
    "rm -rf",
    "mkfs",
    ":(){ :|:& };:",    // 经典 fork bomb
    "dd if=/dev/zero",  // 磁盘清零
    ">/dev/sd"          // 直接写块设备
};

}  // namespace

bool DangerousPatterns::contains_dangerous(std::string_view cmd) {
  const std::string lowered = to_lower(cmd);
  for (const auto pat : kDangerousPatterns) {
    if (lowered.find(to_lower(pat)) != std::string::npos) return true;
  }
  return false;
}

std::string DangerousPatterns::first_match(std::string_view cmd) {
  const std::string lowered = to_lower(cmd);
  for (const auto pat : kDangerousPatterns) {
    if (lowered.find(to_lower(pat)) != std::string::npos) return std::string(pat);
  }
  return "";
}

}  // namespace agenticdsl::policy
