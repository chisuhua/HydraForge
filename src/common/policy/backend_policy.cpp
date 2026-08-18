// src/common/policy/backend_policy.cpp
// 功能描述：BackendConfig 默认策略表 + override + spec 路由 (ADR-0075 D5)
// 作者：from-roadmap-phase-6c-execution-envbackend change
// 最后修改日期：2026-08-18
#include "agenticdsl/policy/backend_policy.h"

#include <cstdlib>

namespace agenticdsl {

BackendConfig BackendConfig::with_defaults() {
  BackendConfig config;

  // D-5 档 1: local — 审批 + PATH/HOME/USER/LANG env + /tmp,$HOME paths + 无网络
  config.local_policy.requires_approval = true;
  config.local_policy.allowed_env_vars = {"PATH", "HOME", "USER", "LANG"};
  config.local_policy.allowed_paths = {"/tmp"};
  if (const char* home = std::getenv("HOME")) {
    config.local_policy.allowed_paths.insert(home);
  }
  config.local_policy.allow_network = false;
  config.local_policy.max_memory_mb = 1024;
  config.local_policy.max_cpu_cores = 1;

  // D-5 档 2: docker:* ephemeral — 免审批 + 全 env + 全 paths + 网络 + 512MB/2 核
  config.docker_ephemeral_policy.requires_approval = false;
  config.docker_ephemeral_policy.allowed_env_vars = {"*"};
  config.docker_ephemeral_policy.allowed_paths = {"*"};
  config.docker_ephemeral_policy.allow_network = true;
  config.docker_ephemeral_policy.max_memory_mb = 512;
  config.docker_ephemeral_policy.max_cpu_cores = 2;

  // D-5 档 3: docker:prod named — 审批 + per-deployment env/paths (默认空集 = 全拒)
  config.docker_prod_policy.requires_approval = true;
  config.docker_prod_policy.allowed_env_vars = {};
  config.docker_prod_policy.allowed_paths = {};
  config.docker_prod_policy.allow_network = false;
  config.docker_prod_policy.max_memory_mb = 1024;
  config.docker_prod_policy.max_cpu_cores = 2;

  return config;
}

void BackendConfig::override_default_policy(const std::string& backend_spec,
                                            BackendPolicy policy) {
  overrides_[backend_spec] = std::move(policy);
}

const BackendPolicy* BackendConfig::find_policy(const std::string& backend_spec) const {
  auto it = overrides_.find(backend_spec);
  if (it != overrides_.end()) {
    return &it->second;
  }
  if (backend_spec == "local") {
    return &local_policy;
  }
  if (backend_spec == "docker:prod") {
    return &docker_prod_policy;
  }
  if (backend_spec.rfind("docker:", 0) == 0) {
    return &docker_ephemeral_policy;
  }
  return nullptr;
}

}  // namespace agenticdsl
