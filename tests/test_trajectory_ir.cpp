// tests/test_trajectory_ir.cpp
// 功能描述：TrajectoryIR 单元测试 (T15, ADR-0061-06 v1.1 独立序列化视图)
//          Phase 0: 契约类型编译占位 (≥8 cases)
//          Phase 1: Converter 单向性 (from_parsed_graph)
//          Phase 2: V1 backends (to_sft_data / to_otel_spans) + ConstantFoldingPass 占位
// 设计依据：docs/adr/skill/adr-0061-06-v1-1-amendment-trajectory-ir-decouple.md
//          + openspec/changes/t15-trajectory-ir/specs/trajectory-ir/spec.md
// 关键不变量：ParsedGraph 零修改 (src/core/types/node.h), TrajectoryIR 为独立类
// 作者：HydraForge Sprint 24 T15 ship
// 最后修改日期：2026-08-27

#include "catch_amalgamated.hpp"

#include "agenticdsl/ir/trajectory_ir.h"
#include "core/types/node.h"

#include <memory>
#include <string>
#include <vector>

using agenticdsl::EndNode;
using agenticdsl::NodePath;
using agenticdsl::ParsedGraph;
using agenticdsl::StartNode;
using agenticdsl::ir::ConstantFoldingPass;
using agenticdsl::ir::TrajectoryIR;

namespace {

// 构造最小 ParsedGraph: /main/start -> /main/end
ParsedGraph make_minimal_graph() {
  ParsedGraph pg;
  pg.path = "/main";
  pg.nodes.push_back(std::make_unique<StartNode>(
      "/main/start", std::vector<NodePath>{"/main/end"}));
  pg.nodes.push_back(std::make_unique<EndNode>("/main/end"));
  return pg;
}

}  // namespace

// ============================================================================
// Case 1: 三级 IR 结构完整 (spec "三级 IR 完整定义")
// ============================================================================
TEST_CASE("three_level_ir_complete", "[ir][t15][phase0]") {
  TrajectoryIR::RawIR raw;
  REQUIRE(raw.dsl_text.empty());
  REQUIRE(raw.metadata.is_object());

  TrajectoryIR::ParsedIR parsed;
  REQUIRE(parsed.nodes.empty());
  REQUIRE(parsed.edges.empty());
  REQUIRE(parsed.steps.empty());

  TrajectoryIR::CanonicalIR canonical;
  REQUIRE(canonical.canonical_nodes.empty());
}

// ============================================================================
// Case 2: RawIR 持有 DSL 文本 + metadata
// ============================================================================
TEST_CASE("raw_ir_holds_dsl_text_and_metadata", "[ir][t15][phase0]") {
  TrajectoryIR::RawIR raw;
  raw.dsl_text = "# /main\n## step1";
  raw.metadata = {{"source", "test"}};
  REQUIRE(raw.dsl_text.find("/main") != std::string::npos);
  REQUIRE(raw.metadata["source"] == "test");
}

// ============================================================================
// Case 3: Converter 单向 — 空图与最小图 (spec "Converter 单向性")
// ============================================================================
TEST_CASE("converter_from_parsed_graph_basic", "[ir][t15][phase1]") {
  {
    ParsedGraph empty;
    auto ir = TrajectoryIR::from_parsed_graph(empty);
    REQUIRE(ir.nodes.empty());
    REQUIRE(ir.edges.empty());
  }
  {
    auto pg = make_minimal_graph();
    auto ir = TrajectoryIR::from_parsed_graph(pg);
    REQUIRE(ir.nodes.size() == 2);
    REQUIRE(ir.edges.size() == 1);
    REQUIRE(ir.edges[0].from == "/main/start");
    REQUIRE(ir.edges[0].to == "/main/end");
    REQUIRE(ir.edges[0].weight == 1.0);  // V1 简化
    REQUIRE(ir.nodes[0].type == "start");
    REQUIRE(ir.nodes[1].type == "end");
  }
}

