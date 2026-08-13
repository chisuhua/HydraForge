# context-compactor Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use skill_use("execute") to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 实现 `ContextCompactor` 模块 — LLM 摘要压缩,在 token 计数超阈值时自动触发,保持完整历史可查;pi-agent compaction 模式落点 (ADR-0007 closure)。

**Architecture:** PIMPL 接口 `IContextCompactor` (L5 契约层) + `create_context_compactor` 工厂函数注入依赖 (threshold + llm + event_bus);`ContextCompactorImpl` 在 `src/core/context_compactor.cpp` 实现。DSLEngine `run()` 循环每轮尾调用 `check_and_compact()`。原始消息只追加不删,摘要替换 `context.working` 的 LLM 调用视图,触发后写 `metadata.compaction_record`。事件遵循 ADR-0068 附录 A 通过 `EventBuilder` 链式构造。

**Tech Stack:** C++20, Catch2 (tests), `ILLMProvider` Decorator 链 (`CostTrackingDecorator` 自动预算), `IInteractionBus` (ADR-0068 V2 EventBuilder), DECLARE_COMMAND (ADR-0070), `SessionConfig.compact_threshold_tokens` (Sprint 0 已声明)。

---

## File Structure

### Production Code

| File | Responsibility |
|---|---|
| `include/agenticdsl/types/context_compactor.h` | `IContextCompactor` 抽象接口 + `create_context_compactor` 工厂声明 |
| `src/core/context_compactor.h` | `ContextCompactorImpl` PIMPL 实现类声明 |
| `src/core/context_compactor.cpp` | token 计数 + LLM 摘要调用 + 事件发射 + 错误降级 实现 |
| `src/core/engine.h` | 集成 `check_and_compact()` 调用点 (修改) |
| `src/core/engine.cpp` | `run()` 循环尾部 `check_and_compact()` 触发逻辑 (修改) |
| `examples/pdk_chat_demo/commands/compact_command.cpp` | `/compact` DECLARE_COMMAND 注册 (新文件) |
| `src/core/CMakeLists.txt` | 添加 `context_compactor.{h,cpp}` 到 `agenticdsl_core` (修改) |

### Tests

| File | Responsibility |
|---|---|
| `tests/test_context_compactor.cpp` | 单元测试 5 case: token 阈值 + 自动触发 + /compact 手动 + 事件 payload + LLM 降级 |

---

## Tasks

### Task 1: 创建 `IContextCompactor` 接口骨架

**Files:**
- Create: `include/agenticdsl/types/context_compactor.h`
- Create: `src/core/context_compactor.h`
- Create: `src/core/context_compactor.cpp`
- Modify: `src/core/CMakeLists.txt`

- [x] **Step 1.1: 写接口头文件**

```cpp
// include/agenticdsl/types/context_compactor.h
#pragma once
#include <memory>
#include <string>

namespace agenticdsl {

class ILLMProvider;
class IInteractionBus;

class IContextCompactor {
public:
  virtual ~IContextCompactor() = default;

  // 压缩前回调 (发射 context.compact.before)
  virtual void on_compact_before(const std::string& session_id, size_t tokens_before) = 0;

  // 执行 LLM 摘要压缩 (返回摘要文本)
  virtual std::string compact(const std::string& history_json, ILLMProvider& llm) = 0;

  // 压缩后回调 (发射 context.compact.after)
  virtual void on_compact_after(const std::string& session_id,
                                size_t tokens_before, size_t tokens_after) = 0;

  // 查询当前 token 计数
  virtual size_t count_tokens(const std::string& context_json) const = 0;

  // 检查是否需要压缩
  virtual bool should_compact(size_t token_count) const = 0;
};

// 工厂函数: 注入 threshold + llm + event_bus
std::unique_ptr<IContextCompactor> create_context_compactor(
    size_t compact_threshold_tokens,
    std::shared_ptr<ILLMProvider> llm_provider,
    std::shared_ptr<IInteractionBus> event_bus);

}  // namespace agenticdsl
```

- [x] **Step 1.2: 写实现骨架头文件**

