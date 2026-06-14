// agenticdsl/policy/path_policy.h
// 文件头注释
// 功能描述：路径策略与 Shell 守卫抽象层（ADR-0004 §3-4 实施）
//          - IPathPolicy：路径白名单/黑名单抽象接口
//          - PathPolicy：默认实现（allowed_prefixes + denied_patterns）
//          - ShellGuard：危险 Shell 命令子串检测（仅作最小防御）
//          - SecurityError：统一安全错误类型，供 SecureToolRegistry 使用
// 关联类型 ToolCategory / ToolMetadata / ToolCallContext / ApprovalPolicy /
//          LayerProfile 已在 src/common/policy/execution_policy.h 定义，
//          本头文件通过 #include 引入，不重复声明。
// 设计依据：ADR-0004 (ToolRegistry 安全模型) §3-§4 + ADR-0031 (IExecutionPolicy)
// 作者：docs-code-drift-audit-2026-06 change
// 最后修改日期：2026-06-13
#pragma once

#include "common/policy/execution_policy.h"

#include <array>
#include <optional>
#include <regex>
#include <string>
#include <vector>

namespace agenticdsl {

// =====================================================================
// 路径策略抽象接口（ADR-0004 §3）
// =====================================================================

/**
 * @brief 路径检查抽象接口
 *
 * 实现类负责判定给定路径是否允许访问。
 * 默认实现 PathPolicy 使用白名单前缀 + 黑名单正则组合策略。
 *
 * 不变量：
 *  - check() 必须是 const（无副作用）
 *  - 实现可在多线程并发调用
 *  - 路径不存在时应返回 denied + reason="invalid_path"
 */
class IPathPolicy {
 public:
  virtual ~IPathPolicy() = default;

  struct CheckResult {
    bool allowed;
    std::string reason;  // "allowed" / "invalid_path" / "path_matches_denied_pattern" / "path_not_in_allowed_prefix"
    std::optional<std::string> matched_denied;  // 匹配到的拒绝模式（仅 denied 时填充）
  };

  virtual CheckResult check(const std::string& path) const = 0;
};

/**
 * @brief 默认 PathPolicy 实现：白名单前缀 + 黑名单正则
 *
 * 判定顺序（拒绝优先）：
 *  1. 规范化路径失败 → 拒绝
 *  2. 匹配任何 denied_patterns → 拒绝（并记录匹配的模式）
 *  3. 不在任何 allowed_prefixes 中（且 allowed_prefixes 非空）→ 拒绝
 *  4. 其它 → 允许
 *
 * 线程安全：const 方法，无内部可变状态，可任意并发调用。
 */
class PathPolicy : public IPathPolicy {
 public:
  PathPolicy() = default;

  // 允许的前缀目录（jail root）。空 = 不限制（仅靠 denied_patterns 保护）。
  std::vector<std::string> allowed_prefixes = {
    "/tmp/hydraforge",
    "./workspace"
  };

  // 拒绝的模式（正则，ECMAScript 语法）。优先级最高。
  std::vector<std::regex> denied_patterns = {
    std::regex(R"(/etc/passwd)"),
    std::regex(R"(/\.ssh/)"),
    std::regex(R"(/proc/)"),
    std::regex(R"(/\.aws/)"),
    std::regex(R"(/\.config/)"),
    // Windows 系统目录（跨平台占位）
    std::regex(R"(C:\\Windows)")
  };

  CheckResult check(const std::string& path) const override;
};

// =====================================================================
// Shell 守卫（ADR-0004 §4 最小实施）
// =====================================================================

/**
 * @brief Shell 命令危险检测器（最小静态子串匹配）
 *
 * 注意：本类仅作**应用层**防御，**不替代** OS 级沙箱（bubblewrap / Seatbelt）。
 * 实现策略：大小写不敏感子串匹配 DANGEROUS_PATTERNS 中的任何模式。
 *
 * 不实现原因（Phase 1）：
 *  - 完整 Shell 解析需要 AST 级分析，超出 MVP 范围
 *  - OS 级隔离（ADR-0004 §Phase 2 沙箱）已列入未来工作
 */
struct ShellGuard {
  // 危险命令子串模式（constexpr 数组，C++17 起允许 std::array<const char*, N>）
  static constexpr std::array<const char*, 13> DANGEROUS_PATTERNS = {
    "rm -rf",
    "rm -r /",
    "dd if=",
    "mkfs",
    "| bash",
    "; bash",
    "&& bash",
    "> /dev/",
    "curl | sh",
    "wget | sh",
    "| sh",
    "chmod 777",
    "chown root"
  };

  /**
   * @brief 检查命令是否包含危险模式
   * @param command 待检查的 shell 命令字符串
   * @return true 表示发现危险模式（应当拒绝）
   */
  static bool is_dangerous(const std::string& command);

  /**
   * @brief 建议安全替代命令（可选，Phase 1 仅对 rm -rf 给出建议）
   * @param command 原始命令
   * @return 替代建议字符串，未匹配则返回 std::nullopt
   */
  static std::optional<std::string> suggest_safe_alternative(const std::string& command);
};

// =====================================================================
// 安全错误类型（ADR-0004 §5 最小子集）
// =====================================================================

/**
 * @brief 安全检查错误描述
 *
 * 用于 SecureToolRegistry 在拒绝调用时返回结构化错误。
 * 不使用 std::expected（C++23 才标准化）——改用 (error, message) 对。
 */
struct SecurityError {
  enum class Code {
    PermissionDenied,   // 工具未注册或被禁用
    PathViolation,      // 路径违反 PathPolicy
    DangerousCommand,   // Shell 命令触发 ShellGuard
    ToolNotRegistered,  // 工具不存在
    Unknown
  };

  Code code = Code::Unknown;
  std::string message;
  std::string tool_name;
  std::string details;
};

} // namespace agenticdsl
