// include/agenticdsl/executor/signature_validator.h
// T4 signature-validation-real-impl: 替换 node_executor.cpp:309 Placeholder
#pragma once

#include <optional>
#include <string>
#include <vector>

namespace agenticdsl::executor {

enum class SignatureMode { Strict, Warn, Ignore };

struct SignatureParam {
    std::string name;
    std::string type;
};

struct SignatureAST {
    std::vector<SignatureParam> inputs;
    std::vector<SignatureParam> outputs;
};

class SignatureValidator {
public:
    explicit SignatureValidator(SignatureMode mode = SignatureMode::Strict);

    SignatureAST parse_signature_ast(const std::string& sig_str);
    bool is_valid_type(const std::string& type) const;

    void enforce_mode(bool is_valid, const std::string& reason) const;

    void set_mode(SignatureMode mode) { mode_ = mode; }

private:
    SignatureMode mode_;
};

}  // namespace agenticdsl::executor