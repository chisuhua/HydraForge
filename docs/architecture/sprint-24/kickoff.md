# Sprint 24 Kickoff: Distillation Data Plane + 横切化收官

> **创建**: solo-dev @ 2026-08-29
> **关联 Milestone**: Sprint 24
> **下一修订**: Sprint 24 收官 (Sprint 25 启动前)
> **Self-Review**: 单人开发模式（作者 = 评审人 = 实施者，Oracle 提供 Virtual Architect 第二意见）

## Sprint 目标 (2026-08-29 → 2026-09-12)

主目标: **完成 P0 蒸馏数据面 ship + 启动 5 处 patch 后续任务 (T26/T15 bus pattern 横切化)**

**关键路径**:
1. **P0 路线图级阻塞** (本次 Sprint 启动周必须 ship): `capture-mode-and-distillation-writer-v1` change 完整 ship
2. **T26 横切架构闭环**: ADR-0085 V1 已 ship，本次启动 5 处后续任务 (T26.a/b/c/d/e)
3. **24h Cooling-Off**: 本 kickoff 创建后等待 24h 才执行实施（single-dev 模式强制约束）

## 启动周任务 (Week 1)

### P0 路线图级 (本周必须 ship)

- [ ] **capture-mode-and-distillation-writer-v1** OpenSpec change 完整 ship (1.5 sprint)
      前置: ✅ ADR-0080 v1.2 Approved (零代码) + ✅ ADR-0061-13 Approved (幻影 change) + ✅ ADR-0083 RewardSignal ship + ✅ T15 TrajectoryIR ship
      OpenSpec change: `openspec/changes/capture-mode-and-distillation-writer-v1/`
      估时: 1.5 sprint
      关键 ship gate: ADR-TRACKING-01 WARNING 自动消失（ADR-0080 v1.2 + ADR-0061-13 头部加 `⏳ tracking: in-progress` 注记）

### T26 后续任务 (Sprint 24-26 排期)

- [ ] **T26.c** T15 BusPattern 消费 OTel 导出 (0.3 sprint)
      前置: ✅ capture-mode-and-distillation-writer-v1 ship + ✅ T15 to_otel_spans 已 ship
      OpenSpec change: `openspec/changes/t26-c-bus-pattern-otel/`
      估时: 0.3 sprint
      ROI: 高（可观测性最大缺口）

- [ ] **T26.a** T19 GEPA 反射路径注入 L0 装饰链 (0.5 sprint)
      前置: ✅ capture-mode-and-distillation-writer-v1 ship + ✅ T19 Phase 2 ship + ✅ ADR-0084 Mutation ship
      OpenSpec change: `openspec/changes/t26-a-gepa-decorator-chain/`
      估时: 0.5 sprint
      ROI: 高（核心成本控制）
      **关键决策点**: 链深 < 4 硬约束（GEPA 4 decorators 临界，需采用方案 B: Retry 移至 L3 AgentHookRegistry）

- [ ] **T26.b** T20 MCTS mutation 参数配置化 (0.5 sprint)
      前置: ✅ capture-mode-and-distillation-writer-v1 ship + ✅ T20 V1 ship
      OpenSpec change: `openspec/changes/t26-b-mcts-mutation-config/`
      估时: 0.5 sprint
      ROI: 高（消除硬编码 Yolo）
      **关键决策点**: YoloPolicy 行为迁移（方案 B: 升级支持可选参数，最小破坏）

### 治理改进 (本周必须 ship)

- [x] ✅ **5 处 ADR 评审勘误** (Oracle 二次审查 post-ship)
      Commits: `9efd139` (TrajectoryIR schema_version) + `871cb4a` (4 docs patches) + `f811ce7` (adr_lint ADR-TRACKING-01)
      状态: ✅ 已 ship (Day 1)

- [x] ✅ **ADR-TRACKING-01 规则** (Oracle 决策 5)
      状态: ✅ 已 ship (commit `f811ce7`)
      验证: 35 个 WARNING 包含 ADR-0080 v1.2 + ADR-0061-13，需 capture-mode-and-distillation-writer-v1 ship 后自动消失

## 排期表

| Sprint 周次 | 启动任务 | ship 目标 |
|---|---|---|
| **Sprint 24 W1 (启动周)** | capture-mode-and-distillation-writer-v1 Phase 0 + Phase 1 | BREAKING 字段迁移 + FileDistillationWriter V1 + ≥8 cases PASS |
| **Sprint 24 W2** | capture-mode-and-distillation-writer-v1 Phase 2 + Phase 3 | CLI flag + bridge + ship + archive + ADR-TRACKING-01 WARNING 消失 |
| **Sprint 24 W3** | T26.c T15 BusPattern OTel (后续) | OTel exporter via BusPattern |
| **Sprint 25 W1** | T26.a T19 GEPA L0 装饰链 + T26.b T20 MCTS 配置化 | GEPA 装饰链注入 + YoloPolicy 参数化 |

