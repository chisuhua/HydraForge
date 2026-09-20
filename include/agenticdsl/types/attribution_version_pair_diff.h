// ADR-0086 v1.0 首次实施 + v1.1 amendment
// VersionPairDiff — compare two versions + 数据新鲜度判定
//
// v1.0 决策 2: compare() 算法 (sample_count 检查集成 v1.1)
// v1.1 决策 9: judge_data_freshness 完整版算法 (C3 stub, 当前返回 Incomplete)

#pragma once

#include "agenticdsl/types/attribution_record.h"

namespace agenticdsl::evolution {

class VersionPairDiff {
public:
    // v1.0 决策 2 算法 + v1.1 集成 sample_count fail-fast (决策 2 v1.1)
    static AttributionRecord compare(
        const VersionSnapshot& child,
        const VersionSnapshot& parent,
        const std::vector<ConfounderRecord>& confounders);

// v1.1 决策 9 数据新鲜度判定 (完整版算法 per proposal §1 决策 9)
// 当前 stub: 因 C3 walk_ancestors 未 ship, 完整 lineage walk 不可用
// 返回 Insufficient (C3 实装后启用 fast-path + lineage + harness 对比)
// v1.1 amendment (Critical C1): signature 改 ::agenticdsl::genome::IGenomeRegistry&
static AttributionVerdict judge_data_freshness(
    const GenomeVersion& v,
    const GenomeVersion& current,
    ::agenticdsl::genome::IGenomeRegistry& registry);
};

}  // namespace agenticdsl::evolution