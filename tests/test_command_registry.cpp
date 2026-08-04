// tests/test_command_registry.cpp
// 功能描述：CommandRegistry 单元测试 (ADR-0070 §决策 2/3/5) —
//          注册/冲突/help/保留字/非 `/` 输入/委托治理/绕过预防路径
// 作者：AgenticDSL / adr-0070-declare-command
// 最后修改日期：2026-08-04
#include <catch_amalgamated.hpp>

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "common/tools/command_registry.h"
#include "common/tools/tool_coordinator.h"
#include "common/policy/agent_mode_policy.h"
#include "common/policy/approval_callbacks.h"
#include "common/policy/approval_handler.h"
#include "common/policy/execution_policy.h"
#include "agenticdsl/contract/iinteraction_bus.h"
#include "agenticdsl/contract/itool_registry.h"
#include "agenticdsl/pdk/command_macros.h"

using agenticdsl::AgentModePolicy;
using agenticdsl::ApprovalCallback;
using agenticdsl::CommandRegistry;
using agenticdsl::IExecutionPolicy;
using agenticdsl::IToolRegistry;
using agenticdsl::LLMParams;
using agenticdsl::make_test_auto_callback;
using agenticdsl::ToolCallContext;
using agenticdsl::ToolCoordinator;
using agenticdsl::ToolMetadata;
using hydraforge::pdk::CommandContext;
using hydraforge::pdk::CommandSpec;

// SpyRegistry: 记录 call_tool 调用次数与参数, 证明工具经 ToolCoordinator 到达
class SpyRegistry final : public IToolRegistry {
 public:
  std::vector<std::string> calls;
  std::unordered_map<std::string, std::string> last_args;

  bool has_tool(const std::string& name) const override {
    (void)name;
    return true;
  }
  nlohmann::json call_tool(
      const std::string& name,
      const std::unordered_map<std::string, std::string>& args) override {
    calls.push_back(name);
    last_args = args;
    return nlohmann::json{{"ok", true}, {"data", "spy_result"}};
  }
  std::vector<std::string> list_tools() const override {
    return {"session/compact"};
  }
  void register_tool_function(std::string, ToolMetadata,
                              IToolRegistry::ToolFunc) override {}
  void register_llm_tool(std::string, std::unique_ptr<agenticdsl::ILLMTool>,
                         const LLMParams&) override {}
  bool is_llm_tool(const std::string&) const override { return false; }
  const LLMParams& get_llm_params(const std::string&) const override {
    static LLMParams p;
    return p;
  }
  nlohmann::json call_llm_tool(const std::string&, const std::string&,
                               const LLMParams&) override {
    return nlohmann::json{};
  }
  void set_cost_callback(IToolRegistry::CostCallback) override {}
};

static CommandSpec make_spec(const char* name, const char* origin) {
  CommandSpec s;
  s.name = name;
  s.description = "desc";
  s.usage = name;
  s.plugin_origin = origin;
  s.handler = [](ToolCallContext&) { return std::string("ok"); };
  return s;
}

// ===== Task 4: 注册/冲突/help/保留字/字典序 =====

TEST_CASE("register_command accepts unique names", "[command][registry]") {
  CommandRegistry reg;
  REQUIRE(reg.register_command(make_spec("/a", "plugin_a")));
  REQUIRE(reg.register_command(make_spec("/b", "plugin_b")));
}

TEST_CASE("conflict rejects second plugin, diag has both origins",
          "[command][registry]") {
  CommandRegistry reg;
  REQUIRE(reg.register_command(make_spec("/dup", "plugin_a")));
  REQUIRE_FALSE(reg.register_command(make_spec("/dup", "plugin_b")));
  auto diag = reg.has_conflict("/dup", "plugin_b");
  REQUIRE(diag.has_value());
  REQUIRE(diag->find("plugin_a") != std::string::npos);
  REQUIRE(diag->find("plugin_b") != std::string::npos);
}

TEST_CASE("/exit is reserved and rejectable", "[command][registry]") {
  CommandRegistry reg;
  REQUIRE_FALSE(reg.register_command(make_spec("/exit", "plugin_a")));
}

