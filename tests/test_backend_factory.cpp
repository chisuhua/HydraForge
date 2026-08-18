// tests/test_backend_factory.cpp
// 功能描述：backend 工厂 create_backend 3 种 spec 解析测试 (ADR-0075 D1)
// 作者：from-roadmap-phase-6c-execution-envbackend change
// 最后修改日期：2026-08-18
#include "catch_amalgamated.hpp"

#include "agenticdsl/env/docker_backend.h"
#include "agenticdsl/env/env_backend.h"
#include "agenticdsl/env/local_backend.h"
#include "agenticdsl/policy/backend_policy.h"

using namespace agenticdsl;

TEST_CASE("factory creates LocalBackend for local spec", "[backend_factory]") {
  auto config = BackendConfig::with_defaults();
  auto backend = create_backend("local", config);
  REQUIRE(backend != nullptr);
  auto caps = backend->capabilities();
  REQUIRE(caps.supports_persistent_fs == true);
}

TEST_CASE("factory parses docker container-id spec", "[backend_factory]") {
  auto config = BackendConfig::with_defaults();
  config.docker_unavailable_policy = DockerUnavailablePolicy::FailFast;
  auto backend = create_backend("docker:abc123def456", config);
  REQUIRE(backend != nullptr);
  const auto* docker = dynamic_cast<const DockerBackend*>(backend.get());
  REQUIRE(docker != nullptr);
  REQUIRE(docker->mode() == DockerBackend::Mode::ExecIntoExisting);
}

TEST_CASE("factory parses docker image:tag spec", "[backend_factory]") {
  auto config = BackendConfig::with_defaults();
  config.docker_unavailable_policy = DockerUnavailablePolicy::FailFast;
  auto backend = create_backend("docker:python:3.12", config);
  REQUIRE(backend != nullptr);
  const auto* docker = dynamic_cast<const DockerBackend*>(backend.get());
  REQUIRE(docker != nullptr);
  REQUIRE(docker->mode() == DockerBackend::Mode::Ephemeral);
}

TEST_CASE("factory rejects unknown spec", "[backend_factory]") {
  auto config = BackendConfig::with_defaults();
  REQUIRE(create_backend("k8s:pod-x", config) == nullptr);
  REQUIRE(create_backend("docker:", config) == nullptr);
}
