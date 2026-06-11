# 性能基准

**ID**: BENCH-001
**日期**: 2026-05-23
**状态**: 已批准
**关联**: BOOT-001, TEST-001, API-001, ROUTER-001, ADR-0001

---

## 概述

本文档定义 AgenticDSL 项目的性能基准测试标准、度量指标和性能目标。

## 度量指标体系

### 延迟指标

| 指标 | 定义 | 目标 | 告警阈值 |
|------|------|------|---------|
| **P50 延迟** | 中位数延迟 | < 500ms | > 1s |
| **P95 延迟** | 95% 分位延迟 | < 2s | > 5s |
| **P99 延迟** | 99% 分位延迟 | < 5s | > 10s |
| **平均延迟** | 所有请求的平均延迟 | < 1s | > 2s |
| **首次 token 时间 (TTFT)** | 首 token 响应时间 | < 1s | > 2s |

### 吞吐量指标

| 指标 | 定义 | 目标 | 告警阈值 |
|------|------|------|---------|
| **QPS** | 每秒请求数 | > 10 | < 5 |
| **TPS** | 每秒 token 数 | > 100 | < 50 |
| **并发数** | 同时处理的请求数 | > 50 | < 10 |

### 质量指标

| 指标 | 定义 | 目标 | 告警阈值 |
|------|------|------|---------|
| **质量分数** | 质量评估器评分 | > 0.85 | < 0.7 |
| **回退率** | 回退到云端的比例 | < 30% | > 50% |
| **路由准确率** | 路由决策正确比例 | > 80% | < 60% |

### 资源指标

| 指标 | 定义 | 目标 | 告警阈值 |
|------|------|------|---------|
| **CPU 使用率** | 平均 CPU 占用 | < 70% | > 90% |
| **内存使用** | 平均内存占用 | < 4GB | > 8GB |
| **GPU 利用率** | GPU 占用 | > 60% | < 30% |

---

## 基准测试场景

### 1. CloudLLMAdapter 基准

#### 场景 1.1：OpenAI API 调用

```cpp
TEST_CASE("基准 - OpenAI GPT-4 生成", "[benchmark][cloud]") {
    auto adapter = std::make_unique<CloudLLMAdapter>();
    adapter->initialize({
        .provider = "openai",
        .api_key = std::getenv("OPENAI_API_KEY"),
        .model = "gpt-4",
        .temperature = 0.7f,
        .max_tokens = 500
    });

    std::vector<double> latencies;
    const int iterations = 100;

    for (int i = 0; i < iterations; ++i) {
        auto start = std::chrono::high_resolution_clock::now();

        auto result = adapter->generate(
            "Explain quantum computing in 50 words"
        );
        REQUIRE(result.is_ok());

        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration<double, std::milli>(end - start);
        latencies.push_back(duration.count());
    }

    // 输出基准数据
    print_latency_stats(latencies, "OpenAI GPT-4");
}

void print_latency_stats(const std::vector<double>& latencies, const std::string& name) {
    std::sort(latencies.begin(), latencies.end());

    double mean = std::accumulate(latencies.begin(), latencies.end(), 0.0) / latencies.size();
    double p50 = latencies[latencies.size() * 0.50];
    double p95 = latencies[latencies.size() * 0.95];
    double p99 = latencies[latencies.size() * 0.99];

    std::cout << name << " Latency Stats:\n";
    std::cout << "  Mean: " << mean << "ms\n";
    std::cout << "  P50: " << p50 << "ms\n";
    std::cout << "  P95: " << p95 << "ms\n";
    std::cout << "  P99: " << p99 << "ms\n";

    // 验证是否满足目标
    REQUIRE(p95 < 5000.0);  // P95 < 5s
    REQUIRE(mean < 3000.0); // Mean < 3s
}
```

#### 场景 1.2：Anthropic API 调用

```cpp
TEST_CASE("基准 - Anthropic Claude-3 生成", "[benchmark][cloud]") {
    auto adapter = std::make_unique<CloudLLMAdapter>();
    adapter->initialize({
        .provider = "anthropic",
        .api_key = std::getenv("ANTHROPIC_API_KEY"),
        .model = "claude-3-opus",
        .temperature = 0.7f,
        .max_tokens = 500
    });

    // ... 类似测试代码
}
```

---

### 2. LLMRouter 基准

#### 场景 2.1：路由决策性能

