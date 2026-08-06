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
// 4. LLMProviderFactory 路由: 未识别 provider → MockLLMProvider (P2.C 兜底契约)
// ============================================================
TEST_CASE("LLMProviderFactory unknown provider falls back to MockLLMProvider", "[provider_factory][p1]") {
  LLMProviderFactory factory;
  LLMConfig config;
  config.provider = "unknown_backend_xyz";
  auto provider = factory.create(config);
  // P2.C (2026-06-24): 兜底契约保证 caller 永不收到 nullptr (允许 engine.cpp 移除 fallback)
  REQUIRE(provider != nullptr);
  auto* mock = dynamic_cast<MockLLMProvider*>(provider.get());
  CHECK(mock != nullptr);
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
  // P2.C (2026-06-24): unknown 路径不再返回 nullptr, 而是 Mock provider
  // 故 null_count 改为 mock_count 增量, 总和仍等于 total
  CHECK(mock_count.load() > 0);
  CHECK(openai_count.load() > 0);
  // P2.C: 兜底契约保证永不返回 nullptr
  CHECK(null_count.load() == 0);
}

// ============================================================
// 7. LLMProviderFactory 动态注册: 运行时注册后立即创建
// ============================================================
TEST_CASE("LLMProviderFactory registers and creates a dynamic provider",
          "[provider_factory][dynamic]") {
  LLMProviderFactory factory;
  std::atomic<int> calls{0};
  const bool registered = factory.register_dynamic(
      "runtime-provider",
      [&calls](const LLMConfig& config) {
        calls.fetch_add(1);
        (void)config;
        return std::make_unique<MockLLMProvider>();
      });
  REQUIRE(registered);

  LLMConfig config;
  config.provider = "runtime-provider";
  auto provider = factory.create(config);
  REQUIRE(provider != nullptr);
  CHECK(calls.load() == 1);
  CHECK(factory.has_dynamic("runtime-provider"));
  CHECK(factory.dynamic_names() == std::vector<std::string>{"runtime-provider"});
  CHECK(factory.current_default().empty());
}

// ============================================================
// 8. LLMProviderFactory 拒绝无效和重复的动态提供者
// ============================================================
TEST_CASE("LLMProviderFactory rejects invalid and duplicate dynamic providers",
          "[provider_factory][dynamic]") {
  LLMProviderFactory factory;
  LLMProviderFactory::DynamicFactoryFn null_fn;
  CHECK_FALSE(factory.register_dynamic("", null_fn));
  CHECK_FALSE(factory.register_dynamic("runtime-provider", null_fn));
  CHECK(factory.dynamic_names().empty());

  auto callback = [](const LLMConfig& config) {
    (void)config;
    return std::make_unique<MockLLMProvider>();
  };
  REQUIRE(factory.register_dynamic("runtime-provider", callback));
  CHECK_FALSE(factory.register_dynamic("runtime-provider", callback));
  CHECK(factory.dynamic_names().size() == 1);
  CHECK_FALSE(factory.register_dynamic("openai", callback));  // reserved backend
}

// ============================================================
// 9. LLMProviderFactory 动态默认仅在 provider 为空时生效
// ============================================================
TEST_CASE("LLMProviderFactory dynamic default applies only for empty provider",
          "[provider_factory][dynamic]") {
  LLMProviderFactory factory;
  REQUIRE(factory.register_dynamic(
      "runtime-provider",
      [](const LLMConfig& config) {
        (void)config;
        return std::make_unique<MockLLMProvider>();
      }));
  REQUIRE(factory.switch_default("runtime-provider"));
  CHECK(factory.current_default() == "runtime-provider");

  LLMConfig empty_cfg;
  empty_cfg.provider.clear();
  auto a = factory.create(empty_cfg);
  REQUIRE(a != nullptr);
  CHECK(dynamic_cast<MockLLMProvider*>(a.get()) != nullptr);

  LLMConfig explicit_cfg;
  explicit_cfg.provider = "openai";
  auto b = factory.create(explicit_cfg);
  REQUIRE(b != nullptr);
  CHECK(dynamic_cast<MockLLMProvider*>(b.get()) == nullptr);  // routes to cloud, not dynamic
}
