// include/agenticdsl/ir/trajectory_ir.h
// 功能描述：TrajectoryIR — ParsedGraph 的独立序列化视图 (T15, ADR-0061-06 v1.1)
//          三级 IR: RawIR (text-near) / ParsedIR (结构化) / CanonicalIR (pass 输出)
//          单向 Converter: from_parsed_graph() (ParsedGraph → ParsedIR 快照)
//          V1 Backends: to_sft_data() / to_otel_spans()
//          V1 Pass 占位: ConstantFoldingPass (pass-through)
// 设计依据：docs/adr/skill/adr-0061-06-trajectory-ir.md
//          + docs/adr/skill/adr-0061-06-v1-1-amendment-trajectory-ir-decouple.md
//          + openspec/changes/t15-trajectory-ir/specs/trajectory-ir/spec.md
// 关键不变量：
//   - ParsedGraph 零修改 (前向声明, 不 #include core/types/node.h)
//   - TrajectoryIR 与 ParsedGraph 无继承/耦合关系 (独立类)
//   - V2 延后: to_rl_data / to_eval_data / 跨框架 frontend / 完整 pass pipeline
// 作者：HydraForge Sprint 24 T15 ship
// 最后修改日期：2026-08-27
#pragma once

#include <cstdint>
#include <sstream>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

namespace agenticdsl {
struct ParsedGraph;  // 前向声明 — Converter 声明所需最小引用 (不耦合定义)
}  // namespace agenticdsl

namespace agenticdsl::ir {

// ============================================================================
// TrajectoryIR — 轨迹中间表示 (独立序列化视图)
// 不变量 (ADR-0061-06 v1.1 §决策 2):
//   - 独立类, 与 ParsedGraph 无继承关系
//   - schema 演化独立于 L0 运行时
//   - 蒸馏工具消费 TrajectoryIR (安全), 不影响 L0 运行时
// ============================================================================
class TrajectoryIR {
 public:
  // IR 层级枚举
  enum class IRLevel : uint8_t { RawIR = 0, ParsedIR = 1, CanonicalIR = 2 };

  // RawIR — text-near DSL 表示
  struct RawIR {
    std::string dsl_text;
    nlohmann::json metadata = nlohmann::json::object();
  };

  // NodeRecord — 节点记录 (值类型, type=字符串无 enum 依赖)
  struct NodeRecord {
    std::string id;  // NodePath, e.g. "/main/step1"
    std::string type;  // NodeType 字符串, e.g. "start" / "tool_call"
    nlohmann::json metadata = nlohmann::json::object();
  };

  // EdgeRecord — 边记录 (weight=1.0 V1 简化)
  struct EdgeRecord {
    std::string from;
    std::string to;
    double weight = 1.0;
  };

  // StepRecord — 步骤记录 (V1 占位; V2 集成 ADR-0061-13 DistillationRecord.reward)
  struct StepRecord {
    std::string node_id;
    nlohmann::json metadata = nlohmann::json::object();
  };

  // ParsedIR — 结构化 JSON (Converter 输出)
  struct ParsedIR {
    std::vector<NodeRecord> nodes;
    std::vector<EdgeRecord> edges;
    std::vector<StepRecord> steps;
  };

  // CanonicalIR — pass pipeline 输出, backends 输入
  // V2 扩展: pass pipeline (ConstantFolding + DeadCodeElim + LoopUnroll)
  struct CanonicalIR {
    std::vector<NodeRecord> canonical_nodes;
    std::vector<EdgeRecord> canonical_edges;
    std::vector<StepRecord> canonical_steps;
    nlohmann::json metadata = nlohmann::json::object();
  };

  // 单向 Converter: ParsedGraph → ParsedIR 快照 (值类型浅拷贝)
  // 实现: src/core/parsed_graph_to_trajectory_ir.cpp
  static ParsedIR from_parsed_graph(const ParsedGraph& pg);

  // V1 Backend: CanonicalIR → SFT 训练数据 JSON
  // (含 nodes/edges/steps/sft_metadata 字段)
  // 实现: src/modules/ir/trajectory_ir_backend.cpp
  static nlohmann::json to_sft_data(const CanonicalIR& canonical);

  // V1 Backend: CanonicalIR → OTLP spans JSON
  // (含 trace_id/span_id/parent_span_id/timestamps)
  // 实现: src/modules/ir/trajectory_ir_backend.cpp
  static nlohmann::json to_otel_spans(const CanonicalIR& canonical);

  // CanonicalIR 确定性 hash (header-inline, 供 SkillCompiler 集成零链接依赖)
  // 序列化: 遍历 nodes/edges/steps 拼接确定性字符串 → std::hash 十六进制
  static std::string hash(const CanonicalIR& canonical) {
    std::string buf;
    buf.reserve(256);
    for (const auto& n : canonical.canonical_nodes) {
      buf += n.id;
      buf += '\x1f';
      buf += n.type;
      buf += '\x1f';
      buf += n.metadata.dump();
      buf += '\x1e';
    }
    buf += '\x1d';
    for (const auto& e : canonical.canonical_edges) {
      buf += e.from;
      buf += '\x1f';
      buf += e.to;
      buf += '\x1f';
      buf += std::to_string(e.weight);
      buf += '\x1e';
    }
    buf += '\x1d';
    for (const auto& s : canonical.canonical_steps) {
      buf += s.node_id;
      buf += '\x1f';
      buf += s.metadata.dump();
      buf += '\x1e';
    }
    buf += '\x1d';
    buf += canonical.metadata.dump();
    std::ostringstream oss;
    oss << std::hex << std::hash<std::string>{}(buf);
    return oss.str();
  }
};

// ============================================================================
// ConstantFoldingPass — V1 占位 (pass-through: 输入输出等价)
// V2 扩展: 常量折叠 + 死代码消除
// 实现: src/modules/ir/trajectory_ir_pass.cpp
// ============================================================================
class ConstantFoldingPass {
 public:
  static TrajectoryIR::CanonicalIR run(const TrajectoryIR::CanonicalIR& input);
};

}  // namespace agenticdsl::ir
