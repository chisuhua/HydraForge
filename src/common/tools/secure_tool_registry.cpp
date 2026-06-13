// src/common/tools/secure_tool_registry.cpp
// 文件头注释
// 功能描述：SecureToolRegistry 装饰器实现（ADR-0004 §5 最小实施）
//          同步路径：check_security → ToolRegistry::call_tool 透传
//          安全检查项：
//            1. disabled_tools_ 黑名单 → PermissionDenied
//            2. 工具未注册 → ToolNotRegistered
//            3. 工具名 "fs." 前缀 + args["path"] → PathPolicy.check
//            4. 工具名 "shell.exec" + args["command"] → ShellGuard.is_dangerous
// 关联：ToolRegistry（被装饰对象）通过 ToolRegistry& 或 shared_ptr 传入
// 设计依据：ADR-0004 (ToolRegistry 安全模型) §5
// 作者：docs-code-drift-audit-2026-06 change
// 最后修改日期：2026-06-13

#include "agenticdsl/tools/secure_tool_registry.h"

#include "common/tools/registry.h"

#include <utility>

namespace agenticdsl {

// =====================================================================
// 构造
// =====================================================================

SecureToolRegistry::SecureToolRegistry(ToolRegistry& registry)
    : registry_ref_(&registry),
      registry_holder_(&registry, [](ToolRegistry*) { /* no-op deleter for non-owning */ }) {
  // 当以引用构造时，holder 仅观察生命周期（引用由调用方保证）
}

SecureToolRegistry::SecureToolRegistry(std::shared_ptr<ToolRegistry> registry)
    : registry_shared_(std::move(registry)),
      registry_holder_(registry_shared_) {
  registry_ref_ = registry_shared_.get();
}

// =====================================================================
// 安全管理 API
// =====================================================================

void SecureToolRegistry::disable_tool(const std::string& tool_name) {
  std::lock_guard<std::mutex> lock(mutex_);
  disabled_tools_[tool_name] = true;
}

void SecureToolRegistry::enable_tool(const std::string& tool_name) {
  std::lock_guard<std::mutex> lock(mutex_);
  disabled_tools_.erase(tool_name);
}

bool SecureToolRegistry::is_disabled(const std::string& tool_name) const {
  std::lock_guard<std::mutex> lock(mutex_);
  auto it = disabled_tools_.find(tool_name);
  return it != disabled_tools_.end() && it->second;
}

void SecureToolRegistry::set_path_policy(const std::string& tool_name, PathPolicy policy) {
  std::lock_guard<std::mutex> lock(mutex_);
  tool_policies_[tool_name] = std::move(policy);
}

PathPolicy SecureToolRegistry::get_path_policy(const std::string& tool_name) const {
  std::lock_guard<std::mutex> lock(mutex_);
  auto it = tool_policies_.find(tool_name);
  if (it != tool_policies_.end()) {
    return it->second;  // 拷贝
  }
  return default_policy_;
}

void SecureToolRegistry::set_default_path_policy(PathPolicy policy) {
  std::lock_guard<std::mutex> lock(mutex_);
  default_policy_ = std::move(policy);
}

PathPolicy SecureToolRegistry::get_default_path_policy() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return default_policy_;
}

// =====================================================================
// 内部：安全检查
// =====================================================================

std::optional<SecurityError> SecureToolRegistry::check_security(
    const std::string& tool_name,
    const std::unordered_map<std::string, std::string>& args) const {
  // Step 1: 禁用黑名单
  if (is_disabled(tool_name)) {
    return SecurityError{
        SecurityError::Code::PermissionDenied,
        "Tool '" + tool_name + "' is disabled by security policy",
        tool_name,
        "disabled=true"
    };
  }

  // Step 2: 工具注册检查（必须先有 base registry 引用）
  if (!registry_ref_) {
    return SecurityError{
        SecurityError::Code::ToolNotRegistered,
        "Internal: registry_ref_ is null",
        tool_name,
        "no_base_registry"
    };
  }
  if (!registry_ref_->has_tool(tool_name)) {
    return SecurityError{
        SecurityError::Code::ToolNotRegistered,
        "Tool '" + tool_name + "' is not registered",
        tool_name,
        "not_in_registry"
    };
  }

  // Step 3: 文件系统工具 → 路径策略
  // 启发式：工具名以 "fs." 开头 + args 含 "path" 键 → 走 PathPolicy
  if (tool_name.rfind("fs.", 0) == 0) {
    auto path_it = args.find("path");
    if (path_it != args.end()) {
      PathPolicy policy = get_path_policy(tool_name);
      auto result = policy.check(path_it->second);
      if (!result.allowed) {
        return SecurityError{
            SecurityError::Code::PathViolation,
            "Path '" + path_it->second + "' " + result.reason,
            tool_name,
            result.matched_denied.value_or("unknown")
        };
      }
    }
  }

  // Step 4: Shell 工具 → 危险命令检测
  // 工具名 == "shell.exec" + args 含 "command" 键 → 走 ShellGuard
  if (tool_name == "shell.exec") {
    auto cmd_it = args.find("command");
    if (cmd_it != args.end()) {
      if (ShellGuard::is_dangerous(cmd_it->second)) {
        return SecurityError{
            SecurityError::Code::DangerousCommand,
            "Shell command contains dangerous pattern",
            tool_name,
            cmd_it->second
        };
      }
    }
  }

  return std::nullopt;  // 全部通过
}

// =====================================================================
// 公开调用入口
// =====================================================================

SecureToolRegistry::Result SecureToolRegistry::call_direct(
    const std::string& tool_name,
    const std::unordered_map<std::string, std::string>& args) {
  // Step 1: 安全检查
  if (auto err = check_security(tool_name, args)) {
    Result r;
    r.allowed = false;
    r.error = std::move(*err);
    return r;
  }

  // Step 2: 透传
  return call_passthrough(tool_name, args);
}

SecureToolRegistry::Result SecureToolRegistry::call_passthrough(
    const std::string& tool_name,
    const std::unordered_map<std::string, std::string>& args) {
  // disabled 检查仍生效（passthrough 不绕过黑名单）
  if (is_disabled(tool_name)) {
    Result r;
    r.allowed = false;
    r.error = SecurityError{
        SecurityError::Code::PermissionDenied,
        "Tool '" + tool_name + "' is disabled",
        tool_name,
        "disabled=true"
    };
    return r;
  }

  if (!registry_ref_ || !registry_ref_->has_tool(tool_name)) {
    Result r;
    r.allowed = false;
    r.error = SecurityError{
        SecurityError::Code::ToolNotRegistered,
        "Tool '" + tool_name + "' is not registered",
        tool_name,
        "not_in_registry"
    };
    return r;
  }

  // 透传：try/catch 吸收 ToolRegistry 内部异常
  try {
    Result r;
    r.allowed = true;
    r.payload = registry_ref_->call_tool(tool_name, args);
    return r;
  } catch (const std::exception& e) {
    Result r;
    r.allowed = false;
    r.error = SecurityError{
        SecurityError::Code::Unknown,
        std::string("Tool execution exception: ") + e.what(),
        tool_name,
        "exception_caught"
    };
    return r;
  }
}

} // namespace agenticdsl
