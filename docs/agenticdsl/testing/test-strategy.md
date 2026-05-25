# TEST-001: 测试策略

**ID**: TEST-001
**日期**: 2026-05-23
**状态**: 已批准
**关联**: BOOT-001, IP-001, ROUTER-001, QUALITY-001

---

## 概述

本文档定义 AgenticDSL 项目的测试策略，涵盖单元测试、集成测试、性能测试和质量评估测试。

## 测试金字塔

```
                    ┌─────────────┐
                    │  端到端测试  │  (E2E)
                    │   5%        │
              ┌─────┴─────────────┴─────┐
              │      集成测试          │  (Integration)
              │       20%              │
        ┌─────┴───────────────┴─────┐
        │        单元测试            │  (Unit)
        │         70%               │
        └───────────────────────────┘
```

| 层级 | 覆盖率目标 | 测试数量 | 单次耗时 |
|------|-----------|---------|---------|
| 单元测试 | 80%+ | 200+ | < 10ms |
| 集成测试 | 60%+ | 50+ | < 1s |
| 端到端测试 | 关键路径 | 20+ | < 30s |

---

## 单元测试

### 测试框架

- **框架**: Catch2
- **位置**: `tests/test_*.cpp`
- **命名**: `test_<module>.cpp`

### 测试规范

```cpp
// 测试文件结构
#define TEST_MODULE "<module>"

TEST_CASE("<场景描述>", "[module][<tag>]") {
    // Arrange - 准备测试数据
    // Act - 执行被测操作
    // Assert - 验证结果
}

TEST_CASE("<场景描述> - 边界情况", "[module][boundary]") {
    // 边界情况测试
}
```

### 当前测试覆盖

| 模块 | 测试文件 | 覆盖内容 |
|------|---------|---------|
| 基础功能 | `test_basic.cpp` | 核心类型、工具 |
| 引擎 | `test_engine.cpp` | DSLEngine, ParsedGraph |
| 执行器 | `test_executor.cpp` | NodeExecutor, 节点执行 |
| 库加载 | `test_library_loader.cpp` | StandardLibraryLoader |
| LLM 流式 | `test_llm_streaming.cpp` | LLM 流式接口 |
| LLM 工具 | `test_llm_tool.cpp` | LLM 工具调用 |
| 无 LLM 模式 | `test_no_llm.cpp` | 降级功能 |
| 解析器 | `test_parser.cpp` | MarkdownParser |
| 路径解析 | `test_path_resolution.cpp` | 路径解析 |
| Prompt 构建 | `test_prompt_builder.cpp` | PromptBuilder |
| 调度器 | `test_scheduler.cpp` | TopoScheduler |
| 工具注册表 | `test_tool_registry.cpp` | ToolRegistry |

### 缺失的测试

| 模块 | 测试文件 | 说明 |
|------|---------|------|
| CloudLLMAdapter | `test_cloud_adapter.cpp` | 云端 LLM 适配器（待实现） |
| LLMRouter | `test_router.cpp` | 路由决策（待实现） |
| QualityEvaluator | `test_quality_eval.cpp` | 质量评估（待实现） |
| Session Registry | `test_session_registry.cpp` | Session 管理（待实现） |
| YIELD 节点 | `test_yield.cpp` | 流式输出（待实现） |

---

## 集成测试

### 目标

验证多个组件协同工作的正确性，确保模块间接口正确。

### 测试策略

```yaml
# tests/integration/
test_integration_<feature>.cpp
```

### 集成测试场景

#### 1. 推理路由集成测试

```cpp
TEST_CASE("LLMRouter - 云端路由", "[integration][router]") {
    // 1. 初始化路由组件
    auto router = std::make_unique<LLMRouter>();
    router->set_cloud_adapter(cloud_adapter);
    router->set_local_adapter(local_adapter);

    // 2. 创建任务配置
    TaskProfile profile;
    profile.task_type = "code";
    profile.quality_requirement = 0.9f;

    // 3. 执行路由
    auto decision = router->route(profile);

    // 4. 验证路由决策
    REQUIRE(decision.backend == Backend::CLOUD);
    REQUIRE(decision.reason == "代码生成需要高质量");
}

TEST_CASE("LLMRouter - 本地路由 + 回退云端", "[integration][router]") {
    // 1. 初始化
    auto router = std::make_unique<LLMRouter>();
    router->set_cloud_adapter(cloud_adapter);
    router->set_local_adapter(local_adapter);

    // 2. 路由到本地
    TaskProfile profile;
    profile.task_type = "chat";
    profile.quality_requirement = 0.5f;
    auto decision = router->route(profile);
    REQUIRE(decision.backend == Backend::LOCAL);

    // 3. 本地推理
    auto result = router->generate("hello", profile);

    // 4. 质量不达标，回退云端
    // （在 mock 中，质量评估器设置不达标）
    REQUIRE(result.backend_used == "cloud");
}
```

#### 2. 质量评估闭环测试

