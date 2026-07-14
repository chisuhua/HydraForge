# Service Composition Spike — Artifacts (📦 WIP)

> **Status**: 🟡 WIP (W2-W3 实施 active,等待 Stage Gate 2026-07-18 + Sprint 23 capacity 1.5 eng × 2 周启动)
> **Owners**: HydraForge 平台团队 (1.5 eng, 2026-07-19 ~ 2026-08-01)
> **OpenSpec change**: `openspec/changes/phase6-service-ification-v1/`
> **ADR**: [`adr-0051-phase6-pdk-composition-spike.md`](../adr/adr-0051-phase6-pdk-composition-spike.md)

---

## 目录结构

| 目录 | 用途 | 创建时机 |
|------|------|---------|
| `observations/` | Layer 3 dual memos (primary + reviewer 独立 1-page memo) | W2 D7-D9 |
| `layer1-checklist.md` | (待 W2 创建) Layer 1 静态代码 review checklist 5 类别 | per tasks.md §5.1 |
| `layer3-memo-template.md` | (待 W2 创建) Layer 3 memo 模板 5 固定 sections | per tasks.md §5.4 |
| `spike-onboarding.md` | (待 W3 创建) G2/G4/G5 team kickoff onboarding 文档 2-3 页 | per tasks.md §8.1 |

---

## 当前文件 (2026-07-16 预创建)

- `README.md` (本文件) — 路径占位 + 用途说明
- `observations/` — 空,等待 W2 D7-D9 Layer 3 dual memos 落入

## D1 启动会议 (2026-07-19) 工具链检查清单

```bash
# 验证目录存在
ls -la docs/service-composition/

# 验证 git track
git status docs/service-composition/  # 应显示 README.md + observations/ (空目录可能不显示,需 .gitkeep)
```

## D1 推荐决策 (Oracle 建议 P1 #D-5)

- ✅ 目录已创建 (2026-07-16 Oracle 自动应用)
- ⏳ D1 确认 git 是否需要 `.gitkeep` 文件以保留空 `observations/` 目录
- ⏳ D1 确认 `layer1-checklist.md` / `layer3-memo-template.md` / `spike-onboarding.md` 是否在此目录创建 (vs `docs/guides/` 顶层)

---

**最后更新**: 2026-07-16 (Oracle D1 议程建议 #D-5 应用)
**关联 OpenSpec**: `openspec validate phase6-service-ification-v1 --strict` ✅ EXIT 0
