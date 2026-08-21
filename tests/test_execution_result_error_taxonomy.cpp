// tests/test_execution_result_error_taxonomy.cpp
// 功能描述：ExecutionResult 错误分类验证（P9 error-taxonomy-execution-boundary）
//          3 类错误 × 3 路径 = 9 cases: is_retryable + record_failure 分流
// 设计依据：openspec/changes/error-taxonomy-execution-boundary (P9)
// 作者：HydraForge Sprint 22 P9 ship
// 最后修改日期：2026-08-20

#include "catch_amalgamated.hpp"

#include "core/types/execution_result.h"
#include "core/types/session.h"

#include <optional>
#include <string>

using namespace agenticdsl;

namespace {

// 构造可重试失败的 ExecutionResult
ExecutionResult make_retryable_failure(ErrorCode code) {
  ExecutionResult r;
  r.success = false;
  r.message = "retryable failure";
  r.error_code = code;
  return r;
}

// 构造不可重试失败的 ExecutionResult
ExecutionResult make_non_retryable_failure(ErrorCode code) {
  ExecutionResult r;
  r.success = false;
  r.message = "non-retryable failure";
  r.error_code = code;
  return r;
}

// 构造成功的 ExecutionResult（无 error_code）
ExecutionResult make_success() {
  ExecutionResult r;
  r.success = true;
  r.message = "ok";
  return r;
}

}  // namespace

TEST_CASE("is_retryable: 可重试错误码返回 true（5 类）",
          "[error_taxonomy][P9][is_retryable]") {
  REQUIRE(make_retryable_failure(ErrorCode::Retry).is_retryable());
  REQUIRE(make_retryable_failure(ErrorCode::Timeout).is_retryable());
  REQUIRE(make_retryable_failure(ErrorCode::ResourceExhausted).is_retryable());
  REQUIRE(make_retryable_failure(ErrorCode::MaxStepsExceeded).is_retryable());
  REQUIRE(make_retryable_failure(ErrorCode::Crash).is_retryable());
  REQUIRE(make_retryable_failure(ErrorCode::BudgetExhausted).is_retryable());
}

TEST_CASE("is_retryable: 不可重试错误码返回 false（非可重试类）",
          "[error_taxonomy][P9][is_retryable]") {
  REQUIRE_FALSE(
      make_non_retryable_failure(ErrorCode::PermissionDenied).is_retryable());
  REQUIRE_FALSE(make_non_retryable_failure(ErrorCode::PathViolation).is_retryable());
  REQUIRE_FALSE(
      make_non_retryable_failure(ErrorCode::DangerousCommand).is_retryable());
  REQUIRE_FALSE(
      make_non_retryable_failure(ErrorCode::ToolNotRegistered).is_retryable());
  REQUIRE_FALSE(make_non_retryable_failure(ErrorCode::Unknown).is_retryable());
  REQUIRE_FALSE(make_non_retryable_failure(ErrorCode::Abort).is_retryable());
  REQUIRE_FALSE(make_non_retryable_failure(ErrorCode::InvalidArg).is_retryable());
}

TEST_CASE("is_retryable: 成功结果（无 error_code）返回 false",
          "[error_taxonomy][P9][is_retryable]") {
  ExecutionResult r = make_success();
  REQUIRE_FALSE(r.is_retryable());
}

TEST_CASE("record_failure: 可重试错误递增 failure_count",
          "[error_taxonomy][P9][record_failure]") {
  UserSession user("p9-retry-user");
  auto& ts = user.create_task_session();

  REQUIRE(ts.failure_count() == 0);
  ts.record_failure(make_retryable_failure(ErrorCode::Timeout));
  REQUIRE(ts.failure_count() == 1);
  ts.record_failure(make_retryable_failure(ErrorCode::Retry));
  REQUIRE(ts.failure_count() == 2);
  ts.record_failure(make_retryable_failure(ErrorCode::ResourceExhausted));
  REQUIRE(ts.failure_count() == 3);
  REQUIRE(ts.determine_failure_mode() == TaskSession::FailureMode::NewSession);
}

TEST_CASE("record_failure: 不可重试错误不递增 failure_count",
          "[error_taxonomy][P9][record_failure]") {
  UserSession user("p9-nonretry-user");
  auto& ts = user.create_task_session();

  REQUIRE(ts.failure_count() == 0);
  ts.record_failure(make_non_retryable_failure(ErrorCode::PermissionDenied));
  REQUIRE(ts.failure_count() == 0);
  ts.record_failure(make_non_retryable_failure(ErrorCode::PathViolation));
  REQUIRE(ts.failure_count() == 0);
  REQUIRE(ts.determine_failure_mode() == TaskSession::FailureMode::KeepSession);
}

TEST_CASE("record_failure: 成功结果不递增 failure_count",
          "[error_taxonomy][P9][record_failure]") {
  UserSession user("p9-success-user");
  auto& ts = user.create_task_session();

  REQUIRE(ts.failure_count() == 0);
  ts.record_failure(make_success());
  REQUIRE(ts.failure_count() == 0);
  REQUIRE(ts.determine_failure_mode() == TaskSession::FailureMode::KeepSession);
}

TEST_CASE("record_failure: 混合（可重试 + 不可重试）只递增可重试部分",
          "[error_taxonomy][P9][record_failure]") {
  UserSession user("p9-mixed-user");
  auto& ts = user.create_task_session();

  ts.record_failure(make_retryable_failure(ErrorCode::Crash));
  ts.record_failure(make_non_retryable_failure(ErrorCode::Unknown));
  ts.record_failure(make_retryable_failure(ErrorCode::Timeout));
  ts.record_failure(make_non_retryable_failure(ErrorCode::Abort));

  // 只有 2 次可重试递增
  REQUIRE(ts.failure_count() == 2);
  REQUIRE(ts.determine_failure_mode() == TaskSession::FailureMode::KeepSession);
}

TEST_CASE("ExecutionResult 无 error_code 时兼容旧行为（失败可重试）",
          "[error_taxonomy][P9][compat]") {
  ExecutionResult r;
  r.success = false;
  r.message = "legacy failure";
  // 无 error_code → 旧行为：失败视为可重试
  REQUIRE(r.is_retryable());

  ExecutionResult s;
  s.success = true;
  REQUIRE_FALSE(s.is_retryable());
}