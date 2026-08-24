# B.2 计划：Sprint 收官文档自动校验（v1.1 descope）

> **生成日期**: 2026-08-24（v1.0）
> **修订日期**: 2026-08-24（v1.1 — descope：Metis + Oracle 评审后重定向）
> **触发事件**: defect-truth-table-2026-08.md v1.1.1 → v1.1.2 修订（12 处状态同步）暴露"文档滞后于代码"问题
> **目标**: 防止下次 Sprint 收官时 defect-truth-table 出现同等规模的漂移
> **状态**: 🔍 Proposed（v1.1 descope，待 Sprint 24 启动评审）

---

## 一、问题陈述

### 1.1 当前痛点

v1.1.1 → v1.1.2 修订暴露 2 个系统性缺陷（v1.0 措辞 3 处，v1.1 校正）：

1. **文档修订依赖人工**：12 个 proposals 全部 ship 后，无人主动同步 `defect-truth-table.md` 的 14 项缺陷状态字段——直到下次评审引用过时文档才暴露问题
2. **现有 drift 检测未覆盖 defect-truth-table**：`scripts/sprint-closeout.sh` **Step 4**（v1.0 误写 Step 8）已运行 `tools/docs_drift_audit.py`，但 6 个现有 scenario 均未覆盖 defect-truth-table 这份特定文档——真实缺口是"audit 场景集缺第 7 场景"，而非"无 drift 检测"

**v1.0 → v1.1 校正**：
- 删除痛点 3 "无预防机制机制"（措辞重复痛点 1，保留单 1 措辞）
- 删除痛点 2 中 "Step 8" 错误前提（实际 Step 4 是文档检查）

### 1.2 量化本次漂移

| 漂移条目 | 数量 |
|---------|------|
| §一 索引表过时条目 | 4 处 |
| §二 详细描述过时条目 | 5 处 |
| §七 盲点过时条目 | 1 处（盲点 7.1 23 行体） |
| §九 参考内容汇编过时条目 | 2 处（**v1.1 校正**："实施率表"误命名——§九 实际是"参考内容汇编"） |
| **合计漂移** | **12 处** |

修复耗时：60-90 分钟（v1.0 估 30 分钟，Metis 评审指出低估 2-3 倍）。

### 1.3 预防价值

如果不修，下次 Batch 3+ 或后续 Phase 7 ship 后预计会出现同等或更大规模漂移：

- Phase 6 → Phase 7 过渡预计 ship ~15-20 个 proposals → 漂移预计 15-25 处
- ADR-0076/0077/0078 等 Wave 4-5 docs-only ADR → 不产生代码但需状态同步
- 长期累计漂移 → 文档不可信 → 重新审计时 Metis/Oracle 重复发现同样问题

---

## 二、目标与非目标（v1.1 descope）

### 2.1 目标

1. **自动检测 defect-truth-table 状态字段与代码真相的漂移**（v1.0 同）
2. **Sprint 收官时强制运行漂移检测**（v1.1 descope：升级 sprint-closeout Step 4 对 Scenario 7 的 severity）
3. ~~**提供自动修复路径**（半自动 patcher 生成 status 字段建议）~~ → **v1.1 否决**（详见 §六 D4）
4. ~~**建立文档漂移的预防性 pipeline**（CI 集成）~~ → **v1.1 否决**（详见 §六 D5）

### 2.2 非目标（v1.1 收紧）

1. **不解决所有文档漂移问题**——仅聚焦 defect-truth-table 这份具体文档（v1.1 校正：v1.0 说"其他文档"范围太广，descope 收敛）
2. **不替代人工 review**——自动检测仅生成警告/门控，最终判定仍需人工
3. **不新建并行 audit 脚本**——扩展现有 `tools/docs_drift_audit.py` Scenario 7（v1.1 核心 descope）
4. **不发明新阈值体系**——沿用现有 audit 退出码语义（`docs_drift_audit.py` 的 `make_finding` + `DRIFT/INFO` severity）
5. **不实现自动 patcher**（v1.0 §3.2，v1.1 否决——见 §六 D4）
6. **不实现每周 CI cron + 自动 Issue**（v1.0 §3.4，v1.1 否决——见 §六 D5）

