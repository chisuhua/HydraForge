# Tasks — Cognitive-Cognitive 协调模式目录

> **关键不变量（Oracle 决策 1）**: **零代码改动** — 仅文档修改 + spec delta
> **估时**: 1-1.5 小时（含 doc 校对 + spec 编写 + 验证命令运行）
> **预期 commit 数**: 1 (单一 doc + spec change commit)
> **前置依赖**: 全部 ✅ ship（详见 design.md §跨 change 依赖）
> **设计依据**: 4 Oracle 综合评审（Path 1-4 sessions, 见 proposal.md 顶部）

## 1. Pre-flight Verification (Setup)

- [ ] 1.1 验证文档 v1.4 已 ship 且包含 §十八（5 模式 + 决策树分支）
  - 命令: `grep -n "## 十八" docs/architecture/agent-orchestration-architecture-2026-08.md`
  - 预期: 1 行匹配
- [ ] 1.2 验证 §九验证命令 #18-#22 已落地
  - 命令: `awk '/^## 九/,/^## 十/' docs/architecture/agent-orchestration-architecture-2026-08.md | grep -E "^# 1[8-9]\.|^# 2[0-2]\."`
  - 预期: 5 行匹配 (#18-#22)
- [ ] 1.3 验证 v1.4 ADR-0050 冻结 + ADR-0077 descoped + ADR-0086 引用标注全部存在
  - 命令: `grep -c "🔒 冻结\|Wave 4 descoped\|adr-0086-credit" docs/architecture/agent-orchestration-architecture-2026-08.md`
  - 预期: ≥4 行
- [ ] 1.4 验证基础环境（不动）
  - 命令: `git status --short | head` (预期仅本 change 新增文件)
  - 命令: `python3 tools/adr_lint.py | tail` (预期 baseline 警告数, 无新增)

## 2. Phase A — OpenSpec change artifact 创建 (本 change 主体)

- [ ] 2.1 创建目录: `mkdir -p openspec/changes/2026-08-30-meta-cognitive-coordination-doc/specs/cognitive-cognitive-coordination-patterns/`
- [ ] 2.2 写 `proposal.md`（已完成, 含 Oracle session 引用 + 4 路径决议 + 不变量 + 风险）
- [ ] 2.3 写 `design.md`（已完成, 含 5 决策 + 接口不变量 + 反例 + ADR 兼容性表）
- [ ] 2.4 写 `tasks.md`（即本文件）
- [ ] 2.5 写 `specs/cognitive-cognitive-coordination-patterns/spec.md` (Phase B)
- [ ] 2.6 验证 OpenSpec artifact 结构
  - 命令: `ls -R openspec/changes/2026-08-30-meta-cognitive-coordination-doc/`
  - 预期: 4 个文件（proposal.md, design.md, tasks.md, specs/.../spec.md）

## 3. Phase B — spec.md 编写

- [ ] 3.1 写 §ADDED Requirements: 5 模式命名（5 个 Requirement, 每个 ≥2 Scenario）
- [ ] 3.2 写 §ADDED Requirements: 决策树分支触发条件（1 Requirement, ≥1 Scenario）
- [ ] 3.3 写 §ADDED Requirements: 强制标注一致性（stream-pipeline 🔴 + debate-round 🟡）
- [ ] 3.4 写 §MODIFIED Requirements: 不新增 contract（cross-reference ADR-0019 §1.4 / ADR-0067）
- [ ] 3.5 写 §MODIFIED Requirements: per-worker 隔离（cross-reference ADR-0020 / ADR-0030 V2）
- [ ] 3.6 spec.md 验证: 5 Requirement × ≥2 Scenario 全部含 WHEN/THEN
- [ ] 3.7 spec.md 强制标注 grep 验证
  - 命令: `grep -c "🔴 V2 占位\|🟡 组合配方" spec.md`
  - 预期: ≥3 行

## 4. Phase C — 验证命令批量运行

- [ ] 4.1 §九验证命令 #18 5 模式名齐全
  - 命令: `for p in sync-delegate fan-out hierarchical-plan debate-round stream-pipeline; do grep -c "\*\*${p}\*\*" docs/architecture/agent-orchestration-architecture-2026-08.md; done`
  - 预期: 每个 = 2
- [ ] 4.2 §九验证命令 #19 stream-pipeline V2 占位标注
  - 命令: `grep "stream-pipeline.*V2 占位\|stream-pipeline.*🔴" docs/architecture/agent-orchestration-architecture-2026-08.md | head -3`
  - 预期: 至少 1 行
- [ ] 4.3 §九验证命令 #20 debate-round 组合配方标注
  - 命令: `grep "debate-round.*组合配方\|debate-round.*🟡" docs/architecture/agent-orchestration-architecture-2026-08.md | head -3`
  - 预期: 至少 1 行
- [ ] 4.4 §九验证命令 #21 §四 决策树引用 §十八
  - 命令: `grep "§十八\|cognitive-cognitive 协调模式" docs/architecture/agent-orchestration-architecture-2026-08.md | head -2`
  - 预期: ≥1 行命中 §四
- [ ] 4.5 §九验证命令 #22 §十七 关联文档引用 §十八
  - 命令: `grep "agent-orchestration.*§十八\|本目录 §十八" docs/architecture/agent-orchestration-architecture-2026-08.md`
  - 预期: ≥1 行

## 5. Phase D — Ship Gate 验证

- [ ] 5.1 文档头 4 字段完整（生成日期 / 最后验证 / 作者 / 状态）
  - 命令: `head -8 docs/architecture/agent-orchestration-architecture-2026-08.md | grep -E "生成日期|最后验证|作者|状态"`
  - 预期: 4 行
- [ ] 5.2 状态词汇合规（🔍 Proposed）
  - 命令: `grep -m1 "🔍 Proposed" docs/architecture/agent-orchestration-architecture-2026-08.md`
  - 预期: 1 行
- [ ] 5.3 Oracle 评审 4 决议在文档中显式表达
  - 命令: `grep -c "Oracle\|路径 [1234]\|Path [1234]" docs/architecture/agent-orchestration-architecture-2026-08.md`
  - 预期: ≥3 行
- [ ] 5.4 零代码改动验证
  - 命令: `git diff --stat HEAD~0..HEAD 2>/dev/null | grep -E "\.cpp|\.h|\.test\." | head`
  - 预期: 0 行（仅 .md / openspec/changes/ 变更）
- [ ] 5.5 ctest baseline 不变验证（185/185 PASS, 0 回归）
  - 命令: `ctest 2>&1 | tail -3`
  - 预期: 0 failures

## 6. Phase E — Commit + Archive

- [ ] 6.1 Git status 确认仅目标文件
  - 命令: `git status --short`
  - 预期: 仅 `M docs/architecture/agent-orchestration-architecture-2026-08.md` + `?? openspec/changes/2026-08-30-meta-cognitive-coordination-doc/` (新增目录)
- [ ] 6.2 Git add 文档修改
  - 命令: `git add docs/architecture/agent-orchestration-architecture-2026-08.md`
- [ ] 6.3 Git add OpenSpec change
  - 命令: `git add openspec/changes/2026-08-30-meta-cognitive-coordination-doc/`
- [ ] 6.4 Git commit (single commit, conventional format)
  ```
  git commit -m "docs(orchestration): §18 cognitive-cognitive coordination pattern catalog

  4-Oracle consensus on Conditional-Go (Path 1, doc-only):
  - §18 5-pattern catalog (sync-delegate/fan-out/hierarchical-plan/debate-round/stream-pipeline)
  - §4 decision tree new branch (cognitive-cognitive coordination)
  - §17 cross-reference update
  - §9 verification commands #18-#22

  3 mandatory annotations enforced:
  - stream-pipeline 🔴 V2 placeholder (iagent_composition.h:67 throws logic_error)
  - debate-round 🟡 composition recipe (3 shipped primitives combined)
  - stream-pipeline example marked pseudocode

  Out of scope (per Oracle Path 2/3/4 No-Go):
  - IAgent extension (Path 2 No-Go 2.3)
  - CognitiveOrchestrator new class (Path 3 No-Go 3.3, naming collision)
  - Full Meta-Cognitive platform (Path 4 No-Go 4.3, ADR-0050 frozen + ADR-0077 descoped)

  Trigger conditions for ADR-0085 V2 MetaAgent + Path 3.2 MCTS Axis6 documented in §18.10."
  ```
- [ ] 6.5 验证 commit hash 存在
  - 命令: `git log -1 --format='%H %s'`
  - 预期: 1 行 commit message

## 7. Phase F — 后续追踪（不在本 change 范围, 但建立指针）

- [ ] 7.1 Step 2: 新 OpenSpec change `adr-0086-credit-assignment-contract.md` 草稿
  - 触发: Oracle Path 4 建议, S4 promotion criteria 前置
  - 估时: 1 sprint
- [ ] 7.2 Step 3: ADR-0085 V2 MetaAgent 评审
  - 触发: §18.10 升级触发条件任一满足
  - 估时: 1-2 sprint（含 Oracle 评审 + 实施）
- [ ] 7.3 Step 4: 路径 3.2 MCTS Axis6 (认知 agent 数量 + 分配搜索维度)
  - 触发: Step 2 ship + §18.10 #3 满足
  - 估时: 0.5-1 sprint
- [ ] 7.4 Step 5: 路径 2.2 ICognitiveAgent 子接口
  - 触发: AgentWorker change (Sprint 24+, G3 open, cap-map §三 T3+T4)
  - 估时: 合并 AgentWorker change (2 sprint)
- [ ] 7.5 Step 6: 路径 3.3 CognitiveOrchestrator 重定位为编排层场景智能体
  - 触发: S4 promotion criteria 全部满足 (含 ADR-0086 ship + 防共谋/多样性/语义对齐指标定义)
  - 估时: 2-3 sprint

## 工时估算

| Phase | 估时 |
|-------|------|
| Phase A (artifact 创建) | 已完成 |
| Phase B (spec.md) | 30 min |
| Phase C (验证命令运行) | 10 min |
| Phase D (ship gate) | 10 min |
| Phase E (commit + archive) | 10 min |
| **总计** | **~1 小时** |

## 风险监控

| 风险 | 监控指标 | 触发升级 |
|------|----------|----------|
| §十八 5 模式命名与原语不匹配 | §九 #18 grep 计数 vs 文件行号 | 任一模式 grep 失败, 立即升级 |
| stream-pipeline 误读为可用 | §九 #19 grep "V2 占位" 命中数 | 0 命中, 立即修复 |
| debate-round 误读为原语 | §九 #20 grep "组合配方" 命中数 | 0 命中, 立即修复 |
| 文档示例代码腐化 | Sprint 收官 grep API 锚点文件:行号 | 行号偏移, §十八 anchor 表更新 |
| 用户需求实为路径 3 运行时决策 | 用户反馈 OR §18.10 4 条件回顾 | 任一条件满足, 启动 ADR-0085 V2 流程 |
