# PLAN-P3: Phase 3 — Compiler Pipeline

**状态**: 待讨论
**前置**: PLAN-P2 完成
**覆盖**: 编译器 7 Phase 实现 + JIT/AOT 双模式

---

## 依赖

- [ ] ADR-0021 (D7, D9, D11, D12) 决策确定
- [ ] SPEC-SKILL-MD 定稿
- [ ] SPEC-COMPILED 定稿
- [ ] PLAN-P2 (Skill Registry) 完成

## 待定义内容

- 编译器 SKILL.md 文件实现
- 7 Phase 每个节点的具体实现文件
- References (json-extract-schema, phase-codegen-patterns, agenticdsl-syntax) 文档创建
- Scripts (validate-dsl.py, extract-phases.py) 创建
- JIT 和 AOT 模式的实现路径
- 测试策略和验证标准
