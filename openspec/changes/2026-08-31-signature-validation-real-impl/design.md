# Design — Signature Validation Real Impl

## Context

Oracle 评审 (session `ses_facbd3ffbffeUjlJgZsgMWFiM4`) 发现 G5 严重度上调: `node_executor.cpp:309` GenerateSubGraph signature 校验是占位符 (`bool is_valid = true; // Placeholder`), strict 恒通过。GenerateSubGraph 治理 ≈ 零。

本 change 将占位符替换为真实 schema 校验, 复用 ADR-0073 已实装的 `tool_schema_validator.{h,cpp}` (nlohmann 递归 validate_node)。

## 决策

### 决策 1 — 提取独立校验函数 (可测试性)

```cpp
// src/modules/executor/signature_validator.h (新建)
namespace agenticdsl {

// 校验 ParsedGraph 的 signature (若存在)
// 返回: true = 合法, false = 非法 (error_msg 填充原因)
// 无 signature → 返回 true (不校验, 不变量 3)
bool validate_subgraph_signature(const ParsedGraph& graph, std::string* error_msg);

} // namespace agenticdsl
```

**提取理由**: 将校验逻辑从 execute_generate_subgraph 内联代码提取为独立函数, 便于单元测试 (不依赖 NodeExecutor 完整上下文)。

### 决策 2 — 校验规则 (复用 ADR-0073 validator)

```cpp
// src/modules/executor/signature_validator.cpp (新建)
bool validate_subgraph_signature(const ParsedGraph& graph, std::string* error_msg) {
  if (!graph.signature.has_value()) return true;  // 不变量 3: 无 signature 不校验

  const auto& sig = graph.signature.value();

  // 1. signature 结构校验 (复用 ADR-0073 ToolSchemaValidator)
  //    signature 应为 JSON Schema 对象, 含 inputs/outputs 字段
  // 2. inputs/outputs 各自的 JSON Schema 结构合法性校验
  //    - type/properties/required/items/enum 字段合法 (ADR-0073 最小子集)
  // 3. 失败时 error_msg 填充具体原因 (e.g. "signature.outputs 缺失 required 字段")

  // ... 调用 ToolSchemaValidator::validate 或 validate_node 递归校验
}
```

**校验规则** (与 ADR-0073 最小子集对齐):
- signature 必须是合法 JSON Schema 对象
- `inputs` / `outputs` 字段必须存在且为数组
- 每个 input/output 项必须含 `name` + `type` 字段
- type 必须是合法 JSON Schema type (string/number/boolean/object/array)

### 决策 3 — execute_generate_subgraph 集成 (替换占位符)

```cpp
// node_executor.cpp:307-325 (修改后)
if (graph.signature.has_value()) {
    std::string sig_error;
    bool is_valid = validate_subgraph_signature(graph, &sig_error);  // 替换占位符
    if (!is_valid && node->signature_validation == "strict") {
        if (node->on_signature_violation.has_value()) {
            // V1: 保持抛异常 (不变量 6, jump 路径待后续 change)
            throw std::runtime_error("Signature validation failed (strict) for " + graph.path + ": " + sig_error);
        } else {
            throw std::runtime_error("Signature validation failed (strict) for " + graph.path + ": " + sig_error);
        }
    } else if (!is_valid && node->signature_validation == "warn") {
        LOG_WARN("Signature validation failed (warn) for " << graph.path << ": " << sig_error);
    } // ignore: 不校验 (is_valid 不被评估, 但 validate_subgraph_signature 已运行 — 优化: ignore 模式跳过校验调用)
}
```

**优化**: `signature_validation == "ignore"` 时**跳过校验调用** (性能, 避免无意义的 validator 运行)。

### 决策 4 — 行为变化 (预期)

| 场景 | 修复前 (占位符) | 修复后 |
|------|----------------|--------|
| 合法 signature + strict | 通过 | 通过 (不变) |
| 非法 signature + strict | **通过 (bug)** | 抛异常 (修复) |
| 非法 signature + warn | 通过 (无告警) | LOG_WARN + 继续 (修复) |
| 非法 signature + ignore | 通过 | 通过 (不变, 跳过校验) |
| 无 signature | 通过 | 通过 (不变, 不校验) |

**R3 说明**: strict 模式行为变化是**预期修复** (占位符的目的), 不是回归。

### 决策 5 — on_signature_violation jump (V1 保持抛异常)

当前注释 "This requires scheduler logic to handle jumps" — V1 保持抛异常, 不实现调度器跳转。jump 路径实装属后续 change (需 ExecutionSession 支持节点跳转)。

## 接口

### 新增文件

- `src/modules/executor/signature_validator.h` + `.cpp` (新建, 可提取)
- `tests/test_signature_validation.cpp` (新建, ≥6 cases)

### 修改文件

- `src/modules/executor/node_executor.cpp` (execute_generate_subgraph 内, 替换占位符)

### 零修改

- `include/agenticdsl/contract/` (不变量 5)
- `src/common/tools/tool_schema_validator.{h,cpp}` (复用, 不修改)

## 反例 (明确不做)

| 反例 | 拒绝理由 |
|------|----------|
| 重复实现 JSON Schema 校验 | 不变量 1: 复用 ADR-0073 validator |
| 修改 strict/warn/ignore 分支结构 | 不变量 2: 仅替换 is_valid 计算 |
| 实装 on_signature_violation jump 调度器跳转 | 不变量 6: V1 保持抛异常, 后续 change |
| 无 signature 的图强制校验 | 不变量 3: 无 signature 不校验 |
| ignore 模式仍运行校验 | 决策 3 优化: ignore 跳过校验调用 |

## 跨 change 依赖

### 前置 (已 ship)
- ✅ ADR-0073 tool_schema_validator (validate_node 递归)
- ✅ Node::signature + signature_validation + on_signature_violation
- ✅ ParsedGraph::signature + output_schema

### 后续
- `generatesubgraph-append-restore` — 断链修复 (依赖本 change 的真校验)
- `generatesubgraph-cognitive-governance` — G5 第二步 (cognitive_domain 检测升级)
- on_signature_violation jump 调度器实装 — 后续 change

### 并行
- T1 workflow-materializer-v1 (独立)
- T2 cognitive-specialists-as-tools (独立)
- T3 evolution-budget-cap (独立)

## ADR 兼容性

| ADR | 兼容性 |
|-----|--------|
| ADR-0073 (Tool JSON Schema) | ✅ 复用 validator, 不修改 |
| dsl.md v3.10 §5.7 | ✅ signature_validation strict/warn/ignore 语义对齐 |
| ADR-0004 (ToolRegistry 安全) | ✅ signature 校验是安全防线的一部分 |
| ADR-0085 §决策 5 | ✅ 无 MetaAgent, 纯校验逻辑 |
