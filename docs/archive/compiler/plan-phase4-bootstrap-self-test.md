# PLAN-P4: Phase 4 — Bootstrap & Self-Test

**状态**: 待讨论
**前置**: PLAN-P3 完成
**覆盖**: Bootstrap Loader + 自举验证 + 交叉编译测试 + 回滚机制

---

## 依赖

- [ ] ADR-0022 (D6) 决策确定
- [ ] SPEC-BOOTSTRAP 定稿
- [ ] PLAN-P3 (Compiler Pipeline) 完成

## 待定义内容

- Bootstrap Loader 的实现文件
- 自举触发检测（metadata.name == "skill-compiler"）
- 自举优化路径（固化模板、固定路径、自注册节点）
- 交叉验证方案（用编译后的编译器编译 daily-report）
- 回滚机制（priority 降级、手写版保留）
- 端到端测试
