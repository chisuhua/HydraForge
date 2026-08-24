# B.2 计划：Sprint 收官文档自动校验

> **生成日期**: 2026-08-24
> **触发事件**: defect-truth-table-2026-08.md v1.1.1 → v1.1.2 修订（12 处状态同步）暴露"文档滞后于代码"问题
> **目标**: 防止下次 Sprint 收官时 defect-truth-table 出现同等规模的漂移
> **状态**: 🔍 Proposed（待 Sprint 24 启动评审）

---

## 一、问题陈述

### 1.1 当前痛点

v1.1.1 → v1.1.2 修订暴露出 3 个系统性缺陷：

1. **文档修订依赖人工**：12 个 proposals 全部 ship 后，无人主动同步 `defect-truth-table.md` 的 14 项缺陷状态字段
2. **Sprint 收官 checklist 未覆盖文档同步**：`scripts/sprint-closeout.sh` Step 8 只检查代码质量 + 测试，未检查文档一致性
3. **无预防机制机制**：缺陷真相表（"单一事实源"）与代码真相长期脱钩，导致下一轮评审（Metis/Oracle）会重复发现"文档 vs 代码"问题

### 1.2 量化本次漂移

| 漂移条目 | 数量 |
|---------|------|
| §一 索引表过时条目 | 4 处 |
| §二 详细描述过时条目 | 5 处 |
| §七 盲点过时条目 | 1 处（盲点 7.1 23 行体） |
| §九 实施率表过时条目 | 2 处 |
| **合计漂移** | **12 处** |

修复耗时：30 分钟（实际 v1.1.2 修订工作量）。

### 1.3 预防价值

如果不修，下次 Batch 3+ 或后续 Phase 7 ship 后预计会出现同等或更大规模漂移：

- Phase 6 → Phase 7 过渡预计 ship ~15-20 个 proposals → 漂移预计 15-25 处
- ADR-0076/0077/0078 等 Wave 4-5 docs-only ADR → 不产生代码但需状态同步
- 长期累计漂移 → 文档不可信 → 重新审计时 Metis/Oracle 重复发现同样问题

---

## 二、目标与非目标

### 2.1 目标

1. **自动检测 defect-truth-table 状态字段与代码真相的漂移**
2. **Sprint 收官时强制运行漂移检测**（非阻塞警告 → 强制门控逐步过渡）
3. **提供自动修复路径**（半自动 patcher 生成 status 字段建议）
4. **建立文档漂移的预防性 pipeline**（CI 集成）

### 2.2 非目标

1. **不解决所有文档漂移问题**——仅聚焦 defect-truth-table（v1.1.2 暴露的具体痛点）
2. **不替代人工 review**——自动检测仅生成警告，最终判定仍需人工
3. **不强制要求 ship gate 文档同步**——v1.1.2 是历史修订，B.2 目标在 Sprint 24+ 预防

---

## 三、技术方案

### 3.1 核心组件：docs-drift-detect.sh

**位置**：`scripts/docs-drift-detect.sh`（新建，~200 行 bash + Python 混合）

**输入**：
- `docs/architecture/defect-truth-table-2026-08.md`（解析 §一索引表）
- `docs/architecture/defect-fix-roadmap-2026-08.md`（解析 12 个 proposals 状态）
- `openspec/changes/archive/`（解析已 archive 的 changes）
- `src/`、`include/`、`pdk/`（grep 关键代码符号）

**输出**：JSON 报告 `docs/audits/<date>-docs-drift-detect.json`

**检测规则**（5 类）：

| 检测类别 | 检查方法 | 示例 |
|---------|---------|------|
| **D1：缺陷状态 vs 代码存在性** | grep `src/` 中修复代码的存在 | P5 SessionWriter 文件存在但缺陷 1.1 标"未 ship" |
| **D2：缺陷状态 vs ADR 头部状态** | parse ADR 文件的 `**状态**:` 头部行 | ADR-0079 v1.1 ✅ 但 defect-truth-table 标"实施 0%" |
| **D3：缺陷优先级 vs roadmap 引用** | grep defect-truth-table 中的 ADR 引用 → 与 roadmap 的 Phase 对齐 | P0 缺陷在 Phase D 而非 Phase A |
| **D4：盲点状态 vs code grep** | grep `src/` 中盲点修复符号 | 盲点 7.1 ExecutionResult 已合并但文档未更新 |
| **D5：版本号一致** | grep `v1.X` 字符串 vs §六修订说明 | §六.6 v1.1.2 已写但 §一索引无 v1.1.2 标记 |