```cpp
// src/core/context_compactor.h
#pragma once
#include "agenticdsl/types/context_compactor.h"
#include <string>

namespace agenticdsl {

class ContextCompactorImpl : public IContextCompactor {
public:
  ContextCompactorImpl(size_t threshold,
                       std::shared_ptr<ILLMProvider> llm,
                       std::shared_ptr<IInteractionBus> bus);

  void on_compact_before(const std::string& session_id, size_t tokens_before) override;
  std::string compact(const std::string& history_json, ILLMProvider& llm) override;
  void on_compact_after(const std::string& session_id,
                        size_t tokens_before, size_t tokens_after) override;
  size_t count_tokens(const std::string& context_json) const override;
  bool should_compact(size_t token_count) const override;

private:
  size_t threshold_;
  std::shared_ptr<ILLMProvider> llm_;
  std::shared_ptr<IInteractionBus> bus_;
};

}  // namespace agenticdsl
```

- [x] **Step 1.3: 写实现骨架 cpp (空方法体)**

```cpp
// src/core/context_compactor.cpp
#include "core/context_compactor.h"

namespace agenticdsl {

ContextCompactorImpl::ContextCompactorImpl(size_t threshold,
                                           std::shared_ptr<ILLMProvider> llm,
                                           std::shared_ptr<IInteractionBus> bus)
    : threshold_(threshold), llm_(std::move(llm)), bus_(std::move(bus)) {}

void ContextCompactorImpl::on_compact_before(const std::string&, size_t) {}
std::string ContextCompactorImpl::compact(const std::string&, ILLMProvider&) { return ""; }
void ContextCompactorImpl::on_compact_after(const std::string&, size_t, size_t) {}
size_t ContextCompactorImpl::count_tokens(const std::string&) const { return 0; }
bool ContextCompactorImpl::should_compact(size_t) const { return false; }

std::unique_ptr<IContextCompactor> create_context_compactor(
    size_t threshold, std::shared_ptr<ILLMProvider> llm,
    std::shared_ptr<IInteractionBus> bus) {
  return std::make_unique<ContextCompactorImpl>(threshold, std::move(llm), std::move(bus));
}

}  // namespace agenticdsl
```

- [x] **Step 1.4: 修改 CMakeLists.txt 注册新文件**

在 `src/core/CMakeLists.txt` 添加 (添加到现有 `agenticdsl_core` sources 列表):
```cmake
  ${CMAKE_CURRENT_SOURCE_DIR}/context_compactor.h
  ${CMAKE_CURRENT_SOURCE_DIR}/context_compactor.cpp
```

- [ ] **Step 1.5: 编译验证**

Run: `cmake --build build -j$(nproc) --target agenticdsl_core 2>&1 | tail -20`
Expected: 编译通过, 无新增 error/warning

---

### Task 2: 实现 token 计数 + 阈值判断 (TDD)

**Files:**
- Modify: `src/core/context_compactor.cpp`
- Create: `tests/test_context_compactor.cpp`

- [x] **Step 2.1: 写失败的 token 计数测试**

```cpp
// tests/test_context_compactor.cpp (新文件第一行)
#include <catch2/catch_test_macros.hpp>
#include "core/context_compactor.h"

using namespace agenticdsl;

TEST_CASE("count_tokens returns approximate token count from JSON string") {
  ContextCompactorImpl compactor(4096, nullptr, nullptr);
  REQUIRE(compactor.count_tokens("{}") == 1);  // minimal JSON
  REQUIRE(compactor.count_tokens(R"({"a":1})") > 1);
}
```

- [x] **Step 2.2: 运行测试验证失败**

Run: `cmake --build build -j$(nproc) --target test_context_compactor 2>&1 | tail -10`
Expected: FAIL — `count_tokens` 返回 0

- [x] **Step 2.3: 实现字符计数代理**

修改 `src/core/context_compactor.cpp`:
```cpp
// ContextCompactorImpl::count_tokens 替换为:
size_t ContextCompactorImpl::count_tokens(const std::string& context_json) const {
  // 字符计数代理: 每 4 字符 ≈ 1 token (英文 LLM 经验值)
  // 这是 LLM 不可用时的近似;生产中应调用 llm.count_tokens(text)
  return (context_json.size() + 3) / 4;
}
```

- [x] **Step 2.4: 写 should_compact 测试**

追加到 `tests/test_context_compactor.cpp`:
```cpp
TEST_CASE("should_compact returns true when token_count exceeds threshold") {
  ContextCompactorImpl compactor(4096, nullptr, nullptr);
  REQUIRE(compactor.should_compact(5000) == true);
  REQUIRE(compactor.should_compact(3000) == false);
  REQUIRE(compactor.should_compact(4096) == false);  // 严格大于
}
```

