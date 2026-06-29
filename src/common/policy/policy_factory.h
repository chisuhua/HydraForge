// src/common/policy/policy_factory.h
// 功能描述：Policy 工厂 — PolicyMode enum + create() 工厂函数
// 设计依据：ADR-0031 §决策 1 + §决策 2 (Oracle session ses_0ee867023ffeaSqWQXET5ESbAo)
// 默认模式: AgentPolicy (Oracle 决议)
// 作者：AgenticDSL Phase3 / Sprint 13 C3 ship
// 最后修改日期：2026-07-31
#pragma once

#include <memory>

#include "agenticdsl/policy/iexecution_policy.h"

namespace agenticdsl {

enum class PolicyMode {
  Plan,
  Agent,
  Yolo
};

class PolicyFactory {
 public:
  static std::unique_ptr<IExecutionPolicy> create(
      PolicyMode mode = PolicyMode::Agent);

  static std::unique_ptr<IExecutionPolicy> create_default() {
    return create(PolicyMode::Agent);
  }
};

}  // namespace agenticdsl