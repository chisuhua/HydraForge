// src/common/policy/approval_callbacks.h
// 功能描述：ApprovalCallback 工厂 — 3 种 transport 实现
// 设计依据：ADR-0031 §决策 5 (Oracle session ses_0ee867023ffeaSqWQXET5ESbAo)
// 作者：AgenticDSL Phase3 / Sprint 13 C3 ship
// 最后修改日期：2026-07-31
#pragma once

#include <memory>
#include <string>

#include "agenticdsl/policy/iexecution_policy.h"
#include "agenticdsl/contract/iinteraction_bus.h"

namespace agenticdsl {

ApprovalCallback make_tui_stdin_callback();

ApprovalCallback make_event_bus_callback(std::shared_ptr<IInteractionBus> bus);

ApprovalCallback make_test_auto_callback(bool decision);

}  // namespace agenticdsl