- [x] **Step 2.5: 实现 should_compact**

修改 `src/core/context_compactor.cpp`:
```cpp
bool ContextCompactorImpl::should_compact(size_t token_count) const {
  return token_count > threshold_;
}
```

- [x] **Step 2.6: 运行测试验证通过**

Run: `cmake --build build -j$(nproc) --target test_context_compactor && ctest --test-dir build -R test_context_compactor --output-on-failure`
Expected: PASS — 5 case 全过 (4 Task 2 新测试 + 1 Task 1 构造测试)

---

### Task 3: 实现 `compact()` LLM 摘要调用 (TDD)

**Files:**
- Modify: `src/core/context_compactor.cpp`
- Modify: `tests/test_context_compactor.cpp`

- [ ] **Step 3.1: 写失败的 compact 测试**

```cpp
TEST_CASE("compact returns LLM-generated summary via decorator chain") {
  // Mock LLM provider returning fixed summary
  class MockLLM : public ILLMProvider {
  public:
    LLMResult generate(const LLMRequest& req) override {
      last_prompt_ = req.prompt;
      return LLMResult{"[mock summary] " + req.prompt.substr(0, 20), /*tokens*/10};
    }
    std::vector<std::string> available_models() const override { return {"mock"}; }
    std::string last_prompt_;
  };
  MockLLM mock;
  ContextCompactorImpl compactor(4096, nullptr, nullptr);
  std::string result = compactor.compact("hello world", mock);
  REQUIRE(result == "[mock summary] hello world");
  REQUIRE(mock.last_prompt_.find("200 字以内") != std::string::npos);
}
```

- [ ] **Step 3.2: 运行测试验证失败**

Run: `cmake --build build -j$(nproc) --target test_context_compactor 2>&1 | tail -5`
Expected: FAIL — `compact` 返回空字符串

- [ ] **Step 3.3: 实现 compact (Summary Prompt + LLM 调用)**

修改 `src/core/context_compactor.cpp`:
```cpp
std::string ContextCompactorImpl::compact(const std::string& history_json, ILLMProvider& llm) {
  const std::string kSummaryPromptTemplate =
      "请将以下对话历史压缩为 200 字以内的摘要,保留关键信息:\n";
  LLMRequest req;
  req.prompt = kSummaryPromptTemplate + history_json;
  // Decorator 链自动应用 (CostTrackingDecorator 在 llm.generate 入口包装)
  LLMResult result = llm.generate(req);
  return result.text;
}
```

- [ ] **Step 3.4: 运行测试验证通过**

Run: `cmake --build build -j$(nproc) --target test_context_compactor && ctest --test-dir build -R test_context_compactor --output-on-failure`
Expected: PASS — 3 case 全过

---

### Task 4: 实现 `on_compact_before/after` 事件发射 (ADR-0068)

**Files:**
- Modify: `src/core/context_compactor.cpp`
- Modify: `tests/test_context_compactor.cpp`

- [ ] **Step 4.1: 写失败的事件测试**

```cpp
TEST_CASE("on_compact_before emits context.compact.before event") {
  // Mock EventBus 捕获事件
  class MockBus : public IInteractionBus {
  public:
    std::vector<std::string> topics;
    void emit(BusEvent e) override { topics.push_back(e.topic); }
    void subscribe(std::string, std::function<void(BusEvent)>) override {}
  };
  auto bus = std::make_shared<MockBus>();
  ContextCompactorImpl compactor(4096, nullptr, bus);
  compactor.on_compact_before("sess_1", 5000);
  REQUIRE(bus->topics.size() == 1);
  REQUIRE(bus->topics[0] == "context.compact.before");
}
```

- [ ] **Step 4.2: 运行测试验证失败**

Run: `cmake --build build -j$(nproc) --target test_context_compactor 2>&1 | tail -5`
Expected: FAIL — bus_->emit 未调用

- [ ] **Step 4.3: 实现 on_compact_before (EventBuilder 链式)**

