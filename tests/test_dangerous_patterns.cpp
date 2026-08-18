// tests/test_dangerous_patterns.cpp
// 功能描述：ADR-0073 D3 — DangerousPatterns OWASP 命令注入黑名单单测
// 作者：from-roadmap-phase-6c-schema-complete change (Batch 1 Task 2)
// 最后修改日期：2026-08-18
#include "catch_amalgamated.hpp"
#include "common/policy/dangerous_patterns.h"

using namespace agenticdsl::policy;

TEST_CASE("rm -rf detected", "[dangerous-patterns]") {
  REQUIRE(DangerousPatterns::contains_dangerous("rm -rf /tmp/build") == true);
  REQUIRE(DangerousPatterns::contains_dangerous("echo hello") == false);
  REQUIRE(DangerousPatterns::first_match("rm -rf /tmp/build") == "rm -rf");
}

TEST_CASE("mkfs detected", "[dangerous-patterns]") {
  REQUIRE(DangerousPatterns::contains_dangerous("mkfs.ext4 /dev/sda") == true);
  REQUIRE(DangerousPatterns::first_match("mkfs.ext4 /dev/sda") == "mkfs");
}

TEST_CASE("fork bomb / dd / block-device write detected", "[dangerous-patterns]") {
  REQUIRE(DangerousPatterns::contains_dangerous(":(){ :|:& };:") == true);
  REQUIRE(DangerousPatterns::contains_dangerous("dd if=/dev/zero of=/dev/sda") == true);
  REQUIRE(DangerousPatterns::contains_dangerous("echo x >/dev/sda") == true);
}

TEST_CASE("case-insensitive detection", "[dangerous-patterns]") {
  REQUIRE(DangerousPatterns::contains_dangerous("RM -RF /") == true);
  REQUIRE(DangerousPatterns::contains_dangerous("Mkfs /dev/sdb") == true);
  REQUIRE(DangerousPatterns::contains_dangerous("DD IF=/DEV/ZERO") == true);
}

TEST_CASE("safe commands pass", "[dangerous-patterns]") {
  REQUIRE(DangerousPatterns::contains_dangerous("ls -la") == false);
  REQUIRE(DangerousPatterns::contains_dangerous("rm -r /tmp/x") == false);  // 无 -f, 不匹配
  REQUIRE(DangerousPatterns::contains_dangerous("rm -rfx") == true);  // 子串含 "rm -rf" → 匹配
  REQUIRE(DangerousPatterns::first_match("ls -la").empty());
}
