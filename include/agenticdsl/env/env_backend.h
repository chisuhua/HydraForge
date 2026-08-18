// include/agenticdsl/env/env_backend.h
// 功能描述：IEnvBackend 抽象接口 + 4 个 POD 值类型 + backend 工厂声明 (ADR-0075 D1)
// 设计依据：ADR-0075 §决策 D1 (exec 原语) + §不变量 1 (工厂扩展点)
//          + ADR-0003 (shared_ptr<const IEnvBackend> 线程安全)
// 作者：from-roadmap-phase-6c-execution-envbackend change
// 最后修改日期：2026-08-18
#pragma once

#include <cstddef>
#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace agenticdsl {

struct BackendConfig;  // agenticdsl/policy/backend_policy.h

/// @brief backend 错误码表 (ADR-0075 §决策 D2 表 + D3 扩展)
enum class BackendErrorCode {
  Success,
  ForkFailed,         // ERR_BACKEND_FORK_FAILED
  CommandNotFound,    // ERR_BACKEND_COMMAND_NOT_FOUND
  Timeout,            // ERR_BACKEND_TIMEOUT
  OutputTooLarge,     // ERR_OUTPUT_TOO_LARGE
  Unavailable,        // ERR_BACKEND_UNAVAILABLE
  SecurityViolation,  // ERR_BACKEND_SECURITY_VIOLATION
  Unknown
};

/// @brief 错误码 → ADR-0075 字符串表
const char* backend_error_name(BackendErrorCode code);

/// @brief 执行请求 — cmd + args 强制分离防 shell 注入 (ADR-0075 D2)
struct ExecRequest {
  std::string cmd;                  // 可执行文件绝对路径 (execve 直接调用, 不经 PATH 解析)
  std::vector<std::string> args;    // 参数数组, 逐参 execve 传递, 不经 shell 解析
  std::string working_dir;          // 可选; 空 = 继承当前目录
};

/// @brief 执行选项
struct ExecOptions {
  uint64_t timeout_ms = 30000;               // 超时 (默认 30s)
  size_t max_output_bytes = 64 * 1024;       // 输出截断阈值 (默认 64KB, 见 docs/specs/env-backend.md
                                             // 与 ADR-0075 D1 line 96 1MB 上限的差异说明)
  bool capture_stderr = true;                // 是否捕获 stderr
  std::map<std::string, std::string> env;    // env 白名单 — 仅透传显式声明,
                                             // 不继承 parent env (ADR-0075 §不变量 4)
};

/// @brief 执行结果
struct ExecResult {
  int exit_code = -1;                        // WEXITSTATUS; 信号死亡 = 128 + signo; 无法执行 = 127
  std::string stdout_buf;
  std::string stderr_buf;
  bool timed_out = false;
  BackendErrorCode error_code = BackendErrorCode::Success;
  uint64_t duration_ms = 0;
};

/// @brief backend 能力查询 (BackendPolicy 决策输入)
struct BackendCapabilities {
  bool supports_isolation = false;       // 是否进程/容器级隔离
  bool supports_persistent_fs = false;   // 文件系统是否跨 exec 持久
  uint32_t max_concurrent_execs = 1;     // 并发 exec 排队上限
};

/// @brief IEnvBackend 抽象接口 (ADR-0075 D1)
/// 线程安全: 实现必须 const-correct, 无 mutable 状态 (ADR-0003);
/// 多线程并发 exec 经 capabilities().max_concurrent_execs 排队语义由调用方控制。
class IEnvBackend {
 public:
  virtual ~IEnvBackend() = default;
  virtual ExecResult exec(const ExecRequest& req, const ExecOptions& opts) const = 0;
  virtual BackendCapabilities capabilities() const = 0;
};

/// @brief backend 工厂 (ADR-0075 §不变量 1 扩展点, 为 K8sBackend/SSHBackend 预留)
/// @param backend_spec "local" / "docker:<container_id>" / "docker:<image>:<tag>"
/// @param config BackendConfig (策略 + docker_host + fallback policy + audit bus)
/// @return shared_ptr<const IEnvBackend> 不可变实例; 未知 spec 返回 nullptr
/// @note docker daemon 不可达时: fail_fast → 仍返回 DockerBackend (exec 时返回
///       ERR_BACKEND_UNAVAILABLE); fallback_to_local → 记录 warning + 返回 LocalBackend
std::shared_ptr<const IEnvBackend> create_backend(const std::string& backend_spec,
                                                  const BackendConfig& config);

}  // namespace agenticdsl
