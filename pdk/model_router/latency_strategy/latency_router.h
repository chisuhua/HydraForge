// pdk/model_router/latency_strategy/latency_router.h
// 功能描述：LatencyModelRouterPolicy — 延迟优先模型路由策略 (C7 Phase 2)。
//          实现 agenticdsl::pdk::IModelRouter 接口。
//          路由算法:
//            1. 过滤 required_tags (所有 tag 必须在 model.tags 中)
//            2. 过滤 max_latency (若从 RoutingContext budget_remaining 字段设限)
//            3. 排序 avg_latency_ms asc → 返回最低延迟模型
//            4. 空结果时 throw ModelRoutingError(NoViableModel)
// 设计依据：openspec/changes/2026-06-26-adr-0034-model-router-plugin/
//            specs/model-router-plugin/spec.md — latency-strategy-end-to-end requirement
// 作者：C7 Phase 2

#pragma once

#include "agenticdsl/pdk/model_router.h"

#include <algorithm>
#include <limits>
#include <string>
#include <vector>

namespace agenticdsl {
namespace pdk {

class LatencyModelRouterPolicy : public IModelRouter {
public:
  std::string name() const override { return "latency"; }

  std::string route(const RoutingContext& ctx,
                    const std::vector<ModelCapability>& candidates) override {
    std::vector<const ModelCapability*> viable;

    for (const auto& cap : candidates) {
      // 1. 过滤 required_tags: 所有 tag 必须在 model.tags 中
      bool all_tags_present = true;
      for (const auto& required_tag : ctx.required_tags) {
        if (std::find(cap.tags.begin(), cap.tags.end(), required_tag)
            == cap.tags.end()) {
          all_tags_present = false;
          break;
        }
      }
      if (!all_tags_present) continue;

      // 2. 过滤 max_latency (若 budget_remaining 被设限, 复用此字段作为 max_latency)
      if (ctx.budget_remaining.has_value()) {
        int max_latency = static_cast<int>(ctx.budget_remaining.value());
        if (cap.avg_latency_ms > max_latency) continue;
      }

      viable.push_back(&cap);
    }

    // 3. 空结果 → throw
    if (viable.empty()) {
      throw ModelRoutingError(
          ModelRoutingError::Code::NoViableModel,
          "no model satisfies latency/tag constraints");
    }

    // 4. 排序 avg_latency_ms asc, 返回最低延迟
    std::sort(viable.begin(), viable.end(),
              [](const ModelCapability* a, const ModelCapability* b) {
                return a->avg_latency_ms < b->avg_latency_ms;
              });

    return viable.front()->model_id;
  }
};

} // namespace pdk
} // namespace agenticdsl