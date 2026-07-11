# Phase 5 剩余工作 — ADR 状态同步 + Sprint 22 Gates Master Plan

> **目的**: 追踪 Phase 5 收官前的 ADR drift 修复 + Sprint 22 Review Gates 执行
> **创建日期**: 2026-07-10
> **触发条件**: Strategic Alignment Gate §9.4 + Drift Gate (Sprint 22 累计 3 changes)
> **基础**: 上游 `docs/superpowers/plans/2026-07-03-phase5-self-bootstrapping.md` (C9-C16 全部 ship + archived)
> **关联 docs**: `docs/active-status.md` (活跃看板) / `docs/adversarial-reviews/decisions-2026-07-07.md` (B2 决策)
> **关联工具**: `tools/adr_lint.py` + `tools/docs_drift_audit.py` + `tools/adr_relationships.py`
> **覆盖**: Sprint 22 (2026-07-10 ~ 2026-07-31)
> **责任人**: Sisyphus (创建) → 用户 (后续维护)

---

## 一、当前项目基线 (2026-07-10)

| 维度 | 状态 | 证据 |
|------|------|------|
| OpenSpec active change 数 | **0** (Phase 5 全部 8 个 change C9-C16 已 ship + archived) | `openspec list` = "No active changes found" |
| Test count | **72 ctest, 19 Approved/72 ctest, 19 Approved PASS** | baseline 25 + Sprint 1-21 累计 47 新增 |
| ASan | **72 ctest, 19 Approved/72 ctest, 19 Approved (100%)** | Sprint 21 后 `test_execute_parallel` use-after-scope 修复 |
| TSan | 跳过 (机器性能受限) | pre-existing race 已修复, Sprint 10 验证 |
| Phase 5 实施进度 | 🟡 ~70% (C9-C16 全 ship, C16 §5 Cloud plugin 顺延, C19/C20 远期) | master plan §一 |
| **ADR 状态 drift** | 🔴 **12 个 ADR 🔍 Proposed 状态与实际 ship 不符** | `grep "🔍 Proposed" docs/adr/*.md` 返回 12 行 |

**核心矛盾**: C2 (ADR-0030 V2) + C14 (ADR-0035/0040/0041/0043/0044) + C16 (ADR-0042/0045/0046) 实施均已 ship, 但 ADR 主文档的 `## 状态` 行仍标注 🔍 Proposed, 与 `docs/README.md` 表格一致 — 反映 OpenSpec change 与 ADR 文档状态的脱钩问题。

---

## 二、Change 依赖关系图

```
                  [Sprint 22 周 1]          [Sprint 22 周 2]
                      │                          │
                      ▼                          ▼
                 [C17 ADR sync] ──soft──→ [C18 Drift + Strategic Gate]
                 (12 ADR 状态同步)         (Drift audit + Phase 6 评估)
                 估时 1-2 天              估时 2-3 天
                      │                          │
                      │                          │
                      ▼                          ▼
              [后续 optional:                [后续 optional:
               Phase 6 启动]                  C16 §5 Cloud plugin v3]
```

**关键依赖事实**:
- C17 独立 (0 依赖, 仅需 12 个 ADR 主文档编辑权限)
- C18 依赖 C17 (软依赖, Drift 检查需要 ADR 状态已对齐)
- C18 产出 Phase 6 启动评估, 不强制后续 change

---

## 三、Change 总览 (2 个)

| # | Change 名 | 类型 | 估时 | 依赖 | 状态 |
|---|-----------|------|------|------|------|
| **C17** | `2026-07-10-phase5-adr-states-final-sync` | 文档同步 | 1-2 天 | 无 | ⚪ **immediate** |
| **C18** | `2026-07-10-phase5-sprint22-drift-strategic-gate` | Gate 审计 + 战略评估 | 2-3 天 | C17 ✅ (soft) | ⚪ **immediate** |

---

## 四、Change 详细追踪

### C17: `2026-07-10-phase5-adr-states-final-sync` (Sprint 22 周 1)

