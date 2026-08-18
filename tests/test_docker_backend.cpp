// tests/test_docker_backend.cpp
// 功能描述：DockerBackend 测试 (ADR-0075 D3 / C12) — 全部经 httplib mock daemon,
//          不依赖真实 Docker daemon (CI 无 /var/run/docker.sock)
// 作者：from-roadmap-phase-6c-execution-envbackend change
// 最后修改日期：2026-08-18
#include "catch_amalgamated.hpp"

#include "agenticdsl/env/docker_backend.h"
#include "agenticdsl/env/env_backend.h"
#include "agenticdsl/env/local_backend.h"
#include "agenticdsl/policy/backend_policy.h"
#include "test_helpers/http_mock_server.h"

#include <nlohmann/json.hpp>
#include <string>
#include <vector>

using namespace agenticdsl;
using agenticdsl::test::HttpMockServer;

namespace {

/// 注册完整 ephemeral 生命周期 mock handlers, 记录调用顺序与 create body
struct EphemeralMock {
  std::vector<std::string> calls;
  std::string create_body;

  void register_handlers(httplib::Server& svr) {
    svr.Get("/_ping", [](const httplib::Request&, httplib::Response& res) {
      res.set_content("OK", "text/plain");
    });
    svr.Post(R"(/containers/create.*)",
             [this](const httplib::Request& req, httplib::Response& res) {
               calls.push_back("create");
               create_body = req.body;
               res.status = 201;
               res.set_content(R"({"Id":"mockcid123"})", "application/json");
             });
    svr.Post(R"(/containers/mockcid123/start)",
             [this](const httplib::Request&, httplib::Response& res) {
               calls.push_back("start");
               res.status = 204;
             });
    svr.Post(R"(/containers/mockcid123/wait)",
             [this](const httplib::Request&, httplib::Response& res) {
               calls.push_back("wait");
               res.set_content(R"({"StatusCode":0})", "application/json");
             });
    svr.Get(R"(/containers/mockcid123/logs.*)",
            [this](const httplib::Request&, httplib::Response& res) {
              calls.push_back("logs");
              res.set_content("hello-from-container\n", "text/plain");
            });
    svr.Delete(R"(/containers/mockcid123.*)",
               [this](const httplib::Request&, httplib::Response& res) {
                 calls.push_back("delete");
                 res.status = 204;
               });
  }
};

DockerBackendConfig make_cfg(int port, std::string image = "python:3.12") {
  DockerBackendConfig cfg;
  cfg.docker_host = "127.0.0.1:" + std::to_string(port);
  cfg.image = std::move(image);
  return cfg;
}

}  // namespace

TEST_CASE("ephemeral container full lifecycle create+run+delete",
          "[docker_backend]") {
  HttpMockServer mock;
  EphemeralMock handlers;
  handlers.register_handlers(mock.server());

  DockerBackend backend(make_cfg(mock.port()));
  ExecRequest req{"/bin/echo", {"hello"}, ""};
  auto result = backend.exec(req, ExecOptions{});

  REQUIRE(result.error_code == BackendErrorCode::Success);
  REQUIRE(result.exit_code == 0);
  REQUIRE(result.stdout_buf.find("hello-from-container") != std::string::npos);
  // 生命周期顺序: create → start → wait → logs → delete (无残留)
  REQUIRE(handlers.calls == std::vector<std::string>{
                                "create", "start", "wait", "logs", "delete"});
}

TEST_CASE("exec into existing container mode", "[docker_backend]") {
  HttpMockServer mock;
  mock.server().Get("/_ping", [](const httplib::Request&, httplib::Response& res) {
    res.set_content("OK", "text/plain");
  });
  mock.server().Post(R"(/containers/prod-container/exec)",
                     [](const httplib::Request&, httplib::Response& res) {
                       res.status = 201;
                       res.set_content(R"({"Id":"execid9"})", "application/json");
                     });
  mock.server().Post(R"(/exec/execid9/start)",
                     [](const httplib::Request&, httplib::Response& res) {
                       res.set_content("exec-output\n", "text/plain");
                     });
  mock.server().Get(R"(/exec/execid9/inspect)",
                    [](const httplib::Request&, httplib::Response& res) {
                      res.set_content(R"({"ExitCode":0})", "application/json");
                    });

  DockerBackendConfig cfg;
  cfg.docker_host = "127.0.0.1:" + std::to_string(mock.port());
  cfg.container_id = "prod-container";
  DockerBackend backend(cfg);
  REQUIRE(backend.mode() == DockerBackend::Mode::ExecIntoExisting);

  auto result = backend.exec(ExecRequest{"/bin/ls", {}, ""}, ExecOptions{});
  REQUIRE(result.error_code == BackendErrorCode::Success);
  REQUIRE(result.exit_code == 0);
  REQUIRE(result.stdout_buf.find("exec-output") != std::string::npos);
}

