// src/core/genome/hmac.cpp
// HMAC-SHA256 wrapper impl via OpenSSL EVP API
// 作者: C2 Sprint 35
// 日期: 2026-09-18

#include "agenticdsl/genome/hmac.h"

#include <openssl/evp.h>
#include <openssl/hmac.h>

#include <array>
#include <cstring>
#include <stdexcept>
#include <vector>

namespace agenticdsl::genome {

namespace {
constexpr char kHexDigits[] = "0123456789abcdef";

std::string to_hex(const unsigned char* bytes, size_t len) {
    std::string out;
    out.reserve(len * 2);
    for (size_t i = 0; i < len; ++i) {
        out.push_back(kHexDigits[bytes[i] >> 4]);
        out.push_back(kHexDigits[bytes[i] & 0xF]);
    }
    return out;
}

bool from_hex(const std::string& hex, std::vector<unsigned char>& out) {
    if (hex.size() % 2 != 0) return false;
    out.clear();
    out.reserve(hex.size() / 2);
    for (size_t i = 0; i < hex.size(); i += 2) {
        auto hi = static_cast<unsigned char>(hex[i]);
        auto lo = static_cast<unsigned char>(hex[i + 1]);
        unsigned char byte = 0;
        if (hi >= '0' && hi <= '9') byte = (hi - '0') << 4;
        else if (hi >= 'a' && hi <= 'f') byte = (hi - 'a' + 10) << 4;
        else if (hi >= 'A' && hi <= 'F') byte = (hi - 'A' + 10) << 4;
        else return false;
        if (lo >= '0' && lo <= '9') byte |= (lo - '0');
        else if (lo >= 'a' && lo <= 'f') byte |= (lo - 'a' + 10);
        else if (lo >= 'A' && lo <= 'F') byte |= (lo - 'A' + 10);
        else return false;
        out.push_back(byte);
    }
    return true;
}
}  // namespace

std::string hmac_sign(const std::string& key, const std::string& canonical_bytes) {
    unsigned char out[EVP_MAX_MD_SIZE];
    unsigned int out_len = 0;
    if (!HMAC(EVP_sha256(),
              key.data(), static_cast<int>(key.size()),
              reinterpret_cast<const unsigned char*>(canonical_bytes.data()),
              canonical_bytes.size(),
              out, &out_len)) {
        throw std::runtime_error("HMAC computation failed");
    }
    return to_hex(out, out_len);
}

bool hmac_verify(const std::string& key, const std::string& canonical_bytes,
                  const std::string& expected_hex_sig) {
    std::vector<unsigned char> expected;
    if (!from_hex(expected_hex_sig, expected)) return false;
    std::string actual = hmac_sign(key, canonical_bytes);
    if (actual.size() != expected.size() * 2) return false;
    // constant-time compare
    unsigned char diff = 0;
    for (size_t i = 0; i < actual.size(); ++i) {
        diff |= static_cast<unsigned char>(actual[i]) ^
                static_cast<unsigned char>(expected_hex_sig[i]);
    }
    return diff == 0;
}

}  // namespace agenticdsl::genome