| 字段 | 值 |
|------|---|
| **类型** | 文档同步 (ADR 状态校准, Metis 审查后范围修正 12 → 5) |
| **估时** | 0.5-1 天 (范围缩减后) |
| **依赖** | 无 (Phase 5 全部 change 已 ship, ADR 状态需对齐) |
| **关联 ADR** | **5 个待翻转**: ADR-0035 / 0040 / 0041 / 0043 / 0044 + **7 个待排除**: ADR-0030 V2 / 0037 / 0038 / 0039 / 0042 / 0045 / 0046 |
| **关联工具** | `tools/adr_lint.py` + `tools/docs_drift_audit.py` + `docs/adr-management/STATUS-GLOSSARY.md` |
| **关联 Metis session** | `ses_0b02706b7ffepKdYy3qxnmOzXy` (2026-07-10, FAIL 裁决后用户选项 A 采纳) |
| **目录** | `openspec/changes/2026-07-10-phase5-adr-states-final-sync/` |
| **状态** | ⚪ **immediate** (Metis 审查后修正范围) |

**目标** (Metis 审查后修正):

**Part A — 5 个 ADR 翻转** (`🔍 Proposed` → `✅ Approved`):
1. ADR-0035 (Inference Engine Plugin Spec) → C14 ship
2. ADR-0040 (Inference Plugin Build Strategy) → C14 ship
3. ADR-0041 (PluginLoader Lifecycle Extension) → C14 + C16 ship
4. ADR-0043 (PDK Tool Naming Convention) → C13/C14 D3 已应用
5. ADR-0044 (Inference Plugin Security Model) → C14 ship

**Part B — 7 个 ADR 排除原因文档化** (保持 `🔍 Proposed`):
1. ADR-0030 V2 — P1-P4 退出条件未满足 (P2 已 Oracle 延迟)
2. ADR-0037 — 纯规范 (因果排序机制零实施)
3. ADR-0038 — BatchingQueue 增量决议延迟
4. ADR-0039 — JSON 查询工具未实现 (`available_models()` C++ ≠ ADR 规范)
5. ADR-0042 — 主文档第 10 行硬性 banner
6. ADR-0045 — 实施顺序 5 步仅 step 2 部分交付
7. ADR-0046 — 4 通道架构仅通道 ① 完成

**Part C — 文档同步**:
1. 5 个 ADR 主文档 `## 状态` 行更新 + ship 证据段追加
2. `docs/README.md` §adr/ 表格**新增 5 行** (ADR-0035/0040/0041/0043/0044)
3. `docs/active-status.md` §一 ADR Approved 计数 14 → 19 (+5)
4. AGENTS.md § Recent Changes 追加 C17 记录
5. 重跑 `tools/adr_relationships.py` 生成 relationships.md

**不修改**: master plan `2026-07-03-...` §一 Phase 2 行 (因 ADR-0030 V2 保持 Proposed)

**Ship gate**:
- `python3 tools/adr_lint.py` exit 0
- `python3 tools/docs_drift_audit.py` 0 DRIFT items
- 5 个翻转 ADR 主文档 `## 状态` 行含 `<change 名> ship` 引用
- `docs/README.md` §adr/ Approved 行数 +5
- `openspec validate 2026-07-10-phase5-adr-states-final-sync` exit 0

### C18: `2026-07-10-phase5-sprint22-drift-strategic-gate` (Sprint 22 周 2)

| 字段 | 值 |
|------|---|
| **类型** | Review Gate (Drift + Strategic Alignment) |
| **估时** | 2-3 天 |
| **依赖** | C17 ✅ (soft — Drift 检查基于 C17 后 ADR 状态) |
| **关联 ADR** | 全部 Phase 5 关联 ADR (C17 已校准后 19 个 Approved: 14 existing + 5 C17 FLIP) |
| **关联工具** | `tools/check_roadmap_drift.py` + Oracle 战略咨询 |
| **目录** | `openspec/changes/2026-07-10-phase5-sprint22-drift-strategic-gate/` |
| **状态** | ⚪ **immediate** |

**目标**:
- **Drift Gate**: 重跑 3 类 drift 检测 (代码 ↔ ADR ↔ docs/active-status.md ↔ master plan), 输出 0 CRITICAL drift
- **Strategic Alignment Gate**: 评估 Phase 5 → Phase 6 启动条件, 产出 1 个评估 ADR (`adr-0050-phase6-strategic-evaluation.md`)
- **Stage Gate**: 评估 Stage 1 → Stage 2 切换条件 (C10+C11+C12 全链 ship + 2 周稳定 + 团队时间投入)

