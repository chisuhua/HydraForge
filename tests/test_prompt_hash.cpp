// tests/test_prompt_hash.cpp
// 功能描述：T21 payload redact — Prompt Hash Helper 测试套件 (Phase 0, T0)
//          hash_prompt(): 不可逆 64-bit hash (16 hex chars), 用于事件 payload 脱敏
//          estimate_tokens(): V1 简化 token 估算 = chars / 4 (round-up)
// 设计依据：openspec/changes/t21-payload-redact/tasks.md Phase 0 (T0.1-T0.6)
//          + ADR-0080 D10 PII 约束 (G11 mutation.* hash 范式)
// 作者：HydraForge Sprint 25 T21 payload redact
// 最后修改日期：2026-08-28

#include "catch_amalgamated.hpp"

#include "agenticdsl/prompt/prompt_hash.h"

#include <string>

using namespace agenticdsl;

// T0-1: 确定性 — 相同输入 → 相同 hash
TEST_CASE("hash_same_input_same_output", "[prompt][t0]") {
  const std::string a = hash_prompt("SELECT * FROM users WHERE id=1");
  const std::string b = hash_prompt("SELECT * FROM users WHERE id=1");
  REQUIRE(a == b);
  REQUIRE_FALSE(a.empty());
}

// T0-2: 唯一性 — 不同输入 → 不同 hash
TEST_CASE("hash_different_input_different_output", "[prompt][t0]") {
  const std::string a = hash_prompt("api_key=sk-1234567890abcdef");
  const std::string b = hash_prompt("api_key=sk-abcdef1234567890");
  REQUIRE(a != b);
}

// T0-3: 边界 — 空字符串返回合法 16 hex hash
TEST_CASE("hash_empty_string_returns_valid_hex", "[prompt][t0]") {
  const std::string h = hash_prompt("");
  REQUIRE(h.size() == 16);
  // 全部字符必须是 hex digit
  for (const char c : h) {
    REQUIRE(((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') ||
             (c >= 'A' && c <= 'F')));
  }
}

// T0-4: 性能/长输入 — 长 prompt 正确处理且产生 16 hex hash
TEST_CASE("hash_long_input_handles_correctly", "[prompt][t0]") {
  std::string long_prompt;
  long_prompt.reserve(100000);
  for (int i = 0; i < 10000; ++i) {
    long_prompt += "用户输入需要脱敏的敏感内容片段 " + std::to_string(i) + " ";
  }
  const std::string h = hash_prompt(long_prompt);
  REQUIRE(h.size() == 16);
  // 长输入 hash 确定性
  REQUIRE(h == hash_prompt(long_prompt));
}

// T0-5: V1 简化算法 — chars / 4 (round-up)
TEST_CASE("estimate_tokens_chars_div_4", "[prompt][t0]") {
  REQUIRE(estimate_tokens("abcd") == 1);       // 4 chars / 4 = 1
  REQUIRE(estimate_tokens("abcdefgh") == 2);   // 8 chars / 4 = 2
  REQUIRE(estimate_tokens("abcde") == 2);      // 5 chars → round-up = 2
}

// T0-6: 边界 — 空字符串返回 0
TEST_CASE("estimate_tokens_empty_string_returns_zero", "[prompt][t0]") {
  REQUIRE(estimate_tokens("") == 0);
}
