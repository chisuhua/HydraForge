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

// =====================================================================
// IToolRegistry 9 override 实现 (P1.T2 新增, 委托式多继承)
// =====================================================================

// 基础查询 (3)

// 注: has_tool 在 SecureToolRegistry 中需要返回 "未禁用且已注册" 的复合状态
// 为保持装饰器语义, 仍返回 wrapped ToolRegistry 的 has_tool (调用方需结合 is_disabled 自行判断)
bool SecureToolRegistry::has_tool(const std::string& name) const {
  return registry_ref_ && registry_ref_->has_tool(name);
}

nlohmann::json SecureToolRegistry::call_tool(
    const std::string& name,
    const std::unordered_map<std::string, std::string>& args) {
  // 走 call_direct 走安全检查, 然后 Result.allowed + payload → json 转换
  Result r = call_direct(name, args);
  if (r.allowed) {
    return r.payload;
  }
  // 安全检查失败: 返回结构化错误 JSON
  nlohmann::json err;
  err["success"] = false;
  err["error_code"] = static_cast<int>(r.error.code);
  err["error_message"] = r.error.message;
  err["tool_name"] = r.error.tool_name;
  err["details"] = r.error.details;
  return err;
}

std::vector<std::string> SecureToolRegistry::list_tools() const {
  if (registry_ref_) {
    return registry_ref_->list_tools();
  }
  return {};
}

// 函数工具注册 (1, 模板桥接 — 委托到 wrapped ToolRegistry)

void SecureToolRegistry::register_tool_function(std::string name, ToolMetadata meta, ToolFunc fn) {
  if (registry_ref_) {
    registry_ref_->register_tool_function(std::move(name), std::move(meta), std::move(fn));
  }
}

// LLM 工具管理 (4) — 全部委托到 wrapped ToolRegistry (LLM 工具不涉及安全检查)

void SecureToolRegistry::register_llm_tool(
    std::string name,
    std::unique_ptr<ILLMTool> tool,
    const LLMParams& default_params) {
  if (registry_ref_) {
    registry_ref_->register_llm_tool(std::move(name), std::move(tool), default_params);
  }
}

bool SecureToolRegistry::is_llm_tool(const std::string& name) const {
  return registry_ref_ && registry_ref_->is_llm_tool(name);
}

const LLMParams& SecureToolRegistry::get_llm_params(const std::string& name) const {
  if (!registry_ref_) {
    static const LLMParams kEmpty{};
    return kEmpty;
  }
  return registry_ref_->get_llm_params(name);
}

nlohmann::json SecureToolRegistry::call_llm_tool(
    const std::string& name,
    const std::string& prompt,
    const LLMParams& params) {
  if (registry_ref_) {
    return registry_ref_->call_llm_tool(name, prompt, params);
  }
  return nlohmann::json{{"error", "registry_ref_ is null"}};
}

// 成本回调 (1) — 委托到 wrapped ToolRegistry

void SecureToolRegistry::set_cost_callback(CostCallback cb) {
  if (registry_ref_) {
    registry_ref_->set_cost_callback(std::move(cb));
  }
}

} // namespace agenticdsl