**关键 ship 列表**:
1. **Drift audit**:
   - 重跑 `tools/check_roadmap_drift.py` 输出 0 CRITICAL
   - 比对 `docs/README.md` ADR 状态 vs `docs/adr-management/relationships.md` vs 实际 `adr_lint.py` 输出
   - 比对 `docs/active-status.md` §一 状态计数 vs `master plan §一` 基线表 vs 实际 code grep
2. **Strategic evaluation**:
   - 创建 `docs/adr/adr-0050-phase6-strategic-evaluation.md` (🔍 Proposed 状态)
   - 内容: Phase 5 收官评估 + Phase 6 候选方向 (自进化 / 服务化 / 第三方生态 / Cloud-native) + 启动条件
   - Oracle 咨询 Phase 6 战略路径 (1 session)
3. **Stage Gate evaluation**:
   - `docs/handoff/2026-07-31-stage-gate-evaluation.md` (Stage 1 → 2 决策)
   - 内容: C10+C11+C12 稳定运行报告 (72 ctest, 19 Approved/72 ctest, 19 Approved ctest + ASan + 2 周无 P0 bug) + C19 触发条件评估 + 团队时间投入可用性
4. **Drift Log 同步**:
   - `master plan §十` 追加 C17 + C18 行
   - `master plan §十一` 记录 C18 评估对 C19/C20 placeholder 的影响
5. **Strategic Pivots 同步**:
   - `master plan §十二` 追加 Phase 5 收官 pivot (如有)
6. AGENTS.md § Recent Changes + `docs/active-status.md` §六 下一步行动更新

**Ship gate**:
- `tools/check_roadmap_drift.py` exit 0 (0 CRITICAL, 0 WARNING)
- `docs/adr/adr-0050-phase6-strategic-evaluation.md` 创建, `python3 tools/adr_lint.py` exit 0
- `docs/handoff/2026-07-31-stage-gate-evaluation.md` 创建, Stage Gate 决议明确
- `master plan §十/§十一/§十二` 已同步
- `openspec validate 2026-07-10-phase5-sprint22-drift-strategic-gate` exit 0

---

## 五、Sprint 22 任务切分

> **目标**: Phase 5 收官前消除 ADR drift, 完成 Sprint 22 Review Gates, 准备 Phase 6 启动评估
> **里程碑**: 12 个 ADR 状态全部对齐 + Drift 0 + Phase 6 战略评估 ADR ship
> **总工期**: 3-5 天 (Sprint 22 周 1-2)

### 5.1 C17 任务: ADR 状态同步 (0.5-1 天, Metis 审查后修正)

**Part A — 5 个 ADR 状态翻转** (Day 1 上午):

1. 列出 5 个待翻转 ADR (Metis 审查确认):
   - ADR-0035 / 0040 / 0041 / 0043 / 0044 (C14+C16 已 ship 的相关 ADR)
2. 对每个 ADR:
   - 找到关联已 ship 的 OpenSpec change (C14/C16, 见 ADR 文档内的 OpenSpec change 引用)
   - 更新 `## 状态` 行: `🔍 Proposed (2026-07-06 — 架构方案讨论产出, 待 review)` → `✅ Approved (2026-07-10 — C14/C16 ship)`
   - 追加 1 行 ship 证据:
     ```
     > **实施依据**: `<change 名>` 已 ship + archived (per `openspec/changes/archive/<change 名>/`), 验证: ctest + ASan 通过 (具体 ship 证据详见各 ADR §状态 行 + 上游 master plan 2026-07-03-phase5-self-bootstrapping.md §三 表).
     ```
3. 提交 1 commit per ADR (便于 git log 追溯)

**Part B — 7 个 ADR 排除原因文档化** (Day 1 上午):

4. 将排除原因列表追加到本 master plan §十一 Adjustment Log
5. 追加排除列表到 `docs/active-status.md` §一 或 §六 (顺延项)
6. **不修改** 7 个排除 ADR 的 `## 状态` 行 (保持 `🔍 Proposed` + 现有 banner)

**Part C — 文档同步** (Day 1 下午):

7. 更新 `docs/README.md` §adr/ 表格: **新增 5 行** (覆盖 0035-0046 范围, 之前完全未列出)
8. 更新 `docs/active-status.md` §一: ADR Approved 计数 `14 → 19` (+5)
9. **不修改** master plan `2026-07-03-...` §一 Phase 2 行 (ADR-0030 V2 保持 Proposed)
10. 重跑 `python3 tools/adr_relationships.py` 生成 relationships.md
11. AGENTS.md § Recent Changes 追加 C17 行

