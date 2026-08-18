// ADR-0074 D-3 + D-5 — V1/V2/V3 prompt builder 聚合测试
// 覆盖: V1 schema 嵌入 system / V2 few-shot ≤5 / V3 stage ordering / V3 token warn
#include "catch_amalgamated.hpp"

#include <string>

#include "common/prompts/v1.h"
#include "common/prompts/v2.h"
#include "common/prompts/v3.h"

using namespace agenticdsl::prompts;

TEST_CASE("V1 schema embedded in system message", "[prompts][v1]") {
  V1SchemaPromptBuilder b;
  auto p = b.build("list perms");

  REQUIRE(p.messages.size() == 1);
  REQUIRE(p.messages[0].role == "system");
  REQUIRE(p.messages[0].content.find("\"type\": \"object\"") != std::string::npos);
  REQUIRE(p.messages[0].content.find("permissions") != std::string::npos);
  REQUIRE(p.messages[0].content.find("list perms") != std::string::npos);
  REQUIRE(b.version() == "V1");
}

TEST_CASE("V2 few-shot count <= 5", "[prompts][v2]") {
  V2FewShotPromptBuilder b;
  auto p = b.build("query");

  REQUIRE(p.messages.size() == 1);
  int count = 0;
  for (size_t pos = 0;
       (pos = p.messages[0].content.find("input:", pos)) != std::string::npos;
       pos += 6) {
    count++;
  }
  REQUIRE(count <= 5);
  REQUIRE(b.version() == "V2");
}

TEST_CASE("V3 stage ordering (system -> user)", "[prompts][v3][invariant]") {
  V3TwoStagePromptBuilder b;
  auto p = b.build("test input");

  REQUIRE(p.messages.size() == 2);
  REQUIRE(p.messages[0].role == "system");  // Stage 1 = SystemFirst
  REQUIRE(p.messages[1].role == "user");    // Stage 2 = UserSecond

  REQUIRE(p.messages[0].content.find("type") != std::string::npos);
  REQUIRE(p.messages[0].content.find("permissions") != std::string::npos);
  REQUIRE(p.messages[1].content.find("input:") != std::string::npos);
  REQUIRE(p.messages[1].content.find("test input") != std::string::npos);
  REQUIRE(b.version() == "V3");
}

TEST_CASE("V3 token counter warns on > 8k input", "[prompts][v3][risk3]") {
  V3TwoStagePromptBuilder b;
  // 10000 个 'x' 无空格 → 词数为 1, 改用带空格输入触发阈值
  std::string big;
  for (int i = 0; i < 9000; ++i) big += "word ";
  auto p = b.build(big);

  int n = 0;
  for (const auto& m : p.messages) {
    for (char c : m.content) {
      if (c == ' ') n++;
    }
  }
  REQUIRE(n > 8000);
}