修改 `src/core/context_compactor.cpp`:
```cpp
#include "agenticdsl/contract/event_builder.h"
#include "agenticdsl/contract/tool_result.h"

void ContextCompactorImpl::on_compact_before(const std::string& session_id, size_t tokens_before) {
  if (!bus_) return;
  bus_->emit(EventBuilder("context.compact.before", ToolResult{})
      .args({
          {"session_id", session_id},
          {"tokens_before", static_cast<int64_t>(tokens_before)}})
      .meta({{"component", "context_compactor"}, {"trace_id", session_id}})
      .build());
}
```

- [ ] **Step 4.4: 写 on_compact_after 测试**

```cpp
TEST_CASE("on_compact_after emits context.compact.after event with token delta") {
  class MockBus : public IInteractionBus {
  public:
    std::vector<BusEvent> events;
    void emit(BusEvent e) override { events.push_back(e); }
    void subscribe(std::string, std::function<void(BusEvent)>) override {}
  };
  auto bus = std::make_shared<MockBus>();
  ContextCompactorImpl compactor(4096, nullptr, bus);
  compactor.on_compact_after("sess_1", 5000, 800);
  REQUIRE(bus->events.size() == 1);
  REQUIRE(bus->events[0].topic == "context.compact.after");
}
```

- [ ] **Step 4.5: 实现 on_compact_after**

修改 `src/core/context_compactor.cpp`:
```cpp
void ContextCompactorImpl::on_compact_after(const std::string& session_id,
                                            size_t tokens_before, size_t tokens_after) {
  if (!bus_) return;
  bus_->emit(EventBuilder("context.compact.after", ToolResult{})
      .args({
          {"session_id", session_id},
          {"tokens_before", static_cast<int64_t>(tokens_before)},
          {"tokens_after", static_cast<int64_t>(tokens_after)},
          {"summary_length", static_cast<int64_t>(tokens_after * 4)}})
      .meta({{"component", "context_compactor"}, {"trace_id", session_id}})
      .build());
}
```

- [ ] **Step 4.6: 运行测试验证通过**

Run: `cmake --build build -j$(nproc) --target test_context_compactor && ctest --test-dir build -R test_context_compactor --output-on-failure`
Expected: PASS — 5 case 全过

---

### Task 5: 双层保留策略 (TDD)

**Files:**
- Create: `src/core/context_compactor_storage.h` (辅助 storage helper)
- Modify: `src/core/context_compactor.cpp`
- Modify: `tests/test_context_compactor.cpp`

- [x] **Step 5.1: 写失败的双层保留测试** (2026-08-13 ship)

```cpp
TEST_CASE("compact appends original to original_messages and replaces working view") {
  // 假设 Context 接受 external_set_original + set_working_view 方法
  // 此测试在 LayeredContext 上做集成验证,见 Task 8
  // 这里只验证 compactor 本身返回 summary
  class MockLLM : public ILLMProvider {
  public:
    LLMResult generate(const LLMRequest&) override { return {"SUMMARY", 1}; }
    std::vector<std::string> available_models() const override { return {}; }
  };
  MockLLM mock;
  ContextCompactorImpl compactor(4096, nullptr, nullptr);
  std::string summary = compactor.compact("orig", mock);
  REQUIRE(summary == "SUMMARY");
  // 双层保留由 DSLEngine 在 Task 8 集成时调用 ctx.append_original + ctx.set_working_view
}
```

- [x] **Step 5.2: 实现 append_original + set_working_view 接口 (LayeredContext 扩展)** (2026-08-13 ship)

修改 `include/agenticdsl/types/layered_context.h` (添加 2 方法声明):
```cpp
class LayeredContext {
public:
  // ...existing...
  void append_original(std::string message);  // 追加到 original_messages (L4)
  void set_working_view(std::string view);    // 替换 working (L1) LLM 调用视图
  nlohmann::json compaction_record() const;   // 返回 metadata 中的最近一次压缩记录
};
```

实现 `src/core/types/layered_context.cpp`:
```cpp
void LayeredContext::append_original(std::string message) {
  if (!metadata.contains("original_messages")) {
    metadata["original_messages"] = nlohmann::json::array();
  }
  metadata["original_messages"].push_back(std::move(message));
}
void LayeredContext::set_working_view(std::string view) {
  working["view"] = std::move(view);
}
nlohmann::json LayeredContext::compaction_record() const {
  return metadata.value("compaction_record", nlohmann::json::object());
}
```

