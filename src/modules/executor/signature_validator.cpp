// src/modules/executor/signature_validator.cpp
// T4 signature-validation-real-impl
#include "agenticdsl/executor/signature_validator.h"
#include <regex>
#include <sstream>
#include <stdexcept>
#include <set>
#include "common/log/log.h"

namespace agenticdsl::executor {

const std::set<std::string>& type_whitelist() {
    static const std::set<std::string> w = {
        "string", "number", "boolean", "object", "array", "integer", "null"
    };
    return w;
}

SignatureValidator::SignatureValidator(SignatureMode mode) : mode_(mode) {}

bool SignatureValidator::is_valid_type(const std::string& type) const {
    return type_whitelist().count(type) > 0;
}

SignatureAST SignatureValidator::parse_signature_ast(const std::string& sig_str) {
    SignatureAST ast;
    // Parse "(input: type1, ...) -> {output: type2, ...}"
    static const std::regex re(R"(^\(([^)]*)\)\s*->\s*\{([^}]*)\}\s*$)");
    std::smatch m;
    if (!std::regex_match(sig_str, m, re)) {
        enforce_mode(false, "signature format invalid: " + sig_str);
        return ast;
    }

    std::regex comma_re(R"((\w+)\s*:\s*(\w+))");
    for (auto it = std::sregex_iterator(m[1].first, m[1].second, comma_re);
         it != std::sregex_iterator(); ++it) {
        std::string name = (*it)[1].str();
        std::string type = (*it)[2].str();
        if (!is_valid_type(type)) {
            enforce_mode(false, "invalid input type '" + type + "' in signature");
        }
        ast.inputs.push_back({name, type});
    }
    for (auto it = std::sregex_iterator(m[2].first, m[2].second, comma_re);
         it != std::sregex_iterator(); ++it) {
        std::string name = (*it)[1].str();
        std::string type = (*it)[2].str();
        if (!is_valid_type(type)) {
            enforce_mode(false, "invalid output type '" + type + "' in signature");
        }
        ast.outputs.push_back({name, type});
    }
    return ast;
}

void SignatureValidator::enforce_mode(bool is_valid, const std::string& reason) const {
    if (is_valid) return;
    switch (mode_) {
        case SignatureMode::Strict:
            throw std::invalid_argument("SignatureValidator (strict): " + reason);
        case SignatureMode::Warn:
            LOG_WARN("SignatureValidator (warn): " << reason);
            break;
        case SignatureMode::Ignore:
            break;
    }
}

}  // namespace agenticdsl::executor