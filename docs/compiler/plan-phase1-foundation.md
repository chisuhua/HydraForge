# PLAN-P1: Phase 1 — Foundation

**状态**: 待讨论
**前置**: ADR-0019 决策完成
**覆盖**: Fork/Join 并发实现 + 动态图注入修复 + LayeredContext L4 落地

---

## 依赖

- [ ] ADR-0019 (D1, D2) 决策确定
- [ ] ADR-0008 (LayeredContext) 已批准——需确定迁移起点

## 待定义内容

- Fork/Join 实现的具体代码文件和接口
- 主循环检查 `dynamic_graphs_` 的时序和触发条件
- LayeredContext L4 Working 层的第一个落地实现
- 测试策略和验证标准
- 与其他模块的接口变更