```cpp
TEST_CASE("基准 - LLMRouter 路由决策", "[benchmark][router]") {
    auto router = std::make_unique<LLMRouter>();
    router->set_cloud_adapter(std::make_unique<MockCloudLLM>());
    router->set_local_adapter(std::make_unique<MockLocalLLM>());

    TaskProfile profile;
    profile.task_type = "code";
    profile.complexity = 0.8f;
    profile.quality_requirement = 0.9f;

    std::vector<double> latencies;
    const int iterations = 10000;

    for (int i = 0; i < iterations; ++i) {
        auto start = std::chrono::high_resolution_clock::now();
        auto decision = router->route(profile);
        auto end = std::chrono::high_resolution_clock::now();

        latencies.push_back(
            std::chrono::duration<double, std::microi>(end - start).count()
        );
    }

    // 路由决策应该 < 1ms
    double p99_us = get_percentile(latencies, 0.99);
    std::cout << "路由决策 P99: " << p99_us << "μs\n";
    REQUIRE(p99_us < 1000.0);  // < 1ms
}
```

#### 场景 2.2：回退机制性能

```cpp
TEST_CASE("基准 - LLMRouter 本地回退云端", "[benchmark][router]") {
    auto router = std::make_unique<LLMRouter>();
    router->set_cloud_adapter(std::make_unique<MockCloudLLM>(/* low_quality */));
    router->set_local_adapter(std::make_unique<MockLocalLLM>());

    TaskProfile profile;
    profile.quality_requirement = 0.9f;  // 高质量要求

    auto start = std::chrono::high_resolution_clock::now();
    auto result = router->generate("test prompt", profile);
    auto end = std::chrono::high_resolution_clock::now();

    // 包含本地 + 云端的总时间
    auto total_ms = std::chrono::duration<double, std::milli>(end - start).count();
    REQUIRE(result.backend_used == "cloud");
    REQUIRE(total_ms < 10000);  // < 10s
}
```

---

### 3. QualityEvaluator 基准

#### 场景 3.1：快速评估性能

```cpp
TEST_CASE("基准 - QualityEvaluator 快速评估", "[benchmark][quality]") {
    auto evaluator = std::make_unique<QualityEvaluator>();

    std::string test_output = R"({
        "key": "value",
        "nested": {
            "array": [1, 2, 3]
        }
    })";

    std::vector<double> latencies;
    const int iterations = 1000;

    for (int i = 0; i < iterations; ++i) {
        auto start = std::chrono::high_resolution_clock::now();
        auto metrics = evaluator->quick_evaluate(test_output, {.expected_format = "json"});
        auto end = std::chrono::high_resolution_clock::now();

        latencies.push_back(
            std::chrono::duration<double, std::microi>(end - start).count()
        );
    }

    double p99_us = get_percentile(latencies, 0.99);
    std::cout << "快速评估 P99: " << p99_us << "μs\n";
    REQUIRE(p99_us < 10000.0);  // < 10ms
}
```

#### 场景 3.2：深度评估性能

```cpp
TEST_CASE("基准 - QualityEvaluator 深度评估", "[benchmark][quality]") {
    auto evaluator = std::make_unique<QualityEvaluator>();
    evaluator->set_cloud_adapter(std::make_unique<MockCloudLLM>());

    std::string test_output = "The capital of France is Paris.";
    std::string test_prompt = "What is the capital of France?";

    std::vector<double> latencies;
    const int iterations = 100;

    for (int i = 0; i < iterations; ++i) {
        auto start = std::chrono::high_resolution_clock::now();
        auto metrics = evaluator->deep_evaluate(test_output, test_prompt, {});
        auto end = std::chrono::high_resolution_clock::now();

        latencies.push_back(
            std::chrono::duration<double, std::milli>(end - start).count()
        );
    }

    double p99_ms = get_percentile(latencies, 0.99);
    std::cout << "深度评估 P99: " << p99_ms << "ms\n";
    REQUIRE(p99_ms < 5000.0);  // < 5s（调用云端）
}
```

---

### 4. 端到端基准

#### 场景 4.1：完整推理工作流

```cpp
TEST_CASE("基准 - 端到端推理工作流", "[benchmark][e2e]") {
    auto engine = DSLEngine::create();
    engine->load_library("lib/inference/router.md");

    std::vector<double> latencies;
    const int iterations = 50;

    for (int i = 0; i < iterations; ++i) {
        auto start = std::chrono::high_resolution_clock::now();

        auto result = engine->execute(R"(
## /main
  type: dsl_call
  subgraph: /lib/inference/router
  inputs:
    task_type: "chat"
    quality_requirement: 0.7

## /process
  type: assign
  assign:
    result: "{{ main.text }}"
)");

        auto end = std::chrono::high_resolution_clock::now();
        latencies.push_back(
            std::chrono::duration<double, std::milli>(end - start).count()
        );
    }

    print_latency_stats(latencies, "E2E 推理工作流");
}
```

#### 场景 4.2：并发请求

