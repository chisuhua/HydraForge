// src/common/policy/dangerous_patterns.h
// 功能描述：ADR-0073 D3 — dangerous pattern 检测 (OWASP 命令注入黑名单)
// 作者：from-roadmap-phase-6c-schema-complete change (Batch 1 Task 2)
// 最后修改日期：2026-08-18
#pragma once

#include <string>
#include <string_view>

namespace agenticdsl::policy {

class DangerousPatterns {
 public:
  // cmd 是否包含任一危险 pattern (大小写不敏感)
  static bool contains_dangerous(std::string_view cmd);

  // 审计日志用: 首个匹配 pattern (无匹配返回空串)
  static std::string first_match(std::string_view cmd);
};

}  // namespace agenticdsl::policy
