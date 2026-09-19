// include/agenticdsl/genome/hmac.h
// C2 genome-registry — HMAC-SHA256 wrapper (per D10 + Oracle bg_a818a6a1)
// OpenSSL EVP_sha256, 返回 hex string
// 作者: C2 Sprint 35
// 日期: 2026-09-18
#pragma once

#include <string>

namespace agenticdsl::genome {

// canonical bytes → HMAC-SHA256 → hex string (lowercase, 64 chars)
std::string hmac_sign(const std::string& key, const std::string& canonical_bytes);

// verify signature (constant-time compare)
bool hmac_verify(const std::string& key, const std::string& canonical_bytes,
                  const std::string& expected_hex_sig);

}  // namespace agenticdsl::genome