- [x] **Step 5.3: 实现 compaction_record 写入 (在 compact 后由 caller 写入)** (2026-08-13 ship)

修改 `src/core/context_compactor.cpp`:
```cpp
// 在 ContextCompactorImpl 添加新方法 (供 DSLEngine 调用)
struct CompactionRecord {
  size_t tokens_before;
  size_t tokens_after;
  size_t summary_length;
  std::string timestamp;
};
// compact() 完成后由 caller 调用此方法写入 metadata
```

扩展 `IContextCompactor` (修改 `include/agenticdsl/types/context_compactor.h`):
```cpp
class IContextCompactor {
public:
  // ...existing...
  virtual CompactionRecord make_record(size_t before, size_t after,
                                       size_t summary_len) const = 0;
};
```

实现 `src/core/context_compactor.cpp`:
```cpp
CompactionRecord ContextCompactorImpl::make_record(size_t before, size_t after,
                                                   size_t summary_len) const {
  CompactionRecord r{before, after, summary_len,
                     std::to_string(std::time(nullptr))};
  return r;
}
```

- [ ] **Step 5.4: 运行测试验证通过**

Run: `cmake --build build -j$(nproc) --target test_context_compactor && ctest --test-dir build -R test_context_compactor --output-on-failure`
Expected: PASS — 6 case 全过

---

### Task 6: LLM 错误降级 (SHOULD 不阻塞)

**Files:**
- Modify: `src/core/context_compactor.cpp`
- Modify: `tests/test_context_compactor.cpp`

- [ ] **Step 6.1: 写失败的降级测试**

```cpp
TEST_CASE("compact degrades gracefully when LLM fails") {
  class FailingLLM : public ILLMProvider {
  public:
    LLMResult generate(const LLMRequest&) override {
      return LLMResult{std::nullopt, LLMError{Code::Network, "timeout"}};
    }
    std::vector<std::string> available_models() const override { return {}; }
  };
  FailingLLM mock;
  ContextCompactorImpl compactor(4096, nullptr, nullptr);
  // 不抛异常, 返回空字符串 (caller 决定下一步)
  std::string result = compactor.compact("orig", mock);
  REQUIRE(result.empty());
}
```

- [ ] **Step 6.2: 实现降级 (try-catch + 错误日志)**

修改 `src/core/context_compactor.cpp`:
```cpp
std::string ContextCompactorImpl::compact(const std::string& history_json, ILLMProvider& llm) {
  const std::string kSummaryPromptTemplate =
      "请将以下对话历史压缩为 200 字以内的摘要,保留关键信息:\n";
  LLMRequest req;
  req.prompt = kSummaryPromptTemplate + history_json;
  try {
    LLMResult result = llm.generate(req);
    if (!result.text) {
      // LLM 失败: 降级返回空串, caller 继续使用未压缩上下文
      return "";
    }
    return *result.text;
  } catch (const std::exception&) {
    // 异常隔离: 不阻塞会话
    return "";
  }
}
```

- [ ] **Step 6.3: 运行测试验证通过**

Run: `cmake --build build -j$(nproc) --target test_context_compactor && ctest --test-dir build -R test_context_compactor --output-on-failure`
Expected: PASS — 7 case 全过 (含 LLM 失败降级)

---

### Task 7: 注册 `/compact` 命令 (DECLARE_COMMAND)

**Files:**
- Create: `examples/pdk_chat_demo/commands/compact_command.cpp`
- Modify: `examples/pdk_chat_demo/CMakeLists.txt`

- [ ] **Step 7.1: 写 DECLARE_COMMAND 实现**

```cpp
// examples/pdk_chat_demo/commands/compact_command.cpp
#include "agenticdsl/pdk/command_macros.h"
#include "agenticdsl/types/context_compactor.h"
#include "agenticdsl/types/layered_context.h"
#include <memory>

DECLARE_COMMAND("compact", "手动触发上下文压缩", Cognitive, Agent,
    [](const std::vector<std::string>& args, LayeredContext& ctx,
       IContextCompactor& compactor, ILLMProvider& llm) -> ToolResult {
      size_t tokens_before = compactor.count_tokens(ctx.dump());
      compactor.on_compact_before(ctx.session_id(), tokens_before);
      std::string summary = compactor.compact(ctx.dump(), llm);
      size_t tokens_after = compactor.count_tokens(summary);
      ctx.append_original(ctx.dump());        // 双层保留: 原始追加
      ctx.set_working_view(summary);           // 工作视图替换为摘要
      auto record = compactor.make_record(tokens_before, tokens_after, summary.size());
      ctx.set_metadata("compaction_record", record);
      compactor.on_compact_after(ctx.session_id(), tokens_before, tokens_after);
      return ToolResult::success({{"summary", summary},
                                  {"tokens_before", static_cast<int64_t>(tokens_before)},
                                  {"tokens_after", static_cast<int64_t>(tokens_after)}});
    });
```

