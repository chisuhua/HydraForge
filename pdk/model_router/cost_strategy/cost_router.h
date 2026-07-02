// pdk/model_router/cost_strategy/cost_router.h
// 功能描述：CostModelRouterPolicy — 成本优先模型路由策略 (C7 Phase 1 MVP)。
//          实现 agenticdsl::pdk::IModelRouter 接口。
//          路由算法:
//            1. 过滤 required_tags: 所有 tag 必须在 model.tags 中
//            2. 过滤 budget_remaining: per_token_cost ≤ budget (若未设 budget 则跳过)
//            3. 排序 per_token_cost asc → 返回最便宜的模型
//            4. 空结果时 throw ModelRoutingError(NoViableModel)
// 设计依据：openspec/changes/2026-06-26-adr-0034-model-router-plugin/
//           specs/model-router-plugin/spec.md — cost-strategy-end-to-end requirement
// 作者：C7 Phase 1 MVP
// 最后修改日期：2026-07-02

#pragma once

#include "agenticdsl/pdk/model_router.h"

#include <algorithm>
#include <string>
#include <vector>

namespace agenticdsl {
namespace pdk {

class CostModelRouterPolicy : public IModelRouter {
public:
  std::string name() const override { return "cost"; }

  std::string route(const RoutingContext& ctx,
                    const std::vector<ModelCapability>& candidates) override {
    std::vector<const ModelCapability*> viable;

    for (const auto& cap : candidates) {
      // 1. 过滤 required_tags: 所有 tag 必须在 model.tags 中
      bool all_tags_present = true;
      for (const auto& required_tag : ctx.required_tags) {
        auto it = std::find(cap.tags.begin(), cap.tags.end(), required_tag);
        if (it == cap.tags.end()) {
          all_tags_present = false;
          break;
        }
      }
      if (!all_tags_present) continue;

      // 2. 过滤 budget_remaining
      if (ctx.budget_remaining.has_value() &&
          cap.per_token_cost > ctx.budget_remaining.value()) {
        continue;
      }

      viable.push_back(&cap);
    }

    // 3. 空结果 → throw
    if (viable.empty()) {
      throw ModelRoutingError(
          ModelRoutingError::Code::NoViableModel,
          "no model satisfies cost/tag constraints");
    }

    // 4. 排序 per_token_cost asc, 返回最便宜的
    std::sort(viable.begin(), viable.end(),
              [](const ModelCapability* a, const ModelCapability* b) {
                return a->per_token_cost < b->per_token_cost;
              });

    return viable.front()->model_id;
  }
};

} // namespace pdk
} // namespace agenticdsl
