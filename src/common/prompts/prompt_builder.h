// ADR-0074 D-3: V1/V2/V3 prompt builders 抽象接口
#pragma once

#include <string>
#include <utility>
#include <vector>

namespace agenticdsl::prompts {

enum class PromptStage {
  SystemFirst,
  UserSecond
};

struct PromptMessage {
  std::string role;  // "system" or "user"
  std::string content;
};

struct PromptPayload {
  std::vector<PromptMessage> messages;

  void add_system(const std::string& content) {
    PromptMessage m{"system", content};
    messages.push_back(std::move(m));
  }
  void add_user(const std::string& content) {
    PromptMessage m{"user", content};
    messages.push_back(std::move(m));
  }
};

class PromptBuilder {
public:
  virtual ~PromptBuilder() = default;

  virtual PromptPayload build(const std::string& user_input) const = 0;

  virtual std::string version() const = 0;
};

}  // namespace agenticdsl::prompts
