// agenticdsl/tools/secure_tool_registry.h
// 文件头注释
// 功能描述：SecureToolRegistry 装饰器（ADR-0004 §5 最小实施）
//          - 包装 ToolRegistry，在 call_tool 前同步执行安全检查
//          - 检查项：(a) 工具是否被禁用 (b) 路径是否违反 PathPolicy (c) Shell 命令是否危险
//          - 仅交付同步 call_direct() 路径；异步 call_secure() 等待 EventBus/TUI 后再实施
//          - 不修改 ToolRegistry 本身（保持 engine.h Stage 4 解耦成果不变）
// 设计依据：ADR-0004 (ToolRegistry 安全模型) §5
// 作者：docs-code-drift-audit-2026-06 change
// 最后修改日期：2026-06-13
#pragma once

#include "agenticdsl/policy/path_policy.h"

#include <nlohmann/json.hpp>

#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>

namespace agenticdsl {

class ToolRegistry;

/**
 * @brief 安全工具注册装饰器（同步路径）
 *
 * 装饰器模式：在不修改 ToolRegistry 的前提下为其增加安全层。
 * 使用方式：
 *   ToolRegistry base;
 *   SecureToolRegistry secure(base);
 *   secure.disable_tool("shell.exec");      // 显式禁用
 *   auto r1 = secure.call_direct("fs.read", {{"path", "/etc/passwd"}});  // PathViolation
 *   auto r2 = secure.call_direct("fs.read", {{"path", "./workspace/x"}}); // OK
 *   auto r3 = secure.call_direct("web_search", {});                       // 透传
 *
 * 关键约束：
 *  - call_direct() 不抛异常；所有错误通过 SecurityError 返回
 *  - 持有 ToolRegistry 引用，生命周期由调用方保证
 *  - 线程安全：内部用 std::mutex 保护 disabled_tools_ / tool_policies_
 */
class SecureToolRegistry {
 public:
  // 装饰结果：成功（has_value=true）或拒绝（has_value=false, error 填充）
  struct Result {
    bool allowed = false;
    nlohmann::json payload;   // 成功时为工具返回值
    SecurityError error;      // 失败时填充
  };

  /**
   * @brief 构造装饰器，包装已存在的 ToolRegistry 引用
   * @param registry 被包装的 ToolRegistry（生命周期需长于本装饰器）
   */
  explicit SecureToolRegistry(ToolRegistry& registry);

  /**
   * @brief 构造装饰器，包装已存在的 ToolRegistry 共享指针
   * @param registry 被包装的 ToolRegistry 共享所有权
   */
  explicit SecureToolRegistry(std::shared_ptr<ToolRegistry> registry);

  // === 安全管理 API ===

  /**
   * @brief 禁用某个工具（黑名单）
   */
  void disable_tool(const std::string& tool_name);

  /**
   * @brief 重新启用某个工具
   */
  void enable_tool(const std::string& tool_name);

  /**
   * @brief 检查工具是否被禁用
   */
  bool is_disabled(const std::string& tool_name) const;

  /**
   * @brief 为某个工具设置自定义 PathPolicy（覆盖默认策略）
   */
  void set_path_policy(const std::string& tool_name, PathPolicy policy);

  /**
   * @brief 获取当前 PathPolicy（无自定义时返回默认）
   */
  PathPolicy get_path_policy(const std::string& tool_name) const;

  // === 调用入口（同步） ===

  /**
   * @brief 安全同步调用
   *
   * 检查流程：
   *  1. 工具是否被禁用 → PermissionDenied
   *  2. 工具是否注册 → ToolNotRegistered
   *  3. 工具名以 "fs." 开头 + args 含 "path" → PathPolicy.check
   *  4. 工具名 == "shell.exec" + args 含 "command" → ShellGuard.is_dangerous
   *  5. 全部通过 → 透传 ToolRegistry::call_tool
   *
   * @param tool_name 工具名称
   * @param args 参数表（key=value 字符串）
   * @return Result 包含 allowed + payload 或 error
   */
  Result call_direct(const std::string& tool_name,
                      const std::unordered_map<std::string, std::string>& args);

  /**
   * @brief 透传调用（不做安全检查，仅做"未注册"判定）
   *
   * 用于 ReadOnly 类工具的快速通道；调用方需自行保证安全。
   * 注意：本方法**不**绕开 disable_tool()——禁用列表仍生效。
   */
  Result call_passthrough(const std::string& tool_name,
                          const std::unordered_map<std::string, std::string>& args);

  // === 默认策略访问 ===

  /**
   * @brief 替换默认 PathPolicy（影响所有未自定义的工具）
   */
  void set_default_path_policy(PathPolicy policy);

  /**
   * @brief 获取默认 PathPolicy（拷贝）
   */
  PathPolicy get_default_path_policy() const;

 private:
  // 实际执行检查（内部）
  std::optional<SecurityError> check_security(
      const std::string& tool_name,
      const std::unordered_map<std::string, std::string>& args) const;

  // 持有可能的两种所有权
  ToolRegistry* registry_ref_ = nullptr;               // 引用场景
  std::shared_ptr<ToolRegistry> registry_shared_;      // 共享所有权场景
  std::shared_ptr<ToolRegistry> registry_holder_;      // 内部统一持有，确保生命周期

  mutable std::mutex mutex_;
  std::unordered_map<std::string, PathPolicy> tool_policies_;
  PathPolicy default_policy_;
  std::unordered_map<std::string, bool> disabled_tools_;
};

} // namespace agenticdsl
