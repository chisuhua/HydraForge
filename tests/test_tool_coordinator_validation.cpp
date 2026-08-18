// tests/test_tool_coordinator_validation.cpp
// 功能描述：ADR-0073 D3 — ToolCoordinator 4 步 sanitization pipeline 测试
//          (1 happy + 4 拒绝路径 + V2 legacy + safe shell)
// 作者：from-roadmap-phase-6c-schema-complete change (Batch 2 Task 5/6)
// 最后修改日期：2026-08-18
#include <catch_amalgamated.hpp>

#include <memory>
#include <string>
#include <unordered_map>

#include "common/policy/policy_factory.h"
#include "common/tools/tool_coordinator.h"

using namespace agenticdsl;

namespace {

// 最小 IToolRegistry 实现: call_tool 固定返回 {"echo": args} 成功
class StubRegistry : public IToolRegistry {
 public:
  bool has_tool(const std::string&) const override { return true; }

  nlohmann::json call_tool(
      const std::string&,
      const std::unordered_map<std::string, std::string>& args) override {
    nlohmann::json echo = nlohmann::json::object();
    for (const auto& [k, v] : args) echo[k] = v;
    return {{"echo", echo}};
  }

  std::vector<std::string> list_tools() const override { return {"stub"}; }

  void register_tool_function(std::string, ToolMetadata, ToolFunc) override {}

  void register_llm_tool(std::string, std::unique_ptr<ILLMTool>,
                         const LLMParams&) override {}
  bool is_llm_tool(const std::string&) const override { return false; }
  const LLMParams& get_llm_params(const std::string&) const override {
    static LLMParams params{};
    return params;
  }
  nlohmann::json call_llm_tool(const std::string&, const std::string&,
                               const LLMParams&) override {
    return nlohmann::json::object();
  }

  void set_cost_callback(CostCallback) override {}
};

struct Fixture {
  StubRegistry registry;
  std::shared_ptr<IExecutionPolicy> policy =
      PolicyFactory::create(PolicyMode::Yolo);
  ToolCoordinator coord;

  Fixture()
      : coord(registry, policy,
              [](const ApprovalRequest&, int) { return true; },
              nullptr) {}

  ToolCallContext make_ctx() const {
    ToolCallContext ctx;
    ctx.session_id = "test-session";
    ctx.caller_layer = "workflow";
    return ctx;
  }
};

ToolMetadata make_schema_meta(const std::string& schema_json,
                              ToolCategory category = ToolCategory::ReadOnly) {
  ToolMetadata meta;
  meta.name = "test_tool";
  meta.description = "schema validation test tool";
  meta.category = category;
  meta.min_layer = LayerProfile::Workflow;
  meta.input_schema = nlohmann::json::parse(schema_json);
  return meta;
}

}  // namespace

TEST_CASE("happy path: valid input passes 4-step pipeline",
          "[tool-coord][validation][happy]") {
  Fixture f;
  auto meta = make_schema_meta(
      R"({"type":"object","properties":{"path":{"type":"string"}},"required":["path"]})");
  std::unordered_map<std::string, std::string> args = {{"path", "/tmp/x.txt"}};

  auto result = f.coord.execute(meta, f.make_ctx(), args);

  REQUIRE(result.error_code != ErrorCode::InvalidParams);
  REQUIRE(result.ok == true);
}

TEST_CASE("Step 2 (Strict): string for integer field rejected",
          "[tool-coord][validation][strict]") {
  Fixture f;
  auto meta = make_schema_meta(
      R"({"type":"object","properties":{"port":{"type":"integer"}},"required":["port"]})");
  meta.validation_mode = ToolMetadata::ValidationMode::Strict;
  std::unordered_map<std::string, std::string> args = {{"port", "8080"}};

  auto result = f.coord.execute(meta, f.make_ctx(), args);

  REQUIRE(result.ok == false);
  REQUIRE(result.error_code == ErrorCode::InvalidParams);
  REQUIRE(result.meta["validation_stage"] == "coercion");
  REQUIRE(result.meta["field_path"] == "port");
}

