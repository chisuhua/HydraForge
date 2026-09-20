# Tasks: adr-0068-appendix-a-evolution-themes

## 1. Pre-flight (0.1d)
- [ ] 1.1 验证 ADR-0068 Appendix A 现状 (grep "evolution." 主题清单)
- [ ] 1.2 验证 EventBuilder 强制注册检查代码路径

## 2. Implementation (0.3d)
- [ ] 2.1 docs/adr/adr-0068-event-emission-contract.md: v1.9 amendment 注册 2 个 evolution 主题 + payload schema 引用
- [ ] 2.2 (可选) src/common/contract/event_builder.cpp: 主题白名单更新
- [ ] 2.3 docs/adr/adr-0068-event-emission-contract.md 头部状态字段翻牌 (✅ Approved v1.9)

## 3. Test (0.1d)
- [ ] 3.1 test_event_emission_contract 验证: emit(evolution.transition.denied, ...) 不抛 UnknownTopicError
- [ ] 3.2 ctest 全量 248+ tests 零回归验证

## 4. Doc sync (0.1d)
- [ ] 4.1 docs/README.md ADR 表 ADR-0068 行 status 字段翻牌 (✅ Approved v1.9)
- [ ] 4.2 docs/active-status.md 更新 ADR-0068 状态 (如有)
- [ ] 4.3 ADR-0088 §状态 段补 D8 "字符串常量 → 完全 ship" 标记

## 5. Archive (0.1d)
- [ ] 5.1 openspec archive → archive/2026-09-20-adr-0068-appendix-a-evolution-themes/
- [ ] 5.2 git ls-files 验证 4 文件完整
- [ ] 5.3 commit 命名: `feat(adr-0068-v1-9): evolution.themes registration`

## Acceptance

- [ ] AC-1: evolution.transition.denied 主题注册
- [ ] AC-2: evolution.readiness.denied 主题注册
- [ ] AC-3: EventBuilder enforcement 不 REJECT 新主题
- [ ] AC-4: ADR-0088 D8 完全 ship 标记
- [ ] AC-5: test_event_emission_contract 零回归
- [ ] AC-6: ctest 全量 248+ tests 零回归