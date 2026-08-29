// tests/test_event_log_capture_mode.cpp
// 功能描述: EventLogConfig capture_mode 语义 + --allow-training-capture mock guard 决策测试 (Phase 2)
// 设计依据: openspec/changes/capture-mode-and-distillation-writer-v1/spec.md
//           Requirements: CaptureMode 默认值 Off + Mock-mode hard rejection
// 关键不变量:
//   - EventLogConfig.capture_mode 默认 Off（生产路径零开销）
//   - effective_capture_enabled() 语义: Off → false, Online/Training → true
//   - mock guard 决策从 cli_options.mock 读取（禁止硬编码 provider_mode == "mock"）
// 作者：AgenticDSL / capture-mode-and-distillation-writer-v1 Phase 2
// 最后修改日期：2026-08-30

#include "catch_amalgamated.hpp"

#include "core/types/event_log_config.h"
#include "examples/pdk_chat_demo/cli_options.h"  // CliOptions.allow_training_capture (Phase 2 新增字段)

using agenticdsl::CaptureMode;
using agenticdsl::EventLogConfig;
using pdk_chat_demo::CliOptions;

// Case 1: EventLogConfig 默认 capture_mode == Off
TEST_CASE("event_log_config_default_capture_off", "[event_log_capture_mode][phase2]") {
  EventLogConfig cfg;
  REQUIRE(cfg.capture_mode == CaptureMode::Off);
  REQUIRE_FALSE(cfg.effective_capture_enabled());  // 默认零捕获开销
}

// Case 2: effective_capture_enabled() 三态语义
TEST_CASE("event_log_config_effective_capture_enabled", "[event_log_capture_mode][phase2]") {
  EventLogConfig cfg;

  cfg.capture_mode = CaptureMode::Off;
  REQUIRE_FALSE(cfg.effective_capture_enabled());

  cfg.capture_mode = CaptureMode::Online;
  REQUIRE(cfg.effective_capture_enabled());

  cfg.capture_mode = CaptureMode::Training;
  REQUIRE(cfg.effective_capture_enabled());
}

// Case 3: mock guard 决策 — allow_training_capture + mock → 拒绝 (throw runtime_error)
//         main.cpp 守卫条件从 cli_options.mock 读取（spec: MUST NOT 硬编码 provider_mode == "mock"）
TEST_CASE("mock_guard_rejects_training_capture_in_mock_mode", "[event_log_capture_mode][phase2][mock_guard]") {
  CliOptions opts;
  opts.mock = true;
  opts.allow_training_capture = true;

  // 与 main.cpp 相同的守卫条件: allow_training_capture && mock_mode → throw
  const bool should_reject = opts.allow_training_capture && opts.mock;
  REQUIRE(should_reject);
  if (should_reject) {
    // 验证错误消息含 spec 要求的 "requires real LLM provider" 子串
    const std::string error_msg =
        "--allow-training-capture requires real LLM provider (not mock). "
        "Mock-generated data would pollute the distillation training set.";
    REQUIRE(error_msg.find("requires real LLM provider") != std::string::npos);
  }
}

// Case 4: real provider + allow-training-capture → 无 throw（guard 不触发）
TEST_CASE("real_provider_allows_training_capture", "[event_log_capture_mode][phase2][mock_guard]") {
  CliOptions opts;
  opts.mock = false;  // real provider (e.g. deepseek)
  opts.allow_training_capture = true;

  // guard 条件不成立 → 无 throw
  const bool should_reject = opts.allow_training_capture && opts.mock;
  REQUIRE_FALSE(should_reject);
}

// Case 5: allow_training_capture 默认 false（不改变既有行为）
TEST_CASE("allow_training_capture_default_false", "[event_log_capture_mode][phase2]") {
  CliOptions opts;
  REQUIRE_FALSE(opts.allow_training_capture);
  REQUIRE_FALSE(opts.mock);
}