**关键 diff 模式**（正则）：

```bash
# D1: 缺陷 X.Y 状态行 vs 关键符号
pattern="^\| $DEFECT_ID.*\|"
status_field=$(echo "$line" | awk -F'|' '{print $5}')
if [[ "$status_field" == *0%* || "$status_field" == *未 ship* ]]; then
    # 检查代码符号是否存在
    if grep -rq "$SYMBOL_PATTERN" src/ include/ pdk/; then
        report_drift "$DEFECT_ID" "D1" "代码存在但文档标未 ship"
    fi
fi
```

### 3.2 半自动 Patcher：docs-drift-patch.py

**位置**：`scripts/docs-drift-patch.py`（新建，~150 行 Python）

**输入**：docs-drift-detect.sh 输出的 JSON 报告

**输出**：defect-truth-table.md patch 建议（diff 格式）

**行为**：
- 对 D1 类漂移：从 §二 详细描述自动推断 "✅ 已 ship" 措辞（含 proposal 引用 + 测试 case 数）
- 对 D2 类漂移：从 ADR 头部状态读取"Approved/Shipped" → 同步到 §九 实施率表
- 对 D4 类漂移：从 grep 命中的代码位置构造更新建议
- 对 D3、D5 类漂移：仅生成建议，不自动 patch（需人工判定）

**限制**：
- 不会触碰 §六 修订说明（人工 v1.X.X 命名）
- 不会改 defect 描述主体（仅更新 "**当前状态**" 行）
- 不会动 §一/§二/§九 的"备注"列（语义内容）

### 3.3 集成点：sprint-closeout.sh

**位置**：`scripts/sprint-closeout.sh`（修改，+1 个 Step）

**新增 Step 9**：docs-drift-detect

```bash
# Step 9: 文档漂移检测（v1.1.2 修订后新增）
if [ -f scripts/docs-drift-detect.sh ]; then
    ./scripts/docs-drift-detect.sh --format=summary --threshold=3
    EC=$?
    if [ "$EC" -eq 2 ]; then
        echo "WARN: defect-truth-table 与代码 drift ≥ 3 处（建议同步 v1.1.X）"
        echo "      详细: docs/audits/<date>-docs-drift-detect.json"
    fi
    # Sprint 24+ 升级为强制门控（EC=2 时 exit 1）
fi
```

**阶段过渡**：
- **Sprint 24**：非阻塞警告（exit code 2 仅警告，不阻塞 ship）
- **Sprint 25**：warning + 必须提交 docs-drift-patch.py 自动修复 patch（保留人工 review 权）
- **Sprint 26+**：升级为强制门控（drift ≥ 5 时 exit 1 阻塞 ship）

### 3.4 CI 集成：每周自动运行

**位置**：`.github/workflows/docs-drift-check.yml`（新建）

**触发**：每周一 00:00 UTC（cron schedule）

**内容**：
- 拉取最新 main
- 运行 docs-drift-detect.sh
- 如果 drift ≥ 5 → 自动创建 GitHub Issue（标题："docs-drift-detect: N 处漂移需同步"）
- Issue body 包含 drift 详情 + 自动 patch 建议

---

## 四、实施步骤

### Phase 1（推荐 Sprint 24，1 周）

| 任务 | 工时 | Owner |
|------|------|-------|
| T1.1: 实现 `scripts/docs-drift-detect.sh`（5 类检测规则） | 1 天 | Architecture WG |
| T1.2: 实现 `scripts/docs-drift-patch.py`（半自动 patcher） | 1 天 | Architecture WG |
| T1.3: 修改 `scripts/sprint-closeout.sh` Step 9（v1.1.2 阶段：警告级） | 0.5 天 | Sprint WG |
| T1.4: 在 `defect-truth-table.md` §六追加 "B.2 关联" 章节（追踪计划） | 0.5 天 | Architecture WG |
| T1.5: 文档化 B.2 + 集成到 `tools/` 工具链文档 | 0.5 天 | Architecture WG |

### Phase 2（推荐 Sprint 25，0.5 周）

| 任务 | 工时 | Owner |
|------|------|-------|
| T2.1: 添加 CI workflow（每周一 drift check） | 0.5 天 | DevOps |
| T2.2: 升级 Step 9 为强制提交 patch（半自动门控） | 0.5 天 | Sprint WG |
| T2.3: 运行在 Batch 3+ 收官验证（dry-run 1 次 + 实际 1 次） | 1 天 | Architecture WG |

### Phase 3（推荐 Sprint 26，0.5 周）

