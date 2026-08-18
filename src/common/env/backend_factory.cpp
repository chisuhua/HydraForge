// src/common/env/backend_factory.cpp
// 功能描述：create_backend 工厂 — 解析 "local" / "docker:<container_id>" /
//          "docker:<image>:<tag>" 三种 spec (ADR-0075 §不变量 1 扩展点)
// 作者：from-roadmap-phase-6c-execution-envbackend change
// 最后修改日期：2026-08-18
#include "agenticdsl/env/docker_backend.h"
#include "agenticdsl/env/env_backend.h"
#include "agenticdsl/env/local_backend.h"
#include "agenticdsl/policy/backend_policy.h"

#include <cstdio>

namespace agenticdsl {

std::shared_ptr<const IEnvBackend> create_backend(const std::string& backend_spec,
                                                  const BackendConfig& config) {
  if (backend_spec == "local") {
    return std::make_shared<LocalBackend>(config.bus);
  }

  if (backend_spec.rfind("docker:", 0) == 0) {
    const std::string rest = backend_spec.substr(7);
    if (rest.empty()) {
      return nullptr;
    }

    // daemon 可达性探测 + fallback policy (ADR-0075 §风险 中风险 第 1 项: 不静默失败)
    if (!docker_daemon_reachable(config.docker_host) &&
        config.docker_unavailable_policy ==
            DockerUnavailablePolicy::FallbackToLocal) {
      std::fprintf(stderr,
                   "[backend_factory] warning: docker daemon unreachable (%s), "
                   "fallback to LocalBackend\n",
                   config.docker_host.c_str());
      return std::make_shared<LocalBackend>(config.bus);
    }
    // fail_fast (默认): 仍构造 DockerBackend, exec 时返回 ERR_BACKEND_UNAVAILABLE

    DockerBackendConfig dcfg;
    dcfg.docker_host = config.docker_host;
    dcfg.bus = config.bus;
    if (const BackendPolicy* policy = config.find_policy(backend_spec)) {
      dcfg.max_memory_mb = policy->max_memory_mb;
      dcfg.max_cpu_cores = policy->max_cpu_cores;
    }
    // spec 解析: 含 ':' 或 '@sha256:' → image:tag (ephemeral); 单 token → container id
    if (rest.find(':') != std::string::npos) {
      dcfg.image = rest;
    } else {
      dcfg.container_id = rest;
    }
    return std::make_shared<DockerBackend>(std::move(dcfg));
  }

  // 未知 spec (K8sBackend/SSHBackend 预留扩展点, ADR-0075 §不变量 1)
  return nullptr;
}

}  // namespace agenticdsl