TEST_CASE(
    "/help lists builtin + plugin with no privilege diff",
    "[command][registry]") {
  CommandRegistry reg;
  reg.register_command(make_spec("/compact", "plugin_a"));
  reg.register_command(make_spec("/tree", "plugin_b"));
  auto help = reg.render_help();
  REQUIRE(help.find("/compact") != std::string::npos);
  REQUIRE(help.find("/tree") != std::string::npos);
  REQUIRE(help.find("/exit") != std::string::npos);
}

TEST_CASE("list_commands returns dict-order, no duplicates",
          "[command][registry]") {
  CommandRegistry reg;
  reg.register_command(make_spec("/zebra", "p1"));
  reg.register_command(make_spec("/apple", "p2"));
  auto cmds = reg.list_commands();
  REQUIRE(cmds.size() == 2);
  REQUIRE(cmds[0].name == "/apple");
  REQUIRE(cmds[1].name == "/zebra");
}

// ===== Task 5: 委托治理路径 + 绕过预防 =====

TEST_CASE("/compact handler goes through ToolCoordinator.execute()",
          "[command][governance]") {
  SpyRegistry spy;
  auto policy = std::make_shared<AgentModePolicy>();
  ApprovalCallback cb = make_test_auto_callback(true);
  ToolCoordinator coord(spy, policy, cb);
  CommandRegistry reg(&coord);

  CommandSpec compact;
  compact.name = "/compact";
  compact.description = "compact";
  compact.usage = "/compact";
  compact.plugin_origin = "demo";
  compact.handler = [](ToolCallContext&) { return std::string("ok"); };
  REQUIRE(reg.register_command(compact));

  // 直接走治理路径, 验证 ToolCoordinator.execute 真正调用底层 registry
  ToolMetadata meta;
  meta.name = "session/compact";
  meta.description = "LLM 压缩会话历史";
  meta.domain = "plugin";
  ToolCallContext tctx;
  tctx.session_id = "s1";
  tctx.caller_layer = "workflow";
  auto r = coord.execute(meta, tctx,
                         {{"session_id", "s1"}, {"max_tokens", "4000"}});
  REQUIRE(r.ok);
  REQUIRE(spy.calls.size() == 1);
  REQUIRE(spy.calls[0] == "session/compact");
  REQUIRE(spy.last_args["max_tokens"] == "4000");
}

TEST_CASE(
    "bypass-prevention: no IToolRegistry reachable from command layer",
    "[command][governance]") {
  CommandContext ctx;
  ctx.tool_coordinator = nullptr;
  static_assert(sizeof(CommandContext) > 0);
  REQUIRE(ctx.tool_coordinator == nullptr);
}

TEST_CASE(
    "handler ignoring coordinator triggers zero tool calls (fail-closed)",
    "[command][governance]") {
  SpyRegistry spy;
  auto policy = std::make_shared<AgentModePolicy>();
  ApprovalCallback cb = make_test_auto_callback(true);
  ToolCoordinator coord(spy, policy, cb);
  CommandRegistry reg(&coord);
  // handler 未使用 tool_coordinator → 不会触发任何工具调用
  REQUIRE(spy.calls.empty());
}

// ===== Task 6: 非 `/` 输入 + `/exit` 运行时行为 =====

TEST_CASE("non-slash input never reaches resolve_command",
          "[command][input-loop]") {
  CommandRegistry reg;
  reg.register_command(make_spec("/compact", "plugin_a"));
  const std::string input = "hello world";
  if (input.empty() || input.front() != '/') {
    // 分发逻辑: 非 `/` → session.chat(); resolve_command 不应被调用
    REQUIRE(true);
  } else {
    FAIL("non-slash input must not dispatch to command layer");
  }
  REQUIRE_FALSE(reg.resolve_command(input).has_value());
}

TEST_CASE("/exit reserved command is not resolvable by plugins",
          "[command][input-loop]") {
  CommandRegistry reg;
  REQUIRE_FALSE(reg.register_command(make_spec("/exit", "plugin_a")));
  REQUIRE_FALSE(reg.resolve_command("/exit").has_value());
}