**Ship gate**:
- `python3 tools/adr_lint.py` exit 0
- `python3 tools/docs_drift_audit.py` 0 DRIFT items
- `git grep "🔍 Proposed" docs/adr/` 精确值: 排除的 7 个 + ADR-0030 V2 + ADR-0031 Partial(不算 Proposed) → 共 8 个仍含 `🔍 Proposed` (预期)
- `docs/README.md` Approved 行 +5 (总计 19)
- 5 个翻转 ADR 主文档的 `## 状态` 行均含 ship 证据
- `openspec validate 2026-07-10-phase5-adr-states-final-sync` exit 0

### 5.2 C18 任务: Drift + Strategic Gate (2-3 天)

**实施步骤**:
1. **Day 1**: Drift audit
   - 跑 `tools/check_roadmap_drift.py` 获取当前 drift 列表
   - 4 路并行检查: ADR 状态 ↔ 代码 ↔ active-status ↔ master plan
   - 输出 `docs/audits/2026-07-10-drift-gate.md`
2. **Day 2**: Oracle 战略咨询 + adr-0050 写
   - Oracle session 评估 Phase 6 候选方向
   - 写 `docs/adr/adr-0050-phase6-strategic-evaluation.md` (含 §决策 + §候选方向 + §启动条件)
3. **Day 3**: Stage Gate evaluation + 同步
   - 写 `docs/handoff/2026-07-31-stage-gate-evaluation.md`
   - 同步 `master plan §十/§十一/§十二`
   - AGENTS.md + `docs/active-status.md` 更新
   - ship gate 验证

**Ship gate**: 同 C18 字段表

---

## 六、风险

| 风险 | 来源 | Mitigation | 关联 Change |
|------|------|-----------|------------|
| ~~12 个 ADR 状态变更引入 commit 历史混乱~~ → **已规避** (Metis 审查后缩减至 5 个) | C17 | ~~每个 ADR 独立 commit + 引用 ship commit hash~~ → **5 个 ADR 独立 commit** (commit 数量减半, 历史追溯清晰度提高) | C17 |
| Strategic evaluation 缺乏业务输入 | C18 | Oracle session 前先列 Phase 6 候选方向 (内部假设), 用 Oracle 验证而非代替决策 | C18 |
| Stage Gate 评估窗口过短 (C12 ship 2026-07-04 → 评估 2026-07-10, 仅 6 天 vs 2 周要求) | C18 | 文档化 "稳定运行 6 天" 而非 2 周, 标注为 "提前评估 + 持续监控", Stage 2 启动决策延后至 2026-07-18 (2 周整) | C18 |
| C19/C20 触发条件不明导致 placeholder 无法填实 | C18 | C18 评估时明确 C19 触发检查表 + C20 Oracle 验证清单, 写入 §十一 Adjustment Log | C18 |
| **新风险**: 7 个排除 ADR 在后续 Sprint 中被误判为"待翻转"导致重复劳动 | C17 scope-fix | §十一 Adjustment Log 追加排除列表 + 排除原因, AGENTS.md §Recent Changes 注明 C17 Metis 审查后 scope-fix | C17 |
| **新风险**: ADR-0045/0046 实施率 20-25% 但用户/团队可能误认为已 Approved | C17 scope-fix | §十一 明确标注实施率 + 后续行动指向 Phase 6, 在 C18 Strategic Gate 评估时纳入 Phase 6 候选方向 | C17 + C18 |

---

## 七、维护规则

1. **状态变更**: C17/C18 进入 archive 时, 本文件相应行的 "状态" 列更新为 `✅ archived (YYYY-MM-DD)`
2. **新增 change**: 如发现本 plan 未列出的剩余工作, 在本文件末尾追加新行, 并标注依赖关系
3. **依赖变更**: 如发现 change 间新依赖, 立即更新 §二 依赖图
4. **同步检查**: C17 ship 后检查本文件与 `docs/active-status.md` 的一致性
5. **本文件状态**: 自身 archived 取决于 C17/C18 都 ship 后

---

## 八、参考链接