---

## 三、技术方案（v1.1 descope —— 核心重定向）

### 3.1 核心组件：扩展 `tools/docs_drift_audit.py` Scenario 7

**位置**：`tools/docs_drift_audit.py`（已存在，981 行 Python），新增 `scan_scenario7(root)` 函数

**输入**（已存在的 `_build_source_index()` 缓存复用）：
- `docs/architecture/defect-truth-table-*.md`（glob 最新文件，**禁止硬编码月份**——避免 -2026-09 起静默失效）
- `tools/docs_drift_audit.py` 已缓存的代码索引（`src/` + `include/` + `pdk/` 符号存在性）
- `docs/adr/*.md`（已存在于场景 4 索引）
- `openspec/changes/archive/`（已存在于场景 5 索引）

**输出**：现有 `make_finding()` schema（保持向后兼容）：

````python
finding = {
    "scenario": 7,
    "scenario_name": "defect-truth-table vs code/ADR/archive drift",
    "drift_type": "D1 | D2 | D4",  # 见 §3.2
    "location": "defect-truth-table.md:90",  # 文件:行号
    "defect_id": "1.1",  # 缺陷 ID
    "description": "缺陷 1.1 标 SessionWriter 实施 0%，但 src/core/session_writer.{h,cpp} 已 ship",
    "severity": "DRIFT",
    "expected": "✅ 已 ship",  # 代码/ADR 真相
    "actual": "SessionWriter 实施 0%",  # 文档现状
    "auto_fix_suggestion": None  # v1.1 不实现 patcher，仅人工修正
}
```

**5 类检测规则**（v1.1 简化：只实施 D1/D2/D4，否决 D3/D5）：

| 类别 | 检查方法 | 来源 |
|------|---------|------|
| **D1**：缺陷状态 vs 代码存在性 | glob 关键代码符号是否存在 | **v1.0 同，新增强制 glob** |
| **D2**：缺陷状态 vs ADR 头部状态 | 复用场景 4 的 `STATUS_LINE_PATTERN`（已存在 line 549） | **v1.1 descope：不新写 ADR 解析器**，复用场景 4 |
| **D3**：缺陷优先级 vs roadmap 引用 | （v1.1 **否决**——bespoke 章节解析代价高于价值，详见 §六）| v1.1 移除 |
| **D4**：盲点状态 vs code grep | 同 D1 路径 | **v1.0 同** |
| **D5**：版本号一致性 | （v1.1 **否决**——与 GOVERNANCE.md 数据纪律定位不同质，归 `adr_lint.py` 域，详见 §六）| v1.1 移除 |

### 3.2 集成点：sprint-closeout Step 4 升级 severity（**零新集成**）

**位置**：`scripts/sprint-closeout.sh`（已存在 341 行），Step 4（line 200-204）

**当前 Step 4**：

```bash
if [ "$RUN_DOCS_AUDIT" = true ]; then
  print_step "python3 tools/docs_drift_audit.py..."
  if python3 tools/docs_drift_audit.py 2>&1 | tail -20; then
    # ... warn 处理
```

**v1.1 升级**：

```bash
# Sprint 24: Scenario 7 命中 → print_warn（保留现有语义）
# Sprint 25: Scenario 7 命中 → print_fail（升一级，与项目渐进门控一致）
if [ "$RUN_DOCS_AUDIT" = true ]; then
  output=$(python3 tools/docs_drift_audit.py --json 2>&1)
  if echo "$output" | grep -q '"scenario": 7'; then
    if [ "${SCENARIO_7_SEVERITY:-warn}" = "fail" ]; then
      print_fail "Scenario 7 drift detected (defect-truth-table)"
    else
      print_warn "Scenario 7 drift detected (defect-truth-table) — Sprint 25 起将升级为 fail"
    fi
  fi