```cpp
TEST_CASE("QualityEvaluator - 快速评估", "[integration][quality]") {
    auto evaluator = std::make_unique<QualityEvaluator>();
    
    QualityEvaluator::Config config;
    config.expected_format = "json";
    config.quality_threshold = 0.8f;

    // 有效 JSON
    auto metrics1 = evaluator->quick_evaluate(
        R"({"key": "value"})", config);
    REQUIRE(metrics1.format_score > 0.9f);

    // 无效 JSON
    auto metrics2 = evaluator->quick_evaluate(
        "not json", config);
    REQUIRE(metrics2.format_score < 0.5f);
}

TEST_CASE("QualityEvaluator - 深度评估", "[integration][quality]") {
    // 深度评估需要 mock 云端 LLM
    auto evaluator = std::make_unique<QualityEvaluator>();
    evaluator->set_cloud_adapter(mock_cloud_adapter);

    auto metrics = evaluator->deep_evaluate(
        "The capital of France is Paris.",
        "What is the capital of France?",
        config);

    REQUIRE(metrics.content_score > 0.8f);
}
```

#### 3. 端到端推理工作流测试

```cpp
TEST_CASE("端到端 - 推理工作流", "[integration][e2e]") {
    // 1. 初始化 DSL 引擎
    auto engine = DSLEngine::create();
    engine->load_library("lib/inference/router.md");

    // 2. 加载工作流
    auto workflow = R"(
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
)";

    // 3. 执行工作流
    auto result = engine->execute(workflow);

    // 4. 验证结果
    REQUIRE(result["result"].is_string());
}
```

---

## Mock 策略

### HTTP Mock

使用 `httplib` 的 mock server 进行 HTTP 测试：

```cpp
class MockHttpServer {
public:
    MockHttpServer() : server_(httplib::Server{}) {
        server_.Post("/v1/chat/completions",
            [this](const httplib::Request& req, httplib::Response& res) {
                handle_chat_completions(req, res);
            });
    }

    void start(int port) { server_.listen("localhost", port); }
    void stop() { server_.stop(); }

private:
    httplib::Server server_;
    std::vector<json> requests_;
};

TEST_CASE("CloudLLMAdapter - OpenAI 格式", "[mock][cloud]") {
    MockHttpServer server;
    server.start(18080);
    SCOPE_EXIT { server.stop(); };

    // 配置使用 mock server
    CloudLLMAdapter::Config config;
    config.provider = "openai";
    config.base_url = "http://localhost:18080";
    config.model = "gpt-4";

    CloudLLMAdapter adapter;
    adapter.initialize(config);

    // 调用
    auto result = adapter.generate("Hello");

    // 验证
    REQUIRE(result.is_ok());
    REQUIRE(server.received_requests().size() == 1);
}
```

### 云端 LLM Mock

```cpp
class MockCloudLLM : public CloudLLMAdapter {
public:
    MockCloudLLM() {
        set_quality_score(0.95f);  // 高质量
        set_latency_ms(500);
    }

    std::string generate(const std::string& prompt,
                         const Config& override_config) override {
        // 返回固定回复
        return "Mock response for: " + prompt;
    }
};

class LowQualityMockCloudLLM : public CloudLLMAdapter {
public:
    LowQualityMockCloudLLM() {
        set_quality_score(0.6f);  // 低质量
        set_latency_ms(100);
    }

    std::string generate(const std::string& prompt,
                         const Config& override_config) override {
        return "Low quality response";
    }
};
```

### 本地 LLM Mock

```cpp
class MockLocalLLM : public LocalLLMAdapter {
public:
    MockLocalLLM() {
        set_quality_score(0.7f);
        set_latency_ms(50);
    }

    std::string generate(const std::string& prompt,
                         const Config& override_config) override {
        return "Local mock response";
    }
};
```

---

## 性能测试

### 基准测试框架

```cpp
TEST_CASE("性能基准 - CloudLLMAdapter 生成", "[benchmark][cloud]") {
    auto adapter = std::make_unique<CloudLLMAdapter>();
    adapter->initialize(test_config);

    BENCHMARK("OpenAI GPT-4") {
        return adapter->generate("Explain quantum computing in 50 words");
    };
}

TEST_CASE("性能基准 - LLMRouter 路由决策", "[benchmark][router]") {
    auto router = std::make_unique<LLMRouter>();
    router->set_cloud_adapter(cloud_adapter);
    router->set_local_adapter(local_adapter);

    TaskProfile profile;
    profile.task_type = "code";
    profile.quality_requirement = 0.9f;

    BENCHMARK("路由决策") {
        return router->route(profile);
    };
}
```

### 性能指标

| 指标 | 目标 | 告警阈值 |
|------|------|---------|
| 单元测试耗时 | < 10ms/测试 | > 50ms |
| CloudLLMAdapter 延迟 | < 5s (API) | > 10s |
| LocalLLMAdapter 延迟 | < 500ms | > 2s |
| 路由决策耗时 | < 1ms | > 10ms |
| 质量评估耗时 | < 10ms | > 100ms |

