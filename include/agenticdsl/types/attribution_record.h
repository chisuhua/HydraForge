// ADR-0086 v1.0 首次实施 + v1.1 amendment
// Attribution Record types (信用归因记录类型)
//
// v1.0: 5 基础类型（AttributionRecord / AttributionMethod / AttributionVerdict /
//                       ConfounderRecord / VersionSnapshot）
// v1.1: + ConfounderKind::HarnessChange + HarnessChangeRecord + ControlStatus
//
// 路径: include/agenticdsl/types/ (v1.0 不变量 6 严格遵守)
// namespace: agenticdsl::evolution (修正 Oracle 🟠-3 统一)

#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace agenticdsl::evolution {

// ============================================================================
// v1.1 决策 4: HarnessChangeRecord schema (5 字段) — 必须先定义
// (因 ConfounderRecord::harness_change 字段需 std::optional<HarnessChangeRecord>)
// ============================================================================

enum class ControlStatus {
    Controlled,
    Uncontrolled
};

struct HarnessChangeRecord {
    std::string source_id;            // 格式: "name@old_version→new_version" (不变量 9)
    ControlStatus control_status;
    std::string detection_method;     // 固定 "walk_ancestors"
    std::string evidence_refs;        // causal_time 引用 (ADR-0080)
};

// ============================================================================
// v1.0 决策 1: 5 基础类型
// ============================================================================

enum class AttributionMethod {
    DirectComparison,
    StatisticalTest,
    CounterfactualAnalysis,
    ExpertJudgment
};

enum class AttributionVerdict {
    NotAttempted,    // 默认 fail-closed (v1.0 不变量 4)
    Attributed,
    Confounded,
    Insufficient
};

enum class ConfounderKind {
    TaskDifficulty,
    Environment,
    Opponent,
    EvaluatorDrift,
    ResourceChange,
    // v1.1 新增 (修正决策 4): Genome 版本漂移（旧 Harness 数据归因到新 Harness）
    HarnessChange
};

struct ConfounderRecord {
    ConfounderKind kind;
    std::string description;
    std::optional<double> magnitude;
    // v1.1 新增 optional 字段 (决策 4 schema 扩展)
    std::optional<HarnessChangeRecord> harness_change;
};

struct VersionSnapshot {
    std::string name;
    uint64_t version = 0;
    uint32_t sample_count = 0;     // v1.1 决策 2: 用于 fail-fast 检查
    double cost_per_eval = 0.0;
};

struct AttributionRecord {
    std::string child_version;
    std::string parent_version;
    double eval_delta = 0.0;
    AttributionMethod method = AttributionMethod::DirectComparison;
    AttributionVerdict verdict = AttributionVerdict::NotAttempted;  // fail-closed 默认
    std::string reason;
    std::vector<ConfounderRecord> confounders;
};

// ============================================================================
// v1.1 决策 2: kMinBaselineSamples=5 (Hotelling T² 经验值, T14 验证)
// ============================================================================

constexpr uint32_t kMinBaselineSamples = 5;

// ============================================================================
// v1.1 决策 9: GenomeVersion struct (单一所有权, C3 复用避免 ODR)
// ============================================================================

struct GenomeVersion {
    std::string name;
    uint64_t version;
};

}  // namespace agenticdsl::evolution

// IGenomeRegistry forward declaration in global agenticdsl::genome (C3 Critical C1)
// Avoids nested namespace resolution when included from agenticdsl::evolution::testing.
namespace agenticdsl::genome { class IGenomeRegistry; }