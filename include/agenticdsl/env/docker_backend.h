// include/agenticdsl/env/docker_backend.h
// 功能描述：DockerBackend — cpp-httplib + Docker REST API (ADR-0075 D3 / C12)
// 设计依据：ADR-0075 §决策 D3 (docker exec REST API, 禁止 docker CLI)
// 适配说明：proposal 指定 libcurl, 但 external/ 未 vendor libcurl;
//          改用已 vendor 的 cpp-httplib (client 侧 set_address_family(AF_UNIX)
//          支持 /var/run/docker.sock unix socket), 零新增外部依赖。
// 作者：from-roadmap-phase-6c-execution-envbackend change
// 最后修改日期：2026-08-18
#pragma once

#include "agenticdsl/contract/iinteraction_bus.h"
#include "agenticdsl/env/env_backend.h"

#include <cstdint>
#include <memory>
#include <string>

namespace agenticdsl {

/// @brief DockerBackend 配置
struct DockerBackendConfig {
  std::string docker_host = "/var/run/docker.sock";  // unix socket 路径 或 "host:port" (TCP, 测试用)
  std::string container_id;   // mode (a): exec into existing container (dev/test)
  std::string image;          // mode (b): ephemeral container 镜像 (可含 @sha256: digest 锁定)
  uint32_t max_memory_mb = 512;      // HostConfig.Memory (per D-5 ephemeral 默认策略)
  uint32_t max_cpu_cores = 2;        // HostConfig.NanoCpus = cores * 1e9
  bool privileged = false;           // D-7: true → 所有 exec 返回 SecurityViolation
  std::shared_ptr<IInteractionBus> bus;  // 审计事件总线 (可选)
};

/// @brief DockerBackend — Docker REST API 执行 backend
///
/// 双模式:
///  (a) exec into existing container: POST /containers/{id}/exec → /exec/{id}/start → inspect
///  (b) ephemeral container: create → start → wait → logs → delete (无残留)
///
/// 安全约束 (ADR-0075 §决策 D3):
///  - 禁止 Privileged mode (D-7)
///  - 禁止 host 根目录挂载 (不构造 Binds)
///  - tmpfs /tmp mount + network bridge 默认
///  - 镜像 digest 锁定透传 (image@sha256:...)
///
/// 线程安全: 配置在构造后不可变, exec() const (HTTP client 每次调用新建), ADR-0003 合规。
class DockerBackend final : public IEnvBackend {
 public:
  enum class Mode { ExecIntoExisting, Ephemeral };

  explicit DockerBackend(DockerBackendConfig cfg);

  ExecResult exec(const ExecRequest& req, const ExecOptions& opts) const override;
  BackendCapabilities capabilities() const override;

  Mode mode() const { return cfg_.container_id.empty() ? Mode::Ephemeral
                                                       : Mode::ExecIntoExisting; }

 private:
  DockerBackendConfig cfg_;
};

/// @brief 探测 Docker daemon 可达性 (GET /_ping)
/// @param docker_host unix socket 路径 或 "host:port"
bool docker_daemon_reachable(const std::string& docker_host);

}  // namespace agenticdsl
