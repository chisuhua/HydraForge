// include/agenticdsl/policy/backend_policy.h
// 功能描述：BackendPolicy + BackendConfig — per-backend 执行策略 (ADR-0075 D5)
// 设计依据：ADR-0075 §决策 D5 (EnvValidationHook 策略表) + design.md D-5 (3 档默认策略)
// 作者：from-roadmap-phase-6c-execution-envbackend change
// 最后修改日期：2026-08-18
#pragma once

#include "agenticdsl/contract/iinteraction_bus.h"

#include <cstdint>
#include <map>
#include <memory>
#include <set>
#include <string>

namespace agenticdsl {

/// @brief per-backend 执行策略
struct BackendPolicy {
  bool requires_approval = true;                 // 是否需要审批 (hook 层检查 __approved 标记)
  std::set<std::string> allowed_env_vars;        // env 白名单; 含 "*" = 全部允许
  std::set<std::string> allowed_paths;           // working_dir 白名单 (前缀匹配); 含 "*" = 全部允许
  bool allow_network = false;
  uint32_t max_memory_mb = 1024;
  uint32_t max_cpu_cores = 1;
  std::set<std::string> image_allowlist;         // docker 镜像白名单; 空 = 不限制
};

/// @brief docker daemon 不可达时的 fallback 策略 (ADR-0075 §风险 中风险 第 1 项)
enum class DockerUnavailablePolicy {
  FailFast,        // 默认, 保守: exec 返回 ERR_BACKEND_UNAVAILABLE, 不静默降级
  FallbackToLocal  // 显式 opt-in: 记录 warning + 回退 LocalBackend
};

/// @brief backend 配置 — 默认策略表 (D-5 3 档) + per-environment override
struct BackendConfig {
  BackendPolicy local_policy;              // "local"
  BackendPolicy docker_ephemeral_policy;   // "docker:*" ephemeral
  BackendPolicy docker_prod_policy;        // "docker:prod" named
  DockerUnavailablePolicy docker_unavailable_policy = DockerUnavailablePolicy::FailFast;
  std::string docker_host = "/var/run/docker.sock";  // unix socket 或 "host:port"
  std::shared_ptr<IInteractionBus> bus;    // 审计事件总线 (可选, 传给 backend)

  /// @brief D-5 默认策略表 3 档
  static BackendConfig with_defaults();

  /// @brief per-environment override (按 backend_spec 精确匹配优先于默认档)
  void override_default_policy(const std::string& backend_spec, BackendPolicy policy);

  /// @brief spec → policy 路由: override 精确匹配 → "local" → "docker:prod" →
  ///        "docker:*" ephemeral → nullptr (未知)
  const BackendPolicy* find_policy(const std::string& backend_spec) const;

 private:
  std::map<std::string, BackendPolicy> overrides_;
};

}  // namespace agenticdsl
