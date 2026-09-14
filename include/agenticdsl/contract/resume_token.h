// include/agenticdsl/contract/resume_token.h
// ResumeToken - ChatSession 断线恢复上下文
// 设计依据: docs/superpowers/specs/2026-09-11-chat-session-pdk-lift-design.md §6.3 (D4 裁决)
// 作者: chat-session-pdk-lift Change 2
// 日期: 2026-09-11
//
// D4 裁决: ResumeToken 只承载"恢复所需的最小引用", 不持久化 provider/stop_token。
// ADR-0079 4-Scope 对齐 (spec §D7): session_id→Conversation / leaf_node_id→Attempt,
// model→Step / budget_used→Execution。ChatSession 仅承担 Attempt 入口引用,
// 不二次实现装配逻辑, 避免 ADR-0079 v1.2 amendment 时返工。

#pragma once

#include <string>

namespace agenticdsl {

struct ResumeToken {
  std::string session_id;    // SessionManager::open 入参
  std::string leaf_node_id;  // SessionManager::build_context_entries 入参
  std::string model;         // provider 恢复后一致性校验 (恢复时不清空 provider_mode)
  double budget_used = 0.0;  // 累计 budget 值 (恢复后可继续累加)
};

}  // namespace agenticdsl
