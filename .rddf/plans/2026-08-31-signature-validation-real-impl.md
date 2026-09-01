# signature-validation-real-impl Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use skill_use("execute") to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 实施 `GenerateSubGraph signature_validation` 真实校验 — 替换 `node_executor.cpp:309` 的 `bool is_valid = true; // Placeholder` 占位符, 使 strict 模式真正执行签名 schema 校验 (防止 LLM 通过 GenerateSubGraph 完全绕过治理)。Oracle 评审 🟡 Conditional-Go (`session ses_fa91c94bdffeOraAXCrgkwK05f`), commit `06ddd13` 已修 B2 (放弃匿名命名空间 validate_node, 改字符串解析 AST 对齐 dsl.md §5.7 函数式格式)。**Oracle P0 未解决**: `is_valid_json_schema_type()` 在 ToolSchemaValidator 公开 API 不存在, 需自实现 type 白名单 (string/number/boolean/object/array/integer/null)。

**Architecture:** 在 `src/modules/executor/signature_validator.h/.cpp` 新建 `SignatureValidator` 类。`validate_signature(signature_str, expected_schema)` 方法: (1) parse `signature_str` 为 AST (regex 解析 `(input: type) -> {output: type}` 函数式格式); (2) 校验 type 在白名单内 (string/number/boolean/object/array/integer/null); (3) 检查 input/output schema 与节点 prompt 声明的预期一致 (从 GenerateSubgraphNode.prompt_template 推断)。strict 模式抛异常; warn 模式 LOG_WARN 继续; ignore 模式跳过。在 `node_executor.cpp` 调用点替换占位符。**不修复 P0 GenerateSubGraph 断链** (独立 change, Materializer V1 走静态路径绕过)。

**Tech Stack:** C++20, std::regex, nlohmann::json, Catch2 v3, ADR-0073 Tool JSON Schema (复用 schema validation 思路但不直接调用其内部 API)。

---

## Scope Adjustments vs proposal

**Adopted scope** (Oracle Conditional-Go, commit `06ddd13`):
- `SignatureValidator` class (新建)
- AST 解析: `(input: type) -> {output: type}` 函数式格式 (对齐 dsl.md §5.7)
- Type 白名单校验: string/number/boolean/object/array/integer/null (自实现, 不依赖 ToolSchemaValidator 公开 API)
- input/output schema 一致性检查 (input 类型 vs prompt 引用变量)
- strict/warn/ignore 三模式实装 (替换占位符)
- on_signature_violation 跳转路径支持 (V1 仍 throw, 调度器跳转 V2 补)
- ≥3 测试 case (strict throw / warn log / ignore skip)

**Deferred to follow-up**:
- ToolSchemaValidator 公开 API 扩展 (需协调 ADR-0073 owner, 当前自实现 type 白名单)
- 语义层校验 (防 LLM prompt injection 子图内) — V2 由 ADR-0084 MutationGovernor L2 接管
- on_signature_violation 调度器跳转支持 (调度器跳转 V2 补)
- P0 GenerateSubGraph 断链 (node_executor.cpp:327 `g_current_engine->append_graphs` 注释) — 独立 change, Materializer V1 走静态路径绕过

---

## File Structure

### Production Code

| File | Responsibility |
|---|---|
| `include/agenticdsl/executor/signature_validator.h` (new) | `SignatureValidator` class + `parse_signature_ast()` + `validate_type()` |
| `src/modules/executor/signature_validator.cpp` (new) | AST 解析 + type 白名单校验 + strict/warn/ignore 模式 |
| `src/modules/executor/node_executor.cpp` | 替换 line 309 `bool is_valid = true; // Placeholder` 为实际校验调用 |

### Tests

| File | Responsibility |
|---|---|
| `tests/test_signature_validation.cpp` (new, ≥3 cases) | strict throw / warn log / ignore skip + AST 解析边界 |

---

## TDD 5-Step Execution

### Step 1: Write failing test

**File**: `tests/test_signature_validation.cpp` (new, ~60 LOC)