## 风险与备选

### 风险 1: BREAKING 字段迁移遗漏消费者

**描述**: `bool capture_prompt_bytes → CaptureMode capture_mode` 迁移 5 个消费者（engine.h/cpp, tracing_decorator.h/cpp, event_log_config.h），任一遗漏导致全量 ctest 失败
**影响**: 中（仅内部 API，但既有测试可能引用 bool 字段）
**备选**:
- 方案 A (本 Sprint 采用): 不保留 bool 兼容层，迁移后 `grep "capture_prompt_bytes" = 0 命中` 强验证
- 方案 B: 保留 bool 作为 deprecated 字段（破坏迁移彻底性，但降低风险）

### 风险 2: Training 模式 PII 泄漏

**描述**: Training 模式可能意外捕获敏感 prompt 内容（如 API key）
**影响**: 高（合规违规）
**备选**:
- 方案 A (本 Sprint 采用): 三重保护 (agent_id + 路径 + WARNING) + payload redact 复用 T21 hash_prompt()
- 方案 B: 默认 Training 模式全 disabled，需显式 env var 启用

### 风险 3: Mock-mode 训练集污染

**描述**: Mock 生成的低质数据进入 DistillationWriter → 污染 fine-tune 训练集
**影响**: 高（模型质量下降）
**备选**:
- 方案 A (本 Sprint 采用): `--allow-training-capture` + mock-mode hard rejection（参考 model_switching）
- 方案 B: 默认开启 training capture（风险高，不推荐）

### 风险 4: ADR-TRACKING-01 WARNING 不消失

**描述**: capture-mode-and-distillation-writer-v1 change 目录名需含 "0080" 和 "0061-13" 子串才能匹配 ADR-TRACKING-01 规则
**影响**: 低（仅警告级别）
**备选**:
- 方案 A (本 Sprint 采用): change 目录名含 "capture-mode-and-distillation-writer-v1"，用 doc 链接而非目录名匹配（如需则扩展规则）
- 方案 B: 调整规则匹配逻辑（支持 description-based matching）

## 自审清单 (Sprint 启动前)

- [x] ✅ 前置 Sprint 23 决议 issue 全部冷却期已结束
- [x] ✅ capability-map v2.5 已更新 (引用 §八 + §一 +1 新能力 #31)
- [x] ✅ 4 个 ADR 状态字段已翻转 (✅ Approved)：ADR-0080 v1.2 / ADR-0071 / ADR-0074 / ADR-0083
- [x] ✅ capture-mode-and-distillation-writer-v1 OpenSpec change 已创建 (5 文件)
- [x] ✅ 排期表与能力地图 §八 一致
- [x] ✅ 风险评估与备选方案记录完整
- [x] ✅ ADR-TRACKING-01 规则已 ship + 实战验证（35 WARNING 包含 2 个目标 ADR）

## 关联文档

- **Capability Map**: `docs/architecture/capability-application-map-2026-08.md` (§八 T26 跟踪段 + §一 #31 新能力)
- **Sprint Pre-Launch Plan**: `openspec/changes/2026-08-25-sprint-24-pre-launch-self-review/` (Step 1-5 治理流程)
- **相关 ADRs**:
  - `docs/adr/adr-0080-v1-2-amendment-d10-decouple.md` (CaptureMode 父 ADR)
  - `docs/adr/skill/adr-0061-13-distillation-output-format.md` (IDistillationWriter 派生)
  - `docs/adr/adr-0083-evaluator-reward-contract.md` (RewardSignal 复用)
  - `docs/adr/adr-0068-event-emission-contract.md` (事件契约)
  - `docs/adr/adr-0085-cross-cutting-pattern-pdk.md` (T26 横切架构)
- **Oracle Sessions**:
  - `ses_fb4e00320ffeqQVZ2S61tF3dZi` (二次审查 6 ADR)
  - `ses_fb4cd8ff8ffeJlYBgU3JogcnfB` (决策 1-5 实施级方案)
- **Audit Reports**:
  - `docs/architecture/adr-self-review-checklist.md` (12 项通用 + 4 类专用清单)
  - `docs/architecture/adr-implementation-status-gap-analysis.md` (ADR 状态唯一事实源)

## 签发

- **创建**: solo-dev @ 2026-08-29
- **关联 Milestone**: Sprint 24
- **下一修订**: Sprint 24 收官 (Sprint 25 启动前 2026-09-12)
- **冷却期**: 创建后 24h 后开始实施 (single-dev 模式硬约束)
- **实施启动**: 2026-08-30 18:00 后
