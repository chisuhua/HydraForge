// src/common/tools/tool_hook_registry.h
// 功能描述：ToolHookRegistry 具体实现 — hook 存储、ADR-0043 glob 匹配、priority 排序执行
// 设计依据：ADR-0069 §决策 1-4 / ADR-0043 tool_glob 约定
// 作者：AgenticDSL Phase 6 / adr-0069-tool-coordinator-hooks change
// 最后修改日期：2026-08-04
#pragma once

#include "agenticdsl/contract/itool_hook_registry.h"

#include <cstddef>
#include <string>
#include <vector>

namespace agenticdsl {

struct PreHookEntry {
  std::string tool_glob;
  PreHook hook;
  int priority;
  HookErrorPolicy policy;
  std::size_t seq;
};

struct PostHookEntry {
  std::string tool_glob;
  PostHook hook;
  int priority;
  HookErrorPolicy policy;
  std::size_t seq;
};

class ToolHookRegistry : public IToolHookRegistry {
 public:
  ToolHookRegistry() = default;

  void register_pre_hook(const std::string& tool_glob,
                         PreHook hook,
                         int priority,
                         HookErrorPolicy policy) override;

  void register_post_hook(const std::string& tool_glob,
                          PostHook hook,
                          int priority,
                          HookErrorPolicy policy) override;

  PreHookResult apply_pre_hooks(
      const ToolMetadata& meta,
      const ToolCallContext& ctx,
      const std::unordered_map<std::string, std::string>& args,
      std::vector<std::string>& warnings) const override;

  ToolResult apply_post_hooks(
      const ToolMetadata& meta,
      const ToolCallContext& ctx,
      ToolResult result,
      std::vector<std::string>& warnings) const override;

 private:
  std::vector<PreHookEntry> pre_hooks_;
  std::vector<PostHookEntry> post_hooks_;
  std::size_t next_seq_ = 0;
};

}  // namespace agenticdsl
