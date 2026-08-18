// tests/test_backend_security.cpp
// 功能描述：OWASP shell 注入防御 + privileged mode 拒绝测试 (ADR-0075 D2/D7)
//          execve 数组逐参传递 → shell 元字符不解析
// 作者：from-roadmap-phase-6c-execution-envbackend change
// 最后修改日期：2026-08-18
#include "catch_amalgamated.hpp"

#include "agenticdsl/env/docker_backend.h"
#include "agenticdsl/env/env_backend.h"
#include "agenticdsl/env/local_backend.h"

#include <algorithm>

using namespace agenticdsl;

namespace {
std::string trim(std::string s) {
  s.erase(std::remove(s.begin(), s.end(), '\n'), s.end());
  return s;
}
}  // namespace

TEST_CASE("OWASP injection: 'ls; rm -rf /' not parsed as shell",
          "[backend_security]") {
  LocalBackend backend;
  // 若走 shell 解析, 会拆成两条命令; execve 逐参传递 → echo 原样输出
  ExecRequest req{"/bin/echo", {"ls; rm -rf /"}, ""};
  auto result = backend.exec(req, ExecOptions{});
  REQUIRE(result.error_code == BackendErrorCode::Success);
  REQUIRE(trim(result.stdout_buf) == "ls; rm -rf /");
}

TEST_CASE("OWASP injection: '$(whoami)' not expanded", "[backend_security]") {
  LocalBackend backend;
  ExecRequest req{"/bin/echo", {"$(whoami)"}, ""};
  auto result = backend.exec(req, ExecOptions{});
  REQUIRE(result.error_code == BackendErrorCode::Success);
  REQUIRE(trim(result.stdout_buf) == "$(whoami)");
}

TEST_CASE("OWASP injection: backticks not evaluated", "[backend_security]") {
  LocalBackend backend;
  ExecRequest req{"/bin/echo", {"`whoami`"}, ""};
  auto result = backend.exec(req, ExecOptions{});
  REQUIRE(result.error_code == BackendErrorCode::Success);
  REQUIRE(trim(result.stdout_buf) == "`whoami`");
}

TEST_CASE("docker privileged mode rejected as security violation",
          "[backend_security]") {
  DockerBackendConfig cfg;
  cfg.docker_host = "127.0.0.1:1";  // 不可达; privileged 检查应在任何 HTTP 之前
  cfg.image = "python:3.12";
  cfg.privileged = true;
  DockerBackend backend(cfg);
  auto result = backend.exec(ExecRequest{"/bin/ls", {}, ""}, ExecOptions{});
  REQUIRE(result.error_code == BackendErrorCode::SecurityViolation);
}
