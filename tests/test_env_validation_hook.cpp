// tests/test_env_validation_hook.cpp
// 功能描述：EnvValidationHook + ToolCoordinator 集成测试 (ADR-0075 D5 / C13)
//          hook 顺序 per ADR-0069 §决策 D3 (pre-hook → ApprovalHandler → call_tool)
// 作者：from-roadmap-phase-6c-execution-envbackend change
// 最后修改日期：2026-08-18
#include "catch_amalgamated.hpp"

#include "agenticdsl/contract/itool_hook_registry.h"
#include "agenticdsl/env/env_backend.h"
#include "agenticdsl/policy/backend_policy.h"
#include "common/hooks/env_validation_hook.h"
#include "common/policy/agent_mode_policy.h"
#include "common/policy/approval_callbacks.h"
#include "common/policy/execution_policy.h"
#include "common/tools/registry.h"
#include "common/tools/tool_coordinator.h"
#include "common/tools/tool_hook_registry.h"

using namespace agenticdsl;

namespace {

ToolMetadata make_exec_meta() {
  ToolMetadata m;
  m.name = "shell/exec";
  m.description = "shell exec tool";
  m.domain = "shell";
  m.category = ToolCategory::Execute;
  m.min_layer = LayerProfile::Workflow;
  // Dangerous category requires plan OR agent approval (registry.cpp V2 validation).
  // 测试用 auto-approve callback, 但 ToolMetadata 自身必须声明至少 plan/agent 之一,
  // 否则 register_tool_function 抛 invalid_argument. test_auto_callback(true)
  // 顶层 gate 仍 skip 审批, 见 ApprovalHandler::process_request.
  m.approval.requires_approval_in_plan = true;
  m.approval.requires_approval_in_agent = false;
  m.approval.requires_approval_in_yolo = false;
  return m;
}

ToolCallContext make_ctx() {
  ToolCallContext ctx;
  ctx.session_id = "env_hook_test";
  ctx.caller_layer = "workflow";
  return ctx;
}

}  // namespace

TEST_CASE("hook continues when backend policy satisfied", "[env_validation_hook]") {
  auto hook = make_env_validation_hook(BackendConfig::with_defaults());
  auto r = hook(make_exec_meta(), make_ctx(),
                {{"backend", "local"}, {"cmd", "/bin/ls"}, {"__approved", "true"}});
  REQUIRE(r.action == PreHookResult::Continue);
}

TEST_CASE("hook denies env var not in whitelist", "[env_validation_hook]") {
  auto hook = make_env_validation_hook(BackendConfig::with_defaults());
  auto r = hook(make_exec_meta(), make_ctx(),
                {{"backend", "local"},
                 {"cmd", "/bin/ls"},
                 {"__approved", "true"},
                 {"env.SECRET_KEY", "x"}});
  REQUIRE(r.action == PreHookResult::Deny);
  REQUIRE(r.deny_reason.find("env var not allowed: SECRET_KEY") !=
          std::string::npos);
}

TEST_CASE("hook denies working_dir not in allowed paths", "[env_validation_hook]") {
  auto hook = make_env_validation_hook(BackendConfig::with_defaults());
  auto r = hook(make_exec_meta(), make_ctx(),
                {{"backend", "local"},
                 {"cmd", "/bin/ls"},
                 {"__approved", "true"},
                 {"working_dir", "/etc"}});
  REQUIRE(r.action == PreHookResult::Deny);
  REQUIRE(r.deny_reason.find("working_dir not allowed: /etc") !=
          std::string::npos);
}

TEST_CASE("hook denies when backend requires approval and not approved",
          "[env_validation_hook]") {
  auto hook = make_env_validation_hook(BackendConfig::with_defaults());
  auto r = hook(make_exec_meta(), make_ctx(),
                {{"backend", "local"}, {"cmd", "/bin/ls"}});
  REQUIRE(r.action == PreHookResult::Deny);
  REQUIRE(r.deny_reason.find("Backend policy requires approval") !=
          std::string::npos);
}

TEST_CASE("ephemeral docker backend does not require approval",
          "[env_validation_hook]") {
  auto hook = make_env_validation_hook(BackendConfig::with_defaults());
  auto r = hook(make_exec_meta(), make_ctx(),
                {{"backend", "docker:python:3.12"},
                 {"cmd", "pytest"},
                 {"env.ANYTHING", "ok"}});
  REQUIRE(r.action == PreHookResult::Continue);
}

TEST_CASE("docker:prod named backend requires approval", "[env_validation_hook]") {
  auto hook = make_env_validation_hook(BackendConfig::with_defaults());
  auto r = hook(make_exec_meta(), make_ctx(),
                {{"backend", "docker:prod"}, {"cmd", "/bin/ls"}});
  REQUIRE(r.action == PreHookResult::Deny);
  REQUIRE(r.deny_reason.find("Backend policy requires approval") !=
          std::string::npos);
}

TEST_CASE("hook skips non-Execute category tools", "[env_validation_hook]") {
  auto hook = make_env_validation_hook(BackendConfig::with_defaults());
  ToolMetadata m = make_exec_meta();
  m.category = ToolCategory::ReadOnly;
  auto r = hook(m, make_ctx(), {{"backend", "local"}, {"cmd", "/bin/ls"}});
  REQUIRE(r.action == PreHookResult::Continue);
}

TEST_CASE("tool_coordinator_dispatch_full_flow", "[env_validation_hook]") {
  // 端到端: ToolCoordinator.execute → EnvValidationHook (pre-hook) →
  //         ApprovalHandler (auto-approve) → registry.call_tool → LocalBackend
  ToolRegistry registry;
  ToolHookRegistry hooks;
  hooks.register_pre_hook("*", make_env_validation_hook(BackendConfig::with_defaults()),
                          0, HookErrorPolicy::FailClosed);

  auto backend = create_backend("local", BackendConfig::with_defaults());
  registry.register_tool_function(
      "shell/exec", make_exec_meta(),
      [backend](const std::unordered_map<std::string, std::string>& args)
          -> nlohmann::json {
        ExecRequest req;
        req.cmd = args.at("cmd");
        if (args.count("args")) req.args = {args.at("args")};
        auto r = backend->exec(req, ExecOptions{});
        return {{"exit_code", r.exit_code}, {"stdout", r.stdout_buf}};
      });

  auto policy = std::make_shared<AgentModePolicy>();
  ToolCoordinator coordinator(registry, policy, make_test_auto_callback(true));
  coordinator.set_hook_registry(&hooks);

  auto result = coordinator.execute(
      make_exec_meta(), make_ctx(),
      {{"backend", "local"},
       {"cmd", "/bin/echo"},
       {"args", "hello-env-hook"},
       {"__approved", "true"}});
  REQUIRE(result.ok);
  REQUIRE(result.data["stdout"].get<std::string>().find("hello-env-hook") !=
          std::string::npos);
}
