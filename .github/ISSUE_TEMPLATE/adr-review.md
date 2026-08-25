---
name: ADR Self-Review
about: ADR 自审 issue (single-developer mode)
title: "[ADR-XXXX] Self-Review: <title>"
labels: ["adr-review", "self-review", "sprint-23"]
assignees: []
---

## ADR 概述

<!-- 1-2 段说明本 ADR 是什么、解决什么问题、影响哪些 Gap -->

**ADR 文件**: `docs/adr/<path>/adr-XXXX-*.md`
**状态**: 🔍 Proposed → (待自审)
**关联 Gap**: GXX
**关联 TD 项**: TXX (如适用)
**Oracle session**: (引用 Oracle session ID)

---

## Self-Review Checklist (8-12 项)

<!-- 按 ADR 实际内容定制决策点 -->

### 设计完整性
- [ ] **1. 背景与上下文**: ADR §背景 段落清晰说明问题与动机
- [ ] **2. 决策 (Decision)**: §决策 段落列出具体决策项, 每项有推荐方案与理由
- [ ] **3. 不变量 (Invariants)**: §不变量 段落列出实施后必须保持的属性

### 风险与备选
- [ ] **4. 风险评估**: §风险 段落识别 ≥3 项风险, 含影响范围与缓解措施
- [ ] **5. 备选方案**: §备选 段落列出 ≥2 项替代方案, 解释为何不采纳
- [ ] **6. 触发条件 (如适用)**: §触发 段落列出 ADR 实施的前置条件

### 实施与依赖
- [ ] **7. 实施计划**: §实施 段落列出具体实施步骤与估时
- [ ] **8. 依赖关系**: ADR 引用的其他 ADR 全部存在且状态正确
- [ ] **9. 反向影响**: 不影响已 ship 的能力 (或明确标注 breaking change)

### 与现有架构协调
- [ ] **10. 契约层一致性**: 与 `include/agenticdsl/contract/` 现有契约协调 (命名/接口/语义)
- [ ] **11. 类型系统兼容**: 与 `types/` 现有类型兼容 (不引入冲突的字段或语义)
- [ ] **12. 文档同步**: `capability-application-map-2026-08.md` §二/§三/§四/§八 引用同步

---

## 自审决策 (24h Cooling-Off 后填写)

<!-- 创建 issue 后 24h 才能填写; 期间如发现新问题则更新此节 -->

### 最终决策

- [ ] ✅ **Approved** — 接受全部决策点, 进入实施
- [ ] ❌ **Rejected** — 拒绝, 需修改后重新评审
- [ ] ⏸ **Deferred** — 延期, 待前置条件满足

### 风险接受声明

<!-- 列出本次自审明确接受的风险, 防止"埋雷" -->

- (例) ADR-0083 ExecutionTrace 由实施者定义最小版本, T15 集成时同步
- (例) ADR-0080 v1.2 隐私 fail-open 语义需 V2 集成 ADR-0081 scrub hook

### 修改 (如有)

<!-- 列出自审过程中对 ADR 的修订 -->

- (例) §决策 2 由"强制三重保护"改为"强制 CLI + 可选路径前缀"

---

## 冷却期

- **Issue 创建**: YYYY-MM-DD HH:MM
- **冷却期结束**: YYYY-MM-DD HH:MM (24h 后)
- **最终决策时间**: YYYY-MM-DD HH:MM
- **冷却期内变更**: (记录新增反对意见或调整)

---

## 签发

- **作者 / 评审人**: solo-dev (单人模式, 自审 = 自决)
- **日期**: YYYY-MM-DD
- **关联 Sprint**: Sprint XX
- **关联 capability-map**: `docs/architecture/capability-application-map-2026-08.md` §X