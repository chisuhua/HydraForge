// tests/test_tool_coordinator_validation.cpp
// 功能描述：ADR-0073 D3 — ToolCoordinator 4 步 sanitization pipeline 测试
//          (1 happy + 4 拒绝路径 + V2 legacy + safe shell)
//          + P1 fix 新增: coercion 透明化 / 不可转换 warning / enum 重试 / audit session_id
// 作者：from-roadmap-phase-6c-schema-complete change (Batch 2 Task 5/6)
//       + from-roadmap-phase-6c-validation-refinements (Phase 6c Wave 1 followup P1 ship)
// 最后修改日期：2026-08-23
#include <catch_amalgamated.hpp>

#include <memory>
#include <string>
#include <unordered_map>

#include "agenticdsl/contract/bus_event.h"
#include "common/policy/policy_factory.h"
#include "common/tools/tool_coordinator.h"
#include "test_helpers/mock_bus.h"

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

// =====================================================================
// from-roadmap-phase-6c-validation-refinements P1 fix 新增测试
// =====================================================================

namespace {

// 带 MockBus 的 Fixture,用于验证 audit event 发射与 session_id 继承
struct FixtureWithBus {
  StubRegistry registry;
  std::shared_ptr<IExecutionPolicy> policy =
      PolicyFactory::create(PolicyMode::Yolo);
  test::MockBus bus;
  std::unique_ptr<ToolCoordinator> coord;

  FixtureWithBus()
      : coord(std::make_unique<ToolCoordinator>(
            registry, policy,
            [](const ApprovalRequest&, int) { return true; },
            std::shared_ptr<IInteractionBus>(&bus, [](IInteractionBus*){}))) {}

  ToolCallContext make_ctx(const std::string& session_id = "sess_abc123",
                           const std::string& trace_id = "trace_xyz789",
                           const std::string& caller_layer = "workflow") const {
    ToolCallContext ctx;
    ctx.session_id = session_id;
    ctx.trace_id = trace_id;
    ctx.caller_layer = caller_layer;
    return ctx;
  }
};

}  // namespace

TEST_CASE("P1#1: Warn-mode integer coercion value preserved via v.dump()",
          "[tool-coord][validation][p1-1]") {
  Fixture f;
  auto meta = make_schema_meta(
      R"({"type":"object","properties":{"port":{"type":"integer"}}})");
  meta.validation_mode = ToolMetadata::ValidationMode::Warn;
  std::unordered_map<std::string, std::string> args = {{"port", "8080"}};

  auto result = f.coord.execute(meta, f.make_ctx(), args);

  // coerce "8080" → 8080 (integer), v.dump() → "8080" (无 quotes)
  // 写回 effective_args[k] = v.dump() 后,工具接收 string "8080"
  REQUIRE(result.ok == true);
  REQUIRE(result.error_code != ErrorCode::InvalidParams);
  REQUIRE(result.data["echo"]["port"] == "8080");
}

TEST_CASE("P1#2: Warn-mode 'abc' integer rejected with coercion_failed",
          "[tool-coord][validation][p1-2]") {
  FixtureWithBus fb;
  auto meta = make_schema_meta(
      R"({"type":"object","properties":{"port":{"type":"integer"}}})");
  meta.validation_mode = ToolMetadata::ValidationMode::Warn;
  std::unordered_map<std::string, std::string> args = {{"port", "abc"}};

  auto result = fb.coord->execute(meta, fb.make_ctx(), args);

  REQUIRE(result.ok == false);
  REQUIRE(result.error_code == ErrorCode::InvalidParams);
  REQUIRE(result.meta["validation_stage"] == "coercion");
  REQUIRE(result.meta["reason"] == "coercion_failed");
  REQUIRE(result.meta["failures"].is_array());
  REQUIRE(result.meta["failures"].size() == 1);
  REQUIRE(result.meta["failures"][0]["field_path"] == "port");
  REQUIRE(result.meta["failures"][0]["target_type"] == "integer");

  // 必须发 tool.audit.denied, reason=coercion_failed
  const BusEvent* denied = fb.bus.last("tool.audit.denied");
  REQUIRE(denied != nullptr);
  REQUIRE(denied->payload.meta.value("session_id", "") == "sess_abc123");
  REQUIRE(denied->payload.meta.value("trace_id", "") == "trace_xyz789");
}