fi
```

**阶段过渡**：
- **Sprint 24**：Scenario 7 drift → warn（与现有 audit 一致）
- **Sprint 25 起**：Scenario 7 drift → fail（门控生效）
- **紧急 override**：`--skip-drift` / `--force` flag 保留（避免误报阻塞 ship）

### 3.3 回归测试：v1.1.1 fixture 化（v1.0 不可行的修正）

**位置**：`tools/tests/`（复用 `tools/docs_drift_audit.py` 现有测试路径）

**fixture 设计**：

```bash
# Step 1: 复制 v1.1.1 版本 defect-truth-table.md 作为 fixture
mkdir -p tools/tests/fixtures
git show e1422c8:docs/architecture/defect-truth-table-2026-08.md \
    > tools/tests/fixtures/defect-truth-table-v1.1.1.md

# Step 2: Scenario 7 测试用例（集成进 docs_drift_audit.py 现有 run_audit 测试）
def test_scenario7_regression_v1_1_1():
    output = run_audit_with_fixture("v1.1.1")
    drifts = [f for f in output if f["scenario"] == 7]
    assert len(drifts) == 12, f"expected 12 drift items, got {len(drifts)}"
    assert {f["defect_id"] for f in drifts} == \
        {"1.1", "1.2", "1.3", "1.5", "3.3", "7.1", "ADR-0079", "ADR-0060"}
```

**关键改进**：v1.0 假设 `git checkout` 回放 v1.1.1——技术上需 git worktree 或 git stash，不可行。v1.1 用 fixture 文件直接读取 + 测试断言 12 条 drift，零 git 操作。

### 3.4 文档格式契约：defect-truth-table 家族规范（v1.1 新增）

**位置**：`docs/architecture/README.md` §二（文档头元数据规范）

**新增条目**：

```markdown
### defect-truth-table 家族专条

- **文件名**: `defect-truth-table-YYYY-MM.md`（按月轮换）
- **章节结构**（不可重构）：
  - §一 索引表（4 列：缺陷 ID / 描述 / 主参考 ADR / 现状）
  - §二 详细描述（按层分组：Layer A-F）
  - §六 修订说明（v1.0 → v1.X 的修订项）
  - §七 新发现盲点
  - §九 参考内容汇编（**非**"实施率表"，v1.1 校正误命名）
- **状态字段取值枚举**（§一索引表 + §二当前状态）：
  - `✅ 已 ship` / `🟡 部分` / `❌ 未 ship` / `🟡 分层部分解决` / `✅ 已 ship (L3 契约 ship)` / `⚪ 有意例外`
