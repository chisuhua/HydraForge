# Signature Validation Real Impl — GenerateSubGraph signature 校验占位符修复

> **状态**: 🔍 Proposed (2026-08-31, Oracle 评审发现: signature_validation 是占位符, strict 恒通过)
> **关联文档**:
> - `docs/architecture/axis6-chain-workflow-architecture-2026-08.md` §六 G5 (GenerateSubGraph 治理不对称, Oracle 严重度上调)
> - `docs/architecture/agent-orchestration-architecture-2026-08.md` §十一 (GenerateSubGraph 分析)
> - `docs/adr/adr-0073-tool-json-schema-contract.md` (Tool JSON Schema 契约, nlohmann validator 已实装)
> - `docs/specs/dsl.md` §5.7 (generate_subgraph 节点 + signature_validation strict/warn/ignore)
> **Oracle 评审**: session `ses_facbd3ffbffeUjlJgZsgMWFiM4` (G5 严重度上调: `node_executor.cpp:309` 是 `bool is_valid = true; // Placeholder`, 校验根本没实现, strict 恒通过)
> **最后更新**: 2026-08-31

## Why

### 缺口 (Oracle G5 严重度上调)

**发现**: `node_executor.cpp:309` GenerateSubGraph 的 signature 校验是占位符:

```cpp
// node_executor.cpp:307-309 (实装)
if (graph.signature.has_value()) {
    // Perform validation based on signature_validation policy
    bool is_valid = true; // Placeholder for actual validation logic
```

**含义**: `is_valid = true` 恒成立 → strict 模式恒通过 → `signature_validation: strict` 配置形同虚设 → GenerateSubGraph 生成的任何子图 (无论 signature 是否合法) 都被接受。

**与 Axis6 chain 的治理不对称 (G5)**: Axis6 chain 有完整治理链 (IEvaluator + governor authorize + attribution + 回归门), 而 GenerateSubGraph 治理为零 (signature 校验是占位符)。LLM 可通过 GenerateSubGraph 生成等效于 axis6 chain 的子图, 完全绕过治理。

### 修复 (0.5 sprint)

将占位符替换为**真实 schema 校验**, 复用 ADR-0073 已实装的 `tool_schema_validator.{h,cpp}` (nlohmann 递归 validate_node)。

### 前置依赖 (全部已 ship)

| 依赖 | 状态 | 验证 |
|------|------|------|
| `tool_schema_validator.h` + `.cpp` (ADR-0073) | ✅ 实装 | `validate_node()` 递归校验 |
| `Node::signature` + `signature_validation` + `on_signature_violation` | ✅ 实装 | `src/core/types/node.h:53,201,202` |
| `ParsedGraph::signature` + `output_schema` | ✅ 实装 | `node.h:93,96` |
| `on_signature_violation` jump 路径 | ⚠️ 注释 ("This requires scheduler logic to handle jumps") | 本 change 处理 |

## What Changes

### Phase 0 (本 change 立即, ~0.5 sprint)

1. **`src/modules/executor/node_executor.cpp`** 修改 (execute_generate_subgraph 内, line 305-325):
   - 替换 `bool is_valid = true; // Placeholder` 为真实校验:
     - 调用 `ToolSchemaValidator` (ADR-0073) 校验 `graph.signature` 的 inputs/outputs schema
     - 校验规则:
       - signature 存在且含 inputs/outputs 字段 → 校验 JSON Schema 结构合法性
       - signature 存在但结构非法 → `is_valid = false`
       - signature 不存在 → `is_valid = true` (无 signature 不校验, 保持现状)
   - 保留 strict/warn/ignore 三分支行为不变 (strict 抛异常 / warn LOG_WARN / ignore 跳过)

2. **`src/modules/executor/signature_validator.h` + `.cpp`** (新建, 可选提取):
   - `bool validate_subgraph_signature(const ParsedGraph& graph, std::string* error_msg)` — 独立函数, 复用 ToolSchemaValidator
   - 将校验逻辑从 execute_generate_subgraph 内联代码提取为独立函数 (可测试性)

3. **`tests/test_signature_validation.cpp`** (新建, ≥6 cases):
   - 合法 signature (inputs + outputs 完整) → is_valid=true
   - signature 缺 outputs → is_valid=false
   - signature 结构非法 (非 JSON Schema) → is_valid=false
   - strict 模式 + 非法 signature → 抛 runtime_error
   - warn 模式 + 非法 signature → LOG_WARN + 继续 (不抛)
   - ignore 模式 + 非法 signature → 跳过 (不校验不告警)
   - 无 signature → is_valid=true (不校验)

4. **on_signature_violation jump 路径**: 当前注释 "This requires scheduler logic to handle jumps" — 本 change 处理:
   - strict 模式 + `on_signature_violation` 存在时, 不抛异常, 而是将 violation jump 路径写入 context (`__signature_violation_jump__` key), 由 ExecutionSession/调度器处理跳转
   - 或保持抛异常 (简化 V1) + 文档注明 jump 路径待 scheduler 支持 — **V1 选择保持抛异常**, jump 路径属后续 change

### 明确不做

- ❌ 修改 GenerateSubGraph 的 append_graphs 断链 (独立 change `generatesubgraph-append-restore`)
- ❌ on_signature_violation jump 的调度器实装 (后续 change, 需 ExecutionSession 支持)
- ❌ cognitive_domain 检测升级 (G5 第二步, 依赖本 change + 后续)
- ❌ 新增 contract 头文件
- ❌ 修改 ToolSchemaValidator 实装 (复用, 不修改)

## 不变量

- **不变量 1**: 复用 ADR-0073 `tool_schema_validator` (nlohmann 递归 validate_node), 不重复实现 JSON Schema 校验
- **不变量 2**: strict/warn/ignore 三分支行为不变 (仅 is_valid 从恒 true 变为真实校验结果)
- **不变量 3**: 无 signature 的图不校验 (is_valid=true, 保持现状)
- **不变量 4**: fail-closed — 校验失败 + strict → 抛异常 (与现有行为一致)
- **不变量 5**: contract 零修改 (`include/agenticdsl/contract/`)
- **不变量 6**: on_signature_violation jump 路径 V1 保持抛异常 (调度器跳转属后续 change)

## 风险

| 风险 | 影响 | 缓解 |
|------|------|------|
| **R1 校验过严** | 合法 signature 被误判为非法 | 复用 ADR-0073 已验证的 validator + 测试 case 1 锁定合法 signature 通过 |
| **R2 校验过松** | 非法 signature 漏检 | 测试 case 2-3 锁定非法 signature 被检出 |
| **R3 strict 模式行为变化** | 之前恒通过的图现在可能失败 | 这是**预期行为变化** (占位符修复的目的); 测试 case 4 锁定 strict 抛异常 |
| **R4 on_signature_violation jump 不实装** | strict + jump 路径的图行为不明确 | 不变量 6: V1 保持抛异常, 文档注明 jump 待后续 |
| **R5 validator 性能** | 大 signature 校验慢 | signature 通常 < 1KB, nlohmann validator 性能足够 |
