// tests/test_provider_factory.cpp
// IProviderFactory + LLMProviderFactory + MockProviderFactory 单元测试 (Catch2 v3)
// Phase 1 P1.T1.5: 验证 4 个核心场景 + 1 个多线程并发测试
#include "catch_amalgamated.hpp"

#include "agenticdsl/contract/iprovider_factory.h"
#include "common/llm/llm_provider_factory.h"
#include "common/llm/llm_config.h"
#include "common/llm/llm_types.h"
#include "common/llm/mock_provider.h"
#include "common/llm/mock_provider_factory.h"  // P1.T1.5: MockProviderFactory 完整类型

#include <atomic>
#include <memory>
#include <string>
#include <thread>
#include <vector>

using agenticdsl::IProviderFactory;
using agenticdsl::ILLMProvider;
using agenticdsl::LLMConfig;
using agenticdsl::LLMProviderFactory;
using agenticdsl::MockLLMProvider;
using agenticdsl::MockProviderFactory;

// ============================================================
// 1. MockProviderFactory.create() 返回 MockLLMProvider
// ============================================================
TEST_CASE("MockProviderFactory creates MockLLMProvider", "[provider_factory][p1]") {
  MockProviderFactory factory;
  LLMConfig config;  // 默认空 config
  auto provider = factory.create(config);
  REQUIRE(provider != nullptr);
  // 验证类型: 实际类型是 MockLLMProvider (dynamic_cast 不可用, 检查类名)
  // 替代方案: 调用 MockLLMProvider 独有 API
  auto* mock = dynamic_cast<MockLLMProvider*>(provider.get());
  CHECK(mock != nullptr);
}

// ============================================================
// 2. LLMProviderFactory 路由: mock → MockLLMProvider
// ============================================================
TEST_CASE("LLMProviderFactory routes 'mock' to MockProviderFactory", "[provider_factory][p1]") {
  LLMProviderFactory factory;
  LLMConfig config;
  config.provider = "mock";
  auto provider = factory.create(config);
  REQUIRE(provider != nullptr);
  auto* mock = dynamic_cast<MockLLMProvider*>(provider.get());
  CHECK(mock != nullptr);
}

// ============================================================
// 3. LLMProviderFactory 路由: 空 provider → MockLLMProvider (兜底)
// ============================================================
TEST_CASE("LLMProviderFactory empty provider defaults to MockLLMProvider", "[provider_factory][p1]") {
  LLMProviderFactory factory;
  LLMConfig config;
  config.provider = "";  // 空
  auto provider = factory.create(config);
  REQUIRE(provider != nullptr);
  auto* mock = dynamic_cast<MockLLMProvider*>(provider.get());
  CHECK(mock != nullptr);
}

// ============================================================
// 4. LLMProviderFactory 路由: 未识别 provider → nullptr
// ============================================================
TEST_CASE("LLMProviderFactory unknown provider returns nullptr", "[provider_factory][p1]") {
  LLMProviderFactory factory;
  LLMConfig config;
  config.provider = "unknown_backend_xyz";
  auto provider = factory.create(config);
  CHECK(provider == nullptr);
}

// ============================================================
// 5. LLMProviderFactory 路由: openai → CloudLLMAdapter (跳过网络实际调用)
// ============================================================
TEST_CASE("LLMProviderFactory routes 'openai' to CloudLLMAdapter (no network)", "[provider_factory][p1]") {
  LLMProviderFactory factory;
  LLMConfig config;
  config.provider = "openai";
  // CloudLLMAdapter 构造需要 LLMConfig, 无网络调用 (直到 generate() 才会发 HTTP)
  // 这里只验证 create() 返回非空 unique_ptr
  auto provider = factory.create(config);
  REQUIRE(provider != nullptr);
  // 不做 dynamic_cast 检查 (CloudLLMAdapter 内部 state 复杂)
  // 仅验证: 路由到非 Mock 实现
  auto* mock = dynamic_cast<MockLLMProvider*>(provider.get());
  CHECK(mock == nullptr);
}

// ============================================================
// 6. 多线程并发 create() 1000 次 (无 data race)
// ============================================================
TEST_CASE("LLMProviderFactory concurrent create() thread-safe", "[provider_factory][p1][thread]") {
  LLMProviderFactory factory;  // 共享 1 个 factory 实例
  const int kThreads = 8;
  const int kIterations = 125;  // 8 * 125 = 1000 total
  std::atomic<int> mock_count{0};
  std::atomic<int> openai_count{0};
  std::atomic<int> null_count{0};
  std::atomic<int> total{0};

  std::vector<std::thread> threads;
  threads.reserve(kThreads);

  for (int t = 0; t < kThreads; ++t) {
    threads.emplace_back([&]() {
      for (int i = 0; i < kIterations; ++i) {
        LLMConfig config;
        // 交替测试不同 provider
        config.provider = (i % 3 == 0) ? "mock" : ((i % 3 == 1) ? "openai" : "unknown");
        auto provider = factory.create(config);
        if (!provider) {
          null_count.fetch_add(1);
        } else if (dynamic_cast<MockLLMProvider*>(provider.get())) {
          mock_count.fetch_add(1);
        } else {
          openai_count.fetch_add(1);
        }
        total.fetch_add(1);
      }
    });
  }

  for (auto& th : threads) {
    th.join();
  }

  CHECK(total.load() == kThreads * kIterations);
  CHECK(mock_count.load() + openai_count.load() + null_count.load() == total.load());
  // mock 路径 + openai 路径 + unknown 路径 = 总数
  // (具体分布取决于 i%3, 不做严格断言, 只验证不崩溃 + 计数正确)
  CHECK(mock_count.load() > 0);
  CHECK(openai_count.load() > 0);
  CHECK(null_count.load() > 0);
}