TEST_CASE("privileged mode request rejected with SecurityViolation",
          "[docker_backend]") {
  HttpMockServer mock;  // 不应收到任何请求
  auto cfg = make_cfg(mock.port());
  cfg.privileged = true;
  DockerBackend backend(cfg);
  auto result = backend.exec(ExecRequest{"/bin/ls", {}, ""}, ExecOptions{});
  REQUIRE(result.error_code == BackendErrorCode::SecurityViolation);
}

TEST_CASE("resource limits appear in create body", "[docker_backend]") {
  HttpMockServer mock;
  EphemeralMock handlers;
  handlers.register_handlers(mock.server());

  auto cfg = make_cfg(mock.port());
  cfg.max_memory_mb = 512;
  cfg.max_cpu_cores = 2;
  DockerBackend backend(cfg);
  auto result = backend.exec(ExecRequest{"/bin/true", {}, ""}, ExecOptions{});
  REQUIRE(result.error_code == BackendErrorCode::Success);

  auto body = nlohmann::json::parse(handlers.create_body);
  REQUIRE(body["HostConfig"]["Memory"] == 512ll * 1024 * 1024);
  REQUIRE(body["HostConfig"]["NanoCpus"] == 2000000000ll);
  // D-7: Privileged 强制 false
  REQUIRE(body["HostConfig"]["Privileged"] == false);
}

TEST_CASE("docker_daemon_unavailable_fail_fast", "[docker_backend]") {
  HttpMockServer mock;
  mock.server().Get("/_ping", [](const httplib::Request&, httplib::Response& res) {
    res.status = 503;
  });
  auto config = BackendConfig::with_defaults();
  config.docker_host = "127.0.0.1:" + std::to_string(mock.port());
  config.docker_unavailable_policy = DockerUnavailablePolicy::FailFast;

  auto backend = create_backend("docker:python:3.12", config);
  REQUIRE(backend != nullptr);
  auto result = backend->exec(ExecRequest{"/bin/true", {}, ""}, ExecOptions{});
  REQUIRE(result.error_code == BackendErrorCode::Unavailable);
}

TEST_CASE("docker_unavailable_fallback_to_local", "[docker_backend]") {
  // 无 server 监听的端口 → daemon 不可达
  auto config = BackendConfig::with_defaults();
  config.docker_host = "127.0.0.1:1";
  config.docker_unavailable_policy = DockerUnavailablePolicy::FallbackToLocal;

  auto backend = create_backend("docker:python:3.12", config);
  REQUIRE(backend != nullptr);
  // 回退到 LocalBackend (记录 warning, 不静默)
  REQUIRE(dynamic_cast<const LocalBackend*>(backend.get()) != nullptr);
  auto result = backend->exec(ExecRequest{"/bin/echo", {"fallback-ok"}, ""},
                              ExecOptions{});
  REQUIRE(result.error_code == BackendErrorCode::Success);
  REQUIRE(result.stdout_buf.find("fallback-ok") != std::string::npos);
}

TEST_CASE("image digest lock passed through to create body",
          "[docker_backend]") {
  HttpMockServer mock;
  EphemeralMock handlers;
  handlers.register_handlers(mock.server());

  auto cfg = make_cfg(mock.port(), "python:3.12@sha256:abc123def");
  DockerBackend backend(cfg);
  auto result = backend.exec(ExecRequest{"/bin/true", {}, ""}, ExecOptions{});
  REQUIRE(result.error_code == BackendErrorCode::Success);
  REQUIRE(handlers.create_body.find("@sha256:abc123def") != std::string::npos);
}
