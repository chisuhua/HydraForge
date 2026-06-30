// tests/test_trace_exporter.cpp
// Sprint 16 Coverage Backfill: 测试 src/modules/trace/trace_exporter.cpp
#include "catch_amalgamated.hpp"
#include "trace/trace_exporter.h"
#include "agenticdsl/types/trace_record.h"

using namespace agenticdsl;

TEST_CASE("TraceExporter::on_node_start records timestamp", "[trace]") {
  TraceExporter exporter;
  nlohmann::json initial_ctx = {{"foo", "bar"}};
  std::optional<ExecutionBudget> budget;

  exporter.on_node_start("/main/step1", NodeType::ASSIGN, initial_ctx, budget);

  auto traces = exporter.get_traces();
  REQUIRE(traces.size() == 1);
  REQUIRE(traces[0].node_path == "/main/step1");
  REQUIRE(traces[0].status == "running");
}

TEST_CASE("TraceExporter::on_node_end records status and duration", "[trace]") {
  TraceExporter exporter;
  nlohmann::json ctx = {{"foo", "bar"}};
  std::optional<ExecutionBudget> budget;

  exporter.on_node_start("/main/step1", NodeType::TOOL_CALL, ctx, budget);
  exporter.on_node_end("/main/step1", "success", std::nullopt, ctx, ctx,
                       std::nullopt, budget);

  auto traces = exporter.get_traces();
  REQUIRE(traces.size() == 1);
  REQUIRE(traces[0].status == "success");
  REQUIRE(traces[0].error_code.has_value() == false);
}

TEST_CASE("TraceExporter::on_node_end with error_code", "[trace]") {
  TraceExporter exporter;
  nlohmann::json ctx = {{"key", "value"}};
  std::optional<ExecutionBudget> budget;

  exporter.on_node_start("/main/error_step", NodeType::ASSERT, ctx, budget);
  exporter.on_node_end("/main/error_step", "failed", std::string("E001"),
                       ctx, ctx, std::nullopt, budget);

  auto traces = exporter.get_traces();
  REQUIRE(traces.size() == 1);
  REQUIRE(traces[0].status == "failed");
  REQUIRE(traces[0].error_code.has_value());
  REQUIRE(traces[0].error_code.value() == "E001");
}

TEST_CASE("TraceExporter::clear_traces empties trace list", "[trace]") {
  TraceExporter exporter;
  nlohmann::json ctx = {{"key", "value"}};
  std::optional<ExecutionBudget> budget;

  exporter.on_node_start("/main/step1", NodeType::ASSIGN, ctx, budget);
  exporter.on_node_start("/main/step2", NodeType::TOOL_CALL, ctx, budget);
  REQUIRE(exporter.get_traces().size() == 2);

  exporter.clear_traces();
  REQUIRE(exporter.get_traces().empty());
}

TEST_CASE("TraceExporter handles empty trace list", "[trace]") {
  TraceExporter exporter;
  REQUIRE(exporter.get_traces().empty());
}