// ============================================================================
// Case 4: Converter 单向不变量 — 修改 ParsedGraph 不影响已生成快照
// (spec "ParsedGraph 修改不影响 TrajectoryIR")
// ============================================================================
TEST_CASE("converter_unidirectional_invariant", "[ir][t15][phase1]") {
  auto pg = make_minimal_graph();
  auto snapshot = TrajectoryIR::from_parsed_graph(pg);
  REQUIRE(snapshot.nodes.size() == 2);
  REQUIRE(snapshot.nodes[0].metadata.is_object());

  // 修改原 ParsedGraph (metadata + 追加节点)
  pg.nodes[0]->metadata = {{"mutated", true}};
  pg.nodes.push_back(std::make_unique<EndNode>("/main/end2"));

  // 快照保持不变 (值类型 + 浅拷贝语义)
  REQUIRE(snapshot.nodes.size() == 2);
  REQUIRE(snapshot.edges.size() == 1);
  REQUIRE(snapshot.nodes[0].metadata.count("mutated") == 0);
}

// ============================================================================
// Case 5: V1 Steps 占位 (spec "V1 Steps 占位")
// ============================================================================
TEST_CASE("converter_steps_placeholder_v1", "[ir][t15][phase1]") {
  auto pg = make_minimal_graph();
  auto ir = TrajectoryIR::from_parsed_graph(pg);
  // V1: steps 为空占位 (V2 集成 ADR-0061-13 DistillationRecord.reward)
  REQUIRE(ir.steps.empty());
}

// ============================================================================
// Case 6: to_sft_data 序列化 (spec "to_sft_data 序列化")
// ============================================================================
TEST_CASE("to_sft_data_basic", "[ir][t15][phase2]") {
  TrajectoryIR::CanonicalIR canonical;
  canonical.canonical_nodes.push_back(
      TrajectoryIR::NodeRecord{"/main/start", "start", nlohmann::json::object()});
  canonical.canonical_nodes.push_back(
      TrajectoryIR::NodeRecord{"/main/end", "end", nlohmann::json::object()});
  canonical.canonical_edges.push_back(
      TrajectoryIR::EdgeRecord{"/main/start", "/main/end", 1.0});

  const auto sft = TrajectoryIR::to_sft_data(canonical);
  REQUIRE(sft.contains("nodes"));
  REQUIRE(sft.contains("edges"));
  REQUIRE(sft.contains("steps"));
  REQUIRE(sft.contains("sft_metadata"));
  REQUIRE(sft["nodes"].size() == 2);
  REQUIRE(sft["edges"].size() == 1);
  REQUIRE(sft["nodes"][0]["id"] == "/main/start");
  REQUIRE(sft["nodes"][0]["type"] == "start");
  REQUIRE(sft["edges"][0]["weight"] == 1.0);
}

// ============================================================================
// Case 7: to_otel_spans 序列化 (spec "to_otel_spans 序列化")
// ============================================================================
TEST_CASE("to_otel_spans_basic", "[ir][t15][phase2]") {
  TrajectoryIR::CanonicalIR canonical;
  canonical.canonical_nodes.push_back(
      TrajectoryIR::NodeRecord{"/main/start", "start", nlohmann::json::object()});
  canonical.canonical_nodes.push_back(
      TrajectoryIR::NodeRecord{"/main/end", "end", nlohmann::json::object()});
  canonical.canonical_edges.push_back(
      TrajectoryIR::EdgeRecord{"/main/start", "/main/end", 1.0});

  const auto otel = TrajectoryIR::to_otel_spans(canonical);
  REQUIRE(otel.contains("spans"));
  REQUIRE(otel["spans"].size() == 2);
  for (const auto& span : otel["spans"]) {
    REQUIRE(span.contains("trace_id"));
    REQUIRE(span.contains("span_id"));
    REQUIRE(span.contains("parent_span_id"));
    REQUIRE(span.contains("start_time_unix_nano"));
    REQUIRE(span.contains("end_time_unix_nano"));
  }
  // 子节点 parent_span_id 指向父节点 span_id
  REQUIRE(otel["spans"][1]["parent_span_id"] == otel["spans"][0]["span_id"]);
  // 同 trace
  REQUIRE(otel["spans"][0]["trace_id"] == otel["spans"][1]["trace_id"]);
}

