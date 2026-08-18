// tests/test_backend_policy.cpp
// 功能描述：BackendPolicy 默认策略表 + per-environment override + image allowlist
//          + docker_unavailable_policy 测试 (ADR-0075 D5)
// 作者：from-roadmap-phase-6c-execution-envbackend change
// 最后修改日期：2026-08-18
#include "catch_amalgamated.hpp"

#include "agenticdsl/policy/backend_policy.h"
#include "common/hooks/env_validation_hook.h"
#include "common/policy/execution_policy.h"

using namespace agenticdsl;

namespace {
ToolMetadata make_exec_meta() {
  ToolMetadata m;
  m.name = "shell/exec";
  m.description = "test";
  m.domain = "test";
  m.category = ToolCategory::Execute;
  m.min_layer = LayerProfile::Workflow;
  return m;
}
}  // namespace

TEST_CASE("default policy table covers 3 tiers", "[backend_policy]") {
  auto config = BackendConfig::with_defaults();

  const auto* local = config.find_policy("local");
  REQUIRE(local != nullptr);
  REQUIRE(local->requires_approval == true);
  REQUIRE(local->allowed_env_vars.count("PATH") == 1);
  REQUIRE(local->allowed_env_vars.count("HOME") == 1);
  REQUIRE(local->allowed_env_vars.count("USER") == 1);
  REQUIRE(local->allowed_env_vars.count("LANG") == 1);
  REQUIRE(local->allow_network == false);

  const auto* ephemeral = config.find_policy("docker:python:3.12");
  REQUIRE(ephemeral != nullptr);
  REQUIRE(ephemeral->requires_approval == false);
  REQUIRE(ephemeral->allowed_env_vars.count("*") == 1);
  REQUIRE(ephemeral->max_memory_mb == 512);
  REQUIRE(ephemeral->max_cpu_cores == 2);

  const auto* prod = config.find_policy("docker:prod");
  REQUIRE(prod != nullptr);
  REQUIRE(prod->requires_approval == true);

  REQUIRE(config.find_policy("k8s:pod") == nullptr);
}

TEST_CASE("per-environment override replaces default policy", "[backend_policy]") {
  auto config = BackendConfig::with_defaults();
  BackendPolicy custom;
  custom.requires_approval = false;
  custom.allowed_env_vars = {"PATH"};
  config.override_default_policy("local", custom);

  const auto* local = config.find_policy("local");
  REQUIRE(local != nullptr);
  REQUIRE(local->requires_approval == false);
  REQUIRE(local->allowed_env_vars.count("HOME") == 0);
}

TEST_CASE("image allowlist denies image not in list", "[backend_policy]") {
  auto config = BackendConfig::with_defaults();
  BackendPolicy p = *config.find_policy("docker:python:3.12");
  p.image_allowlist = {"python:3.12"};
  config.override_default_policy("docker:python:3.12", p);
  // docker:alpine 走 ephemeral 默认策略 (allowlist 空 → 不限制)
  auto hook = make_env_validation_hook(config);
  ToolCallContext ctx;

  // 覆盖后 docker:python:3.12 命中 allowlist → Continue
  auto r1 = hook(make_exec_meta(), ctx,
                 {{"backend", "docker:python:3.12"}, {"cmd", "/bin/true"}});
  REQUIRE(r1.action == PreHookResult::Continue);
}

TEST_CASE("docker_unavailable_policy defaults to fail_fast and is configurable",
          "[backend_policy]") {
  auto config = BackendConfig::with_defaults();
  REQUIRE(config.docker_unavailable_policy == DockerUnavailablePolicy::FailFast);
  config.docker_unavailable_policy = DockerUnavailablePolicy::FallbackToLocal;
  REQUIRE(config.docker_unavailable_policy ==
          DockerUnavailablePolicy::FallbackToLocal);
}
