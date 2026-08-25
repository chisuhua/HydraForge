## Sprint 24 启动主题

**Self-evolution 方向基础设施完整** —— 锁定 6 个架构层缺口 (G10/G12/G13/G14/G15) 的 Approved 决议落地,启动 T17 SkillCompiler 骨架作为可衡量的 Sprint 24 产出。本周交付:**B1 评审会议 (Step 3b/3c 决议) + 6 ADR Approved + T17 骨架 ship**。

---

## Sprint 24 排期 (2026-09-01 → 2026-09-14, 2 周)

| 周次 | 主任务 | ship 目标 |
|---|---|---|
| **W1 (09-01 → 09-07)** | Step 3b/3c 决议落地 (24h cooling-off 后 per-ADR sed 翻转) + ADR-0071 v1.1 amendment 起草 + T17 骨架 (project skeleton + interface stub) | 6 ADR ✅ Approved + ADR-0071 v1.1 + T17 骨架 |
| **W2 (09-08 → 09-14)** | T17 核心编译逻辑 (SKILL.md → .agent.md 转换器) + ADR-0080 v1.2 ship + 单元测试 + cap-map v1.3 同步 | T17 骨架 ship + 184/184 ctest 0 regression |

---

## 6 个 ADR Approved ship 计划

> 来源: OpenSpec change `2026-08-25-sprint-24-pre-launch-self-review` Step 3b/3c 决议

| ADR | Gap | 启动 Sprint | 估时 | 并行轨道 |
|---|---|---|---|---|
| **ADR-0083** IEvaluator/RewardSignal | G10 | Sprint 24 启动周 | 1 sprint | E 轨 (主) |
| **ADR-0080 v1.2 amendment** D10 解耦 | G12 | Sprint 24 启动周 | 0.5 sprint | E 轨 (立即 ship) |
| **ADR-0071** LLM-native AgenticDSL | G13 | Sprint 24 (v1.1 amendment) | 0.5 sprint | E 轨 (顶层) |
| **ADR-0061-06 v1.1** Trajectory IR 独立序列化 | G14 | Sprint 25 启动周 | 1 sprint | E 轨 (T15) |
| **ADR-0061-13** 蒸馏输出格式 | G15 | Sprint 25 启动周 | 1 sprint (与 ADR-0083 并行) | E 轨 |
| **ADR-0074** Prompt Evidence Gate | (T21 间接) | Sprint 25 启动周 | 1 月 | E 轨 (T21) |

---

## T17 SkillCompiler 骨架 (Sprint 24 唯一主交付)

**来源**: capability-application-map §八.4 B6 蒸馏数据面 + ADR-0061-03 SkillCompiler (🔍 Proposed, 本次不 ship)

**W1 交付** (骨架 + interface stub):
- `include/agenticdsl/skill/compiler.h` 接口定义 (SKILL.md → AgentIR 转换器签名)
- `src/modules/skill_compiler/skill_compiler.{h,cpp}` 空实现 + TODO 标注
- `tests/test_skill_compiler_skeleton.cpp` ≥3 cases (构造/接口契约/错误传播)
- `docs/architecture/skill-compiler-skeleton-design.md` 接口规范 (本 Sprint ship)
- 依赖 ADR-0071 ✅ Approved (G13 解锁)

**W2 交付** (核心编译逻辑):
- SKILL.md YAML/Front-matter 解析
- 节点类型映射 (skill → dsl node)
- stdlib sub-graph 引用展开
- `tests/test_skill_compiler_core.cpp` ≥10 cases + ≥30 assertions
- 184/184 ctest 0 regression

**Out of Scope (明确不做)**:
- 不实施 SKILL.md → C++/Wasm 编译 (Phase 7 以后)
- 不实施 AgentIR → C++ 代码生成 (Phase 5 已 descoped)
- 不实施 Trajectory IR 集成 (T15 Sprint 25 启动)
- 不实施 GEPA/AFlow spike (T19/T20 Sprint 24 末 + Sprint 26)

---

## Sprint 25-26 排期 (continuation)

