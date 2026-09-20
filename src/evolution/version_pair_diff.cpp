// ADR-0086 v1.0 首次实施 + v1.1 amendment
// version_pair_diff.cpp — VersionPairDiff 算法实现
//
// v1.0 决策 2 算法: compare() 含 sample_count 前置 fail-fast (v1.1 决策 2)
// v1.1 决策 9: judge_data_freshness stub (C3 walk_ancestors 依赖)

#include "agenticdsl/types/attribution_version_pair_diff.h"

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
    const GenomeVersion& v, const GenomeVersion& current, IGenomeRegistry& registry) {
    (void)registry;  // C3 实装 walk_ancestors 后启用
    // Fast-path (决策 9 修正 excludes-self 假阳性):
    // data.version == current.version → 直接 Attributed
    if (v.name == current.name && v.version == current.version) {
        return AttributionVerdict::Attributed;
    }
    // C3 stub: walk_ancestors 不可用 → Insufficient 防止假阴性
    return AttributionVerdict::Insufficient;
}

}  // namespace agenticdsl::evolution