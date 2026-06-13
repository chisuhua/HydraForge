// src/common/policy/path_policy.cpp
// 文件头注释
// 功能描述：PathPolicy + ShellGuard 实现（ADR-0004 §3-4）
//          - PathPolicy::check()：denied → allowed 前缀 → 缺省拒绝
//          - ShellGuard::is_dangerous()：大小写不敏感子串匹配
// 关联：secure_tool_registry.cpp (装饰器) 消费本文件的 check 接口
// 设计依据：ADR-0004 (ToolRegistry 安全模型) §3-§4
// 作者：docs-code-drift-audit-2026-06 change
// 最后修改日期：2026-06-13

#include "agenticdsl/policy/path_policy.h"

#include <algorithm>
#include <cctype>
#include <filesystem>

namespace agenticdsl {

// =====================================================================
// PathPolicy::check
// =====================================================================
IPathPolicy::CheckResult PathPolicy::check(const std::string& path) const {
  namespace fs = std::filesystem;

  // Step 1: 规范化路径（解析符号链接 + 相对路径）
  // canonical() 在路径不存在时抛 filesystem_error；用 try/catch 转 denied
  std::string canonical;
  try {
    canonical = fs::weakly_canonical(fs::path(path)).string();
  } catch (...) {
    return {false, "invalid_path", std::nullopt};
  }

  // Step 2: 检查 denied patterns（优先级最高）
  // C++17 std::regex 无 str() / pattern() / source() 公共访问器；
  // 这里以 enumerated 字符串作为 matched_denied 标识（调试足够）。
  for (size_t i = 0; i < denied_patterns.size(); ++i) {
    const auto& pattern = denied_patterns[i];
    if (std::regex_search(canonical, pattern)) {
      return {false, "path_matches_denied_pattern",
              std::string("denied_pattern_index=") + std::to_string(i)};
    }
  }

  // Step 3: 检查 allowed prefixes
  // 空 allowed_prefixes 表示"仅靠 denied_patterns 保护"，放行
  bool in_allowed = allowed_prefixes.empty();
  for (const auto& prefix : allowed_prefixes) {
    if (canonical.rfind(prefix, 0) == 0) {  // starts_with 等价（C++20 前）
      in_allowed = true;
      break;
    }
  }
  if (!in_allowed) {
    return {false, "path_not_in_allowed_prefix", std::nullopt};
  }

  // Step 4: 通过
  return {true, "allowed", std::nullopt};
}

// =====================================================================
// ShellGuard::is_dangerous
// =====================================================================
bool ShellGuard::is_dangerous(const std::string& command) {
  // 转为小写副本（避免每次子串匹配都 lowercase 整个命令）
  std::string lower_cmd;
  lower_cmd.reserve(command.size());
  std::transform(command.begin(), command.end(), std::back_inserter(lower_cmd),
                 [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

  for (const auto* pattern : DANGEROUS_PATTERNS) {
    if (lower_cmd.find(pattern) != std::string::npos) {
      return true;
    }
  }
  return false;
}

std::optional<std::string> ShellGuard::suggest_safe_alternative(const std::string& command) {
  // 简单规则：仅对高频危险模式给出建议
  if (command.find("rm -rf") != std::string::npos) {
    return "rm -i (interactive mode)";
  }
  if (command.find("| bash") != std::string::npos ||
      command.find("; bash") != std::string::npos) {
    return "use a dedicated tool instead of piping to bash";
  }
  return std::nullopt;
}

} // namespace agenticdsl
