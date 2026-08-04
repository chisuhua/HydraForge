// src/common/tools/tool_hook_registry.cpp
// 功能描述：ToolHookRegistry 实现 — hook 存储、ADR-0043 glob 匹配、priority 排序、FailClosed/FailOpen 处理
// 设计依据：ADR-0069 §决策 1-4 / ADR-0043 tool_glob 约定
// 作者：AgenticDSL Phase 6 / adr-0069-tool-coordinator-hooks change
// 最后修改日期：2026-08-04
#include "common/tools/tool_hook_registry.h"

#include <algorithm>
#include <sstream>

namespace agenticdsl {
namespace {

std::vector<std::string> split(const std::string& s, char delim) {
  std::vector<std::string> parts;
  std::string cur;
  for (char c : s) {
    if (c == delim) {
      if (!cur.empty()) parts.push_back(std::move(cur));
      cur.clear();
    } else {
      cur += c;
    }
  }
  if (!cur.empty()) parts.push_back(std::move(cur));
  return parts;
}

bool match_tool_glob(const std::string& pattern, const std::string& name) {
  if (pattern == "*") return true;
  auto p = split(pattern, '/');
  auto n = split(name, '/');
  if (p.size() != n.size()) return false;
  for (std::size_t i = 0; i < p.size(); ++i) {
    if (p[i] != "*" && p[i] != n[i]) return false;
  }
  return true;
}

}  // namespace

void ToolHookRegistry::register_pre_hook(const std::string& tool_glob,
                                         PreHook hook,
                                         int priority,
                                         HookErrorPolicy policy) {
  pre_hooks_.push_back({tool_glob, std::move(hook), priority, policy, next_seq_++});
}

void ToolHookRegistry::register_post_hook(const std::string& tool_glob,
                                          PostHook hook,
                                          int priority,
                                          HookErrorPolicy policy) {
  post_hooks_.push_back({tool_glob, std::move(hook), priority, policy, next_seq_++});
}

PreHookResult ToolHookRegistry::apply_pre_hooks(
    const ToolMetadata& meta,
    const ToolCallContext& ctx,
    const std::unordered_map<std::string, std::string>& args,
    std::vector<std::string>& warnings) const {
  std::vector<const PreHookEntry*> matched;
  matched.reserve(pre_hooks_.size());
  for (const auto& e : pre_hooks_) {
    if (match_tool_glob(e.tool_glob, meta.name)) matched.push_back(&e);
  }

  std::sort(matched.begin(), matched.end(),
            [](const PreHookEntry* a, const PreHookEntry* b) {
              if (a->priority != b->priority) return a->priority < b->priority;
              return a->seq < b->seq;
            });

  std::unordered_map<std::string, std::string> current = args;
  for (const auto* e : matched) {
    std::string hook_name = "pre_hook_" + std::to_string(e->seq);
    try {
      PreHookResult r = e->hook(meta, ctx, current);
      if (r.action == PreHookResult::Deny) {
        r.deny_reason = "[" + hook_name + "] " + r.deny_reason;
        return r;
      }
      if (r.action == PreHookResult::ModifyArgs) {
        current = std::move(r.modified_args);
      }
    } catch (const std::exception& ex) {
      if (e->policy == HookErrorPolicy::FailClosed) {
        PreHookResult r;
        r.action = PreHookResult::Deny;
        r.deny_reason = "[" + hook_name + "] exception: " + ex.what();
        return r;
      }
      warnings.push_back("[" + hook_name + "] skipped: " + ex.what());
    } catch (...) {
      if (e->policy == HookErrorPolicy::FailClosed) {
        PreHookResult r;
        r.action = PreHookResult::Deny;
        r.deny_reason = "[" + hook_name + "] exception: unknown";
        return r;
      }
      warnings.push_back("[" + hook_name + "] skipped: unknown exception");
    }
  }

  PreHookResult ok;
  ok.action = PreHookResult::Continue;
  ok.modified_args = std::move(current);
  return ok;
}

ToolResult ToolHookRegistry::apply_post_hooks(
    const ToolMetadata& meta,
    const ToolCallContext& ctx,
    ToolResult result,
    std::vector<std::string>& warnings) const {
  std::vector<const PostHookEntry*> matched;
  matched.reserve(post_hooks_.size());
  for (const auto& e : post_hooks_) {
    if (match_tool_glob(e.tool_glob, meta.name)) matched.push_back(&e);
  }

  std::sort(matched.begin(), matched.end(),
            [](const PostHookEntry* a, const PostHookEntry* b) {
              if (a->priority != b->priority) return a->priority < b->priority;
              return a->seq < b->seq;
            });

  ToolResult current = std::move(result);
  for (const auto* e : matched) {
    std::string hook_name = "post_hook_" + std::to_string(e->seq);
    try {
      PostHookResult r = e->hook(meta, ctx, current);
      if (r.modify_result) current = std::move(r.modified_result);
    } catch (const std::exception& ex) {
      if (e->policy == HookErrorPolicy::FailClosed) {
        return ToolResult::error(
            ErrorCode::PermissionDenied,
            "[" + hook_name + "] exception: " + ex.what());
      }
      warnings.push_back("[" + hook_name + "] skipped: " + ex.what());
    } catch (...) {
      if (e->policy == HookErrorPolicy::FailClosed) {
        return ToolResult::error(
            ErrorCode::PermissionDenied,
            "[" + hook_name + "] exception: unknown");
      }
      warnings.push_back("[" + hook_name + "] skipped: unknown exception");
    }
  }
  return current;
}

}  // namespace agenticdsl
