// include/agenticdsl/pdk/itemporal_client.h
// 功能描述：ITemporalClient 纯虚抽象接口 (pkm-temporal-demo-scaffold Task 1)。
//          定义 Temporal workflow 客户端的 5 个核心方法, 解耦
//          MockTemporalClient (in-memory 状态机) 与真实 gRPC 后端
//          (pkgm-temporal-agent)。命名空间 agenticdsl::pdk。
//          5 方法:
//            1. start_workflow_blocking(workflow_id, args) -> json
//            2. start_workflow_async(workflow_id, args) -> json
//            3. poll(workflow_id) -> json
//            4. signal(workflow_id, signal_name, payload) -> json
//            5. query(workflow_id, query_name) -> json
//          状态机: CREATED -> RUNNING -> COMPLETED / FAILED
//          引擎零变更 (纯 PDK 接口, 通过 call_tool 调用)。
// 设计依据：.rddf/plans/pkm-temporal-demo-scaffold.md Task 1 Step 3
// 作者：pkm-temporal-demo-scaffold Task 1
// 最后修改日期：2026-07-28

#pragma once

#include <nlohmann/json.hpp>

#include <string>

namespace agenticdsl {
namespace pdk {

// ============================================================================
// ITemporalClient - Temporal workflow 客户端抽象接口
// ============================================================================
// 纯虚接口, 派生类必须实现全部 5 方法。
// 实现者:
//   - MockTemporalClient: in-memory 状态机 (pdk/temporal_agent/mock_client.h)
//   - RealTemporalClient: gRPC 后端 (pkgm-temporal-agent, 未来)
// ============================================================================
class ITemporalClient {
public:
  virtual ~ITemporalClient() = default;

  // 阻塞启动 workflow, 直到 COMPLETED/FAILED 才返回
  virtual nlohmann::json start_workflow_blocking(
      const std::string& workflow_id,
      const nlohmann::json& args) = 0;

  // 异步启动 workflow, 立即返回 (state=RUNNING)
  virtual nlohmann::json start_workflow_async(
      const std::string& workflow_id,
      const nlohmann::json& args) = 0;

  // 轮询 workflow 当前状态 + 元数据
  virtual nlohmann::json poll(const std::string& workflow_id) = 0;

  // 向 workflow 发送 signal (触发分支/恢复)
  virtual nlohmann::json signal(
      const std::string& workflow_id,
      const std::string& signal_name,
      const nlohmann::json& payload) = 0;

  // 查询 workflow 只读元数据 (不修改状态)
  virtual nlohmann::json query(
      const std::string& workflow_id,
      const std::string& query_name) = 0;
};

}  // namespace pdk
}  // namespace agenticdsl
