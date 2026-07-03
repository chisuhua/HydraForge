// pdk/model_router/quality_strategy/quality_router.h
// 功能描述：QualityModelRouterPolicy — 质量优先模型路由策略 (C7 Phase 2)。
//          实现 agenticdsl::pdk::IModelRouter 接口。

#pragma once

#include "agenticdsl/pdk/model_router.h"

#include <algorithm>
#include <cstddef>
#include <string>
#include <vector>

namespace agenticdsl {
namespace pdk {

class QualityModelRouterPolicy : public IModelRouter {
public:
  std::string name() const override { return "quality"; }

  std::string route(const RoutingContext& ctx,
                    const std::vector<ModelCapability>& candidates) override {
    if (candidates.empty()) {
      throw ModelRoutingError(
          ModelRoutingError::Code::NoViableModel,
          "no candidates provided to quality router");
    }

    // Empty required_tags: 按 n_ctx + max_tokens 总分排序
    if (ctx.required_tags.empty()) {
      std::vector<const ModelCapability*> sorted;
      for (const auto& cap : candidates) sorted.push_back(&cap);
      std::sort(sorted.begin(), sorted.end(),
                [](const ModelCapability* a, const ModelCapability* b) {
                  return (a->n_ctx + a->max_tokens) >
                         (b->n_ctx + b->max_tokens);
                });
      return sorted.front()->model_id;
    }

    // 有 required_tags: 按匹配度计分
    std::vector<std::pair<const ModelCapability*, int>> scored;
    for (const auto& cap : candidates) {
      int match_count = 0;
      for (const auto& required_tag : ctx.required_tags) {
        if (std::find(cap.tags.begin(), cap.tags.end(), required_tag)
            != cap.tags.end()) {
          ++match_count;
        }
      }
      scored.emplace_back(&cap, match_count);
    }

    // 降序排列
    std::sort(scored.begin(), scored.end(),
              [](const auto& a, const auto& b) {
                return a.second > b.second;
              });

    // 所有分数 = 0 → fallback candidates[0]
    if (scored.front().second == 0) {
      return candidates[0].model_id;
    }

    return scored.front().first->model_id;
  }
};

} // namespace pdk
} // namespace agenticdsl