- **检测脚本对枚举外取值** → 输出 WARNING（"文档结构变更？"），不静默误判
```

**目的**：让 Scenario 7 对格式变化显式报警而非静默失效；下次轮换（-2026-11）零脚本改动。

---

## 四、实施步骤（v1.1 descope）

### Phase 1（P0-1 + P0-2 + P0-3，总估时 2.5 天）

| 任务 | 工时 | 累计 |
|------|------|------|
| **P0-1** 修订 B.2 计划文档（本文档）| 0.5 天 | 0.5 天 |
| **P0-2** 扩展 `tools/docs_drift_audit.py` 新增 `scan_scenario7()`（含回归 fixture `tools/tests/fixtures/defect-truth-table-v1.1.1.md`） | 1.5 天 | 2 天 |
| **P0-3** 升级 `scripts/sprint-closeout.sh` Step 4（Scenario 7 severity warn → fail 阶段过渡） | 0.5 天 | 2.5 天 |

### Phase 2（v1.1 冻结 P3，仅在触发条件下启动）

| 任务 | 触发条件 | 估时 |
|------|---------|------|
| **P3-1** 评估 patcher（`docs-drift-patch.py`）| 场景 7 连续 2 Sprint 误报率 <10% + 人工同步 >1 小时/Sprint | 1 天 |
| **P3-2** 每周 CI cron + 自动 Issue | 出现"Sprint 之间 defect-truth-table 自发漂移"案例 | 0.5 天 |
| **P3-3** 场景 7 规则推广到 `batch-N-ship-gate.md` | 该文档出现状态漂移（一次性观察）| 1 天 |

### Phase 3（v1.1 否定删除）

| v1.0 计划任务 | v1.1 处置 |
|-------------|---------|
| ~~Phase 2 Sprint 25（CI + 半自动门控）~~ | **整段删除**——Sprint 25 已合并到 P0-3（门控升级）；CI cron 移到 P3-2 |
| ~~Phase 3 Sprint 26+（强制门控）~~ | **整段删除**——P0-3 Sprint 25 起门控生效，无 Sprint 26 阶段 |
| ~~Phase 3 ECI workflow 创建~~ | **整段删除**——P3-2 触发条件极难满足（机制上不可能 Sprint 之间漂移） |

**总估时对比**：v1.0 = 7 天（4 + 2 + 1）→ v1.1 = **2.5 天**（**descope 65%**）

---

## 五、验收标准（v1.1 收紧）

### 5.1 功能性验收

- [ ] `tools/docs_drift_audit.py` 新增 `scan_scenario7()` 函数（**复用** `_build_source_index()` 缓存）
- [ ] `tools/tests/fixtures/defect-truth-table-v1.1.1.md` fixture 创建
- [ ] Scenario 7 回归测试：对 v1.1.1 fixture 恰好输出 **12 条 drift**（v1.0 验收标准原样保留）
- [ ] Scenario 7 单元测试：枚举外状态字段 → WARNING 而非 silent（v1.1 新增）
- [ ] `tools/tests/fixtures/README.md` 说明 fixture 维护方法（每 Sprint 更新 v1.1.1 → v1.1.X）
- [ ] ctest 全量零回归（181-182 个测试）
- [ ] `python3 tools/adr_lint.py` 0 errors（78 ADR）
- [ ] `openspec validate --all` 0 failures（66 items）

### 5.2 性能验收

- [ ] `scan_scenario7()` 运行时间 ≤ 5 秒（v1.1 改进：复用已有 index 缓存，目标从 v1.0 30 秒降到 5 秒）
- [ ] 整体 `docs_drift_audit.py` 运行时间 ≤ 60 秒（当前 ~45 秒）

### 5.3 长期价值验收（Phase 2 之后）

- [ ] Sprint 25+ 的 defect-truth-table 漂移条目 ≤ 3 处（vs v1.1.2 前的 12 处）
- [ ] Sprint 27 评估场景 7 误报率（v1.1 阶段过渡数据）
- [ ] Metis/Oracle 后续评审不再重复发现"文档 vs 代码"漂移

---

## 六、关联决策项（v1.1 更新）

### 已决议

| # | 决策项 | v1.0 计划 | v1.1 决议 | 理由 |
|---|--------|---------|---------|------|
| **D1** | Sprint 24 启动 B.2？ | 启动 7 天 3 阶段 | **启动 2.5 天 1 阶段**（Phase 1）| descope 65%，保留核心价值 |
| **D2** | 阈值 5 是否合理？ | 起步 5 | **沿用 `docs_drift_audit.py` 现有退出码语义**（不发明第二套阈值）| 单一阈值体系；门控 severity 由 Sprint 24/25 阶段过渡控制 |
| **D3** | 复用 `docs_drift_audit.py`？ | "复用现有脚本" | **强烈：在其中扩展 Scenario 7**（不是"复用"，是"嵌入"）| 981 行成熟基建复用，零新集成点 |

### v1.1 新增

| # | 决策项 | v1.1 决议 | 理由 |
|---|--------|---------|------|
| **D4** | `docs-drift-patch.py` 半自动 patcher？ | **否决（冻结）** | 自动写"单一事实源"文档属于**污染风险**——检测工具错是噪音，写入工具错是污染；ROI 不匹配（patcher 投入 ~3 天，仅省 20 分钟人工）|
| **D5** | 每周 CI cron + 自动开 Issue？ | **否决（冻结）** | sprint-closeout 已是最优检查频率；批次性文档在 Sprint 之间不会自发漂移（只在 ship/评审时变化，而这两个时刻都有 sprint-closeout 覆盖）；cron 只会产生噪音 |
| **D6** | ADR 状态解析归属？ | **收敛到 `tools/adr_lint.py`（格式层）+ `docs_drift_audit.py` 场景 4（语义层）**，**禁止第三处** | 三处 ADR 状态正则会相互漂移，制造元漂移；B.2 Scenario 7 必须复用场景 4 的 `STATUS_LINE_PATTERN`，不新写 ADR 解析器 |

### D4-D6 解冻条件（D4/D5）

- **D4 patcher 解冻**：场景 7 连续 2 Sprint 误报率 <10% + 人工同步实测 >1 小时/Sprint
- **D5 cron 解冻**：出现"Sprint 之间 defect-truth-table 自发漂移"的实际案例
- **解冻评审**：架构组 Sprint Planning 评审 + Oracle 复审

---

## 七、关联文档

- **触发文档**：`docs/architecture/defect-truth-table-2026-08.md`（v1.1.2，§六.6 v1.1.2 修订说明）
- **现有工具**：`tools/docs_drift_audit.py`（981 行，6 场景，扩展基础）
- **现有脚本**：`scripts/sprint-closeout.sh`（341 行，Step 4 已接线）
- **现有 CI**：`.github/workflows/`（sprint-closeout 触发时已运行 audit）
- **治理规则**：`docs/GOVERNANCE.md` §一.5（数据纪律：计数必须可复现）

---

## 八、关联 ADR

- **ADR-0019**（IInteractionBus MVP）：文档架构层
- **ADR-0050**（Phase 6 战略评估）：Candidate B 战略评估中提到"工具链完善"

---

## 九、追踪

| Sprint | 状态 | 备注 |
|--------|------|------|
| 23（当前） | 🔍 Proposed v1.1 descope | Metis + Oracle 评审后重定向完成 |
| 24 | ⏳ 待启动 | Phase 1（2.5 天核心脚本 + 门控） |
| 25 | ⏳ 待启动 | Phase 1 后半段（Scenario 7 severity → fail 升级） |
| 26+ | ⏳ 待启动 | Phase 2 触发条件达成时启动 P3 任务 |

---

**审批与维护**：
- v1.0 提议：2026-08-24（v1.1.2 修订同步）
- v1.1 修订：2026-08-24（Metis + Oracle 评审后 descope）
- 维护者：架构组 + Sprint 收官机制
- 审查频率：每 Sprint 收官
- 与 `scripts/sprint-closeout.sh` Step 4 集成

---

## 附录 A：v1.0 → v1.1 修订对照表

| # | v1.0 措辞 | v1.1 措辞 | 修订原因 |
|---|----------|-----------|---------|
| 1 | §1.1 痛点 2："sprint-closeout.sh Step 8 只检查代码质量 + 测试，未检查文档一致性" | "现有 drift 检测未覆盖 defect-truth-table：sprint-closeout Step 4 已运行 docs_drift_audit.py，但 6 个现有 scenario 均未覆盖 defect-truth-table" | 事实错误——Step 4 才是文档检查；真实缺口是 audit 场景集缺第 7 场景 |
| 2 | §1.1 痛点 3："无预防机制机制：缺陷真相表与代码真相长期脱钩" | **删除**（与痛点 1 措辞重复） | 简化问题陈述 |
| 3 | §1.2 漂移条目 §九 名称："§九 实施率表过时条目 2 处" | "§九 参考内容汇编过时条目 2 处" | 误命名——§九 实际是"参考内容汇编"，不是独立"实施率表" |
| 4 | §1.2 修复耗时："30 分钟" | "60-90 分钟" | Metis 评审指出低估 2-3 倍（实际含交叉引用 god object ADR） |
| 5 | §2.1 目标 3："提供自动修复路径（半自动 patcher 生成 status 字段建议）" | **否决（删除）** | D4 否决——自动写单一事实源文档 =污染风险 |
| 6 | §2.1 目标 4："建立文档漂移的预防性 pipeline（CI 集成）" | **否决（删除）** | D5 否决——sprint-closeout 已是最优频率 |
| 7 | §2.2 非目标 5："不强制要求 ship gate 文档同步" | **删除** | 与 §2.1 目标 2 重复 |
| 8 | §3.1 5 类检测规则：全部实施 | §3.1 简化：只实施 D1/D2/D4，否决 D3/D5 | descope 核心 |
| 9 | §3.1 D2 检测方法："parse ADR 文件的 `**状态**:` 头部行" | "复用场景 4 的 `STATUS_LINE_PATTERN`（已存在 line 549）" | 误命名 + 避免第三处 ADR 解析器（D6） |
| 10 | §3.1 核心组件："新建 `scripts/docs-drift-detect.sh`（~200 行 bash + Python 混合）" | "扩展 `tools/docs_drift_audit.py` 新增 `scan_scenario7()` 函数" | 工具落点违反项目惯例（`tools/*.py` Python 标准库） |
| 11 | §3.2 `docs-drift-patch.py` 半自动 patcher | **整段否决**（删除） | D4 否决——污染风险 vs节省 20 分钟 |
| 12 | §3.3 集成点："`scripts/sprint-closeout.sh` Step 9（新建）" | "`scripts/sprint-closeout.sh` Step 4 升级 Scenario 7 severity（零新集成）" | 集成点错误——不是新建 Step 9，而是增强现有 Step 4 |
| 13 | §3.3 阶段过渡："Sprint 24 warn / Sprint 25 半自动 / Sprint 26 强制" | "Sprint 24 Scenario 7 warn / Sprint 25 Scenario 7 fail" | 阶段过渡简化——半自动 patcher 整段删除 |
| 14 | §3.4 CI integration + 每周 GitHub Issue | **整段否决**（删除） | D5 否决 |
| 15 | §4 Phase 1 + Phase 2 + Phase 3 共 7 天 | Phase 1 2.5 天 + Phase 2 冻结 | 总估时 7 天 → 2.5 天（**descope 65%**） |
| 16 | §4 Phase 3 强制门控：drift ≥ 5 exit 1 | **删除**（沿用 docs_drift_audit.py 现有退出码语义，不发明新阈值） | D2 决议 |
| 17 | §5.1 验收："回归测试：回放 v1.1.1 状态，运行脚本应输出 12 条 drift（git checkout）" | "回归测试：`tools/tests/fixtures/defect-truth-table-v1.1.1.md` fixture + 断言 12 条 drift（零 git 操作）" | v1.0 假设 `git checkout` 在技术上不可行（需 worktree 或 stash），v1.1 用 fixture 文件直接读取 |
| 18 | §5.2 性能："detect ≤ 30 秒" | "scan_scenario7() ≤ 5 秒；整体 ≤ 60 秒" | v1.1 复用缓存，目标从 30 秒降到 5 秒 |
| 19 | §九 D3 决策："先复用现有脚本（避免重复造轮子）" | "强烈：在其中扩展 Scenario 7（不是'复用'，是'嵌入'）" | 措辞强化——从"复用"提升为"嵌入" |
| 20 | （v1.1 新增）§3.4 文档格式契约 | （v1.0 无）| 为下次轮换（-2026-11）零脚本改动铺路 |
| 21 | （v1.1 新增）§六 D4/D5/D6 | （v1.0 无）| 明确否决 3 个原计划组件（patcher / CI cron / 第三处 ADR 解析器） |

**修订总条目**：21 处（v1.1 vs v1.0），其中 6 处删除 / 6 处实质修改 / 1 处新增 / 8 处措辞微调。

**修订范围限定**：零代码变更影响，零 ADR 结论冲突，零 ctest 影响。

**修订触发器**：Metis + Oracle 评审（v1.1.2 commit `eb2e30e` 后同步启动）。