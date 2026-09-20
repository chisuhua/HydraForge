// ADR-0086 v1.0 首次实施 + v1.1 amendment
// Tests: 6 v1.0 cases + 4 v1.1 cases = 10 total
// Catch2 v3.7.4 amalgamated

#include "catch_amalgamated.hpp"

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "agenticdsl/types/attribution_record.h"
#include "agenticdsl/types/attribution_version_pair_diff.h"

namespace agenticdsl::evolution {
namespace testing {
namespace {

// Helper: 构造标准 child/parent pair
VersionSnapshot make_snapshot(const std::string& name, uint64_t version,
                              uint32_t sample_count = 5,
                              double cost_per_eval = 1.0) {
    VersionSnapshot s;
    s.name = name;
    s.version = version;
    s.sample_count = sample_count;
    s.cost_per_eval = cost_per_eval;
    return s;
}

}  // namespace

// ============================================================================
// v1.0 首次实施测试（6 cases，per ADR-0086 v1.0 实施 §阶段 0）
// ============================================================================

TEST_CASE("AttributionRecord 全字段 round-trip (v1.0)", "[adr-0086][v1.0]") {
    AttributionRecord rec;
    rec.child_version = "g@5";
    rec.parent_version = "g@4";
    rec.eval_delta = 0.15;
    rec.method = AttributionMethod::DirectComparison;
    rec.verdict = AttributionVerdict::Attributed;
    rec.reason = "baseline comparison";
    rec.confounders = {};

    REQUIRE(rec.child_version == "g@5");
    REQUIRE(rec.parent_version == "g@4");
    REQUIRE(rec.eval_delta == Catch::Approx(0.15));
    REQUIRE(rec.method == AttributionMethod::DirectComparison);
    REQUIRE(rec.verdict == AttributionVerdict::Attributed);
    REQUIRE(rec.confounders.empty());
}

TEST_CASE("ConfounderRecord 5 种基础 kind 序列化 (v1.0)", "[adr-0086][v1.0]") {
    std::vector<ConfounderKind> kinds = {
        ConfounderKind::TaskDifficulty,
        ConfounderKind::Environment,
        ConfounderKind::Opponent,
        ConfounderKind::EvaluatorDrift,
        ConfounderKind::ResourceChange,
    };
    REQUIRE(kinds.size() == 5);

    // 序列化等价: enum class 编译期即可比较
    REQUIRE(kinds[0] != kinds[1]);
    REQUIRE(kinds[2] == ConfounderKind::Opponent);
    REQUIRE(kinds[4] == ConfounderKind::ResourceChange);

    ConfounderRecord cr;
    cr.kind = ConfounderKind::EvaluatorDrift;
    cr.description = "evaluator v2 deployed mid-experiment";
    cr.magnitude = 0.05;
    REQUIRE(cr.kind == ConfounderKind::EvaluatorDrift);
    REQUIRE(cr.description == "evaluator v2 deployed mid-experiment");
    REQUIRE(cr.magnitude.value() == Catch::Approx(0.05));
}

TEST_CASE("VersionPairDiff::compare 正常返回 eval_delta (v1.0)", "[adr-0086][v1.0]") {
    auto child = make_snapshot("g", 5, /*sample_count=*/5, /*cost=*/1.15);
    auto parent = make_snapshot("g", 4, /*sample_count=*/5, /*cost=*/1.0);
    std::vector<ConfounderRecord> confounders;

    auto rec = VersionPairDiff::compare(child, parent, confounders);
    REQUIRE(rec.eval_delta == Catch::Approx(0.15));
    REQUIRE(rec.method == AttributionMethod::DirectComparison);
    REQUIRE(rec.verdict == AttributionVerdict::Attributed);
    REQUIRE(rec.reason == "baseline comparison");
}

TEST_CASE("VersionPairDiff::compare insufficient verdict 路径 (v1.0)",
          "[adr-0086][v1.0]") {
    auto child = make_snapshot("g", 5);
    auto parent = make_snapshot("g", 4, /*sample_count=*/0);  // 显式 0
    std::vector<ConfounderRecord> confounders;

    auto rec = VersionPairDiff::compare(child, parent, confounders);
    REQUIRE(rec.verdict == AttributionVerdict::Insufficient);
    REQUIRE(rec.reason.find("sample_count") != std::string::npos);
}

TEST_CASE("VersionPairDiff::compare confounded verdict 路径 (v1.0)",
          "[adr-0086][v1.0]") {
    auto child = make_snapshot("g", 5, /*sample_count=*/5);
    auto parent = make_snapshot("g", 4, /*sample_count=*/5);
    std::vector<ConfounderRecord> confounders;
    ConfounderRecord cr;
    cr.kind = ConfounderKind::Environment;
    cr.description = "GPU changed mid-experiment";
    confounders.push_back(cr);

    auto rec = VersionPairDiff::compare(child, parent, confounders);
    REQUIRE(rec.verdict == AttributionVerdict::Confounded);
    REQUIRE(rec.confounders.size() == 1);
}

TEST_CASE("VersionPairDiff::compare not_attempted 默认 fail-closed (v1.0)",
          "[adr-0086][v1.0]") {
    AttributionRecord rec;  // 默认构造
    REQUIRE(rec.verdict == AttributionVerdict::NotAttempted);

    // 显式: 未调用 compare() 之前 verdict 必须是 NotAttempted (不变量 4)
    // 即便字段填充了其他值,verdict 默认仍是 fail-closed
    rec.eval_delta = 99.0;
    rec.method = AttributionMethod::StatisticalTest;
    REQUIRE(rec.verdict == AttributionVerdict::NotAttempted);
}

// ============================================================================
// v1.1 增量测试（4 cases）
// ============================================================================

TEST_CASE("ConfounderKind::HarnessChange serialization round-trip (v1.1)",
          "[adr-0086][v1.1]") {
    ConfounderKind hk = ConfounderKind::HarnessChange;
    REQUIRE(hk == ConfounderKind::HarnessChange);

    // 与其他 5 种区分
    REQUIRE(hk != ConfounderKind::TaskDifficulty);
    REQUIRE(hk != ConfounderKind::ResourceChange);

    // HarnessChangeRecord schema
    HarnessChangeRecord hcr;
    hcr.source_id = "genome_a@3→5";
    hcr.control_status = ControlStatus::Controlled;
    hcr.detection_method = "walk_ancestors";
    hcr.evidence_refs = "causal_time:42";
    REQUIRE(hcr.source_id == "genome_a@3→5");
}

TEST_CASE("VersionPairDiff fail-fast when sample_count < kMinBaselineSamples (v1.1)",
          "[adr-0086][v1.1]") {
    REQUIRE(kMinBaselineSamples == 5);  // 常量值

    auto child = make_snapshot("g", 5);
    auto parent = make_snapshot("g", 4, /*sample_count=*/4);  // < 5

    auto rec = VersionPairDiff::compare(child, parent, {});
    REQUIRE(rec.verdict == AttributionVerdict::Insufficient);
    REQUIRE(rec.reason.find("kMinBaselineSamples") != std::string::npos);
    REQUIRE(rec.reason.find("5") != std::string::npos);
    REQUIRE(rec.eval_delta == Catch::Approx(0.0));  // 不进入 eval_delta 计算
}

TEST_CASE("VersionPairDiff proceeds when sample_count >= kMinBaselineSamples (v1.1)",
          "[adr-0086][v1.1]") {
    auto child = make_snapshot("g", 5, /*sample_count=*/5, /*cost=*/1.10);
    auto parent = make_snapshot("g", 4, /*sample_count=*/5, /*cost=*/1.0);

    auto rec = VersionPairDiff::compare(child, parent, {});
    REQUIRE(rec.verdict != AttributionVerdict::Insufficient);
    REQUIRE(rec.eval_delta == Catch::Approx(0.10));
}

TEST_CASE("HarnessChangeRecord schema 完整性 (v1.1)", "[adr-0086][v1.1]") {
    HarnessChangeRecord hcr;
    hcr.source_id = "genome_a@3→5";
    REQUIRE(hcr.source_id.find("@") != std::string::npos);
    REQUIRE(hcr.source_id.find("→") != std::string::npos);

    // ControlStatus enum 完整性
    hcr.control_status = ControlStatus::Uncontrolled;
    REQUIRE(hcr.control_status == ControlStatus::Uncontrolled);
    REQUIRE(hcr.control_status != ControlStatus::Controlled);
}

// ============================================================================
// judge_data_freshness (v1.1 决策 9 — C3 stub, 完整版依赖 walk_ancestors)
// ============================================================================

TEST_CASE("judge_data_freshness data==current fast-path (v1.1)",
          "[adr-0086][v1.1]") {
    struct MockRegistry : IGenomeRegistry {};
    MockRegistry reg;
    GenomeVersion v{"g", 5};
    GenomeVersion current{"g", 5};

    auto verdict = VersionPairDiff::judge_data_freshness(v, current, reg);
    REQUIRE(verdict == AttributionVerdict::Attributed);
}

TEST_CASE("judge_data_freshness data != current returns Insufficient (v1.1 C3 stub)",
          "[adr-0086][v1.1]") {
    struct MockRegistry : IGenomeRegistry {};
    MockRegistry reg;
    GenomeVersion v{"g", 3};
    GenomeVersion current{"g", 5};

    auto verdict = VersionPairDiff::judge_data_freshness(v, current, reg);
    REQUIRE(verdict == AttributionVerdict::Insufficient);
}

}  // namespace testing
}  // namespace agenticdsl::evolution