TEST_CASE("Step 2 (Warn): string coerced to integer, not rejected",
          "[tool-coord][validation][warn]") {
  Fixture f;
  auto meta = make_schema_meta(
      R"({"type":"object","properties":{"port":{"type":"integer"}}})");
  meta.validation_mode = ToolMetadata::ValidationMode::Warn;
  std::unordered_map<std::string, std::string> args = {{"port", "8080"}};

  auto result = f.coord.execute(meta, f.make_ctx(), args);

  REQUIRE(result.error_code != ErrorCode::InvalidParams);
  REQUIRE(result.ok == true);
  // 转换后的值写回 string map, 下游 registry 收到 "8080"
  REQUIRE(result.data["echo"]["port"] == "8080");
}

TEST_CASE("Step 3: required field missing rejected",
          "[tool-coord][validation][required]") {
  Fixture f;
  auto meta = make_schema_meta(
      R"({"type":"object","properties":{"path":{"type":"string"}},"required":["path","mode"]})");
  std::unordered_map<std::string, std::string> args = {{"path", "x.txt"}};

  auto result = f.coord.execute(meta, f.make_ctx(), args);

  REQUIRE(result.ok == false);
  REQUIRE(result.error_code == ErrorCode::InvalidParams);
  REQUIRE(result.meta["validation_stage"] == "required_field");
  REQUIRE(result.meta["field_path"] == "mode");
}

TEST_CASE("Step 4: dangerous pattern rm -rf rejected with audit hash",
          "[tool-coord][validation][business]") {
  Fixture f;
  auto meta = make_schema_meta(
      R"({"type":"object","properties":{"cmd":{"type":"string"}}})",
      ToolCategory::Execute);
  std::unordered_map<std::string, std::string> args = {
      {"cmd", "rm -rf /tmp/build"}};

  auto result = f.coord.execute(meta, f.make_ctx(), args);

  REQUIRE(result.ok == false);
  REQUIRE(result.error_code == ErrorCode::InvalidParams);
  REQUIRE(result.meta["validation_stage"] == "business_rules");
  REQUIRE(result.meta["reason"] == "dangerous_pattern_detected");
  REQUIRE(result.meta["matched_pattern"].is_string());
  REQUIRE(result.meta.contains("args_hash"));
  // defense-in-depth: 审计元数据不落 raw cmd
  REQUIRE(!result.meta.contains("cmd_raw"));
  REQUIRE(!result.meta.contains("cmd"));
}

TEST_CASE("V2 legacy: no schema, dangerous cmd still rejected at step 4",
          "[tool-coord][validation][v2-legacy]") {
  Fixture f;
  ToolMetadata meta;
  meta.name = "legacy_shell";
  meta.category = ToolCategory::Execute;
  meta.min_layer = LayerProfile::Workflow;
  // input_schema 无值 → V2 legacy, 跳过 step 1-3
  std::unordered_map<std::string, std::string> args = {
      {"cmd", "rm -rf /tmp/legacy"}};

  auto result = f.coord.execute(meta, f.make_ctx(), args);

  REQUIRE(result.ok == false);
  REQUIRE(result.error_code == ErrorCode::InvalidParams);
  REQUIRE(result.meta["validation_stage"] == "business_rules");
}

TEST_CASE("safe shell command passes business rules",
          "[tool-coord][validation][safe-shell]") {
  Fixture f;
  auto meta = make_schema_meta(
      R"({"type":"object","properties":{"cmd":{"type":"string"}}})",
      ToolCategory::Execute);
  std::unordered_map<std::string, std::string> args = {
      {"cmd", "echo hello world"}};

  auto result = f.coord.execute(meta, f.make_ctx(), args);

  REQUIRE(result.error_code != ErrorCode::InvalidParams);
  REQUIRE(result.ok == true);
}
