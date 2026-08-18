// tests/test_local_backend.cpp
// 功能描述：LocalBackend fork+execve 实施测试 (ADR-0075 D2 / C11)
//          happy path / ENOENT / 超时 SIGTERM→SIGKILL / 输出截断 64KB /
//          env 白名单不继承 parent / RLIMIT_CPU / 并发线程安全 / capabilities
// 作者：from-roadmap-phase-6c-execution-envbackend change
// 最后修改日期：2026-08-18
#include "catch_amalgamated.hpp"

#include "agenticdsl/env/env_backend.h"
#include "agenticdsl/env/local_backend.h"
#include "agenticdsl/policy/backend_policy.h"

#include <csignal>
#include <cstdlib>
#include <thread>
#include <vector>

using namespace agenticdsl;

TEST_CASE("local backend happy path", "[local_backend]") {
  LocalBackend backend;
  ExecRequest req{"/bin/echo", {"hello-env-backend"}, ""};
  ExecOptions opts;
  auto result = backend.exec(req, opts);
  REQUIRE(result.error_code == BackendErrorCode::Success);
  REQUIRE(result.exit_code == 0);
  REQUIRE(result.stdout_buf.find("hello-env-backend") != std::string::npos);
  REQUIRE(result.timed_out == false);
}

TEST_CASE("local backend capabilities", "[local_backend]") {
  LocalBackend backend;
  auto caps = backend.capabilities();
  REQUIRE(caps.supports_isolation == true);
  REQUIRE(caps.supports_persistent_fs == true);
  REQUIRE(caps.max_concurrent_execs >= 1);
}

TEST_CASE("local backend execve ENOENT maps to CommandNotFound", "[local_backend]") {
  LocalBackend backend;
  ExecRequest req{"/nonexistent/binary", {}, ""};
  auto result = backend.exec(req, ExecOptions{});
  REQUIRE(result.error_code == BackendErrorCode::CommandNotFound);
  REQUIRE(result.exit_code != 0);
}

TEST_CASE("timeout_5s_kill_grace_period", "[local_backend]") {
  LocalBackend backend;
  ExecRequest req{"/bin/sleep", {"30"}, ""};
  ExecOptions opts;
  opts.timeout_ms = 5000;
  auto result = backend.exec(req, opts);
  REQUIRE(result.timed_out == true);
  REQUIRE(result.error_code == BackendErrorCode::Timeout);
  // SIGTERM @5s → grace 5s → SIGKILL @10s; sleep 默认响应 SIGTERM, 总耗时应 < 12s
  REQUIRE(result.duration_ms < 12000);
}

TEST_CASE("output_truncate_64kb", "[local_backend]") {
  LocalBackend backend;
  // yes 输出无限流, 截断到 max_output_bytes 后立即 SIGTERM 子进程
  ExecRequest req{"/usr/bin/yes", {"x"}, ""};
  ExecOptions opts;
  opts.max_output_bytes = 64 * 1024;
  opts.timeout_ms = 10000;
  auto result = backend.exec(req, opts);
  REQUIRE(result.error_code == BackendErrorCode::OutputTooLarge);
  REQUIRE(result.stdout_buf.size() <= 64 * 1024);
}

TEST_CASE("env whitelist does not inherit parent env", "[local_backend]") {
  ::setenv("HOME", "/host", 1);
  ::setenv("PARENT_SECRET", "should-not-leak", 1);
  LocalBackend backend;
  ExecRequest req{"/usr/bin/env", {}, ""};
  ExecOptions opts;
  opts.env = {{"FOO", "bar"}};
  auto result = backend.exec(req, opts);
  REQUIRE(result.error_code == BackendErrorCode::Success);
  REQUIRE(result.stdout_buf.find("FOO=bar") != std::string::npos);
  // parent env 不透传 (ADR-0075 §不变量 4)
  REQUIRE(result.stdout_buf.find("PARENT_SECRET") == std::string::npos);
  REQUIRE(result.stdout_buf.find("HOME=/host") == std::string::npos);
}

TEST_CASE("rlimit_cpu kills cpu-bound child (fork bomb defense)",
          "[local_backend]") {
  LocalBackend backend(nullptr, /*rlimit_as_bytes=*/1024ull * 1024 * 1024,
                       /*rlimit_cpu_sec=*/1);
  ExecRequest req{"/bin/bash", {"-c", "while true; do :; done"}, ""};
  ExecOptions opts;
  opts.timeout_ms = 15000;
  auto result = backend.exec(req, opts);
  // RLIMIT_CPU=1s → SIGXCPU 强杀, 远早于 15s timeout
  REQUIRE(result.timed_out == false);
  REQUIRE(result.exit_code == 128 + SIGXCPU);
  REQUIRE(result.duration_ms < 10000);
}

TEST_CASE("concurrent exec is thread-safe on shared const backend",
          "[local_backend]") {
  auto config = BackendConfig::with_defaults();
  std::shared_ptr<const IEnvBackend> backend = create_backend("local", config);
  REQUIRE(backend != nullptr);

  constexpr int kThreads = 4;
  constexpr int kExecsPerThread = 25;
  std::vector<std::thread> threads;
  std::vector<int> failures(kThreads, 0);
  for (int t = 0; t < kThreads; ++t) {
    threads.emplace_back([&, t]() {
      for (int i = 0; i < kExecsPerThread; ++i) {
        auto r = backend->exec(ExecRequest{"/bin/true", {}, ""}, ExecOptions{});
        if (r.error_code != BackendErrorCode::Success || r.exit_code != 0) {
          ++failures[t];
        }
      }
    });
  }
  for (auto& th : threads) th.join();
  for (int f : failures) REQUIRE(f == 0);
}
