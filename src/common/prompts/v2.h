// ADR-0074 D-3: V2 few-shot 注入 prompt builder 声明
#pragma once

#include "prompt_builder.h"

namespace agenticdsl::prompts {

class V2FewShotPromptBuilder : public PromptBuilder {
public:
  PromptPayload build(const std::string& user_input) const override;
  std::string version() const override { return "V2"; }
};

}  // namespace agenticdsl::prompts
