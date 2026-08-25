# Sprint 24 Pre-Launch 任务安排 (Single-Developer Mode)

> **状态**: 🟡 Active (2026-08-25)
> **关联**: AGENTS.md "Single-Developer Mode" 章节
> **来源**: Oracle session `ses_fc93a2994ffeyGbFyDJYtP1MhZ` S4 全部 5 项修正 + Sprint 23 全部 ship
> **目标**: Sprint 24 启动周 (2026-08-31 或 2026-09-01) 启动前完成全部前置工作

---

## 一、上下文与目标

### 1.1 Sprint 23 已 ship 状态

| 任务 | 状态 | OpenSpec Archive | Commit |
|---|---|---|---|
| **T14** 行为回归套件 (ADR-0061-02) | ✅ ship | `2026-08-25-2026-08-24-adr-0061-02-behavioral-regression` | `93ebc8d` |
| **T16** SLM 路由 (ADR-0061-04) | ✅ ship | `2026-08-25-2026-08-24-adr-0061-04-slm-routing` | `c5fb833` |
| **Cap-map v1.2** | ✅ updated | — | `d803158` |
| **6 个 ADR 草案** | ✅ drafted | — | `d803158` |
| **4 个会议材料** | ⚠️ 过度设计 | — | `d803158` |

### 1.2 Sprint 24 启动前置 (剩余)

- 6 个 ADR 决议落地 (4 新 + 2 Promotion)
- capability-map v1.3 更新 (Gap 4 闭合 + 任务命运更新)
- Sprint 24 kickoff issue + T17 启动准备

### 1.3 治理范式转换

- **从**: 9 议程 + 表决规则 + 异议角色 + 邮件/Slack召集
- **到**: 6 GitHub issue + Self-review checklist + 24h cooling-off

---

## 二、5 步任务安排 (Day 0 = 2026-08-25)

### Step 1: 创建 6 个 GitHub Issue (Day 0, 30 min)

每个 ADR 一个 issue, 用 `.github/ISSUE_TEMPLATE/adr-review.md` 模板粘贴 body。

```bash
# 假设 GitHub CLI 已认证
cd /workspace/project/HydraForge

gh issue create \
  --title "[ADR-0083] Self-Review: IEvaluator/RewardSignal 契约 (G10 解锁)" \
  --label "adr-review,oracle-p0,sprint-23" \
  --body-file .github/ISSUE_TEMPLATE/adr-review.md \
  --assignee @me

gh issue create \
  --title "[ADR-0080 v1.2] Self-Review: D10 解耦 amendment (G12 死锁解除)" \
  --label "adr-review,oracle-p0,sprint-23" \
  --body-file .github/ISSUE_TEMPLATE/adr-review.md \
  --assignee @me

gh issue create \
  --title "[ADR-0061-13] Self-Review: 蒸馏输出格式 (G15 解锁)" \
  --label "adr-review,oracle-p0,sprint-23" \
  --body-file .github/ISSUE_TEMPLATE/adr-review.md \
  --assignee @me

gh issue create \
  --title "[ADR-0061-06 v1.1] Self-Review: Trajectory IR 标题修订 (G14 解锁)" \
  --label "adr-review,oracle-p0,sprint-23" \
  --body-file .github/ISSUE_TEMPLATE/adr-review.md \
  --assignee @me

gh issue create \
  --title "[ADR-0071] Self-Review: Promotion → Approved (G13 解锁)" \
  --label "adr-review,promotion,oracle-p0,sprint-23" \
  --body-file .github/ISSUE_TEMPLATE/adr-review.md \
  --assignee @me

gh issue create \
  --title "[ADR-0074] Self-Review: Promotion → Approved (T21 前置)" \
  --label "adr-review,promotion,oracle-p0,sprint-23" \
  --body-file .github/ISSUE_TEMPLATE/adr-review.md \
  --assignee @me
```

**产出**: 6 issue URLs

---

### Step 2: 6 个 Issue Self-Review (Day 0-1, 2 小时, ≤20 min/issue)

每个 issue 跑同一份 checklist (见 `docs/architecture/adr-self-review-checklist.md`):