// ============================================================================
// Case 8: ConstantFoldingPass V1 占位 — 输入输出等价 (spec "ConstantFoldingPass 占位")
// ============================================================================
TEST_CASE("constant_folding_pass_passthrough", "[ir][t15][phase2]") {
  TrajectoryIR::CanonicalIR input;
  input.canonical_nodes.push_back(
      TrajectoryIR::NodeRecord{"/main/start", "start", {{"k", "v"}}});
  input.canonical_edges.push_back(
      TrajectoryIR::EdgeRecord{"/main/start", "/main/end", 1.0});
  input.metadata = {{"pass", "test"}};

  const auto output = ConstantFoldingPass::run(input);
  REQUIRE(output.canonical_nodes.size() == input.canonical_nodes.size());
  REQUIRE(output.canonical_edges.size() == input.canonical_edges.size());
  REQUIRE(output.canonical_nodes[0].id == "/main/start");
  REQUIRE(output.canonical_nodes[0].metadata["k"] == "v");
  REQUIRE(output.canonical_edges[0].weight == 1.0);
  REQUIRE(output.metadata["pass"] == "test");
}

// ============================================================================
// Case 9: CanonicalIR hash 确定性 (T3 SkillCompiler 集成前置)
// ============================================================================
TEST_CASE("canonical_ir_hash_deterministic", "[ir][t15][phase2]") {
  TrajectoryIR::CanonicalIR a;
  a.canonical_nodes.push_back(
      TrajectoryIR::NodeRecord{"/main/start", "start", nlohmann::json::object()});
  TrajectoryIR::CanonicalIR b = a;

  const auto ha = TrajectoryIR::hash(a);
  const auto hb = TrajectoryIR::hash(b);
  REQUIRE(ha.empty() == false);
  REQUIRE(ha == hb);  // 同输入同 hash

  // 不同输入不同 hash
  b.canonical_nodes.push_back(
      TrajectoryIR::NodeRecord{"/main/end", "end", nlohmann::json::object()});
  REQUIRE(TrajectoryIR::hash(b) != ha);
}

// ============================================================================
// Case 10: schema_version 字段默认 1.0 + sft/otel 序列化透传
// (ADR-0061-06 v1.1 §不变量 3 落地)
// ============================================================================
TEST_CASE("schema_version_defaults_and_serializes", "[ir][t15][phase2]") {
  // 默认值校验
  TrajectoryIR::ParsedIR parsed;
  REQUIRE(parsed.schema_version == "1.0");

  TrajectoryIR::CanonicalIR canonical;
  REQUIRE(canonical.schema_version == "1.0");

  // to_sft_data 顶层含 schema_version
  canonical.canonical_nodes.push_back(
      TrajectoryIR::NodeRecord{"/main/start", "start", nlohmann::json::object()});
  canonical.canonical_edges.push_back(
      TrajectoryIR::EdgeRecord{"/main/start", "/main/end", 1.0});

  const auto sft = TrajectoryIR::to_sft_data(canonical);
  REQUIRE(sft.contains("schema_version"));
  REQUIRE(sft["schema_version"] == "1.0");

  // to_otel_spans 顶层含 schema_version
  const auto otel = TrajectoryIR::to_otel_spans(canonical);
  REQUIRE(otel.contains("schema_version"));
  REQUIRE(otel["schema_version"] == "1.0");

  // 显式设置值透传
  TrajectoryIR::CanonicalIR custom;
  custom.schema_version = "1.1";
  custom.canonical_nodes.push_back(
      TrajectoryIR::NodeRecord{"/main/a", "tool_call", nlohmann::json::object()});
  const auto sft_custom = TrajectoryIR::to_sft_data(custom);
  REQUIRE(sft_custom["schema_version"] == "1.1");
}
