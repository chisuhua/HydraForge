// ADR-0086 v1.0 首次实施 + v1.1 amendment
// version_pair_diff.cpp — VersionPairDiff 算法实现
//
// v1.0 决策 2 算法: compare() 含 sample_count 前置 fail-fast (v1.1 决策 2)
// v1.1 决策 9: judge_data_freshness stub (C3 walk_ancestors 依赖)

#include "agenticdsl/types/attribution_version_pair_diff.h"
#include "agenticdsl/genome/genome.h"

#include <string>

namespace agenticdsl::evolution {

AttributionRecord VersionPairDiff::compare(
    const VersionSnapshot& child,
    const VersionSnapshot& parent,
    const std::vector<ConfounderRecord>& confounders) {
    AttributionRecord rec;
    rec.child_version = child.name + "@" + std::to_string(child.version);
    rec.parent_version = parent.name + "@" + std::to_string(parent.version);
    rec.confounders = confounders;

    // v1.1 fail-fast (决策 2 v1.1): sample_count < kMinBaselineSamples → Insufficient
    if (parent.sample_count < kMinBaselineSamples) {
        rec.verdict = AttributionVerdict::Insufficient;
        rec.reason = "baseline sample_count " + std::to_string(parent.sample_count) +
                     " < kMinBaselineSamples (5); single baseline cannot estimate stddev";
        rec.eval_delta = 0.0;
        return rec;
    }

    // v1.0 简化: 直接 cost 差计算 eval_delta
    rec.eval_delta = child.cost_per_eval - parent.cost_per_eval;
    rec.method = AttributionMethod::DirectComparison;
    rec.verdict = confounders.empty() ? AttributionVerdict::Attributed
                                      : AttributionVerdict::Confounded;
    rec.reason = confounders.empty() ? "baseline comparison" : "confounders detected";
    return rec;
}

AttributionVerdict VersionPairDiff::judge_data_freshness(
    const GenomeVersion& v, const GenomeVersion& current,
    ::agenticdsl::genome::IGenomeRegistry& registry) {
    // Case 1 (fast-path per ADR-0086 step 1):
    // data.name == current.name && data.version == current.version → 直接 Attributed
    if (v.name == current.name && v.version == current.version) {
        return AttributionVerdict::Attributed;
    }

    // Case 2 (AC-8 cross-name rejection): v1 仅支持同名 lineage
    if (v.name != current.name) {
        return AttributionVerdict::Confounded;
    }

    // Call walk_ancestors (current.name, current.version) — closest-first, self-inclusive
    auto walk_res = registry.walk_ancestors(current.name, current.version);
    if (!walk_res.has_value()) {
        // Case 6 (Critical C2 fail-closed): walk 失败 → Insufficient
        // 任何 GenomeError (NotImplemented/NotFound/BrokenLineage/IOError) 都走此路径
        return AttributionVerdict::Insufficient;
    }
    const auto& walk = walk_res.value();

    // Case 3 (AC-8 (name, version) 对定位, NOT bare version):
    // 在 walk.intermediate_metadata 中查找 (v.name, v.version)
    int64_t data_idx = -1;
    for (size_t i = 0; i < walk.intermediate_metadata.size(); ++i) {
        const auto& g = walk.intermediate_metadata[i];
        if (g.metadata.name == v.name && g.metadata.version == v.version) {
            data_idx = static_cast<int64_t>(i);
            break;
        }
    }
    if (data_idx < 0) {
        // data.version 不在 lineage 中
        return AttributionVerdict::Confounded;
    }

    // Case 4 (harness changed after data generation):
    // 在 closest-first 顺序中, data_idx 之后 (索引更小) 的所有版本若 spec.harness 变更 → Confounded
    const std::string& data_harness = walk.intermediate_metadata[data_idx].spec.harness;
    for (int64_t i = data_idx - 1; i >= 0; --i) {
        if (walk.intermediate_metadata[i].spec.harness != data_harness) {
            return AttributionVerdict::Confounded;
        }
    }

    // Case 5 (in lineage, no harness change) → Attributed
    return AttributionVerdict::Attributed;
}

}  // namespace agenticdsl::evolution