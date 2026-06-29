// src/common/policy/policy_factory.cpp
// 功能描述：Policy 工厂实现
// 作者：AgenticDSL Phase3 / Sprint 13 C3 ship
// 最后修改日期：2026-07-31
#include "common/policy/policy_factory.h"

#include "common/policy/plan_mode_policy.h"
#include "common/policy/agent_mode_policy.h"
#include "common/policy/yolo_mode_policy.h"

namespace agenticdsl {

std::unique_ptr<IExecutionPolicy> PolicyFactory::create(PolicyMode mode) {
  switch (mode) {
    case PolicyMode::Plan:
      return std::make_unique<PlanModePolicy>();
    case PolicyMode::Agent:
      return std::make_unique<AgentModePolicy>();
    case PolicyMode::Yolo:
      return std::make_unique<YoloModePolicy>();
    default:
      return std::make_unique<AgentModePolicy>();
  }
}

}  // namespace agenticdsl