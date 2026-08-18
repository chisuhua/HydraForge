// src/common/env/sha256_util.h
// 功能描述：SHA256 hex 工具 — audit 事件 cmd_hash 脱敏 (ADR-0068 §5.11 args-only-keys)
// 作者：from-roadmap-phase-6c-execution-envbackend change
// 最后修改日期：2026-08-18
#pragma once

#include <openssl/sha.h>

#include <string>

namespace agenticdsl {

/// @brief 计算字符串的 SHA256 hex (小写 64 字符)
inline std::string sha256_hex(const std::string& input) {
  unsigned char digest[SHA256_DIGEST_LENGTH];
  SHA256(reinterpret_cast<const unsigned char*>(input.data()), input.size(),
         digest);
  static const char* kHex = "0123456789abcdef";
  std::string out;
  out.reserve(64);
  for (unsigned char c : digest) {
    out += kHex[c >> 4];
    out += kHex[c & 0x0F];
  }
  return out;
}

}  // namespace agenticdsl
