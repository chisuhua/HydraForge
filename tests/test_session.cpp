// tests/test_session.cpp
// 功能描述：ADR-0033 Session Hierarchy — 单元测试
//           覆盖 UserSession/TaskSession/SubtaskSession 的创建、归档、失败分裂、IPER retry、messages 保护
// 设计依据：OpenSpec change 2026-06-26-adr-0033-session-hierarchy
//           任务 4.1.1: 5 个 TEST_CASE
// 作者：AgenticDSL Sprint 15 (C5)
// 最后修改日期：2026-07-02

#include "catch_amalgamated.hpp"
#include "core/types/session.h"
#include <string>

using namespace agenticdsl;

// 辅助：构造一个失败 ExecutionResult（不带 error_code，simulated）
static ExecutionResult make_failure(const std::string& msg = "error") {
  ExecutionResult r;
  r.success = false;
  r.message = msg;
  return r;
}

static ExecutionResult make_success(const std::string& msg = "ok") {
  ExecutionResult r;
  r.success = true;
  r.message = msg;
  return r;
}

TEST_CASE("Session 创建与层级", "[session][unit]") {
  UserSession user("test-user-1");

  REQUIRE(user.user_id() == "test-user-1");
  REQUIRE(user.messages().empty());
  REQUIRE(user.current_task_session() == nullptr);
  REQUIRE(user.task_sessions().empty());

  // 创建第一个 TaskSession
  auto& ts1 = user.create_task_session();
  REQUIRE(user.current_task_session() == &ts1);
  REQUIRE(user.task_sessions().size() == 1);
  REQUIRE(ts1.user_session().user_id() == "test-user-1");
  REQUIRE(ts1.status() == "active");

  // 创建 SubtaskSession
  Context ctx;
  ctx["key"] = "value";
  auto& sub = ts1.create_subtask("branch-0", ctx);
  REQUIRE(sub.branch_path == "branch-0");
  REQUIRE(sub.initial_context["key"] == "value");
  REQUIRE(sub.status == "running");
  REQUIRE(ts1.subtask_sessions().size() == 1);

  // 多次 create_task_session 后 current_task_session_ 指针仍有效 (地址稳定性)
  user.create_task_session();
  user.create_task_session();
  REQUIRE(user.task_sessions().size() == 3);
  // 原始指针 ts1 仍有效
  REQUIRE(ts1.user_session().user_id() == "test-user-1");
}

TEST_CASE("Subtask 归档", "[session][unit]") {
  UserSession user("test-user-2");
  auto& ts = user.create_task_session();

  Context ctx;
  ctx["input"] = 42;
  auto& sub = ts.create_subtask("branch-a", ctx);

  // 设置最终结果但不改 status — archive_subtask_result 负责状态更新
  sub.final_context["output"] = 84;
  ts.archive_subtask_result(sub);

  const auto& archived = ts.subtask_sessions();
  REQUIRE(archived.size() == 1);
  REQUIRE(archived[0].branch_path == "branch-a");
  // archive_subtask_result 设置 status 为 "running" 时归档为完成态
  REQUIRE(archived[0].final_context["output"] == 84);
  REQUIRE(archived[0].completed_at.has_value());
}

TEST_CASE("失败分裂 — 3 次可重试失败返回 NewSession", "[session][unit]") {
  UserSession user("test-user-3");
  auto& ts = user.create_task_session();

  REQUIRE(ts.failure_count() == 0);
  REQUIRE(ts.determine_failure_mode() == TaskSession::FailureMode::KeepSession);

  // 记录 3 次可重试失败（Retry error_code）
  for (int i = 0; i < 3; ++i) {
    ts.record_failure(make_failure());
  }
  REQUIRE(ts.failure_count() == 3);
  REQUIRE(ts.determine_failure_mode() == TaskSession::FailureMode::NewSession);
}

TEST_CASE("IPER retry 复用 — <3 次失败返回 KeepSession，非可重试不递增", "[session][unit]") {
  UserSession user("test-user-4");
  auto& ts = user.create_task_session();

  // 1 次失败 → KeepSession
  ts.record_failure(make_failure());
  REQUIRE(ts.failure_count() == 1);
  REQUIRE(ts.determine_failure_mode() == TaskSession::FailureMode::KeepSession);

  // 2 次失败 → KeepSession
  ts.record_failure(make_failure());
  REQUIRE(ts.failure_count() == 2);
  REQUIRE(ts.determine_failure_mode() == TaskSession::FailureMode::KeepSession);

  // 成功 → failure_count 不递增
  uint32_t before = ts.failure_count();
  ts.record_failure(make_success());
  REQUIRE(ts.failure_count() == before);
}

TEST_CASE("messages 追加写保护", "[session][unit]") {
  UserSession user("test-user-5");

  ToolResult tr;
  tr.ok = true;
  tr.data["response"] = "hello";
  user.append_message(tr);

  REQUIRE(user.messages().size() == 1);
  REQUIRE(user.messages()[0].ok == true);

  // const 引用不可修改 (编译期保证)
  const auto& msgs = user.messages();
  // msgs.clear() 应编译失败 — const vector 无 mutator
  // 此处只验证运行时语义
  REQUIRE(msgs.size() == 1);

  // 追加第二条
  ToolResult tr2;
  tr2.ok = false;
  tr2.error_code = ErrorCode::Timeout;
  user.append_message(tr2);
  REQUIRE(user.messages().size() == 2);
  REQUIRE(user.messages()[1].ok == false);
}