- 上游 master plan: [`2026-07-03-phase5-self-bootstrapping.md`](2026-07-03-phase5-self-bootstrapping.md) (C9-C16 ship + C19/C20 远期 placeholder)
- 上游 plan: [`2026-06-26-sprint-11-to-18-roadmap.md`](2026-06-26-sprint-11-to-18-roadmap.md) (Sprint 11-18 历史)
- B2 决策: [`docs/adversarial-reviews/decisions-2026-07-07.md`](../adversarial-reviews/decisions-2026-07-07.md) (D1-D5 应用)
- 当前活跃看板: [`docs/active-status.md`](../active-status.md)
- OpenSpec CLI: `openspec list` / `openspec show <change>` / `openspec validate <change>`
- AGENTS.md: 项目根入口文档

---

## 九、Review Gates (沿用 master plan §九 5 种类型)

| Gate | 类型 | 触发 | 检查 | 输出 |
|------|------|------|------|------|
| **🔄 Sprint Review** | Sprint 22 收官 | C17 + C18 ship 后立即 | ship 效果 + bug 引入 + 估时偏差 | 追加 §十 Drift Log |
| **🧭 Architecture Drift** | Sprint 22 (本 plan 主体) | 累计 3 changes | ADR 状态与代码一致性 | C18 主体 |
| **🔗 Dependency Refresh** | C18 ship 前 | 占位 change 触发条件 | C19/C20 启动条件评估 | C18 §Stage Gate |
| **🎯 Strategic Alignment** | Phase 5 收官 | 全部 ship | Phase 6 启动条件 | C18 adr-0050 |
| **🆕 Stage Gate** | Stage 1 → 2 切换 | C12 ship + 2 周稳定 | 启动条件满足 | C18 handoff |

---

## 十、Architecture Drift Log (待填充)

| 日期 | Change | 偏离类型 | 偏离描述 | 响应 Change | 状态 |
|------|--------|---------|---------|-----------|------|
| 2026-07-10 | C17 | retro | 12 个 ADR 状态与 C2/C14/C16 实际 ship 不符, 累计 drift | C17 | 🟡 active |
| 2026-07-10 | C18 | drift + strategic | Drift Gate + Strategic Gate 触发 | C18 | 🟡 active |
| 2026-07-10 | C17 | **scope-fix (Metis 审查)** | C17 原计划翻转 12 个 ADR, Metis 预规划审查 (session `ses_0b02706b7ffepKdYy3qxnmOzXy`) 发现: 7 个存在硬性阻碍 (ADR-0030 V2 P1-P4 未满足 / ADR-0037 纯规范 / ADR-0038 BatchingQueue 延迟 / ADR-0039 JSON 工具未实现 / ADR-0042 硬性 banner / ADR-0045 5步仅step2 / ADR-0046 4通道仅①). **范围修正 12 → 5** | C17 proposal.md + tasks.md + spec.md | ✅ resolved (user 选项 A 决策) |

---

## 十一、Change Adjustment Log

| 日期 | 占位 Change | 调整原因 | 调整内容 | 状态 |
|------|------------|---------|---------|------|
| 2026-07-10 | C19 (原 fork-checkpoint) | C18 Stage Gate 评估明确触发条件 | 占位不变, 触发检查表追加至 C18 handoff | 🟡 active |
| 2026-07-10 | C20 (原 analysis-service) | C18 Strategic Gate 评估 Phase 6 方向 | 占位不变, Oracle 验证清单追加至 adr-0050 | 🟡 active |
| 2026-07-10 | C16 §5 Cloud plugin (`phase5-illmprovider-call-chain-v3`) | C17 ADR 校准后 C16 §5 实施依赖部分解除 (ADR-0035 ✅) 但 ADR-0042/0045 仍 🔍 Proposed | 触发条件细化为 "ADR-0035 ✅ Approved (C17 ship 后满足) + CloudLLMProvider 外部需求" | 🟡 active |
| 2026-07-10 | **C17 排除 ADR 列表** (Metis 审查要求) | 7 个 ADR 排除翻转需文档化理由, 防止后续 Sprint 误判 | 见下表 | 🟡 active |

### 11.3 C17 排除 ADR 列表 (Metis 审查要求文档化)

