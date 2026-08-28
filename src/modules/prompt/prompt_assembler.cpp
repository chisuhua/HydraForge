// src/modules/prompt/prompt_assembler.cpp
// 功能描述：T21 两阶段 Prompt 注入实现 (ADR-0074 §决策 5)。
//          Stage 1 从 few_shots 目录加载 ≤4k tokens 的任务相关示例；
//          Stage 2 注入 ≤4k tokens 的 stdlib 子图；总 ≤8k tokens。
//          超出上限 emit `prompt.token_limit_exceeded` (owner: PromptAssembler)。
//          V1 token 估算 = chars / 4。
// 设计依据：ADR-0074 §决策 5 + openspec/changes/t21-prompt-evidence-gate/
// 作者：HydraForge Sprint 25 T21 ship
// 最后修改日期：2026-08-28

#include "agenticdsl/prompt/prompt_assembler.h"

#include "agenticdsl/contract/event_builder.h"
#include "agenticdsl/prompt/prompt_hash.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <utility>
#include <vector>

namespace fs = std::filesystem;

namespace agenticdsl {

namespace {
constexpr std::size_t kBytesPerToken = 4;  // V1 简化: 1 token ≈ 4 chars

std::string read_file(const fs::path& path) {
  std::ifstream in(path);
  return std::string((std::istreambuf_iterator<char>(in)),
                     std::istreambuf_iterator<char>());
}
}  // namespace

PromptAssembler::PromptAssembler(std::shared_ptr<IInteractionBus> bus,
                                 std::string few_shots_dir)
    : bus_(std::move(bus)), few_shots_dir_(std::move(few_shots_dir)) {}

std::size_t PromptAssembler::estimate_tokens(const std::string& text) {
  return text.size() / kBytesPerToken;
}

std::string PromptAssembler::load_few_shots(std::size_t max_chars) const {
  std::string out;
  std::vector<fs::path> files;
  if (fs::is_directory(few_shots_dir_)) {
    for (const auto& entry : fs::directory_iterator(few_shots_dir_)) {
      if (entry.path().extension() == ".md") files.push_back(entry.path());
    }
  }
  std::sort(files.begin(), files.end());
  for (const auto& path : files) {
    if (out.size() >= max_chars) break;
    out += read_file(path);
    out += "\n";
  }
  if (out.size() > max_chars) out.resize(max_chars);
  return out;
}

AssembledPrompt PromptAssembler::assemble(const std::string& task,
                                          const std::string& stdlib_text) {
  AssembledPrompt p;
  p.stage1_few_shots = load_few_shots(kStageTokenLimit * kBytesPerToken);
  p.stage2_stdlib = stdlib_text;
  if (p.stage2_stdlib.size() > kStageTokenLimit * kBytesPerToken) {
    p.stage2_stdlib.resize(kStageTokenLimit * kBytesPerToken);
  }
  p.task_specific = task;
  p.estimated_tokens_total = estimate_tokens(p.stage1_few_shots) +
                             estimate_tokens(p.stage2_stdlib) +
                             estimate_tokens(p.task_specific);
  p.token_limit_exceeded = p.estimated_tokens_total > kTotalTokenLimit;
  if (p.token_limit_exceeded && bus_) {
    emit_token_limit_exceeded(p.task_specific, p.estimated_tokens_total);
  }
  return p;
}

void PromptAssembler::emit_token_limit_exceeded(const std::string& prompt,
                                                std::size_t estimated) {
  const BusEvent event =
      EventBuilder("prompt.token_limit_exceeded")
          .args(nlohmann::json{{"prompt_hash", hash_prompt(prompt)},
                               {"prompt_length", static_cast<int>(prompt.size())},
                               {"token_estimate", estimate_tokens(prompt)},
                               {"stage", "total"},
                               {"actual_tokens", static_cast<int>(estimated)}})
          .build();
  bus_->emit(event);
}

}  // namespace agenticdsl