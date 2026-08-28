// include/agenticdsl/prompt/prompt_hash.h
// 功能描述：T21 payload redact — Prompt Hash Helper (Phase 0, T0)
//          hash_prompt(): 不可逆 hash (16 hex chars), 用于事件 payload 脱敏
//          estimate_tokens(): V1 简化 token 估算 = ceil(chars / 4)
// 设计依据：openspec/changes/t21-payload-redact/tasks.md Phase 0 (T0.3-T0.4)
//          + ADR-0080 D10 PII 约束 (G11 mutation.* hash 范式)
// 作者：HydraForge Sprint 25 T21 payload redact
// 最后修改日期：2026-08-28
#ifndef AGENTICDSL_PROMPT_PROMPT_HASH_H
#define AGENTICDSL_PROMPT_PROMPT_HASH_H

#include <string>

namespace agenticdsl {

/// 不可逆 prompt hash — 16 hex chars (64-bit collision space)
/// 用于事件 payload PII 脱敏, 相同输入保证相同输出 (确定性)
std::string hash_prompt(const std::string& prompt);

/// V1 简化 token 估算: ceil(chars / 4)
/// V2 升级真实 LLM tokenizer (tiktoken 等)
int estimate_tokens(const std::string& prompt);

}  // namespace agenticdsl

#endif  // AGENTICDSL_PROMPT_PROMPT_HASH_H
