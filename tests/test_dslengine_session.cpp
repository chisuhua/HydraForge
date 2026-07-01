// tests/test_dslengine_session.cpp
// 功能描述：ADR-0033 Session Hierarchy — 集成测试
//           覆盖 DSLEngine::run(UserSession&, ...) 的创建/复用/Context/LayeredContext 桥接
// 设计依据：OpenSpec change 2026-06-26-adr-0033-session-hierarchy
//           任务 4.1.2: 2 个集成 TEST_CASE
// 作者：AgenticDSL Sprint 15 (C5)
// 最后修改日期：2026-07-02

#include "catch_amalgamated.hpp"
#include "core/engine.h"
#include "core/types/session.h"
#include "agenticdsl/types/layered_context.h"
#include "agenticdsl/types/context_flatten.h"
#include <string>

using namespace agenticdsl;

// 辅助：构造最小 DSL（单节点 Assign → End）供集成测试
static const char* MINIMAL_DSL = R"(
### AgenticDSL `/main`
```yaml
# --- BEGIN AgenticDSL ---
graph_type: subgraph
nodes:
  - id: start
    type: start
    next: ["/main/echo"]
  - id: echo
    type: assign
    assign:
      echoed: "ok"
    next: ["/main/end"]
  - id: end
    type: end
# --- END AgenticDSL ---
```
)";

TEST_CASE("DSLEngine session 重载 — 首次调用创建 TaskSession，ctx[user_input] 正确", "[dslengine][session][integration]") {
  auto engine = DSLEngine::from_markdown(MINIMAL_DSL);
  UserSession user("integration-user-1");

  REQUIRE(user.current_task_session() == nullptr);
  REQUIRE(user.messages().empty());

  // 首次调用：自动创建 TaskSession
  auto result = engine->run(user, "hello world");
  REQUIRE(result.success);
  REQUIRE(user.current_task_session() != nullptr);
  REQUIRE(user.current_task_session()->status() == "completed");
  REQUIRE(user.messages().size() == 1);
  REQUIRE(user.messages()[0].ok == true);

  // 验证 ctx["user_input"] 被正确设置
  REQUIRE(user.current_task_session()->context()["user_input"] == "hello world");

  // 验证 task_sessions 历史
  REQUIRE(user.task_sessions().size() == 1);
}

TEST_CASE("多轮复用 + LayeredContext 桥接", "[dslengine][session][integration]") {
  auto engine = DSLEngine::from_markdown(MINIMAL_DSL);
  UserSession user("integration-user-2");

  // 第一轮
  auto r1 = engine->run(user, "first message");
  REQUIRE(r1.success);
  REQUIRE(user.task_sessions().size() == 1);
  REQUIRE(user.messages().size() == 1);

  // 第二轮：复用同一 TaskSession
  Context new_ctx;
  new_ctx["extra"] = "added";
  auto r2 = engine->run(user, "second message", new_ctx);
  REQUIRE(r2.success);
  // 仍只有 1 个 TaskSession（复用）
  REQUIRE(user.task_sessions().size() == 1);
  REQUIRE(user.messages().size() == 2);

  // 验证 context 被替换（顶层覆盖 + user_input 覆盖）
  auto& ctx = user.current_task_session()->context();
  REQUIRE(ctx["user_input"] == "second message");
  REQUIRE(ctx["extra"] == "added");

  // 第三轮：LayeredContext 桥接
  LayeredContext lctx;
  lctx.working["user_input"] = "third via layered";
  lctx.working["layered_key"] = true;
  auto r3 = engine->run(user, "third message", lctx);
  REQUIRE(r3.success);
  REQUIRE(user.messages().size() == 3);

  // LayeredContext 的 working 层被用作 Context
  auto& ctx3 = user.current_task_session()->context();
  REQUIRE(ctx3["user_input"] == "third message");
  REQUIRE(ctx3["layered_key"] == true);
}