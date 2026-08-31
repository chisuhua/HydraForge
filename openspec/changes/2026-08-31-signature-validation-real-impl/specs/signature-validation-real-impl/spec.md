# Signature Validation Real Impl Specification

## Purpose

> 修复 Oracle G5 严重度上调缺口: `node_executor.cpp:309` GenerateSubGraph signature 校验是占位符 (`bool is_valid = true; // Placeholder`), strict 恒通过, GenerateSubGraph 治理 ≈ 零。本 change 将占位符替换为真实 schema 校验, 复用 ADR-0073 已实装的 `tool_schema_validator` (nlohmann 递归 validate_node)。

## ADDED Requirements

### Requirement: validate_subgraph_signature 独立函数

`validate_subgraph_signature(const ParsedGraph& graph, std::string* error_msg)` MUST 为独立函数 (与 NodeExecutor 解耦, 可测试性), MUST 复用 ADR-0073 `tool_schema_validator`, MUST NOT 重复实现 JSON Schema 校验。

#### Scenario: 独立函数存在

- **WHEN** 静态检查 `grep "validate_subgraph_signature" src/modules/executor/signature_validator.h`
- **THEN** 1 行匹配

#### Scenario: 复用 ADR-0073 validator

- **WHEN** 静态检查 `grep "tool_schema_validator\|validate_node" src/modules/executor/signature_validator.cpp`
- **THEN** ≥1 命中

#### Scenario: 无重复 JSON Schema 实现

- **WHEN** 静态检查 signature_validator.cpp 不重新定义 type/properties/required/items/enum 校验逻辑
- **THEN** 0 命中 (全部委托 tool_schema_validator)

### Requirement: 校验规则 (signature 结构合法性)

校验 MUST 覆盖: signature 是合法 JSON Schema 对象 + `inputs`/`outputs` 字段存在且为数组 + 每项含 `name` + `type` 字段 + type 是合法 JSON Schema type。

#### Scenario: 合法 signature 通过

- **WHEN** 输入 ParsedGraph 含合法 signature (inputs+outputs 完整, name+type 齐全)
- **THEN** 返回 true

#### Scenario: 缺 outputs 失败

- **WHEN** 输入 ParsedGraph 含 signature 但缺 outputs 字段
- **THEN** 返回 false + error_msg 含 "outputs"

#### Scenario: 非法结构失败

- **WHEN** 输入 ParsedGraph 含 signature 但结构非法 (非 JSON Schema)
- **THEN** 返回 false + error_msg 含具体原因

#### Scenario: 无 signature 不校验

- **WHEN** 输入 ParsedGraph 无 signature (signature 为 nullopt)
- **THEN** 返回 true (不校验, 不变量 3)

### Requirement: strict/warn/ignore 三分支行为正确

`execute_generate_subgraph` 的 signature_validation 三分支 MUST: strict → 抛异常 (含 error 详情) / warn → LOG_WARN + 继续 / ignore → 跳过校验调用 (性能)。

#### Scenario: strict 非法抛异常

- **WHEN** signature_validation="strict" + 非法 signature
- **THEN** 抛 std::runtime_error, message 含 graph.path + sig_error

#### Scenario: warn 非法告警继续

- **WHEN** signature_validation="warn" + 非法 signature
- **THEN** LOG_WARN 含 graph.path, 不抛异常, 继续执行

#### Scenario: ignore 跳过校验

- **WHEN** signature_validation="ignore"
- **THEN** validate_subgraph_signature 不被调用 (性能优化)

### Requirement: 占位符替换完成

`node_executor.cpp` 的 `bool is_valid = true; // Placeholder` MUST 被替换为真实校验调用, grep 验证占位符不存在。

#### Scenario: 占位符不存在

- **WHEN** 运行 `grep -c "is_valid = true.*Placeholder" src/modules/executor/node_executor.cpp`
- **THEN** 0 命中

#### Scenario: 真实校验调用存在

- **WHEN** 运行 `grep -c "validate_subgraph_signature" src/modules/executor/node_executor.cpp`
- **THEN** ≥1 命中

### Requirement: V1 jump 路径保持抛异常

`on_signature_violation` jump 路径 V1 MUST 保持抛异常 (不变量 6), 调度器跳转实装属后续 change。

#### Scenario: jump 路径保持抛异常

- **WHEN** signature_validation="strict" + on_signature_violation 存在 + 非法 signature
- **THEN** 抛 runtime_error (V1 不实现跳转)

### Requirement: 7 测试覆盖 + 现有测试零回归

`tests/test_signature_validation.cpp` MUST 含 ≥7 cases, `test_executor_with_mock_provider` (现有 generate_subgraph 测试) MUST 零回归。

#### Scenario: 7 cases 完整

- **WHEN** 静态检查 `grep -c "TEST_CASE" tests/test_signature_validation.cpp`
- **THEN** ≥7

#### Scenario: 现有 generate_subgraph 测试零回归

- **WHEN** 运行 `./build/tests/test_executor_with_mock_provider --reporter compact`
- **THEN** 全部 pass

## MODIFIED Requirements

### Requirement: 零 contract 修改

本 change MUST NOT 修改 `include/agenticdsl/contract/` 任何头文件。

#### Scenario: contract 零修改

- **WHEN** 运行 `git diff --stat HEAD -- include/agenticdsl/contract/`
- **THEN** 0 行变更

### Requirement: 零 tool_schema_validator 修改

本 change MUST NOT 修改 `src/common/tools/tool_schema_validator.{h,cpp}` (复用, 不修改)。

#### Scenario: validator 零修改

- **WHEN** 运行 `git diff --stat HEAD -- src/common/tools/tool_schema_validator.h src/common/tools/tool_schema_validator.cpp`
- **THEN** 0 行变更

## CROSS-REFERENCED Requirements

### Requirement: 与 axis6-chain-workflow G5 对齐

本 change 是 `axis6-chain-workflow-architecture-2026-08.md` §六 G5 缺口的**第一步修复** (signature 占位符 → 真校验), G5 第二步 (cognitive_domain 检测升级) 属后续 change。

#### Scenario: G5 缺口状态更新

- **WHEN** 本 change ship 后
- **THEN** axis6-chain-workflow 文档 G5 标注 "第一步已修复 (signature 真校验)" 或 changelog 注记