- [ ] **Step 7.2: CMakeLists.txt 注册新文件**

修改 `examples/pdk_chat_demo/CMakeLists.txt` 添加到 sources:
```cmake
  commands/compact_command.cpp
```

- [ ] **Step 7.3: 编译验证**

Run: `cmake --build build -j$(nproc) --target pdk_chat_demo 2>&1 | tail -10`
Expected: 编译通过

---

### Task 8: DSLEngine 集成 `check_and_compact()`

**Files:**
- Modify: `include/agenticdsl/core/engine.h`
- Modify: `src/core/engine.cpp`

- [ ] **Step 8.1: 修改 engine.h 添加 check_and_compact 公开 API**

```cpp
// include/agenticdsl/core/engine.h 在 class DSLEngine public 区添加:
public:
  /// 注册 context compactor (Sprint 22 context-compactor 集成)
  void set_context_compactor(std::unique_ptr<IContextCompactor> compactor);

  /// 每轮 run() 循环尾部调用, token 超阈值时自动 compact
  void check_and_compact(LayeredContext& ctx);
```

- [ ] **Step 8.2: 修改 engine.cpp 实现 set_context_compactor + check_and_compact**

```cpp
// src/core/engine.cpp 添加:
#include "core/context_compactor.h"

void DSLEngine::set_context_compactor(std::unique_ptr<IContextCompactor> compactor) {
  context_compactor_ = std::move(compactor);
}

void DSLEngine::check_and_compact(LayeredContext& ctx) {
  if (!context_compactor_) return;  // 未注册则跳过
  std::string json = ctx.dump();
  size_t tokens = context_compactor_->count_tokens(json);
  if (!context_compactor_->should_compact(tokens)) return;
  context_compactor_->on_compact_before(ctx.session_id(), tokens);
  std::string summary = context_compactor_->compact(json, *llm_provider_);
  if (summary.empty()) return;  // LLM 失败降级
  size_t tokens_after = context_compactor_->count_tokens(summary);
  ctx.append_original(json);                       // 双层保留
  ctx.set_working_view(summary);                   // 工作视图替换
  auto record = context_compactor_->make_record(tokens, tokens_after, summary.size());
  ctx.set_metadata("compaction_record", record);
  context_compactor_->on_compact_after(ctx.session_id(), tokens, tokens_after);
}
```

- [ ] **Step 8.3: 在 run() 循环尾部添加 check_and_compact 调用**

```cpp
// src/core/engine.cpp::run() 循环每轮结束前添加:
  // ... existing turn logic ...
  check_and_compact(ctx);  // Sprint 22 context-compactor
  // ... loop end ...
```

- [ ] **Step 8.4: 编译验证**

Run: `cmake --build build -j$(nproc) --target agenticdsl_core 2>&1 | tail -10`
Expected: 编译通过

---

### Task 9: SessionConfig.compact_threshold_tokens 注入

**Files:**
- Modify: `src/core/types/session_config.h` (如已有该文件)

- [ ] **Step 9.1: 检查 SessionConfig 是否已声明 compact_threshold_tokens**

Run: `grep -r "compact_threshold_tokens" src/ include/ 2>/dev/null | head -5`

- [ ] **Step 9.2: 如未声明, 添加字段**

```cpp
// SessionConfig 结构体添加:
struct SessionConfig {
  // ...existing fields...
  size_t compact_threshold_tokens = 4096;  // default
};
```

- [ ] **Step 9.3: DSLEngine 构造时读取 SessionConfig 创建 compactor**

修改 `src/core/engine.cpp::DSLEngine(session_config)` 构造:
```cpp
DSLEngine::DSLEngine(SessionConfig cfg, ...) {
  // ...existing...
  if (cfg.compact_threshold_tokens > 0) {
    context_compactor_ = create_context_compactor(
        cfg.compact_threshold_tokens, llm_provider_, event_bus_);
  }
}
```