| 任务 | 工时 | Owner |
|------|------|-------|
| T3.1: 升级 Step 9 为强制门控（drift ≥ 5 exit 1） | 0.5 天 | Sprint WG |
| T3.2: 文档化 B.2 ship gate + 加入 sprint-closeout 文档 | 0.5 天 | Architecture WG |

### 总估时

- **Phase 1**: ~4 天
- **Phase 2**: ~2 天
- **Phase 3**: ~1 天
- **合计**: ~7 天（1.5 sprint）

---

## 五、验收标准

### 5.1 功能性验收

- [ ] `scripts/docs-drift-detect.sh` 能检测 v1.1.2 暴露的 12 处漂移（**回归测试**：回放 v1.1.1 状态，运行脚本应输出 12 条 drift）
- [ ] `scripts/docs-drift-patch.py` 生成的 patch 与 v1.1.2 实际修订一致（**回归测试**：用 v1.1.1 → v1.1.2 diff 作为 ground truth）
- [ ] `sprint-closeout.sh` Step 9 集成正确（warning 阶段）
- [ ] CI workflow 每周一自动触发

### 5.2 性能验收

- [ ] `docs-drift-detect.sh` 运行时间 ≤ 30 秒（grep + Python parse，CI 可接受）
- [ ] `docs-drift-patch.py` 运行时间 ≤ 10 秒
- [ ] CI 总耗时（含 install + check）≤ 5 分钟

### 5.3 长期价值验收（Phase 3 后）

- [ ] Sprint 27+ 的 defect-truth-table 漂移条目 ≤ 3 处（vs v1.1.2 前的 12 处）
- [ ] Metis/Oracle 后续评审不再重复发现"文档 vs 代码"漂移
- [ ] docs-drift-detect 工具被复用至其他架构文档（`defect-fix-roadmap.md`、`batch-N-ship-gate.md`）

---

## 六、风险与缓解

| 风险 | 等级 | 缓解策略 |
|------|------|---------|
| 自动 patch 误改语义 | 🟠 中 | Phase 1 仅警告 + 人工 review；Phase 3 引入 dry-run 模式 |
| CI 误报导致噪音 | 🟢 低 | `threshold=3` 起步；Sprint 24 收集误报 case 后调优 |
| 检测脚本维护负担 | 🟢 低 | 复用 `tools/code-review-graph` 和 `tools/adr_relationships.py` 基础设施 |
| 与 `tools/docs_drift_audit.py` 已有功能重叠 | 🟡 中-低 | 先复用现有脚本（v1.1 已知其存在），无重叠则跳过自建 |

---

## 七、关联文档

- **触发文档**：`docs/architecture/defect-truth-table-2026-08.md`（v1.1.2，§六.6 v1.1.2 修订说明）
- **现有工具**：`tools/docs_drift_audit.py`（ADR drift 检测；可能可复用）
- **现有脚本**：`scripts/sprint-closeout.sh`（Step 8 文档交叉检查）
- **现有 CI**：`.github/workflows/`（可能已有 drift 检查 workflow）

---

## 八、关联 ADR

- **ADR-0019**（IInteractionBus MVP）：文档架构层
- **ADR-0050**（Phase 6 战略评估）：Candidate B 战略评估中提到"工具链完善"

---

## 九、决策项

| # | 决策项 | 状态 | 建议 |
|---|--------|------|------|
| D1 | 是否在 Sprint 24 启动 B.2？ | 待 Sprint Planning 决定 | **建议启动**（v1.1.2 暴露痛点明确） |
| D2 | Phase 3 强制门控的阈值（drift ≥ 5）是否合理？ | 待 Sprint 25 评审 | **建议起步阈值 5**，后续根据误报率调优 |
| D3 | 是否复用 `tools/docs_drift_audit.py`？ | 待 Phase 1 调研 | **建议先调研**（避免重复造轮子） |

---

## 十、追踪

| Sprint | 状态 | 备注 |
|--------|------|------|
| 23（当前） | 🔍 Proposed | v1.1.2 ship 后等待评审 |
| 24 | ⏳ 待启动 | Phase 1（核心脚本） |
| 25 | ⏳ 待启动 | Phase 2（CI + 半自动门控） |
| 26+ | ⏳ 待启动 | Phase 3（强制门控） |

---

**审批与维护**：
- 提议：2026-08-24（v1.1.2 修订同步）
- 维护者：架构组 + Sprint 收官机制
- 审查频率：每 Sprint 收官
- 与 `scripts/sprint-closeout.sh` 集成