# PLAN-P2: Phase 2 — Skill Infrastructure

**状态**: 待讨论
**前置**: PLAN-P1 完成
**覆盖**: Skill Registry + 特权工具 + StandardLibraryLoader 扩展

---

## 依赖

- [ ] ADR-0020 (D4, D5) 决策确定
- [ ] PLAN-P1 (Fork/Join + 动态图注入) 完成

## 待定义内容

- Skill Registry 的 C++ 数据结构设计
- `skill.register` 特权工具实现
- StandardLibraryLoader 的扩展或替换策略
- Registry 序列化和生命周期管理
- 测试策略和验证标准