TEST_CASE("P1#3: Warn + '1' integer+enum[1,2,3] accepted after coercion retry",
          "[tool-coord][validation][p1-3-warn]") {
  Fixture f;
  auto meta = make_schema_meta(
      R"({"type":"object","properties":{"level":{"type":"integer","enum":[1,2,3]}}})");
  meta.validation_mode = ToolMetadata::ValidationMode::Warn;
  std::unordered_map<std::string, std::string> args = {{"level", "1"}};

  auto result = f.coord.execute(meta, f.make_ctx(), args);

  REQUIRE(result.ok == true);
  REQUIRE(result.error_code != ErrorCode::InvalidParams);
  // coerce 后 string "1" → integer 1,v.dump() → "1", 工具接收 string "1"
  REQUIRE(result.data["echo"]["level"] == "1");
}

TEST_CASE("P1#3: Strict + '1' integer+enum[1,2,3] still rejected pre-coerce",
          "[tool-coord][validation][p1-3-strict]") {
  Fixture f;
  auto meta = make_schema_meta(
      R"({"type":"object","properties":{"level":{"type":"integer","enum":[1,2,3]}}})");
  meta.validation_mode = ToolMetadata::ValidationMode::Strict;
  std::unordered_map<std::string, std::string> args = {{"level", "1"}};

  auto result = f.coord.execute(meta, f.make_ctx(), args);

  REQUIRE(result.ok == false);
  REQUIRE(result.error_code == ErrorCode::InvalidParams);
  REQUIRE(result.meta["validation_stage"] == "coercion");
  REQUIRE(result.meta["field_path"] == "level");
}

TEST_CASE("P1#3: Warn + '1' integer+enum[80,443] rejected after coerce (8080)",
          "[tool-coord][validation][p1-3-retry-reject]") {
  Fixture f;
  auto meta = make_schema_meta(
      R"({"type":"object","properties":{"port":{"type":"integer","enum":[80,443]}}})");
  meta.validation_mode = ToolMetadata::ValidationMode::Warn;
  std::unordered_map<std::string, std::string> args = {{"port", "8080"}};

  auto result = f.coord.execute(meta, f.make_ctx(), args);

  REQUIRE(result.ok == false);
  REQUIRE(result.error_code == ErrorCode::InvalidParams);
  REQUIRE(result.meta["validation_stage"] == "schema_validate");
  REQUIRE(result.meta["post_coercion"] == true);
}

TEST_CASE("Audit: tool.audit.denied carries real session_id from ctx",
          "[tool-coord][validation][audit-session]") {
  FixtureWithBus fb;
  auto meta = make_schema_meta(
      R"({"type":"object","properties":{"port":{"type":"integer"}}})");
  meta.validation_mode = ToolMetadata::ValidationMode::Strict;
  std::unordered_map<std::string, std::string> args = {{"port", "8080"}};

  auto result = fb.coord->execute(meta, fb.make_ctx("my_session", "my_trace"),
                                  args);

  REQUIRE(result.ok == false);
  const BusEvent* denied = fb.bus.last("tool.audit.denied");
  REQUIRE(denied != nullptr);
  REQUIRE(denied->payload.meta.value("session_id", "") == "my_session");
  REQUIRE(denied->payload.meta.value("trace_id", "") == "my_trace");
}

TEST_CASE("Audit: ctx with empty session_id and trace_id → graceful degradation",
          "[tool-coord][validation][audit-empty]") {
  FixtureWithBus fb;
  auto meta = make_schema_meta(
      R"({"type":"object","properties":{"port":{"type":"integer"}}})");
  meta.validation_mode = ToolMetadata::ValidationMode::Strict;
  std::unordered_map<std::string, std::string> args = {{"port", "8080"}};

  // session_id 和 trace_id 都为空 — graceful degradation, 不崩溃
  auto result = fb.coord->execute(meta, fb.make_ctx("", "", "workflow"), args);

  REQUIRE(result.ok == false);
  const BusEvent* denied = fb.bus.last("tool.audit.denied");
  REQUIRE(denied != nullptr);
  REQUIRE(denied->payload.meta.value("session_id", "non_empty") == "");
  // trace_id 为空时不写入 meta
  REQUIRE(denied->payload.meta.contains("trace_id") == false);
}