```cpp
TEST_CASE("基准 - 并发请求处理", "[benchmark][e2e][concurrency]") {
    auto engine = DSLEngine::create();
    auto router = std::make_unique<LLMRouter>();
    router->set_cloud_adapter(std::make_unique<MockCloudLLM>());
    router->set_local_adapter(std::make_unique<MockLocalLLM>());

    const int concurrent_requests = 20;
    const int iterations_per_request = 5;
    std::atomic<int> completed{0};

    std::vector<std::thread> threads;
    auto start = std::chrono::high_resolution_clock::now();

    for (int t = 0; t < concurrent_requests; ++t) {
        threads.emplace_back([&]() {
            for (int i = 0; i < iterations_per_request; ++i) {
                auto result = router->generate("test", TaskProfile{});
                completed++;
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto total_s = std::chrono::duration<double>(end - start).count();

    double qps = completed.load() / total_s;
    std::cout << "QPS: " << qps << "\n";
    REQUIRE(qps > 5.0);  // > 5 QPS
}
```

---

## 性能回归检测

### CI 集成

```yaml
# .github/workflows/benchmark.yml
name: Performance Benchmarks

on:
  schedule:
    - cron: '0 0 * * *'  # 每日
  workflow_dispatch:

jobs:
  benchmark:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v3

      - name: Configure
        run: cmake -B build -DAGENTICDSL_BUILD_TESTS=ON

      - name: Build
        run: cmake --build build -j$(nproc)

      - name: Run Benchmarks
        run: |
          ctest -L benchmark --output-on-failure --test-dir build
          ./build/benchmark --json > benchmark_results.json

      - name: Compare with Baseline
        run: |
          python scripts/compare_benchmark.py \
            --current benchmark_results.json \
            --baseline benchmark_baseline.json \
            --threshold 0.1

      - name: Upload Results
        uses: actions/upload-artifact@v3
        with:
          name: benchmark-results-${{ github.run_id }}
          path: benchmark_results.json
```

### 回归判断逻辑

```python
# scripts/compare_benchmark.py
def compare_benchmarks(current, baseline, threshold=0.1):
    """比较基准测试结果，检测性能回归"""
    regressions = []

    for metric, value in current.items():
        if metric not in baseline:
            continue

        baseline_value = baseline[metric]
        diff_pct = (value - baseline_value) / baseline_value

        # 延迟类指标：增加是回归
        if 'latency' in metric or 'p99' in metric or 'p95' in metric:
            if diff_pct > threshold:
                regressions.append({
                    'metric': metric,
                    'baseline': baseline_value,
                    'current': value,
                    'diff_pct': diff_pct
                })

        # 吞吐量类指标：减少是回归
        if 'qps' in metric or 'tps' in metric:
            if diff_pct < -threshold:
                regressions.append({
                    'metric': metric,
                    'baseline': baseline_value,
                    'current': value,
                    'diff_pct': diff_pct
                })

    if regressions:
        print("🚨 性能回归检测:")
        for r in regressions:
            print(f"  {r['metric']}: {r['baseline']} -> {r['current']} ({r['diff_pct']:.1%})")
        return False

    return True
```

---

## 性能优化指南

### 优化优先级

1. **P0 - 阻塞性问题**：延迟 > 10s、内存泄漏、崩溃
2. **P1 - 严重问题**：延迟 > 目标 50%、CPU > 90%
3. **P2 - 一般问题**：延迟 > 目标 20%、QPS < 目标 50%
4. **P3 - 优化建议**：延迟接近目标、可优化项

### 常见优化手段

| 问题 | 优化手段 |
|------|---------|
| 首次调用延迟高 | 预热/预加载、缓存 |
| 内存占用高 | 对象池、减少副本、内存映射 |
| CPU 占用高 | 多线程、异步 IO、向量化 |
| 网络延迟高 | HTTP keep-alive、连接池、CDN |
| 质量不稳定 | 调整采样参数、提示词优化 |

---

## 验证标准

- [ ] P95 延迟 < 5s
- [ ] QPS > 5
- [ ] 路由决策 < 1ms
- [ ] 质量评估 < 10ms
- [ ] 无内存泄漏
- [ ] 性能回归检测 CI 正常

---

## 关联文档

| 文档 | 关系 |
|------|------|
| [BOOT-001: 自举实施路径](../implementation/self-bootstrapping-path.md) | 性能基准是阶段 0-3 实施的验证标准 |
| [TEST-001: 测试策略](test-strategy.md) | 基准测试是测试策略的一部分 |
| [ROUTER-001: 推理路由器](../architecture/inference-router.md) | 路由器需要性能基准验证 |
| [QUALITY-001: 质量评估器](../architecture/quality-evaluator.md) | 评估器需要性能基准验证 |