## ADDED Requirements

### Requirement: ADR-0068 Appendix A 14 Topics MUST be Registered

ADR-0068 Appendix A 中 14 个 `📡` 主题（已发射但无注册订阅方）MUST 更新状态为 `✅ registered`，每行附有 evidence 引用。

#### Scenario: 14 主题从 📡 更新为 ✅ registered

- GIVEN ADR-0068 Appendix A 中 lines 196-209 共 14 个主题状态为 `📡`
- WHEN 本变更 ship 后
- THEN 这 14 个主题状态更新为 `✅ registered`
- AND 每行附有 source file:line 作为 evidence

#### Scenario: Evidence 引用存在且可验证

- GIVEN 14 个主题各自声明了 evidence 引用
- WHEN `grep -n "topic_name" src/ --include="*.cpp"` 执行
- THEN 对应 source file:line 存在 emit 调用

#### Scenario: 现有 ✅ 和 👻 行保持不变

- GIVEN ADR-0068 Appendix A 中现有 `✅` 和 `👻` 状态的行
- WHEN 本变更 ship 后
- THEN 这些行的状态保持不变

#### Scenario: 无代码变更

- GIVEN 本变更是文档同步
- WHEN `ctest` 执行
- THEN 测试计数和结果保持不变（147/147）
- AND `src/`、`include/`、`pdk/`、`tests/` 目录无修改

#### Scenario: 文档 gate 通过

- GIVEN 本变更更新了 `docs/adr/adr-0068-event-emission-contract.md`
- WHEN 变更 ship 后
- THEN `tools/adr_lint.py` 退出码 0
- AND `tools/docs_drift_audit.py` 无新增 DRIFT
- AND `openspec validate --strict` 退出码 0
