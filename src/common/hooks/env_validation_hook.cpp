// src/common/hooks/env_validation_hook.cpp
// 功能描述：EnvValidationHook 实施 — 4 步 policy 校验 + 三层 deny 路径 (ADR-0075 D5)
// 作者：from-roadmap-phase-6c-execution-envbackend change
// 最后修改日期：2026-08-18
#include "common/hooks/env_validation_hook.h"

#include "common/policy/execution_policy.h"  // ToolCategory

#include <string>
#include <utility>

namespace agenticdsl {

namespace {

PreHookResult deny(std::string reason) {
  PreHookResult r;
  r.action = PreHookResult::Deny;
  r.deny_reason = std::move(reason);
  return r;
}

}  // namespace

PreHook make_env_validation_hook(BackendConfig config) {
  return [config = std::move(config)](
             const ToolMetadata& meta, const ToolCallContext&,
             const std::unordered_map<std::string, std::string>& args)
             -> PreHookResult {
    // 仅 Execute 类目 (shell.exec 类) 强制 backend policy; 其他类目放行
    if (meta.category != ToolCategory::Execute) {
      return {};
    }

    const std::string backend =
        args.count("backend") ? args.at("backend") : std::string("local");
    const BackendPolicy* policy = config.find_policy(backend);
    if (policy == nullptr) {
      return deny("unknown backend: " + backend);
    }

    // docker 镜像 allowlist (policy 非空时强制, proposal SHOULD 项)
    if (backend.rfind("docker:", 0) == 0 && !policy->image_allowlist.empty()) {
      const std::string image = backend.substr(7);
      if (policy->image_allowlist.count(image) == 0) {
        return deny("image not in allowlist: " + image);
      }
    }

    // env 白名单: "env.<NAME>" 键
    const bool env_allow_all = policy->allowed_env_vars.count("*") == 1;
    for (const auto& [key, value] : args) {
      (void)value;
      if (key.rfind("env.", 0) == 0) {
        const std::string name = key.substr(4);
        if (!env_allow_all && policy->allowed_env_vars.count(name) == 0) {
          return deny("env var not allowed: " + name);
        }
      }
    }

    // working_dir 白名单 (前缀匹配)
    if (args.count("working_dir") && policy->allowed_paths.count("*") == 0) {
      const std::string& wd = args.at("working_dir");
      bool allowed = false;
      for (const auto& p : policy->allowed_paths) {
        if (wd == p || wd.rfind(p + "/", 0) == 0) {
          allowed = true;
          break;
        }
      }
      if (!allowed) {
        return deny("working_dir not allowed: " + wd);
      }
    }

    // approval gate: requires_approval 的 backend 需调用方携带 __approved=true 标记
    if (policy->requires_approval) {
      const auto it = args.find("__approved");
      if (it == args.end() || it->second != "true") {
        return deny("Backend policy requires approval");
      }
    }

    return {};
  };
}

}  // namespace agenticdsl
