// include/agenticdsl/env/local_backend.h
// 功能描述：LocalBackend — fork + execve POSIX 子进程隔离 (ADR-0075 D2 / C11)
// 设计依据：ADR-0075 §决策 D2 (fork+execve 而非 posix_spawn, waitpid 超时语义)
//          + ADR-0055 SkillInterpreter 安全模式 (FD cleanup + env 白名单 + stdio 重定向)
// 作者：from-roadmap-phase-6c-execution-envbackend change
// 最后修改日期：2026-08-18
#pragma once

#include "agenticdsl/contract/iinteraction_bus.h"
#include "agenticdsl/env/env_backend.h"

#include <cstdint>
#include <memory>

namespace agenticdsl {

/// @brief LocalBackend — 本地 fork+execve 执行 backend
///
/// 安全约束 (ADR-0075 §决策 D2):
///  - cmd + args 数组分离, execve 逐参传递 (不使用 system()/popen() 防 OWASP shell injection)
///  - env 白名单: 仅透传 ExecOptions.env, 不继承 parent env (§不变量 4)
///  - 超时: waitpid 轮询 + SIGTERM (5s grace) → SIGKILL 升级
///  - 输出截断: max_output_bytes (默认 64KB), 截断即 SIGTERM 子进程
///  - 资源限制: setrlimit(RLIMIT_AS) 内存 + RLIMIT_CPU CPU 秒 (fork bomb 防御)
///
/// 线程安全: 无 mutable 成员, exec() const, 可多线程并发调用 (ADR-0003)。
class LocalBackend final : public IEnvBackend {
 public:
  /// @param bus 审计事件总线 (可选, nullptr = 跳过 env.backend.exec.start/end 发射)
  /// @param rlimit_as_bytes RLIMIT_AS 地址空间上限 (默认 1GB)
  /// @param rlimit_cpu_sec RLIMIT_CPU CPU 秒上限 (默认 300s, 防 fork bomb / CPU 死循环)
  explicit LocalBackend(std::shared_ptr<IInteractionBus> bus = nullptr,
                        uint64_t rlimit_as_bytes = 1024ull * 1024 * 1024,
                        uint64_t rlimit_cpu_sec = 300);

  ExecResult exec(const ExecRequest& req, const ExecOptions& opts) const override;
  BackendCapabilities capabilities() const override;

 private:
  std::shared_ptr<IInteractionBus> bus_;  // 审计总线 (InMemoryBus emit 线程安全)
  uint64_t rlimit_as_bytes_;
  uint64_t rlimit_cpu_sec_;
};

}  // namespace agenticdsl
