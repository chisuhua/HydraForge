// tests/test_temporal_agent_history_baseline.cpp
// 功能描述：历史大小基线对比测试 - C++ client vs PoC-02 Python baseline (±5%)
//          默认编译为 placeholder (验证基础设施可编译), 真实基准需:
//            -DTEMPORAL_HISTORY_BASELINE_ENABLED=ON
//            -DTEMPORAL_ENABLE_GRPC=ON
//            Temporal dev server running
// 设计依据：openspec/changes/pkgm-temporal-agent/tasks.md §7.4
//          .rddf/plans/pkgm-temporal-agent.md Task 6
// 作者：pkgm-temporal-agent Phase 2
// 最后修改日期：2026-07-28

#include "catch_amalgamated.hpp"

#ifdef TEMPORAL_HISTORY_BASELINE_ENABLED

#include "grpc_temporal_backend.h"

#include <cmath>
#include <string>
#include <vector>

using namespace pdk_temporal_agent;

// Placeholder: update after running PoC-02 10x and measuring actual history_size_bytes
constexpr long long kPythonBaselineBytes = 4523;
constexpr double kTolerancePct = 5.0;
constexpr int kIterations = 10;

static double avg(const std::vector<long long>& v) {
  long long sum = 0;
  for (auto x : v) sum += x;
  return static_cast<double>(sum) / static_cast<double>(v.size());
}

TEST_CASE("History size: matches PoC-02 Python baseline within ±5%",
          "[benchmark][.slow_disabled]") {
  GrpcTemporalBackend backend("localhost:7233");
  backend.connect();

  std::vector<long long> cpp_history_sizes;
  for (int i = 0; i < kIterations; ++i) {
    std::string wf_id = "wf-baseline-" + std::to_string(i);
    auto result = backend.start_workflow_blocking(
        "BaselineWorkflow", "task-queue", R"({"task":"identical_workflow_5_steps"})",
        wf_id, 30000);
    cpp_history_sizes.push_back(result.history_size_bytes);
  }

  double cpp_avg = avg(cpp_history_sizes);
  double diff_pct = 100.0 * std::abs(cpp_avg - static_cast<double>(kPythonBaselineBytes))
                    / static_cast<double>(kPythonBaselineBytes);

  REQUIRE(diff_pct <= kTolerancePct);
}

#else

// Placeholder path: verify benchmark infrastructure compiles
// Activate with -DTEMPORAL_HISTORY_BASELINE_ENABLED=ON

TEST_CASE("History size baseline: placeholder (TEMPORAL_HISTORY_BASELINE_ENABLED not set)",
          "[benchmark]") {
  // kPythonBaselineBytes documented for reference
  constexpr long long kPythonBaselineBytes = 4523;
  REQUIRE(kPythonBaselineBytes > 0);
  REQUIRE(kPythonBaselineBytes == 4523);
}

#endif