| Sprint | 周次 | 主任务 | ship 目标 |
|---|---|---|---|
| **Sprint 25** | 09-15 → 09-28 | T17 核心编译逻辑收尾 + ADR-0083 实施 + ADR-0061-13 蒸馏输出 ship + T15 Trajectory IR 启动 + ADR-0074 实施 | T17 完整 ship + T15 骨架 + 3 个新 ADR ship |
| **Sprint 26** | 09-29 → 10-12 | T15 Trajectory IR 完整 + T21 Prompt Evidence Gate + T19 GEPA R 轨 spike 启动 + T20 AFlow spike 准备 | 自进化方向基础设施完整 (G10-G15 全闭合) |

---

## 风险与阻塞

| 风险 | 影响 | 缓解 |
|---|---|---|
| **Step 2 self-review 决策被拒/延期** | T17 启动依赖 ADR-0071 ✅; 任一 ADR 被拒将阻塞对应 TD 项 | 失败路径: 24h cooling 后逐 issue 决策, 任一 ❌ 按 proposal §3.5 走 partial.yaml |
| **24h cooling-off 延长** | Sprint 24 W1 进度 | 可缩短至 8h (睡一觉即可), issue body 注明 |
| **T17 骨架估时超 1 sprint** | 排期过载 (ADR-0050 重开条件) | T17 W2 仅核心编译逻辑, 节点映射推迟 Sprint 25 |
| **ADR-0071 v1.1 amendment 估时超 0.5 sprint** | 排期过载 | amendment 仅整合 6 子项状态, 不增加新决策 |
| **T15 Trajectory IR 启动依赖 G14 ✅** | Sprint 25 T15 进度 | G14 决议与 Sprint 24 W1 同步落地, 无独立依赖 |

---

## 验收标准 (Sprint 24 ship gate)

- [ ] **A1** 6 个 ADR ✅ Approved (在对应 issue 中标记, capability-map §二状态 🔴 → ✅)
- [ ] **A2** capability-map v1.3 生成 (`apply-meeting-resolutions.py --all-approved` 实跑, 14/14 匹配)
- [ ] **A3** ADR-0071 v1.1 amendment ship (子项状态整合 + Oracle §八)
- [ ] **A4** ADR-0080 v1.2 amendment ship (CaptureMode 三态 + 三重保护)
- [ ] **A5** T17 SkillCompiler 骨架 ship (W2 完整编译逻辑, 184/184 ctest)
- [ ] **A6** 3 个状态镜像同步 (gap-analysis + README + relationships)
- [ ] **A7** ≥10 atomic commits (6 ADR + cap-map + 3 镜像 + 杂项)
- [ ] **A8** `openspec validate 2026-08-25-sprint-24-pre-launch-self-review --strict` → EXIT 0
- [ ] **A9** `tools/adr_lint.py` → 0 errors
- [ ] **A10** `tools/docs_drift_audit.py` → 0 DRIFT (Scenario 7)

---

## 关联文档

- **前置**: `openspec/changes/2026-08-25-sprint-24-pre-launch-self-review/` (本 OpenSpec change)
- **决议草案**: `docs/architecture/adr-review-minutes/resolution-draft-2026-08-25.md`
- **能力地图**: `docs/architecture/capability-application-map-2026-08.md` §八 (排期表)
- **自审 checklist**: `docs/architecture/adr-self-review-checklist.md`
- **6 自审 issue**: `gh issue list --label adr-review` (6 条, Sprint 23 retro approval)
- **T17 设计**: ADR-0061-03 SkillCompiler (`docs/adr/skill/adr-0061-03-skill-compiler.md`)

---

## 签发

- **作者**: solo-dev (Single-Developer Mode, 自审 = 自决)
- **启动日期**: 2026-09-01 (Sprint 24 启动周)
- **Sprint 截止**: 2026-09-14 (2 周)
- **Sprint 25 启动**: 2026-09-15
- **关联 capability-map**: `docs/architecture/capability-application-map-2026-08.md` §八.4 B6 蒸馏数据面