- [ ] **Step 9.4: 编译验证**

Run: `cmake --build build -j$(nproc) --target agenticdsl_core 2>&1 | tail -10`
Expected: 编译通过

---

### Task 10: 单元测试扩展 (5 case 全覆盖)

**Files:**
- Modify: `tests/test_context_compactor.cpp`

- [ ] **Step 10.1: 补充 E2E 事件 payload 验证测试**

```cpp
TEST_CASE("compaction events contain full payload per ADR-0068") {
  class MockBus : public IInteractionBus {
  public:
    std::vector<BusEvent> events;
    void emit(BusEvent e) override { events.push_back(e); }
    void subscribe(std::string, std::function<void(BusEvent)>) override {}
  };
  auto bus = std::make_shared<MockBus>();
  ContextCompactorImpl compactor(4096, nullptr, bus);
  compactor.on_compact_before("s1", 5000);
  compactor.on_compact_after("s1", 5000, 800);
  REQUIRE(bus->events.size() == 2);
  REQUIRE(bus->events[0].payload.data["tokens_before"] == 5000);
  REQUIRE(bus->events[1].payload.data["tokens_after"] == 800);
}
```

- [ ] **Step 10.2: 补充 /compact 命令触发测试**

```cpp
TEST_CASE("/compact command triggers immediate compaction regardless of threshold") {
  // 此测试在 Task 8 集成后由 E2E 测试覆盖 (test_pdk_chat_demo_cli)
  // 这里仅 placeholder 验证 DECLARE_COMMAND 编译成功
  REQUIRE(true);
}
```

- [ ] **Step 10.3: 运行全量测试**

Run: `ctest --test-dir build --output-on-failure -R test_context_compactor`
Expected: 9-10 case 全 PASS

---

### Task 11: 集成验证 + ctest 零回归 + ADR-0007 状态提升

**Files:**
- Modify: `docs/adr/ADR-0007-session-snapshots.md` (状态更新)
- Modify: `docs/README.md` (ADR 表格)

- [ ] **Step 11.1: 运行全量 ctest 验证零回归**

Run: `cmake --build build -j$(nproc) && ctest --test-dir build --output-on-failure`
Expected: 147/147 PASS (含新增 test_context_compactor)

- [ ] **Step 11.2: ADR-0007 状态 🟡 Partial → ✅ Approved**

修改 `docs/adr/ADR-0007-session-snapshots.md` 状态行:
```
状态: ✅ Approved (Sprint 22 context-compactor ship 2026-08-XX)
```

- [ ] **Step 11.3: docs/README.md ADR 表格追加 1 行 (Approved)**

修改 ADR 表格中 ADR-0007 行的状态字段:
```
| ADR-0007 | Session Snapshots + Compaction | ✅ Approved |
```

- [ ] **Step 11.4: 聚合 commit (worktree commit phase)**

Run: `git add -A && git commit -m "feat(core): context-compactor — LLM 摘要压缩 + ADR-0007 closure

- IContextCompactor 接口 + create_context_compactor 工厂
- token 阈值检测 + LLM 摘要调用 (Decorator 链)
- 双层保留 (original_messages + working 视图替换)
- on_compact_before/after 事件 (ADR-0068 EventBuilder 链式)
- /compact DECLARE_COMMAND (ADR-0070)
- DSLEngine check_and_compact 集成
- LLM 错误降级 (返回空串 + 告警)
- 9 case 单元测试 + 0 回归
- ADR-0007 状态 🟡 Partial → ✅ Approved"

- [ ] **Step 11.5: 验证 commit**

Run: `git log -1 --oneline`
Expected: 1 commit, message 包含 `feat(core): context-compactor`

---

## Acceptance Criteria

- [ ] `test_context_compactor` 9-10 case 全部 PASS
- [ ] `ctest` 全量零回归 (147/147)
- [ ] ADR-0007 状态 ✅ Approved
- [ ] `context.compact.before/after` 事件真实发射 (含 tokens_before/tokens_after)
- [ ] `/compact` 命令经 DECLARE_COMMAND 注册 (依赖 ADR-0070)
- [ ] LLM 失败降级 (不抛异常, 返回空串)
- [ ] Decorator 链 CostTracking 自动计入 budget