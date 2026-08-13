// tests/test_declare_tool_auto_schema.cpp
// ADR-0073 D2/D4: DECLARE_TOOL_V3 schema generation tests
// Covers: string, int, optional<T>, struct types + backward compat V2

#include "catch_amalgamated.hpp"

#include "agenticdsl/tools/schema_generation.h"
#include "agenticdsl/pdk/tool_macros.h"

#include <optional>
#include <string>
#include <vector>

using namespace agenticdsl;

// Test structs for schema generation
struct SimpleArgs {
  std::string path;
  int max_lines = 0;
};

struct OptionalArgs {
  std::string filename;
  std::optional<int> max_results;
};

TEST_CASE("SchemaGenerator<string> generates string type") {
  auto schema = SchemaGenerator<std::string>::to_schema();
  REQUIRE(schema["type"] == "string");
}

TEST_CASE("SchemaGenerator<int> generates integer type") {
  auto schema = SchemaGenerator<int>::to_schema();
  REQUIRE(schema["type"] == "integer");
}

TEST_CASE("SchemaGenerator<long> generates integer type") {
  auto schema = SchemaGenerator<long>::to_schema();
  REQUIRE(schema["type"] == "integer");
}

TEST_CASE("SchemaGenerator<size_t> generates integer type") {
  auto schema = SchemaGenerator<size_t>::to_schema();
  REQUIRE(schema["type"] == "integer");
}

TEST_CASE("SchemaGenerator<float> generates number type") {
  auto schema = SchemaGenerator<float>::to_schema();
  REQUIRE(schema["type"] == "number");
}

TEST_CASE("SchemaGenerator<double> generates number type") {
  auto schema = SchemaGenerator<double>::to_schema();
  REQUIRE(schema["type"] == "number");
}

TEST_CASE("SchemaGenerator<bool> generates boolean type") {
  auto schema = SchemaGenerator<bool>::to_schema();
  REQUIRE(schema["type"] == "boolean");
}

TEST_CASE("SchemaGenerator<std::optional<T>> strips optional") {
  auto schema = SchemaGenerator<std::optional<int>>::to_schema();
  REQUIRE(schema["type"] == "integer");
}

TEST_CASE("SchemaGenerator<std::vector<T>> generates array type") {
  auto schema = SchemaGenerator<std::vector<std::string>>::to_schema();
  REQUIRE(schema["type"] == "array");
  REQUIRE(schema["items"]["type"] == "string");
}

TEST_CASE("SchemaGenerator<std::map<std::string, T>> generates object type") {
  auto schema = SchemaGenerator<std::map<std::string, int>>::to_schema();
  REQUIRE(schema["type"] == "object");
}

TEST_CASE("make_input_schema wraps with JSON Schema 2020-12") {
  auto schema = SchemaGenerator<std::string>::to_schema();
  auto doc = make_input_schema("test_input", schema);
  REQUIRE(doc["$schema"] == "https://json-schema.org/draft/2020-12/schema");
  REQUIRE(doc["title"] == "test_input");
  REQUIRE(doc["type"] == "string");
}

// V2 DECLARE_TOOL backward compatibility test
// This should compile without any changes
namespace backward_compat {

DECLARE_TOOL(test_tool_v2,
  "A test tool for backward compatibility",
  ReadOnly,
  "agent",
  return nlohmann::json{{"result", "ok"}};
)

}  // namespace backward_compat

TEST_CASE("DECLARE_TOOL V2 macro compiles unchanged (backward compat)") {
  auto spec = backward_compat::tool_spec_test_tool_v2;
  REQUIRE(spec.name == "test_tool_v2");
  REQUIRE(spec.description == "A test tool for backward compatibility");
  REQUIRE(spec.metadata.input_schema == std::nullopt);
  REQUIRE(spec.metadata.output_schema == std::nullopt);
}

// DECLARE_TOOL_V3 test
namespace v3_test {

struct TestInputArgs {
  std::string filename;
  int max_lines;
  bool include_hidden = false;
};

DECLARE_TOOL_V3(test_tool_v3,
  "A V3 tool with auto schema",
  ReadOnly,
  "agent",
  TestInputArgs,
  std::nullptr_t,
  return nlohmann::json{{"result", "ok"}};
)

}  // namespace v3_test

TEST_CASE("DECLARE_TOOL_V3 generates input_schema") {
  auto spec = v3_test::tool_spec_test_tool_v3;
  REQUIRE(spec.metadata.input_schema.has_value());
  REQUIRE(spec.metadata.input_schema.value()["type"] == "object");
}

TEST_CASE("DECLARE_TOOL_V3 output_schema is nullopt when nullptr") {
  auto spec = v3_test::tool_spec_test_tool_v3;
  REQUIRE(spec.metadata.output_schema == std::nullopt);
}
