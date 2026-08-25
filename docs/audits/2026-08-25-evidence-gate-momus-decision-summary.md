# Evidence Gate Change — Momus REJECT Decision Summary

> **Date**: 2026-08-25
> **Source**: MOMUS 评审反馈 (`bg_03b93dbe` background task, 13m18s)
> **Decision needed**: X / Y / Z / W (见 §二)

---

## 一、MOMUS REJECT Finding 索引

### MUST-FIX (Blocking)

**MUST-FIX 1**: `.rddf/plans/from-roadmap-phase-6c-evidence-gate.md` 不存在。
**状态**: ✅ 已修复 (skeleton 已生成于 `.rddf/plans/from-roadmap-phase-6c-evidence-gate.md`)

**MUST-FIX 2**: 裁决路径矛盾 + 数据不可用
- 88.2% 在 proposal §关键场景 1 判 PASS,在决策树 [85,90) 判 CONDITIONAL
- baseline mock 模式 + 零模型 → 4.3 完整性必失败 → ABORT
- 决议真实数据消费 OOS,plan 无法产出预期裁决
- **状态**: ⛔ 未修复 — 需 user 决策 X/Y/Z 选项

**MUST-FIX 3**: 验证门禁缺失 + ADR-0074 翻牌无意义
- 8.x 无 lint/validate gate
- ctest 基线 147/147 陈旧 → 184/184
- ADR-0074 已 Approved,7.x 翻牌目标不存在
- **状态**: ✅ 部分修复 (tasks.md 8.2 改 184/184;8.4-8.6 新增 gate;7.1-7.3 改写删除翻牌)

---

## 二、决策矩阵

| 选项 | Scope | 估时 | 风险 | 推荐 |
|------|-------|------|------|------|
| **X** | Ship 模板 + evaluate_gate;真实决议 Sprint 25+ | 2-3 天 | 极低 | ⭐ |
| **Y** | 真实 3 模型测量纳本 change | 2-4 周 + API 成本 | 中-高 | — |
| **Z** | 默认 ABORT + mock 局限诚实记录 | 1 周 | 低 | — |
| **W** | 暂停 evidence-gate,改 Sprint 24 W1 | 1-2 周 | 低 | — |

---

## 三、推荐: X

- 决议需真实 LLM 数据,mock 数据 ship 决议 = 战略灾难
- Single-Dev "小步可逆"原则契合
- 不阻滞 Sprint 24 W1

---

## 四、决策请求

回复 `X` / `Y` / `Z` / `W`。
