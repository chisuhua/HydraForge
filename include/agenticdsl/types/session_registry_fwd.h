// agenticdsl/types/session_registry_fwd.h
// 功能描述：SessionRegistry 前向声明 — 用于 DSLEngine 等跨模块引用时降低编译耦合
// 设计依据：ADR-0019 §1.4 (PIMPL-lite 策略) + Oracle Risk 5 mitigation
// 作者：AgenticDSL Phase 5 / Sprint 20 C11
// 最后修改日期：2026-07-04
#pragma once

namespace agenticdsl {

class SessionRegistry;

}  // namespace agenticdsl