// tests/test_itoolregistry_json_args.cpp
// IToolRegistry JSON args convenience methods — Phase 0 test suite
// 关联: openspec/changes/2026-07-17-hydraforge-itoolregistry-json-args/

#include "catch_amalgamated.hpp"

#include <agenticdsl/contract/itool_registry.h>
#include <common/tools/registry.h>

#include <nlohmann/json.hpp>

using namespace agenticdsl;
using json = nlohmann::json;

// --- Helper: build a registry with one tool registered via map variant ---
namespace {

ToolRegistry make_registry_with_map_tool() {
  ToolRegistry reg;
  reg.register_tool_function(
      "echo",
      ToolMetadata{"echo", "test", "test", ToolCategory::ReadOnly, LayerProfile::Workflow},
      [](const std::unordered_map<std::string, std::string>& args) -> json {
        json out = json::object();
        for (const auto& [k, v] : args) out[k] = v;
        return out;
      }
  );
  return reg;
}

ToolRegistry make_registry_with_json_tool() {
  ToolRegistry reg;
  reg.register_tool_function_json(
      "sum",
      ToolMetadata{"sum", "test", "test", ToolCategory::ReadOnly, LayerProfile::Workflow},
      [](const json& args) -> json {
        int total = 0;
        for (const auto& [k, v] : args.items()) {
          if (v.is_number_integer()) {
            total += v.get<int>();
          } else if (v.is_string()) {
            // v1 round-trip: strings from map conversion need parsing
            try {
              total += std::stoi(v.get<std::string>());
            } catch (...) {}
          }
        }
        return {{"sum", total}};
      }
  );
  return reg;
}

}  // namespace

// ===== 1. call_tool_json with flat string args (should match map call) =====
TEST_CASE("call_tool_json: flat string args equal map call", "[itoolregistry][json]") {
  auto reg = make_registry_with_map_tool();
  json result = reg.call_tool_json("echo", {{"k", "v"}});
  REQUIRE(result["k"] == "v");
}

// ===== 2. call_tool_json nested objects (stringified) =====
TEST_CASE("call_tool_json: nested object is stringified", "[itoolregistry][json]") {
  auto reg = make_registry_with_map_tool();
  json input = {{"outer", {{"inner", 42}}}};
  json result = reg.call_tool_json("echo", input);
  REQUIRE(result.contains("outer"));
  REQUIRE(result["outer"].is_string());  // stringified form
  REQUIRE(result["outer"].get<std::string>() == "{\"inner\":42}");
}

// ===== 3. call_tool_json array args =====
TEST_CASE("call_tool_json: array args become stringified", "[itoolregistry][json]") {
  auto reg = make_registry_with_map_tool();
  json input = {{"items", json::array({1, 2, 3})}};
  json result = reg.call_tool_json("echo", input);
  REQUIRE(result["items"] == "[1,2,3]");
}

// ===== 4. register_tool_function_json handler receives json =====
TEST_CASE("register_tool_function_json: handler receives nlohmann::json", "[itoolregistry][json]") {
  auto reg = make_registry_with_json_tool();
  json result = reg.call_tool_json("sum", {{"a", 1}, {"b", 2}, {"c", 3}});
  REQUIRE(result["sum"] == 6);
}

// ===== 5. json and map variants coexist (separate tools) =====
TEST_CASE("json and map coexistence: separate tools", "[itoolregistry][json]") {
  ToolRegistry reg;
  reg.register_tool_function(
      "via_map",
      ToolMetadata{"via_map", "test", "test", ToolCategory::ReadOnly, LayerProfile::Workflow},
      [](const std::unordered_map<std::string, std::string>&) -> json {
        return {{"called_via", "map"}};
      }
  );
  reg.register_tool_function_json(
      "via_json",
      ToolMetadata{"via_json", "test", "test", ToolCategory::ReadOnly, LayerProfile::Workflow},
      [](const json&) -> json {
        return {{"called_via", "json"}};
      }
  );
  auto via_map_result = reg.call_tool("via_map", {});
  auto via_json_result = reg.call_tool_json("via_json", json::object());
  REQUIRE(via_map_result["called_via"] == "map");
  REQUIRE(via_json_result["called_via"] == "json");
}

// ===== 6. Existing 9 virtual methods unaffected =====
TEST_CASE("existing call_tool(map) signature unchanged", "[itoolregistry][json]") {
  auto reg = make_registry_with_map_tool();
  json result = reg.call_tool("echo", {{"key", "value"}});
  REQUIRE(result["key"] == "value");
}

// ===== 7. Boolean values round-trip (NEW from Oracle review) =====
TEST_CASE("call_tool_json: booleans are stringified", "[itoolregistry][json][edge]") {
  auto reg = make_registry_with_map_tool();
  json result = reg.call_tool_json("echo", {{"flag", true}, {"other", false}});
  REQUIRE(result["flag"] == "true");
  REQUIRE(result["other"] == "false");
}

// ===== 8. Empty json input edge case (NEW) =====
TEST_CASE("call_tool_json: empty object input", "[itoolregistry][json][edge]") {
  auto reg = make_registry_with_map_tool();
  json result = reg.call_tool_json("echo", json::object());
  REQUIRE(result.is_object());
  REQUIRE(result.empty());
}

// ===== 9. Null values stringified as "null" (NEW) =====
TEST_CASE("call_tool_json: null values", "[itoolregistry][json][edge]") {
  auto reg = make_registry_with_map_tool();
  json result = reg.call_tool_json("echo", {{"val", nullptr}});
  REQUIRE(result["val"] == "null");
}

// ===== 10. Mixed type args (string + number + bool + array) (NEW) =====
TEST_CASE("call_tool_json: mixed type args", "[itoolregistry][json][edge]") {
  auto reg = make_registry_with_map_tool();
  json input = {
      {"str", "hello"},
      {"num", 42},
      {"flag", true},
      {"arr", json::array({"a", "b"})},
      {"obj", {{"nested", "value"}}}
  };
  json result = reg.call_tool_json("echo", input);
  REQUIRE(result["str"] == "hello");
  REQUIRE(result["num"] == "42");
  REQUIRE(result["flag"] == "true");
  REQUIRE(result["arr"] == "[\"a\",\"b\"]");
  REQUIRE(result["obj"] == "{\"nested\":\"value\"}");
}