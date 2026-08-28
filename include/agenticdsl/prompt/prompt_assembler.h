// include/agenticdsl/prompt/prompt_assembler.h
// 功能描述：T21 两阶段 Prompt 注入器 (ADR-0074 §决策 5)
//          Stage 1: task-specific few-shots (≤4k tokens)
//          Stage 2: stdlib subgraphs (≤4k tokens, 总 ≤8k)
//          超出 token 上限 → emit `prompt.token_limit_exceeded` 事件。
//          V1 token 估算 = chars / 4 (V2 在线精确计数)
//
// 设计依据：ADR-0074 §决策 5 + openspec/changes/t21-prompt-evidence-gate/
// 作者：HydraForge Sprint 25 T21 ship
// 最后修改日期：2026-08-28
#ifndef AGENTICDSL_PROMPT_PROMPT_ASSEMBLER_H
#define AGENTICDSL_PROMPT_PROMPT_ASSEMBLER_H

#include "agenticdsl/contract/iinteraction_bus.h"

#include <cstddef>
#include <memory>
#include <string>

namespace agenticdsl {

struct AssembledPrompt {
  std::string stage1_few_shots;     // 任务相关 few-shots (≤4k tokens)
  std::string stage2_stdlib;        // stdlib 子图选择 (≤4k tokens)
  std::string task_specific;        // 用户任务原文
  std::size_t estimated_tokens_total = 0;
  bool token_limit_exceeded = false;
};

class PromptAssembler {
 public:
  static constexpr std::size_t kStageTokenLimit = 4000;
  static constexpr std::size_t kTotalTokenLimit = 8000;

  explicit PromptAssembler(std::shared_ptr<IInteractionBus> bus = nullptr,
                           std::string few_shots_dir = "lib/prompt/few_shots");

  AssembledPrompt assemble(const std::string& task,
                           const std::string& stdlib_text);

  static std::size_t estimate_tokens(const std::string& text);  // chars / 4

 private:
  void emit_token_limit_exceeded(const std::string& prompt,
                                 std::size_t estimated);
  std::string load_few_shots(std::size_t max_chars) const;

  std::shared_ptr<IInteractionBus> bus_;
  std::string few_shots_dir_;
};

}  // namespace agenticdsl

#endif  // AGENTICDSL_PROMPT_PROMPT_ASSEMBLER_H