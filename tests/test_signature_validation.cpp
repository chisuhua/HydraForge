// tests/test_signature_validation.cpp
// T4 signature-validation-real-impl: SignatureValidator 核心实装
#include "catch_amalgamated.hpp"
#include "agenticdsl/executor/signature_validator.h"
#include <string>

using namespace agenticdsl::executor;

TEST_CASE("SignatureValidator: strict mode throws on invalid type", "[sigval][strict]") {
    SignatureValidator v(SignatureMode::Strict);
    REQUIRE_THROWS_AS(v.parse_signature_ast("(input: invalid_type) -> {result: string}"),
                       std::invalid_argument);
}

TEST_CASE("SignatureValidator: warn mode logs and continues on invalid type",
          "[sigval][warn]") {
    SignatureValidator v(SignatureMode::Warn);
    REQUIRE_NOTHROW(v.parse_signature_ast("(input: number) -> {result: invalid_type}"));
}

TEST_CASE("SignatureValidator: ignore mode skips validation entirely",
          "[sigval][ignore]") {
    SignatureValidator v(SignatureMode::Ignore);
    REQUIRE_NOTHROW(v.parse_signature_ast("(input: anything_at_all) -> {result: anything}"));
}

TEST_CASE("SignatureValidator: parses valid signature AST correctly",
          "[sigval][parse]") {
    SignatureValidator v(SignatureMode::Strict);
    auto ast = v.parse_signature_ast("(input: string, n: number) -> {result: string, count: number}");
    REQUIRE(ast.inputs.size() == 2);
    REQUIRE(ast.inputs[0].name == "input");
    REQUIRE(ast.inputs[0].type == "string");
    REQUIRE(ast.inputs[1].name == "n");
    REQUIRE(ast.inputs[1].type == "number");
    REQUIRE(ast.outputs.size() == 2);
    REQUIRE(ast.outputs[0].name == "result");
    REQUIRE(ast.outputs[0].type == "string");
}

TEST_CASE("SignatureValidator: type whitelist includes standard JSON Schema types",
          "[sigval][types]") {
    SignatureValidator v(SignatureMode::Strict);
    for (const auto& t : {"string", "number", "boolean", "object", "array", "integer", "null"}) {
        std::string sig = "(input: " + std::string(t) + ") -> {result: " + std::string(t) + "}";
        REQUIRE_NOTHROW(v.parse_signature_ast(sig));
    }
}

TEST_CASE("SignatureValidator: empty signature throws (callers should check has_value first)",
          "[sigval][empty]") {
    SignatureValidator v(SignatureMode::Strict);
    REQUIRE_THROWS_AS(v.parse_signature_ast(""), std::invalid_argument);
}

TEST_CASE("SignatureValidator: malformed signature throws in strict mode",
          "[sigval][strict]") {
    SignatureValidator v(SignatureMode::Strict);
    REQUIRE_THROWS_AS(v.parse_signature_ast("not a valid signature"),
                       std::invalid_argument);
}