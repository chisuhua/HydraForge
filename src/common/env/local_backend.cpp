// src/common/env/local_backend.cpp
// 功能描述：LocalBackend 实施 — fork + execve + pipe 双向 stdio + waitpid 超时
//          + SIGTERM→SIGKILL grace + 输出截断 + setrlimit + env 白名单 (ADR-0075 D2)
// 安全参考：ADR-0055 SkillInterpreter (FD cleanup + env whitelist + stdio 重定向)
// 作者：from-roadmap-phase-6c-execution-envbackend change
// 最后修改日期：2026-08-18
#include "agenticdsl/env/local_backend.h"

#include "agenticdsl/contract/event_builder.h"
#include "common/env/sha256_util.h"

#include <cerrno>
#include <chrono>
#include <csignal>
#include <cstring>
#include <fcntl.h>
#include <string>
#include <sys/resource.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <thread>
#include <unistd.h>
#include <vector>

namespace agenticdsl {

const char* backend_error_name(BackendErrorCode code) {
  switch (code) {
    case BackendErrorCode::Success: return "SUCCESS";
    case BackendErrorCode::ForkFailed: return "ERR_BACKEND_FORK_FAILED";
    case BackendErrorCode::CommandNotFound: return "ERR_BACKEND_COMMAND_NOT_FOUND";
    case BackendErrorCode::Timeout: return "ERR_BACKEND_TIMEOUT";
    case BackendErrorCode::OutputTooLarge: return "ERR_OUTPUT_TOO_LARGE";
    case BackendErrorCode::Unavailable: return "ERR_BACKEND_UNAVAILABLE";
    case BackendErrorCode::SecurityViolation: return "ERR_BACKEND_SECURITY_VIOLATION";
    case BackendErrorCode::Unknown: return "ERR_BACKEND_UNKNOWN";
  }
  return "ERR_BACKEND_UNKNOWN";
}

namespace {

/// RAII pipe fd 对, 析构自动 close (防 FD 泄漏, ADR-0055 模式)
struct PipeFds {
  int fds[2] = {-1, -1};
  PipeFds() = default;
  ~PipeFds() {
    close_end(0);
    close_end(1);
  }
  void close_end(int i) {
    if (fds[i] >= 0) {
      ::close(fds[i]);
      fds[i] = -1;
    }
  }
  PipeFds(const PipeFds&) = delete;
  PipeFds& operator=(const PipeFds&) = delete;
};

void set_nonblocking(int fd) {
  const int flags = ::fcntl(fd, F_GETFL, 0);
  ::fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

}  // namespace

LocalBackend::LocalBackend(std::shared_ptr<IInteractionBus> bus,
                           uint64_t rlimit_as_bytes, uint64_t rlimit_cpu_sec)
    : bus_(std::move(bus)),
      rlimit_as_bytes_(rlimit_as_bytes),
      rlimit_cpu_sec_(rlimit_cpu_sec) {}

BackendCapabilities LocalBackend::capabilities() const {
  return BackendCapabilities{/*supports_isolation=*/true,
                             /*supports_persistent_fs=*/true,
                             /*max_concurrent_execs=*/16};
}

ExecResult LocalBackend::exec(const ExecRequest& req,
                              const ExecOptions& opts) const {
  using Clock = std::chrono::steady_clock;
  const auto t0 = Clock::now();
  ExecResult result;

  // audit: env.backend.exec.start (cmd_hash 脱敏, 不记录 raw args, ADR-0068 §5.11)
  const std::string cmd_hash = sha256_hex(req.cmd);
  if (bus_) {
    bus_->emit(EventBuilder("env.backend.exec.start")
                   .args({{"backend_spec", "local"}, {"cmd_hash", cmd_hash}})
                   .build());
  }

  // argv/envp 在 fork 前构造 (fork 后仅用 async-signal-safe 调用)
  std::vector<std::string> argv_storage;
  argv_storage.push_back(req.cmd);
  argv_storage.insert(argv_storage.end(), req.args.begin(), req.args.end());
  std::vector<char*> argv;
  argv.reserve(argv_storage.size() + 1);
  for (auto& s : argv_storage) argv.push_back(s.data());
  argv.push_back(nullptr);

  std::vector<std::string> env_storage;
  env_storage.reserve(opts.env.size());
  for (const auto& [k, v] : opts.env) env_storage.push_back(k + "=" + v);
  std::vector<char*> envp;
  envp.reserve(env_storage.size() + 1);
  for (auto& s : env_storage) envp.push_back(s.data());
  envp.push_back(nullptr);

  PipeFds out_pipe, err_pipe, exec_err_pipe;
  bool pipe_ok = ::pipe(out_pipe.fds) == 0 && ::pipe(err_pipe.fds) == 0 &&
                 ::pipe(exec_err_pipe.fds) == 0;
  if (!pipe_ok) {
    result.error_code = BackendErrorCode::ForkFailed;
    result.stderr_buf = std::string("pipe() failed: ") + std::strerror(errno);
    return result;
  }
  // exec_err_pipe 写端 CLOEXEC: execve 成功时自动关闭 → parent read 返回 0 = 成功
  ::fcntl(exec_err_pipe.fds[1], F_SETFD, FD_CLOEXEC);

  const pid_t pid = ::fork();
  if (pid < 0) {
    result.error_code = BackendErrorCode::ForkFailed;
    result.stderr_buf = std::string("fork() failed: ") + std::strerror(errno);
    return result;
  }

  if (pid == 0) {
    // ===== 子进程 (仅 async-signal-safe 调用) =====
    ::dup2(out_pipe.fds[1], STDOUT_FILENO);
    ::dup2(err_pipe.fds[1], STDERR_FILENO);
    ::close(out_pipe.fds[0]);
    ::close(out_pipe.fds[1]);
    ::close(err_pipe.fds[0]);
    ::close(err_pipe.fds[1]);
    ::close(exec_err_pipe.fds[0]);

    if (!req.working_dir.empty() && ::chdir(req.working_dir.c_str()) != 0) {
      const int e = errno;
      (void)!::write(exec_err_pipe.fds[1], &e, sizeof(e));
      _exit(127);
    }

    struct rlimit as_lim {rlimit_as_bytes_, rlimit_as_bytes_};
    ::setrlimit(RLIMIT_AS, &as_lim);
    // RLIMIT_CPU: soft=cur (SIGXCPU), hard=cur+5s grace → SIGKILL (per RLIMIT_CPU man page).
    // cur=max (无 grace) 会让内核直接 SIGKILL, 跳 SIGXCPU 默认终止; 加 5s grace
    // 保证 SIGXCPU 先送达, 默认 action Terminate 生效。
    struct rlimit cpu_lim{rlimit_cpu_sec_, rlimit_cpu_sec_ + 5};
    ::setrlimit(RLIMIT_CPU, &cpu_lim);

    ::execve(req.cmd.c_str(), argv.data(), envp.data());
    const int e = errno;  // execve 失败才到达
    (void)!::write(exec_err_pipe.fds[1], &e, sizeof(e));
    _exit(127);
  }

  // ===== 父进程 =====
  out_pipe.close_end(1);
  err_pipe.close_end(1);
  exec_err_pipe.close_end(1);
  set_nonblocking(out_pipe.fds[0]);
  set_nonblocking(err_pipe.fds[0]);

  bool truncated = false;
  const auto append_capped = [&](std::string& dst, const char* data, size_t n) {
    if (dst.size() >= opts.max_output_bytes) {
      truncated = true;
      return;
    }
    const size_t room = opts.max_output_bytes - dst.size();
    dst.append(data, std::min(room, n));
    if (n > room) truncated = true;
  };
  const auto drain = [&]() {
    char buf[8192];
    ssize_t n;
    while ((n = ::read(out_pipe.fds[0], buf, sizeof(buf))) > 0) {
      append_capped(result.stdout_buf, buf, static_cast<size_t>(n));
    }
    if (opts.capture_stderr) {
      while ((n = ::read(err_pipe.fds[0], buf, sizeof(buf))) > 0) {
        append_capped(result.stderr_buf, buf, static_cast<size_t>(n));
      }
    }
  };
  // waitpid 循环, 超时升级 SIGTERM (grace) → SIGKILL
  const auto kill_grace = [&](int grace_ms) {
    ::kill(pid, SIGTERM);
    const auto grace_deadline = Clock::now() + std::chrono::milliseconds(grace_ms);
    int st = 0;
    while (Clock::now() < grace_deadline) {
      if (::waitpid(pid, &st, WNOHANG) == pid) return st;
      std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
    ::kill(pid, SIGKILL);  // 强杀兜底 (proposal grep 验收点)
    ::waitpid(pid, &st, 0);
    return st;
  };

  int status = 0;
  bool exited = false;
  const auto deadline = t0 + std::chrono::milliseconds(opts.timeout_ms);
  while (true) {
    drain();
    if (truncated) {
      // 输出超限: 立即 SIGTERM (1s grace) → SIGKILL, 非超时语义
      status = kill_grace(1000);
      exited = true;
      break;
    }
    if (::waitpid(pid, &status, WNOHANG) == pid) {
      exited = true;
      break;
    }
    if (Clock::now() >= deadline) {
      status = kill_grace(5000);
      exited = true;
      result.timed_out = true;
      break;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }
  drain();  // 收割残留输出

  int child_errno = 0;
  const ssize_t rn =
      ::read(exec_err_pipe.fds[0], &child_errno, sizeof(child_errno));

  result.duration_ms = static_cast<uint64_t>(
      std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now() - t0)
          .count());

  if (rn == static_cast<ssize_t>(sizeof(child_errno))) {
    // execve 失败 (CLOEXEC 管道收到 errno)
    result.exit_code = 127;
    result.error_code = (child_errno == ENOENT)
                            ? BackendErrorCode::CommandNotFound
                            : BackendErrorCode::Unknown;
    result.stderr_buf += std::string("execve failed: ") +
                         std::strerror(child_errno);
  } else if (truncated) {
    result.error_code = BackendErrorCode::OutputTooLarge;
    result.exit_code = -1;
  } else if (result.timed_out) {
    result.error_code = BackendErrorCode::Timeout;
    result.exit_code = -1;
  } else if (exited && WIFEXITED(status)) {
    result.exit_code = WEXITSTATUS(status);
    result.error_code = BackendErrorCode::Success;
  } else if (exited && WIFSIGNALED(status)) {
    result.exit_code = 128 + WTERMSIG(status);
    result.error_code = BackendErrorCode::Success;  // 信号死亡由 exit_code 表达 (e.g. RLIMIT_CPU → SIGXCPU)
  }

  // audit: env.backend.exec.end
  if (bus_) {
    bus_->emit(EventBuilder("env.backend.exec.end")
                   .args({{"backend_spec", "local"},
                          {"cmd_hash", cmd_hash},
                          {"exit_code", result.exit_code},
                          {"timed_out", result.timed_out},
                          {"error_code", backend_error_name(result.error_code)}})
                   .latency_ms(result.duration_ms)
                   .build());
  }
  return result;
}

}  // namespace agenticdsl
