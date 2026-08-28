// src/modules/prompt/prompt_hash.cpp
// 功能描述：T21 payload redact — Prompt Hash Helper 实现 (Phase 0, T0)
//          hash_prompt(): std::hash<std::string> + 16 hex chars encode
//                        (零新依赖, 与 ComplianceDecorator hash-only 范式一致)
//          estimate_tokens(): V1 简化 token 估算 = ceil(chars / 4)
// 设计依据：openspec/changes/t21-payload-redact/tasks.md Phase 0 (T0.3-T0.4)
//          + src/common/llm/compliance_decorator.cpp (hash-only 范式)
// 作者：HydraForge Sprint 25 T21 payload redact
// 最后修改日期：2026-08-28

#include "agenticdsl/prompt/prompt_hash.h"

#include <cstdio>
#include <functional>

namespace agenticdsl {

std::string hash_prompt(const std::string& prompt) {
  // std::hash<std::string> 返回 size_t (64-bit) → 16 hex chars
  // 与 ComplianceDecorator (std::hash<std::string>{}) 同源, 零新依赖
  const std::size_t value = std::hash<std::string>{}(prompt);
  char buf[17] = {0};
  std::snprintf(buf, sizeof(buf), "%016zx", value);
  return std::string(buf);
}

int estimate_tokens(const std::string& prompt) {
  // V1 简化: ceil(chars / 4) — 向上取整保证不低估 token 消耗
  return static_cast<int>((prompt.size() + 3) / 4);
}

}  // namespace agenticdsl
