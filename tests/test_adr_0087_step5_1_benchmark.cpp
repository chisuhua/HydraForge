// tests/test_adr_0087_step5_1_benchmark.cpp
// ADR-0087 step 5.1 benchmark: 4-worker vs 1-worker concurrent real deepseek
// 验证 root cause fix 的性能提升 (预期 ~4× 加速, baseline ~12s → fix 后 ~3s).
//
// Pre-fix (Wave 1 #2 SerializingDecorator workaround): 4 worker 串行化, 4 tasks ~12s
// Post-fix (ADR-0087 step 1-4 root cause): 4 worker 并发直连 deepseek, 4 tasks ~3s
//
// 测试: 跑 1 worker × 4 tasks (serial) + 4 workers × 4 tasks (parallel) 对比
// wall time, 打印 speedup ratio.

#include "catch_amalgamated.hpp"

#include "agenticdsl/cognitive/domain_worker_pool.h"
#include "agenticdsl/contract/bus_event.h"
#include "agenticdsl/contract/iinteraction_bus.h"
#include "agenticdsl/contract/inmemory_bus.h"
#include "common/llm/llm_types.h"

#include "test_helpers/real_llm_env.h"

#include <nlohmann/json.hpp>

#include <atomic>
#include <chrono>
#include <cstdint>
#include <iostream>
#include <memory>

using agenticdsl::DomainTask;
using agenticdsl::DomainWorkerPool;
using agenticdsl::test::real_llm_config;
using agenticdsl::test::real_llm_provider;
using agenticdsl::test::real_llm_env_skipped;
using agenticdsl::test::require_real_llm_env;

namespace {

// 阻塞直到 predicate true 或 timeout
template <typename Pred>
bool wait_until(Pred&& p, std::chrono::milliseconds timeout) {
  auto start = std::chrono::steady_clock::now();
  while (!p()) {
    if (std::chrono::steady_clock::now() - start > timeout) return false;
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
  }
  return true;
}

}  // namespace

TEST_CASE("ADR-0087 step 5.1: 4-worker concurrent real deepseek vs 1-worker serial",
          "[adr-0087][step-5-1][benchmark][realllm]") {
  require_real_llm_env();
  if (real_llm_env_skipped()) {
    SUCCEED("skipped: HYDRAFORGE_SKIP_REAL_LLM=1");
    return;
  }

  auto cfg = real_llm_config();
  auto provider_holder = real_llm_provider();
  agenticdsl::ILLMProvider* shared = provider_holder.get();

  const std::string prompt_template =
      "Reply with just the single word OK. Task #";
  const int kTasks = 4;

  // === Run 1: 1 worker × 4 tasks (serial baseline) ===
  std::int64_t serial_ms = 0;
  {
    auto bus = std::make_shared<agenticdsl::InMemoryBus>();
    std::atomic<int> completed{0};
    bus->subscribe("domain.task.completed", [&](const agenticdsl::BusEvent&) {
      completed.fetch_add(1, std::memory_order_relaxed);
    });

    DomainWorkerPool pool(1, bus);
    pool.register_domain_handler(
        "llm", [shared, model = cfg.model, prompt_template](
                   const DomainTask& task) -> nlohmann::json {
          agenticdsl::GenerationRequest req;
          req.prompt = prompt_template + task.arguments["id"].get<std::string>();
          req.params.model = model;
          auto result = shared->generate(req, std::stop_token{});
          if (!result.has_value()) {
            return {{"llm_error", true},
                    {"code", static_cast<int>(result.error().code)},
                    {"message", result.error().message}};
          }
          return {{"response", result.value().text}};
        });
    pool.start();

    auto serial_start = std::chrono::steady_clock::now();
    for (int i = 0; i < kTasks; ++i) {
      DomainTask task;
      task.domain = "llm";
      task.tool_name = "llm::generate";
      task.arguments = {{"id", std::to_string(i)}};
      task.output_key = "result";
      pool.submit_task(std::move(task));
    }
    bool serial_done = wait_until(
        [&] { return completed.load() >= kTasks; }, std::chrono::seconds(180));
    pool.stop();
    auto serial_end = std::chrono::steady_clock::now();
    serial_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        serial_end - serial_start).count();

    REQUIRE(serial_done);
    std::cerr << "[ADR-0087 step 5.1] 1 worker × " << kTasks
              << " tasks (serial baseline) wall time: " << serial_ms
              << " ms" << std::endl;
  }

  // === Run 2: 4 workers × 4 tasks (parallel) ===
  std::int64_t parallel_ms = 0;
  {
    auto bus = std::make_shared<agenticdsl::InMemoryBus>();
    std::atomic<int> completed{0};
    bus->subscribe("domain.task.completed", [&](const agenticdsl::BusEvent&) {
      completed.fetch_add(1, std::memory_order_relaxed);
    });

    DomainWorkerPool pool(4, bus);
    pool.register_domain_handler(
        "llm", [shared, model = cfg.model, prompt_template](
                   const DomainTask& task) -> nlohmann::json {
          agenticdsl::GenerationRequest req;
          req.prompt = prompt_template + task.arguments["id"].get<std::string>();
          req.params.model = model;
          auto result = shared->generate(req, std::stop_token{});
          if (!result.has_value()) {
            return {{"llm_error", true},
                    {"code", static_cast<int>(result.error().code)},
                    {"message", result.error().message}};
          }
          return {{"response", result.value().text}};
        });
    pool.start();

    auto parallel_start = std::chrono::steady_clock::now();
    for (int i = 0; i < kTasks; ++i) {
      DomainTask task;
      task.domain = "llm";
      task.tool_name = "llm::generate";
      task.arguments = {{"id", std::to_string(i)}};
      task.output_key = "result";
      pool.submit_task(std::move(task));
    }
    bool parallel_done = wait_until(
        [&] { return completed.load() >= kTasks; }, std::chrono::seconds(180));
    pool.stop();
    auto parallel_end = std::chrono::steady_clock::now();
    parallel_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        parallel_end - parallel_start).count();

    REQUIRE(parallel_done);
    std::cerr << "[ADR-0087 step 5.1] 4 workers × " << kTasks
              << " tasks (parallel) wall time: " << parallel_ms << " ms"
              << std::endl;
  }

  // speedup 比值记录 (不作为硬性 REQUIRE, 因为 deepseek latency 受网络/限速波动)
  if (parallel_ms > 0) {
    double speedup = static_cast<double>(serial_ms) /
                      static_cast<double>(parallel_ms);
    std::cerr << "[ADR-0087 step 5.1] speedup ratio: " << speedup
              << "x (serial=" << serial_ms << "ms / parallel=" << parallel_ms
              << "ms)" << std::endl;
    // 软断言: parallel <= serial (至少不多花时间, 排除回归)
    REQUIRE(parallel_ms <= serial_ms);
  }
}
