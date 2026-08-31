# Tasks — Signature Validation Real Impl

> **关键不变量**: 复用 ADR-0073 validator, strict/warn/ignore 分支不变, 无 signature 不校验, fail-closed, contract 零修改, V1 jump 保持抛异常
> **估时**: 0.5 sprint
> **前置依赖**: 全部 ✅ ship (ADR-0073 validator + Node::signature + ParsedGraph::signature)
> **Oracle 评审**: session `ses_facbd3ffbffeUjlJgZsgMWFiM4` (G5 严重度上调: 占位符恒通过)

## 1. Pre-flight Verification

- [ ] 1.1 验证占位符位置
  - 命令: `grep -n "is_valid = true.*Placeholder" src/modules/executor/node_executor.cpp`
  - 预期: 1 行 (line ~309)
- [ ] 1.2 验证 ADR-0073 validator 实装
  - 命令: `grep -c "validate_node\|validate" src/common/tools/tool_schema_validator.cpp`
  - 预期: ≥3
- [ ] 1.3 验证 Node/ParsedGraph signature 字段
  - 命令: `grep -n "signature\b\|signature_validation\|on_signature_violation" src/core/types/node.h | head -5`
  - 预期: ≥3 命中

## 2. Phase 0 — signature_validator 提取 + 实现

- [ ] 2.1 新建 `src/modules/executor/signature_validator.h` + `.cpp`:
  - `bool validate_subgraph_signature(const ParsedGraph& graph, std::string* error_msg)`
  - 校验规则 (design §决策 2): signature JSON Schema 结构 + inputs/outputs 数组 + 每项 name+type
  - 复用 ADR-0073 `tool_schema_validator` (递归 validate_node)
  - 无 signature → 返回 true
- [ ] 2.2 新建 `tests/test_signature_validation.cpp` (≥6 cases):
  - `valid_signature_passes` — 合法 signature (inputs+outputs 完整) → true
  - `missing_outputs_fails` — signature 缺 outputs → false
  - `invalid_structure_fails` — 非 JSON Schema 结构 → false
  - `strict_mode_throws_on_invalid` — strict + 非法 → runtime_error
  - `warn_mode_logs_and_continues` — warn + 非法 → LOG_WARN + 不抛
  - `ignore_mode_skips_validation` — ignore → 跳过校验 (不调用 validator)
  - `no_signature_passes` — 无 signature → true (不校验)
- [ ] 2.3 编译 + 测试通过
  - 命令: `cmake --build build --target test_signature_validation && ./build/tests/test_signature_validation --reporter compact`
  - 预期: 7 cases / 20+ assertions all pass

## 3. Phase 0 — node_executor 集成 (替换占位符)

- [ ] 3.1 修改 `src/modules/executor/node_executor.cpp` execute_generate_subgraph (line ~305-325):
  - 替换 `bool is_valid = true; // Placeholder` 为 `std::string sig_error; bool is_valid = validate_subgraph_signature(graph, &sig_error);`
  - ignore 模式跳过校验调用 (性能优化)
  - strict/warn 分支行为不变 (仅 is_valid 来源从恒 true 变为真实校验)
  - strict 抛异常时含 sig_error 详情
- [ ] 3.2 验证现有 GenerateSubGraph 测试零回归
  - 命令: `./build/tests/test_executor_with_mock_provider --reporter compact`
  - 预期: 全部 pass (现有 generate_subgraph 测试的 mock DSL 含合法 signature 或无 signature)

## 4. Phase 0 — 文档同步

- [ ] 4.1 axis6-chain-workflow G5 缺口状态更新 (占位符已修复)
- [ ] 4.2 orchestration-architecture §十一 GenerateSubGraph 分析注记 (signature 校验已实装)
- [ ] 4.3 §七 P0 断链清单注记 (signature 占位符已修复, append_graphs 断链仍存)

## 5. Ship Gate

- [ ] 5.1 `openspec validate 2026-08-31-signature-validation-real-impl --strict` PASS
- [ ] 5.2 `python3 tools/adr_lint.py` 0 errors
- [ ] 5.3 `python3 tools/docs_drift_audit.py` 0 CRITICAL
- [ ] 5.4 `git diff --stat HEAD -- include/agenticdsl/contract/` = 0 行
- [ ] 5.5 ctest 全量零回归 (含 test_executor_with_mock_provider)

## 6. Commit

- [ ] 6.1 git add:
  - `src/modules/executor/signature_validator.h` + `.cpp`
  - `src/modules/executor/node_executor.cpp`
  - `tests/test_signature_validation.cpp`
  - 架构 doc 更新
- [ ] 6.2 commit message:
  ```
  fix(executor): signature-validation-real-impl — GenerateSubGraph signature 校验占位符修复 (G5)

  Oracle 评审 (session ses_facbd3ffbffeUjlJgZsgMWFiM4) 发现 G5 严重度上调:
  node_executor.cpp:309 signature 校验是占位符 (bool is_valid = true), strict 恒通过,
  GenerateSubGraph 治理 ≈ 零。

  新增 src/modules/executor/signature_validator.{h,cpp} — validate_subgraph_signature()
  独立函数 (可测试性), 复用 ADR-0073 tool_schema_validator (nlohmann 递归 validate_node)。
  校验规则: signature JSON Schema 结构 + inputs/outputs 数组 + 每项 name+type。

  node_executor.cpp 替换占位符为真实校验: strict 抛异常 (含 error 详情) / warn LOG_WARN /
  ignore 跳过校验调用 (性能)。无 signature 不校验 (不变量 3)。V1 jump 路径保持抛异常
  (on_signature_violation 调度器跳转属后续 change)。

  7 新测试 (valid/missing-outputs/invalid-structure/strict-throws/warn-logs/ignore-skips/no-sig)。
  现有 test_executor_with_mock_provider 零回归。估时 0.5 sprint。

  Ultraworked with [Sisyphus](https://github.com/code-yeongyu/oh-my-openagent)
  Co-authored-by: Sisyphus <clio-agent@sisyphuslabs.ai>
  ```

## 7. 工时估算

| Phase | 估时 |
|-------|------|
| signature_validator 提取 + 7 tests | 0.3 sprint |
| node_executor 集成 + 文档同步 + ship gate | 0.2 sprint |
| **总计** | **0.5 sprint** |

## 8. 后续追踪

- `generatesubgraph-append-restore` — 断链修复 (依赖本 change 真校验)
- `generatesubgraph-cognitive-governance` — G5 第二步 (cognitive_domain 检测)
- on_signature_violation jump 调度器实装 — 后续 change
