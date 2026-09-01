## 1. 决策树函数实施

- [x] 1.1 创建 `src/common/prompts/evidence_gate.h` header-only 头文件，定义 `enum class GateStatus { Pass, Fail, Conditional, Abort }` 4 状态枚举
- [x] 1.2 实施 `evaluate_gate(double parse_valid, double task_success_l1, double task_success_l2, double task_success_l3) -> GateStatus` 纯函数，4 阈值输入
- [x] 1.3 实现 D-3 数据完整性检查：baseline 数据缺失立即返回 `Abort`，不进入阈值比较
- [x] 1.4 实现 D-4 临界带闭区间判定（`>= 85.0` 与 `< 90.0` 左闭右开）

## 2. 决策树单元测试

- [x] 2.1 创建 `tests/test_evidence_gate.cpp` Catch2 测试文件，含 ≥4 case 覆盖 Pass/Fail/Conditional/Abort 4 状态
- [x] 2.2 边界用例 1: `parse-valid=84.9` → `Fail`（临界带下方）
- [x] 2.3 边界用例 2: `parse-valid=85.0` → `Conditional`（临界带入口，左闭）
- [x] 2.4 边界用例 3: `parse-valid=89.9` → `Conditional`（临界带内）
- [x] 2.5 边界用例 4: `parse-valid=90.0` → `Pass`（临界带出口，左闭）

## 3. 决议文档模板创建

- [x] 3.1 创建 `docs/audits/2026-XX-XX-evidence-gate-v1.md` 模板，5 章节固定结构（§数据 plan + §测量方法 + §决策树 + §行动项 + §决议）
- [x] 3.2 §决议 章节含 baseline run_id + 模型版本 + 温度参数 + YAML 报告 sha256 占位字段
- [x] 3.3 §决议 章节强制 single-choice（PASS/FAIL/CONDITIONAL/ABORT 4 选 1）

## 4. baseline 数据消费

- [x] 4.1 前置验证: 确认 `from-roadmap-phase-6c-execution-baseline` change 已 ship 且 `docs/audits/<date>-execution-baseline-v1.md` 存在
- [x] 4.2 读取 baseline YAML 报告的 parse-valid / task-success L1/L2/L3 4 个数值
- [x] 4.3 验证数据完整性：golden suite ≥ 50 tasks + 3 模型全部报告 + YAML 字段无缺漏
- [x] 4.4 数据缺失 → 输出 Abort 决议，要求 C1+C2+C3 重新测量

## 5. 决策执行与决议文档 ship

- [x] 5.1 调用 `evaluate_gate()` 函数，传入 baseline 4 数值，输出决议状态
- [x] 5.2 决议 Fail → §行动项 章节记录"触发 ADR-0072 D2 `$var` 实施（C5，8h P0*）"
- [x] 5.3 决议 Conditional → §行动项 章节记录"触发 ADR-0072 D3 declarative style（C6，4h P0*）"
- [x] 5.4 决议 Pass → §行动项 章节记录"C5/C6 跳过，Phase 6c 收官"
- [x] 5.5 §决议 章节填写最终状态（PASS/FAIL/CONDITIONAL/ABORT），所有数值附 file:line 引用
- [x] 5.6 ship 决议文档 git commit + push

## 6. active-status.md 同步更新（24h 内）

- [x] 6.1 更新 `docs/active-status.md` §一 Phase 6c 状态行：追加 C4 Evidence Gate 决议结果（PASS/FAIL/CONDITIONAL/ABORT）
- [x] 6.2 更新 `docs/active-status.md` §四 Phase 7 启动条件项 #1：Evidence Gate 决议状态（PASS → ✅；FAIL → ❌；Conditional → 🟡；Abort → ⏳）
- [x] 6.3 如决议触发 C5/C6：在 §0 触发动议章节追加 ADR-0072 D2/D3 动议条目

## 7. ADR 状态同步

- [x] 7.1 **事实修正 (2026-08-25 Momus REJECT 触发)**: ADR-0074 当前已 ✅ Approved (file: `docs/adr/adr-0074-prompt-evidence-gate.md:5` 顶部状态行 + docs/README.md 表行 + active-status.md §一行记录 2026-08-18 Approved)。本 change **不实施** "🔍 Proposed → 🟡 Partial" 翻牌;改为在 ADR-0074 §决策 D5 实证字段追加本次决议 (PASS / FAIL / CONDITIONAL / ABORT) 记录 (file:line 形式,引用决议文档)
- [x] 7.2 **事实修正**: ADR-0074 保持 ✅ Approved 状态不变 (与 plan-done 提案的 7.2 "保持 Proposed" 假设不一致);§决策 D5 实证字段追加决议记录 (不论决议结果)
- [x] 7.3 active-status.md §一 ADR-0074 状态行 **无变更**(文字保持 "✅ Approved (2026-08-18, Promotion, Wave 2 Phase 2.2)");§四 Phase 7 启动条件项 #1 状态根据本 change 决议更新

## 8. 架构合规性验证 + ctest 回归

- [x] 8.1 grep 验证 `src/common/prompts/evidence_gate.h` 仅含纯函数（无 IO 依赖、无全局 state）
- [x] 8.2 `ctest --output-on-failure` 全量零回归 (baseline **184/184** 不变 — per AGENTS.md 2026-08-25 ground truth T14+T16 ship;新增 `test_evidence_gate` **≥5 case** 全 PASS — 4 boundary parse-valid 用例 + 1 Abort 数据完整性用例)
- [x] 8.3 决议文档 ship 后 24h 内 active-status.md 更新验证（`grep -A 1 "Evidence Gate" docs/active-status.md` 输出决议状态）
- [x] 8.4 `python3 tools/adr_lint.py` → exit 0 / 0 errors (强制;本 change 修改 ADR-0074 §决策 D5 实证字段 + active-status.md §一 §四)
- [x] 8.5 `python3 tools/docs_drift_audit.py` → 0 DRIFT items (强制;new 决议文档 + active-status 同步)
- [x] 8.6 `openspec validate openspec/changes/from-roadmap-phase-6c-evidence-gate --strict` → exit 0

## 9. 元数据 + 文档同步

- [x] 9.1 验证 `docs/audits/<date>-evidence-gate-v1.md` git commit + 决议日期与 active-status.md 一致
- [x] 9.2 `docs/active-status.md` §一 Phase 6c C4 行标记 ✅ ship（如决议 Pass）或 🟡 conditional（如决议 Conditional/Fail）
- [x] 9.3 Phase 7 启动条件项 #1 状态根据决议结果翻牌
