// pdk/temporal_agent/tests/test_itemporal_client.cpp
// 功能描述：ITemporalClient 纯虚接口单元 test (Task 1, TDD Step 1)。
//          验证 5 个纯虚方法的接口形状 (compile-time requires expression)
//          + 运行时 abstract 类不可实例化 + 派生类可 override。
// 设计依据：.rddf/plans/pkm-temporal-demo-scaffold.md Task 1 Step 1
// 作者：pkm-temporal-demo-scaffold Task 1
// 最后修改日期：2026-07-28

#include "catch_amalgamated.hpp"

#include "agenticdsl/pdk/itemporal_client.h"

#include <nlohmann/json.hpp>
#include <memory>
#include <string>

using json = nlohmann::json;
using agenticdsl::pdk::ITemporalClient;

// ============================================================================
// Test 1: 接口形状 - 编译期 requires 表达式验证 5 方法签名
// ============================================================================
TEST_CASE("ITemporalClient: pure virtual interface compiles and 5 methods exist",
          "[pdk][temporal][task1]") {
  // start_workflow_blocking(workflow_id, args) -> json
  constexpr bool kHasStartBlocking = requires(ITemporalClient* c,
                                              std::string id, json args) {
    { c->start_workflow_blocking(id, args) } -> std::convertible_to<json>;
  };
  REQUIRE(kHasStartBlocking);

  // start_workflow_async(workflow_id, args) -> json
  constexpr bool kHasStartAsync = requires(ITemporalClient* c,
                                           std::string id, json args) {
    { c->start_workflow_async(id, args) } -> std::convertible_to<json>;
  };
  REQUIRE(kHasStartAsync);

  // poll(workflow_id) -> json
  constexpr bool kHasPoll = requires(ITemporalClient* c, std::string id) {
    { c->poll(id) } -> std::convertible_to<json>;
  };
  REQUIRE(kHasPoll);

  // signal(workflow_id, signal_name, payload) -> json
  constexpr bool kHasSignal = requires(ITemporalClient* c,
                                       std::string id, std::string name,
                                       json payload) {
    { c->signal(id, name, payload) } -> std::convertible_to<json>;
  };
  REQUIRE(kHasSignal);

  // query(workflow_id, query_name) -> json
  constexpr bool kHasQuery = requires(ITemporalClient* c,
                                      std::string id, std::string name) {
    { c->query(id, name) } -> std::convertible_to<json>;
  };
  REQUIRE(kHasQuery);
}

// ============================================================================
// Test 2: 抽象类不可实例化 (纯虚析构 + 5 纯虚方法 = abstract)
// ============================================================================
TEST_CASE("ITemporalClient: is abstract (cannot instantiate directly)",
          "[pdk][temporal][task1]") {
  // 概念检查: ITemporalClient 是抽象类
  STATIC_REQUIRE(std::is_abstract_v<ITemporalClient>);

  // 虚析构存在 (通过 is_polymorphic 间接验证)
  STATIC_REQUIRE(std::is_polymorphic_v<ITemporalClient>);

  // 默认可构造性: 抽象类不可默认构造
  STATIC_REQUIRE_FALSE(std::is_default_constructible_v<ITemporalClient>);
}

// ============================================================================
// Test 3: 派生类可完整实现接口 (验证所有 5 方法可 override)
// ============================================================================
namespace {

// 最小 stub 实现 - 仅验证接口可被满足 (不测试 MockTemporalClient 行为)
class StubTemporalClient : public ITemporalClient {
public:
  json start_workflow_blocking(const std::string& /*workflow_id*/,
                              const json& /*args*/) override {
    return json{{"state", "COMPLETED"}};
  }
  json start_workflow_async(const std::string& workflow_id,
                            const json& /*args*/) override {
    return json{{"workflow_id", workflow_id}, {"state", "RUNNING"}};
  }
  json poll(const std::string& workflow_id) override {
    return json{{"workflow_id", workflow_id}, {"state", "RUNNING"}};
  }
  json signal(const std::string& workflow_id,
              const std::string& signal_name,
              const json& /*payload*/) override {
    return json{{"workflow_id", workflow_id},
                {"signal", signal_name}, {"ack", true}};
  }
  json query(const std::string& workflow_id,
             const std::string& query_name) override {
    return json{{"workflow_id", workflow_id},
                {"query", query_name}, {"result", "stub"}};
  }
};

} // namespace

TEST_CASE("ITemporalClient: stub implementation satisfies interface",
          "[pdk][temporal][task1]") {
  StubTemporalClient client;
  ITemporalClient& base = client;  // 多态引用

  SECTION("start_workflow_blocking returns json") {
    auto r = base.start_workflow_blocking("wf-1", {{"task", "noop"}});
    REQUIRE(r["state"] == "COMPLETED");
  }

  SECTION("start_workflow_async returns RUNNING") {
    auto r = base.start_workflow_async("wf-2", {{"task", "long"}});
    REQUIRE(r["state"] == "RUNNING");
    REQUIRE(r["workflow_id"] == "wf-2");
  }

  SECTION("poll returns workflow state") {
    auto r = base.poll("wf-2");
    REQUIRE(r["workflow_id"] == "wf-2");
  }

  SECTION("signal returns ack") {
    auto r = base.signal("wf-2", "go", json::object());
    REQUIRE(r["signal"] == "go");
    REQUIRE(r["ack"] == true);
  }

  SECTION("query returns readonly metadata") {
    auto r = base.query("wf-2", "status");
    REQUIRE(r["query"] == "status");
  }

  SECTION("polymorphic ownership via unique_ptr") {
    auto p = std::make_unique<StubTemporalClient>();
    REQUIRE(p->poll("any")["workflow_id"] == "any");
  }
}
