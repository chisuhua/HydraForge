// ADR-0074 D-3: V1 schema constraint prompt builder 声明
#pragma once

#include "prompt_builder.h"

namespace agenticdsl::prompts {

class V1SchemaPromptBuilder : public PromptBuilder {
public:
  PromptPayload build(const std::string& user_input) const override;
  std::string version() const override { return "V1"; }
};

}  // namespace agenticdsl::prompts