| ADR | 决策点 (8-12 项) | 决策 (✅/❌/⏸) | 备注 |
|---|---|---|---|
| ADR-0083 | V1 范围 (TaskSuccess + BehavioralEquivalence vs 完整 Composite) | ✅ | Oracle 推荐 |
| | RewardSignal 三态 vs 二态 | ✅ 三态 | 保留"成功但低效"中间带 |
| | 与 ToolResult.ok 关系 | ✅ 正交 | success=true 但 quality=Poor |
| | 接口位置 (contract/ vs evaluation/) | ✅ contract/ | 与现有契约层一致 |
| | 是否依赖 ExecutionTrace (无属主) | ⚠️ 实施时定义最小版本 | 解 ADR-0083→T15 循环依赖 |
| ADR-0080 v1.2 | CaptureMode 三态 (Off/Online/Training) | ✅ | 过渡路径 |
| | Training 模式 CLI 标志 | ✅ 强制 | 三重保护 (CLI + 路径前缀 + WARNING) |
| | Online→Training 降级行为 | ✅ WARN + 降级 | 静默拒绝破坏可用性 |
| | V1/V2 拆分 | ✅ 拆 | V1 立即解锁, V2 等 ADR-0081 |
| ADR-0061-13 | 三文件分离 (trajectory/policy/meta) | ✅ | 训练消费 policy |
| | DistillationRecord 字段 (reward 必填) | ✅ | 对齐 ADR-0083 |
| | 与 ADR-0078 合并 | ❌ 不合并 | 外部阻塞 |
| | 与 Trajectory IR (0061-06) 边界 | ✅ 正交 | IR 序列化, 本 ADR 加 ML 字段 |
| ADR-0061-06 v1.1 | 标题 (独立序列化视图 vs 升级 ParsedGraph) | ✅ 独立 | 训练/运行时解耦 |
| | ExecutionTrace 边界 | ✅ ADR-0083 实施时定义最小版本 | |
| | V1/V2 拆分 | ✅ V1 立即 | |
| | 与 ADR-0061-06 v2 合并 | ❌ 独立 amendment | |
| ADR-0071 | 整体 Approved vs 拆 6 子 ADR | ✅ 整体 | 顶层方向成熟 |
| | D1-D9 全采纳 | ✅ D1-D8, D9 拆出 ADR-0078 | |
| | 是否需要 v1.1 amendment | ✅ 需要 | 整合 6 子项 |
| ADR-0074 | D1-D7 全采纳 | ✅ D1-D6, D7 拆分 | |
| | 训练数据格式 (D6 JSONL) | ✅ | 对齐 ADR-0061-13 |
| | 与 IEvaluator (0083) 关系 | ✅ 0083 评估, 0074 证据 | |

**每个 issue 末尾留决策 comment**:
```markdown
## Self-Review 决策 (2026-08-XX)

✅ **Approved** (基于 Oracle 预审 + checklist 全过)

风险接受:
- ADR-0083: ExecutionTrace 由实施者定义最小版本, T15 集成时同步
- ADR-0080 v1.2: 隐私 fail-open 语义需 V2 集成 ADR-0081 scrub hook

冷却期结束: 2026-08-XX 24:00 (24h 后如无新增反对意见即视为通过)

签发: solo-dev
```

**产出**: 6 issue 含决策 comment

---

### Step 3: 24h Cooling-Off + Capability-Map v1.3 (Day 1-2, 30 min)

**冷却期目的**: 给"睡一觉再决定"留窗口 (避免冲动决策)

```
T+0  = Day 0 (2026-08-25) issue 创建 + 自审
T+24h = Day 1 (2026-08-26) 冷却期结束
      = 重新阅读 issue + 确认决策无变更
      = 6 个 ADR 状态字段手工更新: 🔍 Proposed → ✅ Approved
```

**手工翻 5+1 个 ADR 状态** (脚本不做):
```bash
# 5 个新 ADR (Draft state: **状态**: 🔍 **Proposed**)
# 1 个 ADR 标题修订 (同样)

# 编辑器替换模式 (例: ADR-0083)
sed -i 's|\*\*状态\*\*: 🔍 \*\*Proposed\*\* (Oracle 评审识别为架构层缺口, v1.1 capability-application-map §八 G10)|**状态**: ✅ **Approved** (Self-review 2026-08-XX, 冷却期已结束)|' \
  docs/adr/adr-0083-evaluator-reward-contract.md

# 类似替换其余 5 个 ADR
```

**更新 capability-map v1.3**:
```bash
python3 scripts/apply-meeting-resolutions.py --dry-run
# 验证: 4 Gaps + 5 TD + §七 v1.3 全匹配

python3 scripts/apply-meeting-resolutions.py
# 实跑: capability-map §二/§八/§七 自动更新
```

**产出**: 1 commit (6 ADR 状态翻转 + capability-map v1.3)

---

### Step 4: Sprint 24 Kickoff Issue (Day 2, 30 min)

创建 1 个 issue 串联 Sprint 24 全部工作:

```bash
gh issue create \
  --title "[Sprint 24] Kickoff: 自进化方向基础设施完整 (T17 + ADR 实施)" \
  --label "sprint-24,kickoff,phase-6-self-evolution" \
  --body-file .github/ISSUE_TEMPLATE/sprint-kickoff.md \
  --assignee @me \
  --milestone "Sprint 24"
```

