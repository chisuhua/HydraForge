// pdk/model_router/slm_strategy/slm_router.h
// 功能描述：SLMModelRouterPolicy — 小模型优先路由策略 (T16, ADR-0061-04)。
//          实现 agenticdsl::pdk::IModelRouter 接口。
//          路由算法:
//            1. 过滤 required_tags (与 cost/quality 一致)
//            2. 过滤 budget_remaining
//            3. 优先选择带 "fast" 或 "slm" tag 的候选
//               - 若有 SLM 候选 → 在 SLM 子集中选最便宜的
//               - 若无 SLM 候选 → fallback 到非 SLM 中最便宜的
//            4. 空结果时 throw ModelRoutingError(NoViableModel)
//          NVIDIA 2025 SLM 路由优先论文 (ADR-0061-04) 启发:
//            - 简单任务 (completion / classification) 用小模型更快更便宜
//            - 复杂任务 (reasoning / code_generation) 才升级到大模型
// 设计依据：openspec/changes/2026-08-24-adr-0061-04-slm-routing/
//           ADR-0061-04 SLM 路由优先（NVIDIA Position Paper 2025）
//           capability-application-map-2026-08.md §八 T16
// 作者：HydraForge Sprint 23 T16 ship (Phase C opportunist)
// 最后修改日期：2026-08-24

#pragma once

#include "agenticdsl/pdk/model_router.h"

#include <algorithm>
#include <string>
#include <vector>

namespace agenticdsl {
namespace pdk {

class SLMModelRouterPolicy : public IModelRouter {
 public:
  std::string name() const override { return "slm"; }

  std::string route(const RoutingContext& ctx,
                    const std::vector<ModelCapability>& candidates) override {
    // 阶段 1: 过滤 required_tags + budget (与 cost/quality 一致)
    std::vector<const ModelCapability*> viable;
    for (const auto& cap : candidates) {
      // 1.1 required_tags 过滤
      bool all_tags_present = true;
      for (const auto& required_tag : ctx.required_tags) {
        auto it = std::find(cap.tags.begin(), cap.tags.end(), required_tag);
        if (it == cap.tags.end()) {
          all_tags_present = false;
          break;
        }
      }
      if (!all_tags_present) continue;

      // 1.2 budget_remaining 过滤
      if (ctx.budget_remaining.has_value() &&
          cap.per_token_cost > ctx.budget_remaining.value()) {
        continue;
      }

      viable.push_back(&cap);
    }

    if (viable.empty()) {
      throw ModelRoutingError(
          ModelRoutingError::Code::NoViableModel,
          "no model satisfies slm/tag/budget constraints");
    }

    // 阶段 2: 分桶 — SLM 候选 vs 非 SLM 候选
    std::vector<const ModelCapability*> slm_bucket;
    std::vector<const ModelCapability*> non_slm_bucket;

    for (auto* cap : viable) {
      // SLM 判定: 带 "fast" 或 "slm" tag
      bool is_slm = false;
      for (const auto& tag : cap->tags) {
        if (tag == "fast" || tag == "slm") {
          is_slm = true;
          break;
        }
      }
      if (is_slm) {
        slm_bucket.push_back(cap);
      } else {
        non_slm_bucket.push_back(cap);
      }
    }

    // 阶段 3: 优先 SLM → 否则 fallback 非 SLM
    std::vector<const ModelCapability*>& target =
        !slm_bucket.empty() ? slm_bucket : non_slm_bucket;

    // 阶段 4: 桶内按 per_token_cost asc 排序
    std::sort(target.begin(), target.end(),
              [](const ModelCapability* a, const ModelCapability* b) {
                return a->per_token_cost < b->per_token_cost;
              });

    return target.front()->model_id;
  }
};

}  // namespace pdk
}  // namespace agenticdsl