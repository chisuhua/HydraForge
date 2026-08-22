// src/core/agent_hook_registry.cpp
// 功能描述：IAgentHookRegistry 的内存参考实现 (ADR-0081 Approved 2026-08-21)
//
//          V1 最小骨架：
//          - shared_mutex 并发模型 (read-shared / write-exclusive)
//          - agent_glob 通配匹配（支持 `*` / `?`，与 ADR-0043 naming 一致）
//          - priority 排序（高优先级先执行）
//          - HookErrorPolicy 复用 ADR-0069 (FailClosed / FailOpen)
//          - 不发射 IInteractionBus 事件（V1 简化，事件留给实施期独立 change）
//
// 设计依据：ADR-0081 + ADR-0069 HookErrorPolicy 复用 + ADR-0043 glob 约定
// 作者：HydraForge Sprint 22 / adr-0081-promote-to-approved
// 最后修改日期：2026-08-21

#include "agenticdsl/contract/iagent_hook_registry.h"

#include <algorithm>
#include <mutex>
#include <shared_mutex>
#include <unordered_map>
#include <utility>
#include <vector>

namespace agenticdsl {

namespace {

// glob_match("react-loop/*", "react-loop-v1") → true
// glob_match("*", "any-agent") → true
// Supports * (match any chars) and ? (match single char).
bool glob_match(const std::string& pattern, const std::string& name) {
  size_t pi = 0, ti = 0;
  size_t star_p = std::string::npos, star_t = 0;

  while (ti < name.size()) {
    if (pi < pattern.size() && (pattern[pi] == '?' || pattern[pi] == name[ti])) {
      ++pi;
      ++ti;
    } else if (pi < pattern.size() && pattern[pi] == '*') {
      star_p = pi++;
      star_t = ti;
    } else if (star_p != std::string::npos) {
      pi = star_p + 1;
      ti = ++star_t;
    } else {
      return false;
    }
  }

  while (pi < pattern.size() && pattern[pi] == '*') ++pi;
  return pi == pattern.size();
}

struct HookEntry {
  AgentPreHook hook;
  int priority;
  HookErrorPolicy policy;
};

struct PostHookEntry {
  AgentPostHook hook;
  int priority;
  HookErrorPolicy policy;
};

}  // namespace

class InMemoryAgentHookRegistry : public IAgentHookRegistry {
 public:
  void register_pre_hook(const std::string& agent_glob,
                         AgentPreHook hook,
                         int priority,
                         HookErrorPolicy policy) override {
    if (!hook) return;
    std::unique_lock<std::shared_mutex> lock(mutex_);
    pre_hooks_[agent_glob].push_back({std::move(hook), priority, policy});
  }

  void register_post_hook(const std::string& agent_glob,
                          AgentPostHook hook,
                          int priority,
                          HookErrorPolicy policy) override {
    if (!hook) return;
    std::unique_lock<std::shared_mutex> lock(mutex_);
    post_hooks_[agent_glob].push_back({std::move(hook), priority, policy});
  }

  AgentPreHookResult apply_pre_hooks(
      const IAgent& agent,
      const std::string& step_input,
      std::vector<std::string>& warnings) const override {
    std::vector<HookEntry> matching;
    std::string agent_name = agent.name();
    {
      std::shared_lock<std::shared_mutex> lock(mutex_);
      for (const auto& [pattern, entries] : pre_hooks_) {
        if (glob_match(pattern, agent_name)) {
          for (const auto& e : entries) matching.push_back(e);
        }
      }
    }
    // priority 高→低 排序
    std::sort(matching.begin(), matching.end(),
              [](const HookEntry& a, const HookEntry& b) {
                return a.priority > b.priority;
              });

    AgentPreHookResult result;
    std::string current_input = step_input;
    for (const auto& entry : matching) {
      try {
        AgentPreHookResult r = entry.hook(agent, current_input);
        if (r.action == AgentPreHookResult::Deny) {
          // fail-closed: deny 不可被后续 hook 覆盖
          return r;
        }
        if (r.action == AgentPreHookResult::ModifyContext) {
          current_input = r.modified_context.count("input") > 0
                              ? r.modified_context.at("input")
                              : current_input;
        }
      } catch (const std::exception& e) {
        if (entry.policy == HookErrorPolicy::FailClosed) {
          AgentPreHookResult deny;
          deny.action = AgentPreHookResult::Deny;
          deny.deny_reason = std::string("pre-hook failed: ") + e.what();
          return deny;
        } else {
          warnings.push_back(std::string("pre-hook failed (FailOpen): ") + e.what());
        }
      }
    }
    return result;
  }

  AgentPostHookResult apply_post_hooks(
      const IAgent& agent,
      const std::string& step_output,
      std::vector<std::string>& warnings) const override {
    std::vector<PostHookEntry> matching;
    std::string agent_name = agent.name();
    {
      std::shared_lock<std::shared_mutex> lock(mutex_);
      for (const auto& [pattern, entries] : post_hooks_) {
        if (glob_match(pattern, agent_name)) {
          for (const auto& e : entries) matching.push_back(e);
        }
      }
    }
    std::sort(matching.begin(), matching.end(),
              [](const PostHookEntry& a, const PostHookEntry& b) {
                return a.priority > b.priority;
              });

    AgentPostHookResult result;
    std::string current_output = step_output;
    for (const auto& entry : matching) {
      try {
        AgentPostHookResult r = entry.hook(agent, current_output);
        if (r.modify_result) {
          current_output = r.modified_output;
          result.modify_result = true;
          result.modified_output = current_output;
        }
      } catch (const std::exception& e) {
        if (entry.policy == HookErrorPolicy::FailClosed) {
          warnings.push_back(std::string("post-hook failed (FailClosed): ") + e.what());
        } else {
          warnings.push_back(std::string("post-hook failed (FailOpen): ") + e.what());
        }
      }
    }
    return result;
  }

 private:
  mutable std::shared_mutex mutex_;
  std::unordered_map<std::string, std::vector<HookEntry>> pre_hooks_;
  std::unordered_map<std::string, std::vector<PostHookEntry>> post_hooks_;
};

// 工厂函数：创建 InMemoryAgentHookRegistry 实例
std::unique_ptr<IAgentHookRegistry> make_in_memory_agent_hook_registry() {
  return std::make_unique<InMemoryAgentHookRegistry>();
}

}  // namespace agenticdsl