**Issue body 模板** (`sprint-kickoff.md`):

```markdown
## Sprint 24 目标 (2026-08-31 → 2026-09-13)

主目标: Phase 6 自进化方向基础设施完整 (G10/G12/G13/G14/G15 全部 Closed)

## 启动周任务 (Week 1)

- [ ] **T17** SkillCompiler (ADR-0061-03) 骨架 (1 sprint)
      前置: ADR-0071 ✅ Approved
      OpenSpec change 已 active (2026-08-24-adr-0061-03-skill-compiler)
- [ ] **ADR-0080 v1.2 amendment** ship (0.5 sprint)
      前置: 本周 6 个 issue 决议
- [ ] **ADR-0071 v1.1 amendment** 起草 (0.5 sprint)
      前置: ADR-0071 ✅ Approved

## 排期表

| Sprint 24 启动周 | T17 骨架 + ADR-0071 v1.1 + ADR-0080 v1.2 ship |
| Sprint 24 末 | T19 GEPA R 轨 spike 启动 |
| Sprint 25 启动周 | ADR-0083 IEvaluator ship + ADR-0061-13 ship |
| Sprint 25 末 | T15 Trajectory IR + T21 Prompt Evidence Gate |

## 风险与备选

- **Sprint 24 排期过载**: 4 个 sprint-week 工作量塞进 2 周, 单人执行必滑期
  备选: T19 GEPA spike 推迟到 Sprint 25
- **G11 变异治理缺位**: T19 spike 启动前需 G11 契约草案
  备选: T19 启动与 G11 草案并行 (1 sprint 双轨)

## 自审清单

- [ ] 6 个 ADR 决议 issue 已批准 (冷却期已结束)
- [ ] capability-map v1.3 已更新 (G10-G15 全部 ✅ Closed)
- [ ] 5+1 个 ADR 状态字段已翻转 (✅ Approved)
- [ ] T17 OpenSpec change tasks.md Phase 1 已启动

签发: solo-dev @ 2026-08-XX
```

**产出**: 1 issue URL + milestone

---

### Step 5: Sprint 24 启动周 (Day 3+, 由 Sprint 24 owner 执行)

按 kickoff issue 任务清单执行, 本任务安排到此结束。

---

## 三、风险与备选

### 风险 1: Sprint 24 排期过载

**描述**: T17(2s) + ADR-0080 v1.2(0.5s) + ADR-0083(1s) + ADR-0071 v1.1(0.5s) ≈ 4 sprint-week 工作量塞进 2 周 Sprint 24, 单人执行约束下可能滑期。

**备选**:
- T19 GEPA spike 推迟到 Sprint 25 (G11 治理契约到位后启动)
- T15 Trajectory IR 推迟到 Sprint 26 (T17 完成后启动)
- ADR-0083 IEvaluator 推迟到 Sprint 25 启动周

### 风险 2: G11 变异治理缺位

**描述**: T19 GEPA R 轨 spike 启动需 G11 契约, 但 G11 无 ADR 草案。

**备选**:
- T19 启动与 G11 草案并行 (1 sprint 双轨, 风险高)
- T19 推迟到 G11 草案 ship 后 (估时 1-2 sprint 延期)

### 风险 3: 冷却期 24h 错过

**描述**: 用户可能希望立即推进 (不等待 24h)。

**备选**:
- 24h 冷却期可缩短至 8h (睡一觉即可), 但需 issue 显式注明
- 24h 冷却期可跳过, 但需写"跳过原因" + 风险自担

### 风险 4: PR Review 缺失

**描述**: Single-dev 模式没有 PR Review 流程, 容易遗漏架构层漏洞。

**备选**:
- 复杂 PR (>200 行) 启用 Oracle pre-review (session 评估)
- 高风险 PR (涉及 ADR 修订) 启用 Oracle 风险评估
- 一般 PR 自我 review + ctest 验证即可

---

## 四、关联文档

### 当前 Sprint 23 产出 (已 ship)

