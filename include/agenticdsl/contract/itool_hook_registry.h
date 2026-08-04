// include/agenticdsl/contract/itool_hook_registry.h
// 功能描述：ToolCoordinator 工具调用链 pre/post hook L3 契约
// 设计依据：ADR-0069 §决策 3 / ADR-0043 tool_glob 约定
// 作者：AgenticDSL Phase 6 / adr-0069-tool-coordinator-hooks change
// 最后修改日期：2026-08-04
#pragma once

#include "common/policy/execution_policy.h"  // ToolMetadata, ToolCallContext
#include "core/types/tool_result.h"          // ToolResult

#include <functional>
#include <string>
#include <unordered_map>
#include <vector>

namespace agenticdsl {

enum class HookErrorPolicy { FailClosed, FailOpen };

struct PreHookResult {
  enum Action { Continue, Deny, ModifyArgs } action = Continue;
  std::unordered_map<std::string, std::string> modified_args;
  std::string deny_reason;
};

struct PostHookResult {
  bool modify_result = false;
  ToolResult modified_result;
};

using PreHook = std::function<PreHookResult(
    const ToolMetadata&,
    const ToolCallContext&,
    const std::unordered_map<std::string, std::string>& args)>;

using PostHook = std::function<PostHookResult(
    const ToolMetadata&,
    const ToolCallContext&,
    const ToolResult&)>;

class IToolHookRegistry {
 public:
  virtual ~IToolHookRegistry() = default;

  virtual void register_pre_hook(const std::string& tool_glob,
                                 PreHook hook,
                                 int priority,
                                 HookErrorPolicy policy) = 0;

  virtual void register_post_hook(const std::string& tool_glob,
                                  PostHook hook,
                                  int priority,
                                  HookErrorPolicy policy) = 0;

  // Execute matching pre-hooks in priority order and return the final action.
  // `warnings` accumulates FailOpen exception messages.
  virtual PreHookResult apply_pre_hooks(
      const ToolMetadata& meta,
      const ToolCallContext& ctx,
      const std::unordered_map<std::string, std::string>& args,
      std::vector<std::string>& warnings) const = 0;

  // Execute matching post-hooks in priority order and return the final result.
  // FailClosed exceptions become an error ToolResult; FailOpen exceptions are
  // recorded in `warnings` and the original result is returned.
  virtual ToolResult apply_post_hooks(
      const ToolMetadata& meta,
      const ToolCallContext& ctx,
      ToolResult result,
      std::vector<std::string>& warnings) const = 0;
};

}  // namespace agenticdsl
