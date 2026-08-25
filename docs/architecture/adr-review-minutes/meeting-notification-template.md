# ADR-0071/0074 评审会议召集通知模板

> **⛛ SUPERSEDED (2026-08-25) — 治理范式转型**
>
> 本邮件/Slack 召集模板为 Single-Developer Mode 前的议会式流程设计,**已废弃**。
>
> Single-Developer Mode 使用 GitHub Issue 作为单一审查入口(无需邮件/Slack 多渠道同步):
> - 创建 issue: `.github/ISSUE_TEMPLATE/adr-review.md` 模板粘贴 body
> - 自审清单: `docs/architecture/adr-self-review-checklist.md`
> - 流程: `openspec/changes/2026-08-25-sprint-24-pre-launch-self-review/` Step 1-2
>
> **保留用途**: 历史参考 + 治理范式对比
> **继任文档**: AGENTS.md "Single-Developer Mode" + `openspec/changes/2026-08-25-sprint-24-pre-launch-self-review/`

> **文件位置**: `docs/architecture/adr-review-minutes/meeting-notification-template.md`
> **用途**: 召集 Phase 6 自进化方向架构评审会时复用
> **创建日期**: 2026-08-25
> **关联**: `adr-0071-0074-distillation-review-2026-08-24.md` (会议纪要 + 议程)

---

## 📧 邮件模板 (Architecture Working Group Mail List)

### Subject
```
[ADR评审召集] P0 自进化方向架构评审会 — 6 个 ADR 决议 (T17/T19/T21 解锁关键)
```

### Body

```
各位架构组成员：

Phase 6 自进化方向架构评审会议召集如下，请预留时间参加。

【会议基本信息】
- 日期: <YYYY-MM-DD>
- 时间: <HH:MM> - <HH:MM>  (建议 90 分钟，议程紧凑)
- 地点: <会议室 / 视频会议链接>
- 召集人: Architecture Working Group
- 优先级: P0 (Oracle 评审 ses_fcba5e477ffeG9wEBHVhU64J0o 标记"本周最高杠杆")
- 关联文档: docs/architecture/adr-review-minutes/adr-0071-0074-distillation-review-2026-08-24.md

【评审对象】6 个 ADR
1. ADR-0083 IEvaluator/RewardSignal 契约 (🔍 Proposed → ?)
2. ADR-0080 v1.2 amendment (D10 解耦) (🔍 Proposed → ?)
3. ADR-0061-13 蒸馏输出格式 (🔍 Proposed → ?)
4. ADR-0071 LLM-native AgenticDSL 架构 (🔍 Proposed → ? 待 Promotion)
5. ADR-0074 Prompt Evidence Gate (🔍 Proposed → ? 待 Promotion)

【会前必读】(~70 分钟)
- 会议纪要全文: docs/architecture/adr-review-minutes/adr-0071-0074-distillation-review-2026-08-24.md (251 行)
- Oracle 评审摘要: capability-application-map §八 (6 个新 ADR 来源)
- 调研报告: docs/architecture/pdk-chat-demo-distill-source-survey-2026-08.md
- 6 个待评审 ADR 草案 (链接见会议纪要 §二)

【议程】8 项
1. ADR-0083 IEvaluator (Oracle 关键缺口 G10)
2. ADR-0080 v1.2 D10 解耦 (Oracle 关键缺口 G12)
3. ADR-0061-13 蒸馏输出格式 (Oracle 关键缺口 G15)
4. ADR-0071 Promotion (Oracle 关键缺口 G13)
5. ADR-0074 Promotion (T21 前置)
6. 4 个 TD 项命运 (T17/T19/T20/T21)
7. 数据面/评估契约/训练管线协同 (3 项决议)
8. Sprint 23-26 排期表

【预期产出】
- 6 个 ADR 决策 (Approved/Rejected/Deferred)
- 4 个 TD 项启动时间表
- 3 项协同决议
- Sprint 24-26 排期表

【风险】
会议延期超过 2 周将延迟 T17/T19/T21 启动。请评估本周/下周时间窗口。

如有时间冲突或需要调整议程，请回复本邮件。

谢谢。
```

---

## 💬 Slack/Discord 通知模板 (#architecture-review 频道)

```
📢 **架构评审会议召集** | P0 自进化方向

🗓️ <YYYY-MM-DD> <HH:MM>-<HH:MM> | <会议室/视频链接>
🎯 优先级: P0 (Oracle 评审"本周最高杠杆")
📋 议程: 8 项 (5 ADR 决议 + 4 TD 命运 + 协同决议 + 排期表)

**评审对象**:
• ADR-0083 IEvaluator/RewardSignal (G10)
• ADR-0080 v1.2 D10 解耦 (G12)
• ADR-0061-13 蒸馏输出格式 (G15)
• ADR-0071 Promotion (G13)
• ADR-0074 Promotion (T21 前置)

📚 会前必读: `docs/architecture/adr-review-minutes/adr-0071-0074-distillation-review-2026-08-24.md` (~60min)

⚠️ **延期影响**: T17/T19/T21 启动延迟

请回复 ✅ 出席 / ❌ 缺席 + 理由 / 🔄 改期建议
```

---

## 📅 日历邀请 (.ics) 关键字段

```
SUMMARY: [P0] ADR-0071/0074 + 3 新 ADR 架构评审
DESCRIPTION: Phase 6 自进化方向架构评审会。6 个 ADR 决议。详见 docs/architecture/adr-review-minutes/adr-0071-0074-distillation-review-2026-08-24.md
LOCATION: <会议室/视频链接>
PRIORITY: 1 (最高)
ORGANIZER: Architecture Working Group <arch-wg@hydraforge.local>
ATTENDEE: <架构组成员邮件列表>
```

---

## 🔄 召集时间窗口建议

考虑到架构组繁忙度,推荐以下时间窗口:

| 选项 | 日期 | 时间 | 备注 |
|---|---|---|---|
| A (推荐) | <本周三/周四> | 14:00-15:30 | 距本周末最近,优先级最高 |
| B | <下周一/周二> | 10:00-11:30 | 周末后,精力充沛 |
| C | <下周三/周四> | 14:00-15:30 | 备选,通常最低冲突 |

**议程时长**: 90 分钟 (8 项议程 × 8-10 分钟 + 10 分钟 Q&A)

---

## ✅ 召集 Checklist

召集人执行:
- [ ] 发送邮件 + Slack 通知 (T-7 天)
- [ ] 创建日历邀请 (T-7 天)
- [ ] T-3 天 提醒 (Slack @here)
- [ ] T-1 天 最终确认 (回复邮件 "明日 X 点 X 会议室")
- [ ] 会议当天: 提前 15 分钟开会议室视频会议,准备投影/屏幕分享
- [ ] 会议结束: 24 小时内更新会议纪要 §决议记录

与会者执行:
- [ ] 通读会议纪要 + 6 个 ADR 草案 (~70 分钟)
- [ ] 准备 §三 决策点立场 (8 议程)
- [ ] 准备 §六 风险评估

---

**维护者**: Architecture Working Group
**关联**: `adr-0071-0074-distillation-review-2026-08-24.md` §七 会议前检查清单