| 文档 | 用途 |
|---|---|
| `docs/architecture/capability-application-map-2026-08.md` (v1.2) | 23 项能力 + 15 项 gap + 17 类应用 + 22 任务 |
| `docs/architecture/pdk-chat-demo-distill-source-survey-2026-08.md` | SessionWriter JSONL 临时数据源调研 |
| `docs/architecture/adr-review-minutes/resolution-draft-2026-08-25.md` | Oracle 预审决议草案 (作为 issue 自审参考) |
| `docs/adr/adr-0083-evaluator-reward-contract.md` | IEvaluator 契约 (草案) |
| `docs/adr/adr-0080-v1-2-amendment-d10-decouple.md` | D10 解耦 (草案) |
| `docs/adr/skill/adr-0061-13-distillation-output-format.md` | 蒸馏输出格式 (草案) |
| `docs/adr/skill/adr-0061-06-v1-1-amendment-trajectory-ir-decouple.md` | Trajectory IR 标题修订 (草案) |
| `docs/adr/adr-0071-llm-native-agenticdsl-architecture.md` | LLM-native AgenticDSL (Promotion 候选) |
| `docs/adr/adr-0074-prompt-evidence-gate.md` | Prompt Evidence Gate (Promotion 候选) |
| `scripts/apply-meeting-resolutions.py` | Capability-map 自动化更新脚本 |

### Sprint 24 启动周产出 (待 ship)

| 文档 | 用途 |
|---|---|
| `.github/ISSUE_TEMPLATE/adr-review.md` | 6 个 ADR self-review issue body 模板 |
| `.github/ISSUE_TEMPLATE/sprint-kickoff.md` | Sprint kickoff issue body 模板 |
| `docs/architecture/adr-self-review-checklist.md` | Self-review 标准化清单 |
| 6 个 GitHub issue | ADR self-review 决策记录 |
| 1 个 Sprint 24 kickoff issue | Sprint 24 任务串联 |
| capability-map v1.3 | Gap 4 闭合 + 任务排期更新 |
| T17 SkillCompiler Phase 1 骨架 | 代码 |

### 归档材料 (仅作历史参考)

| 文档 | 状态 |
|---|---|
| `docs/architecture/adr-review-minutes/adr-0071-0074-distillation-review-2026-08-24.md` | ⚠️ 过度设计 (9 议程 + 表决 + 异议角色) |
| `docs/architecture/adr-review-minutes/meeting-notification-template.md` | ⚠️ 邮件 + Slack 召集流程 (Single-dev 不需要) |
| `docs/architecture/adr-review-minutes/meeting-minutes-form.md` | ⚠️ 会议主席填报表 (改为 self-review checklist) |

---

## 五、执行 Checklist (总览)

```
Day 0 (2026-08-25):
  [x] AGENTS.md 加 Single-Developer Mode 章节
  [x] 创建 docs/architecture/sprint-24-pre-launch.md (本文件)
  [ ] 创建 .github/ISSUE_TEMPLATE/adr-review.md
  [ ] 创建 .github/ISSUE_TEMPLATE/sprint-kickoff.md
  [ ] 创建 docs/architecture/adr-self-review-checklist.md
  [ ] Step 1: 6 个 GitHub issue 创建

Day 0-1 (24h 窗口):
  [ ] Step 2: 6 个 issue self-review + 决策 comment (≤20 min/issue)
  [ ] 冷却期: 至少睡一觉再决定

Day 1 (2026-08-26):
  [ ] Step 3: 6+1 ADR 状态字段手工翻转
  [ ] Step 3: python3 scripts/apply-meeting-resolutions.py 实跑
  [ ] Step 3: capability-map v1.3 commit

Day 2 (2026-08-27):
  [ ] Step 4: Sprint 24 kickoff issue 创建
  [ ] commit: 全部 Sprint 24 准备完成

Day 3+ (Sprint 24 启动周, 2026-08-31):
  [ ] Step 5: T17 骨架 + ADR-0071 v1.1 + ADR-0080 v1.2 ship
  [ ] Phase 6 自进化方向基础设施完整
```

---

## 六、维护与演进

### 文档维护

- **下一修订触发**: (1) 6 个 issue 中任一被拒绝/延期; (2) capability-map v1.3 验收发现数据漂移; (3) Sprint 24 排期过载需重新调整; (4) Single-Developer Mode 流程本身有缺陷
- **定期审计**: 每 Sprint 收官同步 (单开发流程: 自审即可)
- **预防漂移**: `scripts/docs-drift-detect.sh` (B.2 计划) 自动校验文档与代码一致

### 流程改进

- 当前流程仍较"重型" (6 issue + 24h cooling-off), 单开发可考虑进一步简化
- 备选: 复杂 ADR (>200 行) 才启用 issue, 简单修订直接 commit
- 备选: 冷却期改为"睡一觉" (8h), 不强求完整 24h
- 备选: 引入 Oracle auto-review (post-write) 替代人工自审

---

**审批与维护**:
- 创建: 2026-08-25 (基于 Sprint 23 完成状态 + Single-Developer Mode 范式)
- 维护者: solo-dev
- 关联: AGENTS.md "Single-Developer Mode" 章节 + "Sprint 24 Pre-Launch" 摘要
- 下一修订: Sprint 24 收官 (Sprint 25 启动前)