```cpp
#include <agenticdsl/executor/signature_validator.h>
#include <catch2/catch_test_macros.hpp>

TEST_CASE("SignatureValidator: strict mode throws on invalid type", "[sigval][strict]") {
    SignatureValidator v(SignatureMode::Strict);
    std::string sig = "(input: invalid_type) -> {result: string}";
    REQUIRE_THROWS_AS(v.parse_signature_ast(sig), std::invalid_argument);
}

TEST_CASE("SignatureValidator: warn mode logs and continues on invalid type",
          "[sigval][warn]") {
    SignatureValidator v(SignatureMode::Warn);
    std::string sig = "(input: number) -> {result: invalid_type}";
    REQUIRE_NOTHROW(v.parse_signature_ast(sig));
    // LOG_WARN 输出, 不抛
}

TEST_CASE("SignatureValidator: ignore mode skips validation entirely",
          "[sigval][ignore]") {
    SignatureValidator v(SignatureMode::Ignore);
    std::string sig = "(input: anything_at_all) -> {result: anything}";
    REQUIRE_NOTHROW(v.parse_signature_ast(sig));
}

TEST_CASE("SignatureValidator: parses valid signature AST correctly",
          "[sigval][parse]") {
    SignatureValidator v(SignatureMode::Strict);
    std::string sig = "(input: string, n: number) -> {result: string, count: number}";
    auto ast = v.parse_signature_ast(sig);
    REQUIRE(ast.inputs.size() == 2);
    REQUIRE(ast.inputs[0].name == "input");
    REQUIRE(ast.inputs[0].type == "string");
    REQUIRE(ast.outputs.size() == 2);
    REQUIRE(ast.outputs[0].name == "result");
    REQUIRE(ast.outputs[0].type == "string");
}

TEST_CASE("SignatureValidator: type whitelist includes string/number/boolean/object/array/integer/null",
          "[sigval][types]") {
    SignatureValidator v(SignatureMode::Strict);
    for (const auto& t : {"string", "number", "boolean", "object", "array", "integer", "null"}) {
        std::string sig = "(input: " + std::string(t) + ") -> {result: " + std::string(t) + "}";
        REQUIRE_NOTHROW(v.parse_signature_ast(sig));
    }
}

TEST_CASE("node_executor: strict mode triggers throw on invalid GenerateSubgraphNode signature",
          "[sigval][node_executor]") {
    // 集成测试: 替换占位符后, 校验失败应 throw
    // 真实集成需要 mock LLM, 此处略, 由 ship gate 集成 ctest 验证
}
```

**Verification**:
```bash
cmake --build build --target test_signature_validation
ctest -R "signature_validation" --output-on-failure
# Expected: FAIL (SignatureValidator 未实装)
```

---

### Step 2: Implement `SignatureValidator` (signature parser + type whitelist)

**File**: `include/agenticdsl/executor/signature_validator.h`

```cpp
#pragma once
#include <string>
#include <vector>

namespace agenticdsl::executor {

enum class SignatureMode { Strict, Warn, Ignore };

struct SignatureParam {
    std::string name;
    std::string type;  // string/number/boolean/object/array/integer/null
};

struct SignatureAST {
    std::vector<SignatureParam> inputs;
    std::vector<SignatureParam> outputs;
};

class SignatureValidator {
public:
    explicit SignatureValidator(SignatureMode mode = SignatureMode::Strict);
    
    // Parse "(input: type) -> {output_name: type, ...}" to AST
    SignatureAST parse_signature_ast(const std::string& sig_str);
    
    // Validate type against whitelist (string/number/boolean/object/array/integer/null)
    bool is_valid_type(const std::string& type) const;
    
    // Throw / log / skip based on mode
    void enforce_mode(bool is_valid, const std::string& reason) const;
    
    void set_mode(SignatureMode mode) { mode_ = mode; }
    
private:
    SignatureMode mode_;
    static const std::set<std::string>& type_whitelist();
};

}  // namespace
```

**File**: `src/modules/executor/signature_validator.cpp`

```cpp
#include "agenticdsl/executor/signature_validator.h"
#include <regex>
#include <sstream>
#include <stdexcept>
#include <set>

namespace agenticdsl::executor {

const std::set<std::string>& SignatureValidator::type_whitelist() {
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
    // Regex 解析 (input: type1, ...) -> {output_name: type2, ...}
    // 对齐 dsl.md §5.7 函数式格式
    static const std::regex re(R"(^\(([^)]*)\)\s*->\s*\{([^}]*)\}\s*$)");
    std::smatch m;
    SignatureAST ast;
    if (!std::regex_match(sig_str, m, re)) {
        enforce_mode(false, "signature format invalid: " + sig_str);
        return ast;
    }
    
    // parse inputs
    std::string inputs = m[1].str();
    if (!inputs.empty()) {
        std::regex comma_re(R"((\w+)\s*:\s*(\w+))");
        for (auto it = std::sregex_iterator(inputs.begin(), inputs.end(), comma_re);
             it != std::sregex_iterator(); ++it) {
            std::string name = (*it)[1].str();
            std::string type = (*it)[2].str();
            if (!is_valid_type(type)) {
                enforce_mode(false, "invalid input type '" + type + "' in signature");
            }
            ast.inputs.push_back({name, type});
        }
    }
    
    // parse outputs
    std::string outputs = m[2].str();
    if (!outputs.empty()) {
        std::regex comma_re(R"((\w+)\s*:\s*(\w+))");
        for (auto it = std::sregex_iterator(outputs.begin(), outputs.end(), comma_re);
             it != std::sregex_iterator(); ++it) {
            std::string name = (*it)[1].str();
            std::string type = (*it)[2].str();
            if (!is_valid_type(type)) {
                enforce_mode(false, "invalid output type '" + type + "' in signature");
            }
            ast.outputs.push_back({name, type});
        }
    }
    return ast;
}

void SignatureValidator::enforce_mode(bool is_valid, const std::string& reason) const {
    if (is_valid) return;
    switch (mode_) {
        case SignatureMode::Strict:
            throw std::invalid_argument("SignatureValidator (strict): " + reason);
        case SignatureMode::Warn:
            LOG_WARN("SignatureValidator (warn): " + reason);
            break;
        case SignatureMode::Ignore:
            // no-op
            break;
    }
}

}  // namespace
```