| 排除 ADR | 排除原因 | 后续行动 |
|---------|---------|---------|
| **ADR-0030 V2** | P1-P4 退出条件未满足 (P2 FleetOrchestrator 已被 Oracle 延迟 per 上游 master plan `docs/superpowers/plans/2026-07-03-phase5-self-bootstrapping.md` §十六 + AGENTS.md "Fleet 模式 16 路 LLM → DEFER") | Fleet 解除延迟后由 C19 处理, 或 ship 时迁移至 `🟡 Partial` |
| **ADR-0037** | 纯规范: 因果排序机制零实施 (`grep -rn "causal_order\|EventReorderBuffer\|sequence_number" src/ include/` → **0 matches**) | Phase 6 实施 |
| **ADR-0038** | BatchingQueue 增量决议延迟至第二个推理 backend 出现时 (`adr-0038:7` 增量决议 banner) | C15 实施后由 C18 重新评估 |
| **ADR-0039** | JSON 查询工具 (`inference/get/status`) 未实现 (`grep -rn "inference/get/status" src/ pdk/` → **0 matches**); C16 的 `available_models()` C++ 接口 ≠ ADR 规范的 JSON 工具 | C19 实施 |
| **ADR-0042** | 主文档第 10 行硬性 banner "ADR 整体状态仍保持 🔍 Proposed (C16 仅实施部分决策, C17+ 演进路径需独立 change 跟踪)" | C16 §5 Cloud 插件 + 第 2 阶段重新映射交付后由 C20 处理 |
| **ADR-0045** | 实施顺序 5 步 (骨架/ILLMProvider 包装/Agent 循环/事件驱动/DSL tools) 仅 step 2 部分交付 (`src/common/llm/orchestration_illm_provider.cpp` 存在, 但其他步骤未实施); 实施率 ~20% | Phase 6 实施 |
| **ADR-0046** | 4 通道架构仅通道 ① (Tool Layer) 完成基础设施 (lib/inference/*.md 标准化), 通道 ② (Event Layer `subscribe_topic`) / ③ (Query Layer JSON) / ④ (Config Layer) 未实施; 实施率 ~25% | Phase 6 实施 |

---

## 十二、Strategic Pivots Log

| 日期 | 原方向 | 新方向 | 影响 Changes | 决策依据 |
|------|--------|--------|------------|---------|
| 2026-07-10 | Phase 5 全部 ship 等待外部触发 | C17/C18 立即启动, 闭合 ADR drift + 评估 Phase 6 | C19/C20 启动延后, Phase 6 评估提前 | user decision (选项 A: ADR sync + Drift Gate) |
| 2026-07-10 | C17 翻转 12 个 ADR (`🔍 Proposed` → `✅ Approved`) | **C17 范围修正 12 → 5** (Metis FAIL 裁决后) | C17 proposal.md / tasks.md / spec.md / 本 plan §四/§五/§六/§十/§十一 同步更新; 估时 1-2 天 → 0.5-1 天; 后续 C16 §5 实施依赖部分解除 | **Metis session `ses_0b02706b7ffepKdYy3qxnmOzXy` FAIL 裁决 + user 选项 A 决策 (2026-07-10)** |

---

## 十三、3 种响应 Change 类型 (沿用 master plan §十三)

| 类型 | 触发 | 估时 | 命名 | 本计划示例 |
|------|------|------|------|----------|
| **🔧 fix** | 单一偏离 | 1-3 天 | `fix-<module>-<issue>` | — |
| **🔁 retro** | 多项偏离 | 1-2 Sprint | `<date>-<sprint>-retro` | — |
| **↪️ redirect** | 战略调整 | 2-5 天 | `<date>-redirect-<reason>` | C17 (ADR 状态批量同步) + C18 (Drift + Strategic Gate) |

---

## 十四、维护规则

1. **状态变更**: C17/C18 ship + archived 后, 本文件相应行的 "状态" 列更新为 `✅ archived`
2. **新增 change**: 如需新增 (例如 C17/C18 ship 后发现新 drift), 在本文件末尾追加新行
3. **依赖变更**: 立即更新 §二 依赖图
4. **同步检查**: 每次 Sprint 收官时检查本文件与 `docs/active-status.md` 的一致性
5. **本文件永不删除**, 仅追加与状态更新
6. **Review Gates 强制执行**: §九 中所有 gate 不跳过
7. **§十/§十一/§十二 append-only**: 历史完整保留

---

**最后更新**: 2026-07-10 (创建, 基于用户选项 A 决策)
**下次更新**: C17 ship 后, C18 启动时填实 Stage Gate 评估细节
**责任人**: Sisyphus (master plan 创建) → 用户 (后续维护)