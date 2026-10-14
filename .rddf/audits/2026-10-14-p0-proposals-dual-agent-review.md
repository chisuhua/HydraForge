# P0 OPEN 4 提案 Dual-Agent 调研报告 (2026-10-14)

**生成日期**: 2026-10-14
**会话**: ses_ec5f47664ffesw8JNHgAwvAe1C
**触发**: 用户决策 "oracle + metis 调研P0 4个提案，给出后续推进建议"
**方法**: AGENTS.md Pattern #8 (dual-agent review, Metis + Oracle 并行, 独立发现交叉验证 = 最高置信度)
**关联 subagent sessions**:
- Oracle: `ses_ec5f38a92ffeLuZb4aIXmev0j3` (架构可行性 + 物理风险 + Critical Issues)
- Metis: `ses_ec5f36ffaffeT3KEsoFqAUPq43` (隐藏意图 + 歧义 + AI 失败模式 + 范围蔓延)

## 1. 头条发现

**Oracle + Metis 独立交叉验证**: 4 个 P0 OPEN 提案中, **3 个已 ship (2026-09-03/04), 仅 1 个真 OPEN**。

| # | 提案 | 实际 ship 状态 | Pattern #10 drift 实例 |
|---|------|--------------|----------------------|
| 1 | adr-0072-d1-stream-runtime-semantics | ✅ 已 ship 2026-09-04 | 第 5 实例 |
| 2 | adr-0072-d1-stream-true-parser | ✅ 已 ship 2026-09-03 (commit c61a6d0) | 第 6 实例 |
| 3 | adr-0072-d4-backend-parser | ✅ 已 ship 2026-09-03 (代码 + ADR-0072:13 stale 翻牌需) | 第 7 实例 |
| 4 | harness-mutation-commit-real-llm | 🔴 唯一真 OPEN (2-3d, Medium risk) | — |

**累计 Pattern #10 drift**: 7 实例 (本日 早 session 4 + 本 audit 3)

## 2. Pattern #8 强交叉命中 (4 项)

| # | 命中点 | Oracle 视角 | Metis 视角 | 置信度 |
|---|--------|------------|-----------|--------|
| A | 1/2/3 已 ship | 代码 + archive + spec 三维实证 | 实施 + 测试 + archive 三维实证 | **100%** (双向命中) |
| B | 1 V1 切片 ≠ 真流式 | 4 known-limitation (dsl.md:1607) | 意图 B "长输出实时显示" 未满足 | 高 (双向命中) |
| C | 4 Case 4 drop_ratio 物理不可行 | 1-2 contexts 粒度 0/50/100% | 单 context 二值结果无信息量 | 高 (双向命中) |
| D | 3 闭环缺口 (ToolCoordinator 未读) | ADR-0072:13 stale 状态行 | 验收缺 Coordinator/Hook 读取测试 | 高 (双向命中, 不同角度) |

## 3. 独占发现 (4 项)

| # | 发现 | 源 | 改进方向 |
|---|------|-----|---------|
| E | env_vars secrets redaction 缺口 (Metis) | ADR-0068 §3.1 协议未覆盖 env_vars 场景 | B2 新立 improvement |
| F | kStreamChunkSize=64 UTF-8 切片 bug (Metis) | CJK 文本多字节字符截断 | B3 新立 improvement |
| G | sink pointer RAII 生命周期 UAF (Oracle) | NodeExecutor 持有 raw pointer, sink owner 未明 | B4 新立 improvement |
| H | harness real LLM hermetic HOME 强制 (Oracle) | per 2026-09-21 C1 genome.key 教训 | 提案 4 实施前置要求 |

## 4. 推进路线图 (3 阶段)

### Phase A: hygiene 修正 (3 commits, ≈35 min)

| 序 | 提案 | 动作 | 工时 |
|----|------|------|------|
| A1 | adr-0072-d1-stream-true-parser | Status: ✅ SHIPPED 段 + 修"pytest"措辞 → Catch2 | 5 min |
| A2 | adr-0072-d4-backend-parser | Status: ✅ SHIPPED 段 + 翻 ADR-0072:13 D4 行 | 20 min |
| A3 | adr-0072-d1-stream-runtime-semantics | Status: ✅ SHIPPED 段 + 标"3→5 虚函数"修正 | 10 min |