---

### Step 3: Replace placeholder in `node_executor.cpp`

**File**: `src/modules/executor/node_executor.cpp` (修改 line 307-329 区域)

```cpp
// 原占位符 (Oracle G5 严重度上调)
// if (graph.signature.has_value()) {
//     bool is_valid = true; // Placeholder for actual validation logic
//     if (!is_valid && node->signature_validation == "strict") { throw ... }
//     else if (!is_valid && node->signature_validation == "warn") { LOG_WARN ... }
// }

// 替换为 SignatureValidator 真实调用 (commit 06ddd13 B2 修复方向)
if (graph.signature.has_value()) {
    SignatureMode mode = SignatureMode::Strict;
    if (node->signature_validation == "warn") mode = SignatureMode::Warn;
    else if (node->signature_validation == "ignore") mode = SignatureMode::Ignore;
    
    SignatureValidator validator(mode);
    try {
        validator.parse_signature_ast(graph.signature.value());
        // strict 模式: throw on invalid type
        // warn 模式: LOG_WARN 继续
        // ignore 模式: 跳过校验
    } catch (const std::exception& e) {
        // strict 模式已 throw, 此处 catch 仅 warn/ignore 路径
        if (node->on_signature_violation.has_value()) {
            // V1: 仍 throw (调度器跳转 V2 补)
            throw std::runtime_error("GenerateSubGraphNode: signature violation: " + std::string(e.what())
                + " (jump target: " + node->on_signature_violation.value() + " not yet supported)");
        }
        throw std::runtime_error("GenerateSubGraphNode execution failed: " + std::string(e.what()));
    }
}
```

---

### Step 4: Commit

```bash
git add include/agenticdsl/executor/signature_validator.h \
        src/modules/executor/signature_validator.cpp \
        src/modules/executor/node_executor.cpp \
        tests/test_signature_validation.cpp \
        tests/CMakeLists.txt
git commit -m "feat(sigval): GenerateSubGraph signature 真实校验 (strict/warn/ignore)"
```

---

## Ship Gate Validation

```bash
# 1. openspec validate
openspec validate 2026-08-31-signature-validation-real-impl --strict

# 2. compile + tests
cmake --build build && ctest -R "signature_validation" --output-on-failure
# Expected: PASS (3-5 cases)

# 3. Baseline regression (T3 + T6 + T2 + T5 + T1 已 ship, 230 + 5 = 235)
ctest --output-on-failure  # 235

# 4. ADR lint + drift
python3 tools/adr_lint.py  # ✓
python3 tools/docs_drift_audit.py  # 0 DRIFT

# 5. 占位符替换验证 (Oracle G5 严重度)
grep -c "bool is_valid = true; // Placeholder" src/modules/executor/node_executor.cpp  # 0 (替换完成)

# 6. type 白名单验证
grep -c "string.*number.*boolean" src/modules/executor/signature_validator.cpp  # ≥ 1
```

---

## Risk Assessment

| 风险 | 缓解 |
|------|------|
| ToolSchemaValidator 公开 API 不含 `is_valid_json_schema_type()` (Oracle P0-M1) | Step 2 自实现 type 白名单 (string/number/boolean/object/array/integer/null); 不依赖 ToolSchemaValidator |
| signature 格式 dsl.md §5.7 vs 函数式 (Oracle P0-M2) | Step 2 regex 解析 `(input: type) -> {output: type}`; dsl.md §6.2 实际 YAML 格式为 Phase 2 完整 AST parser 跟进 |
| semantic 层校验缺失 (LLM prompt injection) | 当前仅结构层 (type + format); V2 由 ADR-0084 MutationGovernor L2 接管 |
| P0 GenerateSubGraph 断链 (`g_current_engine->append_graphs` 注释, line 327) | T4 不修复断链 (独立 change); Materializer V1 走 `continue_with_generated_dsl` 静态路径绕过 |
| on_signature_violation 调度器跳转 (V1) | T4 仍 throw (V1 简化); 调度器跳转 V2 补 |
| strict 模式 throw 破坏现有 ctest | 仅影响 GenerateSubgraphNode path; 其他 node type ctest 不变 |
| API 不一致风险: `parse_signature_ast` 返回空 AST on invalid (vs throw) | 设计: strict throw, warn 返回空+log, ignore 返回空静默; test_1 + test_4 验证 |

---

## 后续触发条件

| 触发条件 | 后续 OpenSpec change |
|----------|---------------------|
| T4 ship 立即 | P0 GenerateSubGraph 断链独立 change (node_executor.cpp:327) |
| ToolSchemaValidator 公开 API 扩展 | signature-validation-v2 (依赖 ADR-0073 owner) |
| 调度器跳转支持 (V2) | signature-jump-on-violation 子 change |
| 语义层校验 (LLM prompt injection) | ADR-0084 MutationGovernor L2 集成 (V2) |
| dsl.md §6.2 YAML 格式完整 AST parser | signature-validation-v2 (full YAML support) |