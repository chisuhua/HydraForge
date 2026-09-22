// src/common/llm/finetune_provider.h
// 功能描述：Wave 3 Phase 1 D7 serving 集成最小版 — FinetuneBaseModelProvider stub
//          (ADR-0078 D7 + design.md D7-2)
//          Phase 1 仅注册 + config 解析, generate 返回 failure "Phase 2 deferred"
//          (fail-fast, 避免 stub 误触发真实推理 — 防"看起来成功但没数据")
// 设计依据：openspec/changes/wave-3-finetune-base-model-pilot-phase1/design.md D7-2/D7-3
//          + specs/wave-3-finetune-base-model/spec.md R5
// 作者：HydraForge Wave 3 Phase 1 ship
// 最后修改日期：2026-09-23

#ifndef AGENTICDSL_LLM_FINETUNE_PROVIDER_H
#define AGENTICDSL_LLM_FINETUNE_PROVIDER_H

#include "common/llm/llm_config.h"
#include "common/llm/llm_types.h"

#include <memory>
#include <string>
#include <vector>

namespace agenticdsl {

/// Wave 3 Phase 1 fine-tune base model provider stub.
/// 注册 + config 解析, 推理逻辑延后 Wave 3 Phase 2.
class FinetuneBaseModelProvider : public ILLMProvider {
 public:
  /// 模型名 (注册 key, 与 LLMProviderFactory::register_dynamic 一致)
  static constexpr const char* kModelName = "agenticdsl-llama-3.1-70b-lora-v1";

  explicit FinetuneBaseModelProvider(LLMConfig config);

  // Phase 1 stub: 明确失败而非静默空响应 (fail-fast)
  Result<GenerationResult, LLMError> generate(
      const GenerationRequest& req, std::stop_token token) override;

  // Phase 1 无流式
  std::unique_ptr<IGenerationStream> generate_stream(
      const GenerationRequest& req, std::stop_token token) override;

  // 非空: 让路由/工具能发现该模型 (AC-6)
  std::vector<ModelInfo> available_models() const override;

 private:
  LLMConfig config_;
};

}  // namespace agenticdsl

#endif  // AGENTICDSL_LLM_FINETUNE_PROVIDER_H
