// ADR-0074 D-3 + D-5: V3 two-stage 注入 prompt builder 声明
#pragma once

#include "prompt_builder.h"

namespace agenticdsl::prompts {

class V3TwoStagePromptBuilder : public PromptBuilder {
public:
  PromptPayload build(const std::string& user_input) const override;
  std::string version() const override { return "V3"; }
};

}  // namespace agenticdsl::prompts