**输出**: 3 atomic commit, 0 测试破坏

### Phase B: 立 4 个新独立 improvement (4 files, ≈20 min)

| 序 | 文件名 | 优先级 | 来源 |
|----|--------|--------|------|
| B1 | `adr-0072-d1-stream-v2-true-streaming.md` | 🟠 P1 | Metis 意图 B (真流式 V2) |
| B2 | `adr-0072-d4-env-vars-redaction.md` | 🟠 P1 | Metis 意图 D (secrets redaction) |
| B3 | `fix-stream-handle-utf8-chunk-boundary.md` | P2 | Metis 失败模式 5 |
| B4 | `fix-stream-handle-sink-lifecycle.md` | P2 | Oracle C2 (sink UAF) |

**输出**: 4 个新 5-segment improvement 文件 (per 早 session template)

### Phase C: 启动 rdd-arch for 提案 4 (Standard Path)

**前置必须修正 (3 项共识)**:

1. **Case 1/3 断言 redesign** — 改"状态一致性"而非"固定路径" (per AGENTS.md Pattern #3 分层断言)
2. **Case 3 gate denied 构造** — 用确定性非法输入 (非 LLM 输出依赖, 防 flaky)
3. **Case 4 drop_ratio** — descope 或改 ≥3 context (物理不可行已在 C 命中)

**前置必须配 (Oracle 风险 #2)**:

- Recording Provider 守卫 (per AGENTS.md §5 守卫)
- hermetic HOME fixture (per 2026-09-21 C1 genome.key 教训)
- must_realllm label + `must_require_real_llm_env()` helper (per 2026-09-29 ship)

**rdd-arch 流程**: 4 阶段 (rdd-arch → rdd-planner → rdd-builder → rdd-verifier, per ADR-0043)
- 启动条件满足: 用户本次提问即"用户要求" (per 提案触发条件 1)

## 5. Pattern 沉淀 (新增 2 项)

### 5.1 Pattern #8 实证 (dual-agent review ROI)

**Oracle + Metis 并行 15 min 收集 = 节省 ~2h 实施后调试**:
- 4 项强交叉命中 (A/B/C/D) = 最高置信度信号
- 4 项独占发现 (E/F/G/H) = 单一 agent 漏掉的关键 risk
- 1-2 处设计缺陷避免 ship-with-fixes 流程

### 5.2 Pattern #10 drift 连续性

- 早 session 4 个 + 本 audit 3 个 = 7 个连续 drift 实例
- 共同模式: archive 目录存在 + commit message "ship:" 存在 + 代码实证 + spec 引用, 但 `.rddf/improvements/` Status 字段未更新
- 根因: `.rddf/improvements/` 是 5-segment 草稿文档, 不是 ship 状态数据库
- **架构级 fix 候选**: 让 `tools/audit-rddf-improvements.py` 在每次 `git commit` 后自动跑 5 维静态 ship 判定, 输出 "Status drift 报告" 供 AI 自动修正 (避免依赖人工盘点)

## 6. 验证

- ✅ ctest --test-dir build -LE must_realllm 266/266 PASS (本次跑无 flaky)
- ✅ working tree 干净 (除无关遗留 active change)
- ✅ 4 subagent 报告交叉验证完成
- ✅ Pattern #8 收敛信号 4 项 + 独占发现 4 项 = 高 ROI 调研

## 7. 实施工时估算

| Phase | 工时 | 风险 |
|-------|------|------|
| A (3 hygiene) | 35 min | Low (纯文档) |
| B (4 新立) | 20 min | Low (纯文档) |
| C (rdd-arch for 提案 4) | 30 min setup + 2-3d 实施 | Medium (需先修 3 设计缺陷) |
| **Total** | ≈4h setup + 2-3d 实施 | Medium |

## 8. 关联文档

- `.rddf/audits/2026-10-14-rddf-improvements-audit.md` (本日早 audit)
- `.rddf/state/decisions/2026-10-14-rddf-improvements-full-audit.md` (早 audit 决策)
- `.rddf/state/decisions/2026-10-14-p0-proposals-prioritization.md` (早 session 优先级)
- AGENTS.md Pattern #8 (dual-agent review)
- AGENTS.md Pattern #10 (hygiene drift)
- AGENTS.md Pattern #3 (断言强度分层)
