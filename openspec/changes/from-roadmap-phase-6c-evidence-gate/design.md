## Context

ADR-0074 §决策 D4 定义 Evidence Gate 为 Phase 6c → Phase 7 推进的唯一客观门槛（parse-valid ≥85% + task-success L1 ≥70% + 临界带 90%），替代"感觉差不多"的隐性推进。本 change 是 ADR-0074 的 C4 步骤，**消费** `from-roadmap-phase-6c-execution-baseline` 提案产出的 `docs/audits/<date>-execution-baseline-v1.md` 报告，**输出** `docs/audits/<date>-evidence-gate-v1.md` 决议文档 + ADR-0074 §不变量 4 强制要求的 Wave 推进条件。

当前 Evidence Gate 实施缺口：
- `evaluate_gate()` 决策树函数不存在（应接受 4 阈值输入，返回 PASS / FAIL / CONDITIONAL / ABORT 4 状态枚举）；
- `docs/audits/` 目录尚无 evidence-gate 模板（execution-baseline 提案会创建同目录的执行报告）；
- `docs/active-status.md` §四 Phase 7 启动条件项 #1 当前为 ⏳ 阻塞中状态，待 C4 PASS 后翻牌；
- ADR-0074 状态当前 🔍 Proposed，C4 ship + D2 baseline 已 ship 后应翻牌 🟡 Partial（ADR-0074 §状态字段 §复审节点）。

依赖关系（per roadmap.md line 271-273）：
- **前置 blocker**: `from-roadmap-phase-6c-execution-baseline` change 必须先 ship（提供 baseline 测量数据）；
- **后置 unlock**: Evidence Gate PASS → 触发 ADR-0072 D2/D3 条件性 ship（由 `from-roadmap-phase-6c-execution-dsl` change 实施）；
- **Wave 推进政策**: ADR-0074 §不变量 4 强制 Evidence Gate 不可绕过——任何 Wave 推进必须附 Gate 决议文档。

## Goals / Non-Goals

**Goals:**

1. 实现 `evaluate_gate(parse_valid, task_success_l1, task_success_l2, task_success_l3) -> GateStatus` 决策树函数（4 阈值输入 → 4 状态输出），附 3 边界用例单元测试（parse-valid=84.9/85.0/89.9/90.0 → FAIL/CONDITIONAL/CONDITIONAL/PASS）。
2. 创建 `docs/audits/<date>-evidence-gate-v1.md` 决策文档模板（5 章节：§数据 plan + §测量方法 + §决策树 + §行动项 + §决议）。
3. 消费 `from-roadmap-phase-6c-execution-baseline` change 产出的真实 baseline 数据（禁止合成或外部推断），所有决议数值引用具体 file:line 证据。
4. 决策矩阵实施：
   - `parse-valid < 85%` → **FAIL** → 触发 ADR-0072 D2 `$var` 实施（C5，8h P0*）
   - `85% ≤ parse-valid < 90%` → **CONDITIONAL** → 触发 ADR-0072 D3 declarative style（C6，4h P0*）
   - `parse-valid ≥ 90%` → **PASS** → C5/C6 跳过
   - 数据缺失或 incomplete → **ABORT** → 不裁决，要求 C1+C2+C3 重新测量
5. 24h 内更新 `docs/active-status.md` §一（Phase 6c 状态行）与 §四（Phase 7 启动条件项 #1）；如决议涉及 ADR-0072 D2/D3 触发，同步追加 §0 触发动议。
6. ADR-0074 状态字段同步：若决议 PASS → 🔍 Proposed → 🟡 Partial；若任一阈值 FAIL → 保持 🔍 Proposed，决议录入 ADR-0074 §决策 D4 实证字段。

**Non-Goals:**

- baseline 测量本身（C1+C2+C3 由 `from-roadmap-phase-6c-execution-baseline` 提案实施）。
- ADR-0072 D2/D3 的代码实施（C5/C6 由 `from-roadmap-phase-6c-execution-dsl` 提案实施，由本 change 决议触发）。
- 持续测量基础设施（regression cron / 每次 prompt 变更后 24h 测量）——ADR-0074 §决策 D3 末项，留 Sprint 28+。
- 阈值本身的调整（ADR-0074 §不变量 4 强制：阈值变更需架构组评审，不在本提案权限内）。
- LLM 决策引入（per proposal Impact §MUST NOT — 决议必须 human review only）。

## Decisions

### D-1. 决策树实现为独立纯函数

**决策**: `evaluate_gate()` 实现为 `src/common/prompts/evidence_gate.h` header-only 纯函数（无 IO 依赖，便于 Phase 8+ 复用），4 状态枚举 `enum class GateStatus { Pass, Fail, Conditional, Abort }`，输入 4 个 `double` 阈值，输出 `GateStatus`。

**替代方案拒绝**:
- 类成员函数（增加 state，无必要）
- 嵌入 NodeExecutor 调度逻辑（强耦合，难以独立测试）

