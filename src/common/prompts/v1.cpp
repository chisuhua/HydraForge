// ADR-0074 D-3 + ADR-0073 ToolMetadata V3 schema — V1 schema constraint 实现
#include "v1.h"

namespace agenticdsl::prompts {

PromptPayload V1SchemaPromptBuilder::build(const std::string& user_input) const {
  PromptPayload p;
  std::string schema = R"({
  "type": "object",
  "properties": {
    "permissions": {
      "type": "array",
      "items": { "type": "string" }
    }
  },
  "required": ["permissions"]
})";
  std::string system = std::string("You MUST output valid JSON matching this schema:\n") + schema
                     + "\n\nUser request: " + user_input;
  p.add_system(system);
  return p;
}

}  // namespace agenticdsl::prompts
