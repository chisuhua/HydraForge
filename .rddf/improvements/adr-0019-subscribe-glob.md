# adr-0019-subscribe-glob

**优先级**: P1 | **来源**: ADR-0019 IInteractionBus (Oracle 重构)
**阶段**: phase-6a | **分类**: 架构对齐
**类型**: feature
**主题**: canonical topic registry

## 架构依据
- Oracle 评审: 不新增 subscribe_topic，扩展 subscribe 接受 glob
- ADR-0046: PDK Plugin 间通信需要 topic-based subscribe
- 依赖 Change A (BusEvent 契约已定义)

## 范围
- subscribe() 接受 glob pattern (无通配符=精确匹配, O(1))
- InMemoryBus 双路径分发: exact map + wildcard list
- Race test: subscribe/unsubscribe 并发 dispatch

## 关键场景
（无）

## 技术约束
（无）

## 验收标准
- ctest -R test_interaction_bus_glob 全绿
- ctest 全量零回归


## Status: ✅ SHIPPED (2026-10-14 audit verification)

**Ship evidence** (验证日期 2026-10-14 via .rddf/improvements/ 全面审计):
- Commit `4d1cee6` "ship: adr-0019-subscribe-glob — glob subscribe 验证完成". OpenSpec archive: `openspec/changes/archive/2026-07-29-adr-0019-subscribe-glob/` (untracked by .gitignore). Audit source: 5-维静态 ship 判定法 (commit message grep).
- **审计来源**: 主会话 5 维静态 ship 判定法 (提案 self-declared > archive tracked > commit message > ADR status > fallback)
- **Pattern 沉淀**: 30 个 Pattern #10 drift 案例库之一, 详见 `.rddf/audits/2026-10-14-rddf-improvements-audit.md`