### D-2. 决议文档 5 章节固定结构

**决策**: `docs/audits/<date>-evidence-gate-v1.md` 强制 5 章节：§数据 plan（baseline 数据来源）+ §测量方法（如何得到 parse-valid 数字）+ §决策树（阈值 + 触发逻辑）+ §行动项（C5/C6 触发说明）+ §决议（PASS/FAIL/CONDITIONAL/ABORT 单选）。

**理由**: ADR-0074 §不变量 4 强制决议文档必须包含证据链与决策可审计性；固定结构便于 Phase 7+ 横向对比多次 Gate 决议。

### D-3. 数据完整性检查在前置位

**决策**: 决议函数入口先验证 baseline 数据完整性（golden suite ≥ 50 tasks / 3 模型全部报告 / YAML 报告字段无缺漏），任一缺失 → 立即返回 `GateStatus::Abort`，不进入阈值比较。

**理由**: 防止 baseline 异常数据下错误触发 C5/C6 实施（proposal Impact §MUST NOT）；Abort 路径要求 C1+C2+C3 重新测量，与 data-driven 决策原则一致。

### D-4. 临界带判定使用闭区间

**决策**: 阈值边界采用 `parse-valid >= 85.0` 与 `parse-valid < 90.0` 的左闭右开区间，避免浮点比较的边界歧义。决策树函数附 3 边界用例单元测试确保语义明确。

**理由**: 浮点等值比较易出错（baseline YAML 报告含 84.999999 vs 85.0 实际语义不同），闭区间 + 边界测试是行业惯例。

### D-5. 决议 → ADR-0074 状态字段同步

**决策**: 若决议 `Pass` → ADR-0074 状态 🔍 Proposed → 🟡 Partial（D2 baseline ship + C4 Evidence Gate ship），state amendment PR 路径：`docs/adr/adr-0074-prompt-evidence-gate.md` 顶部状态行 + §复审节点追加新行。若任一阈值 FAIL → 保持 🔍 Proposed，决议录入 §决策 D4 实证字段（不翻牌）。

**理由**: ADR-0074 是 Layer 0/1 架构 ADR，C4 是 D4 决策的首次实测验证，但 C5/C6 触发后 ADR-0074 才完整生效，故翻牌时机精确为 Gate Pass。

## Risks / Trade-offs

- **[Risk: 依赖 `from-roadmap-phase-6c-execution-baseline` ship 失败]** → Mitigation: 本 change tasks 明确标注前置 blocker，ship 顺序强制；baseline 失败则本 change 整体降级为"等待重新测量"状态。
- **[Risk: 阈值 85%/70% 不科学（基线样本不足）]** → Mitigation: 决议文档 §行动项 记录阈值质疑意见，提交架构组评审（per ADR-0074 §不变量 4）；不擅自调整阈值。
- **[Risk: 决议报告不可复现]** → Mitigation: §决议 章节强制记录 baseline run_id + 模型版本 + 温度参数 + YAML 报告 sha256；与 `docs/audits/<date>-execution-baseline-v1.md` 形成 immutable 证据链。
- **[Risk: C5/C6 触发后但 ADR-0072 D2/D3 未实施]** → Mitigation: proposal 明确决议文档仅记录建议，C5/C6 启动需独立 OpenSpec change + 人类评审（proposal Impact §MUST NOT 强制）。
- **[Risk: active-status.md 24h 更新漏更新]** → Mitigation: 决议文档 ship 时立即触发 active-status.md 同步更新 PR（pre-commit hook 验证引用关系）；超过 24h 时差视为违反 ship gate。

## Migration Plan

1. `from-roadmap-phase-6c-execution-baseline` change 必须先 ship 并产出 baseline 报告。
2. 本 change 实施决策树函数 + 决议文档模板（无 baseline 依赖，可提前 ship）。
3. 本 change 在 baseline 报告就绪后运行一次评估，输出决议文档。
4. 决议文档 ship 后 24h 内同步 active-status.md §一 §四。
5. ADR-0074 状态翻牌（决议 Pass 时）+ 同步 ADR-0072 D2/D3 触发动议（如适用）。

回滚策略：本 change 不引入运行时依赖（决策树函数仅用于离线决议），无需回滚 plan；决议文档如发现错误可重新评估（追加 v2 决议，不覆盖 v1）。

## Open Questions

1. 阈值是否需要在不同模型上差异化（per-model threshold vs 全模型平均 threshold）？当前 ADR-0074 §决策 D4 采用全模型平均，需在首次 Gate 决议后由架构组评估。
2. Evidence Gate 决议周期（仅 C4 一次性 vs 每次 Wave 推进前重测）？当前 C4 一次性，Wave 3 推进前重测留 Sprint 28+ follow-up。
3. C5/C6 触发后若 ADR-0072 D2/D3 实施效果不佳，是否有回退路径？当前决议文档不可逆，建议 Phase 7+ 引入 Gate v2 复议机制。
