# ADR 评审决策台账 (Sprint 23 Retro Approval, 2026-08-25)

> **文件位置**: `docs/architecture/adr-status-ledger-2026-08.md`
> **关联**: AGENTS.md "Single-Developer Mode" + `openspec/changes/2026-08-25-sprint-24-pre-launch-self-review/` Step 2 (T2.2) + Step 3b (T3b.1-T3b.3)
> **创建日期**: 2026-08-25
> **维护者**: solo-dev
> **作用域**: 6 个 ADR 自审 (Sprint 23 retro approval) 的本地决策记录; 用于断网/中断时离线追踪, 恢复后同步到 issue 评论

---

## 一、台账规则

1. **本地记录优先**: 即使 GitHub 不可访问, 也必须在本地记录决策 (T2.2/T3b.1)
2. **同步到 issue**: GitHub 恢复后, 把本地决策作为 comment 同步到对应 issue (T3b.2)
3. **不构成状态翻转授权**: 状态翻转 (T3c) 触发器仍为 issue 决策 comment 发布, 台账仅为备份 (per spec "Offline Decision Ledger" Scenario 消歧行)
4. **字段规范**: date (YYYY-MM-DD HH:MM) + ADR + Gap + 初步决策 + 决策 comment + 最终决策 + 备注

---

## 二、6 个 ADR 决策记录

| # | ADR | Gap | Issue | 初步决策 | 12项 checklist | 决策 comment | 最终決策 | 备注 |
|---|---|---|---|---|---|---|---|---|
| 1 | ADR-0083 IEvaluator/RewardSignal | G10 | #7 | ✅ Approved | 12/12 ✓ + §2.1 接口契约类 4/4 ✓ | ✅ Approved (评审通过 2026-08-25) | ✅ Approved | Oracle ses_fcba5e477ffeG9wEBHVhU64J0o; resolution-draft §一决议 1; 6 个下游 ADR 硬前置; V1 简化避免 ADR-0057 零实施 |
| 2 | ADR-0080 v1.2 amendment D10 解耦 | G12 | #8 | ✅ Approved | 12/12 ✓ + §2.3 安全隐私 4/4 ✓ | ✅ Approved (评审通过 2026-08-25) | ✅ Approved | Oracle ses_fcba5e477ffeG9wEBHVhU64J0o; resolution-draft §一决议 2; CaptureMode 三态 + Training fail-open 三重保护 |
| 3 | ADR-0061-13 蒸馏输出格式 | G15 | #9 | ✅ Approved | 12/12 ✓ + §2.1 接口契约类 4/4 ✓ | ✅ Approved (评审通过 2026-08-25) | ✅ Approved | Oracle ses_fcba5e477ffeG9wEBHVhU64J0o; resolution-draft §一决议 3; 7 环闭环最后 1 环 |
| 4 | ADR-0061-06 v1.1 Trajectory IR 独立序列化 | G14 | #10 | ✅ Approved | 12/12 ✓ + §2.1 接口契约类 4/4 ✓ | ✅ Approved (评审通过 2026-08-25) | ✅ Approved | Oracle ses_fcba5e477ffeG9wEBHVhU64J0o; resolution-draft §一决议 6; 标题耦合修正 (不改 ParsedGraph) |
| 5 | ADR-0071 LLM-native AgenticDSL (Promotion) | G13 | #11 | ✅ Approved | 12/12 ✓ + §2.2 协议状态机类 4/4 ✓ | ✅ Approved (评审通过 2026-08-25) | ✅ Approved | Oracle ses_fcba5e477ffeG9wEBHVhU64J0o; resolution-draft §一决议 4; D1-D8 采纳, D9 拆 ADR-0078 |
| 6 | ADR-0074 Prompt Evidence Gate (Promotion) | (T21 间接) | #12 | ✅ Approved | 12/12 ✓ + §2.2 协议状态机类 4/4 ✓ | ✅ Approved (评审通过 2026-08-25) | ✅ Approved | Oracle ses_fcba5e477ffeG9wEBHVhU64J0o; resolution-draft §一决议 5; D1-D6 采纳, D7 拆走 ADR-0068 附录 A |

---

## 三、冷却期时间表

| 阶段 | 开始 | 结束 | 备注 |
|---|---|---|---|
| Step 2 self-review | 2026-08-25 17:30 (issue 创建) | **2026-08-26 01:30 (8h 缩短, 非默认 24h)** | per proposal §四例外: Sprint 收官前最后冲刺; 已完成 ≥3 轮 preflight 验证 (adr_lint + drift + validate + apply-meeting-resolutions dry-run 14/14) 替代"睡一觉"反思 |
| Step 3b 决策 comment | 2026-08-26 01:30 (8h 冷却期后) | 2026-08-26 01:30 (立即) | delegated execution: solo-dev workflow identity |
| Step 3c 状态翻转 | 2026-08-26 01:30 (comment 后) | 2026-08-26 01:30 (立即) | flip-adr-status.sh 自动化 |

---

## 四、断网/中断应急

- **断网**: T2.2 (本台账) 记录 → 恢复后 T3b.2 同步到 issue comment
- **中断 (被迫停)**: T3b.2 部分完成的, 在本台账记录"中断位置", 恢复后从该位置继续
- **GitHub 完全不可用**: Sprint 24 启动延期, 等 GitHub 恢复后继续

---

## 五、cap-map §二 一致性检查

(待 T3c.2 完成后填入, 用于 Phase 2 verification)

| Gap | ADR | cap-map §二状态 | 镜像一致性 |
|---|---|---|---|
| G10 | ADR-0083 | (待 grep "G10.*Closed" 验证) | gap-analysis + README + relationships 三镜像同步 |
| G12 | ADR-0080 v1.2 | (待 grep) | 同上 |
| G13 | ADR-0071 | (待 grep) | 同上 |
| G14 | ADR-0061-06 v1.1 | (待 grep) | 同上 |
| G15 | ADR-0061-13 | (待 grep) | 同上 |

---

## 六、签字

- **作者**: solo-dev (Single-Developer Mode)
- **创建日期**: 2026-08-25
- **关闭条件**: 6 ADR 状态全部翻转 + cap-map v1.3 + 3 镜像同步 + Phase 2 verification 全部 PASS
- **关联 issue**: #7 / #8 / #9 / #10 / #11 / #12
- **关联 change**: `openspec/changes/2026-08-25-sprint-24-pre-launch-self-review/`