---

## 质量评估测试

### 质量评估器验证

```cpp
TEST_CASE("QualityEvaluator - 格式检测", "[quality][unit]") {
    auto evaluator = std::make_unique<QualityEvaluator>();

    SECTION("JSON 格式") {
        auto score = evaluator->evaluate_format(R"({"key": "value"})", "json");
        REQUIRE(score > 0.9f);
    }

    SECTION("无效 JSON") {
        auto score = evaluator->evaluate_format("not json", "json");
        REQUIRE(score < 0.3f);
    }

    SECTION("代码格式") {
        auto score = evaluator->evaluate_format("function test() {}", "code");
        REQUIRE(score > 0.8f);
    }
}

TEST_CASE("QualityEvaluator - 准确性", "[quality][unit]") {
    // 使用已知的问答对验证评估准确性
    struct TestCase {
        std::string question;
        std::string answer;
        float expected_min_score;
    };

    std::vector<TestCase> cases = {
        {"What is 2+2?", "4", 0.9f},
        {"What is the capital of France?", "Paris", 0.9f},
        {"Explain relativity", "Einstein", 0.7f},
    };

    for (const auto& test : cases) {
        auto metrics = evaluator->quick_evaluate(test.answer, {});
        REQUIRE(metrics.overall_score >= test.expected_min_score);
    }
}
```

---

## 持续集成

### GitHub Actions 工作流

```yaml
# .github/workflows/test.yml
name: Tests

on:
  push:
    branches: [main, develop]
  pull_request:
    branches: [main]

jobs:
  unit-test:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v3
      - name: Configure
        run: cmake -B build -DAGENTICDSL_BUILD_TESTS=ON
      - name: Build
        run: cmake --build build -j$(nproc)
      - name: Test
        run: ctest --output-on-failure --test-dir build

  integration-test:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v3
      - name: Configure
        run: cmake -B build -DAGENTICDSL_BUILD_TESTS=ON -DAGENTICDSL_ENABLE_INTEGRATION=ON
      - name: Build
        run: cmake --build build -j$(nproc)
      - name: Integration Tests
        run: ctest -L integration --output-on-failure --test-dir build

  benchmark:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v3
      - name: Configure
        run: cmake -B build -DAGENTICDSL_BUILD_TESTS=ON
      - name: Build
        run: cmake --build build -j$(nproc)
      - name: Run Benchmarks
        run: ctest -L benchmark --output-on-failure --test-dir build
      - name: Upload Results
        uses: actions/upload-artifact@v3
        with:
          name: benchmark-results
          path: build/benchmark_results.json
```

### 测试标签

| 标签 | 说明 | CI 运行 |
|------|------|--------|
| `[unit]` | 单元测试 | 每次 |
| `[integration]` | 集成测试 | 每次 |
| `[e2e]` | 端到端测试 | PR + 每日 |
| `[benchmark]` | 性能基准 | PR + 每日 |
| `[slow]` | 慢速测试（> 1s） | 每日 |
| `[cloud]` | 需要云端 API | 手动 |
| `[mock]` | 使用 Mock | 每次 |

---

## 测试数据管理

### 测试数据位置

```
tests/
├── data/
│   ├── prompts/           # 测试用 prompt 模板
│   ├── workflows/         # 测试用工作流文件
│   ├── models/            # 模型配置测试数据
│   └── expected/           # 期望输出
├── fixtures/
│   └── *.json             # 测试夹具数据
└── test_*.cpp
```

### 测试数据示例

```json
// tests/data/prompts/code_generation.json
{
  "name": "code_generation",
  "input": "Write a function to calculate fibonacci numbers in Python",
  "expected_contains": ["def fibonacci", "return"],
  "max_length": 500
}

// tests/data/workflows/simple_chat.agent.md
## /main
  type: tool_call
  tool: inference.cloud_generate
  arguments:
    prompt: "Hello, how are you?"
    model: "gpt-4"
  output_keys: ["text"]
```

---

## 验证标准

- [ ] 单元测试覆盖率 > 80%
- [ ] 所有关键路径有集成测试
- [ ] CloudLLMAdapter / LLMRouter / QualityEvaluator 有单元测试
- [ ] Mock HTTP 测试覆盖 OpenAI / Anthropic 格式
- [ ] 性能基准测试通过
- [ ] CI 工作流正常运行

---

## 关联文档

| 文档 | 关系 |
|------|------|
| [BOOT-001: 自举实施路径](../implementation/self-bootstrapping-path.md) | 测试策略是阶段 0-3 实施的基础 |
| [ROUTER-001: 推理路由器](../architecture/inference-router.md) | 路由决策需要集成测试覆盖 |
| [QUALITY-001: 质量评估器](../architecture/quality-evaluator.md) | 质量评估需要准确性测试 |
| [IP-001: 实施路线图](../implementation-roadmap/01-roadmap.md) | 测试策略是实施的前置条件 |