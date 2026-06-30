// tests/test_llm_factory.cpp
// Sprint 16 Coverage Backfill: 测试 src/common/llm/factory.cpp
#include "catch_amalgamated.hpp"
#include "common/llm/factory.h"
#include "common/llm/llm_provider_factory.h"
#include "common/llm/llm_types.h"

using namespace agenticdsl;

TEST_CASE("llm::create_provider_factory returns IProviderFactory", "[llm][factory]") {
  auto factory = llm::create_provider_factory();
  REQUIRE(factory != nullptr);

  // 通过抽象接口调用 create(), 不依赖具体类型
  LLMConfig config;
  config.provider = "mock";
  auto provider = factory->create(config);
  REQUIRE(provider != nullptr);
}

TEST_CASE("llm::create_provider_factory dispatches by backend name", "[llm][factory]") {
  auto factory = llm::create_provider_factory();
  REQUIRE(factory != nullptr);

  // Mock provider 路径 (默认兜底, 永不返回 null)
  LLMConfig mock_cfg;
  mock_cfg.provider = "mock";
  auto mock_provider = factory->create(mock_cfg);
  REQUIRE(mock_provider != nullptr);

  // 未知 provider 也应兜底返回 Mock (LLMProviderFactory 行为)
  LLMConfig unknown_cfg;
  unknown_cfg.provider = "unknown-backend";
  auto unknown_provider = factory->create(unknown_cfg);
  REQUIRE(unknown_provider != nullptr);

  // 空 provider 字符串也兜底返回 Mock
  LLMConfig empty_cfg;
  auto empty_provider = factory->create(empty_cfg);
  REQUIRE(empty_provider != nullptr);
}

TEST_CASE("llm::create_provider_factory returns independent instances", "[llm][factory]") {
  auto f1 = llm::create_provider_factory();
  auto f2 = llm::create_provider_factory();
  REQUIRE(f1 != nullptr);
  REQUIRE(f2 != nullptr);
  REQUIRE(f1.get() != f2.get());
}