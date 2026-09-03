# control-plane-eval-c2-alignment

**优先级**: P0 | **来源**: governance（per Oracle 推荐 + roadmap Q2b 决策树放松口径）
**阶段**: post-6c | **分类**: governance
**类型**: governance
**主题**: scripts/control-plane-eval.py C2 脚本与决策树对齐;--relaxed 模式;保住复评机制信号价值

## 架构依据

per 修订后 `roadmap.md` Q2b 决策树 + Oracle session `ses_f9ab25dcfffetx4J5UFA7JYBKV` 优先级排序：

**当前问题**：
- `scripts/control-plane-eval.py` 决策树把 C2 (Solo Dev 容量) 当**硬阻塞**条件（per `evaluate_control_plane` L415: `blocking = [n for n in ("C1", "C2", "C5", "C6") if statuses.get(n) == STATUS_FAIL]`）
- `roadmap.md` Q2b 决策树（per ADR-0076 Phase 7 启动条件评估）口径：**"C1+C5 转 PASS 且 C2 不下降"即可复评**
- 两套判定并存 = Sprint 25+ U4 (AgentForge 第 2 agent) ship 后，C1 ✅ + C5 ❌（baseline 未重测）+ C2 ❌（永远 1 人） → 脚本仍输出 "DescopeOrContinue"，**U4 的 8h 代码投入在治理层面不可见**
- 每 Sprint 收官重跑都会输出"明知故犯的 FAIL" → alarm fatigue → 真 FAIL 被忽略

**修复方向**：
- 新增 `--relaxed` CLI flag
- `--relaxed` 模式下：C2 FAIL 从"阻塞"降级为"非阻塞"（但仍 FAIL 真实状态，不掩盖）
- 默认模式（无 `--relaxed`）：行为不变（向后兼容，sprint-closeout 自动跑不破坏）
- 与 `--override C2=true` 区别：override 是"假装 C2 PASS"，relaxed 是"C2 仍 FAIL 但不阻塞决策"

## 范围

- **In Scope**:
  - `scripts/control-plane-eval.py` — 新增 `--relaxed` argparse flag
  - `scripts/control-plane-eval.py:evaluate_control_plane` — relaxed 参数支持
  - `scripts/control-plane-eval.py:detect_c2_solo_dev_capacity` — 输出 details 含 "(relaxed mode: not blocking)" 标记
  - `tests/test_control_plane_eval_relaxed.py`（新建）— 4 类测试：
    - 默认模式 C2 FAIL → DescopeOrContinue（行为保持）
    - `--relaxed` 模式 C2 FAIL → Conditional（新增）
    - `--relaxed` 模式 C1 FAIL + C2 FAIL → DescopeOrContinue（C1 仍阻塞）
    - `--relaxed` 模式 C2 PASS → 无影响（C2 未失败，无需降级）
  - `docs/audits/2026-09-02-control-plane-eval-v1.md` 更新 — 加 `--relaxed` 模式段

- **Out of Scope**:
  - C1/C5/C6 阻塞逻辑改动（仅 C2 涉及决策树放松口径）
  - `--override` 机制改动（保留作为应急 override 通道）
  - C2 detection 逻辑改动（仍读 active-status.md）

## 关键场景

1. **默认模式行为不变**:
   - Given: 默认调用（无 `--relaxed`）+ C2 FAIL
   - When: 脚本运行
   - Then: 输出 "DescopeOrContinue" + exit 1（与原行为一致）

2. **relaxed 模式 C2 不再阻塞**:
   - Given: `--relaxed` 模式 + C2 FAIL + 其他条件 PASS
   - When: 脚本运行
   - Then: 输出 "Conditional" + C2 行 details 含 "(relaxed mode: not blocking)" + exit 1（Conditional 也 exit 1）

3. **relaxed 模式 C1 仍阻塞**:
   - Given: `--relaxed` 模式 + C1 FAIL + C2 FAIL
   - When: 脚本运行
   - Then: 输出 "DescopeOrContinue"（C1 仍阻塞决策树）

4. **relaxed 模式 C2 PASS 无影响**:
   - Given: `--relaxed` 模式 + C2 PASS
   - When: 脚本运行
   - Then: 与默认模式 C2 PASS 行为完全一致（无降级触发）

## 关键决策

1. **`--relaxed` 是 flag 不是 mode** — 不修改默认行为，纯 opt-in
2. **C2 状态仍 FAIL** — 不掩盖真实状态，仅从"blocking 集合"移除
3. **details 标记 "(relaxed mode: not blocking)"** — 让 reviewer 看清 C2 实际状态 + 决策依据
4. **exit code 保持 EXIT_FAIL (1)** — Conditional 仍 exit 1（不是真 PASS），避免 sprint-closeout 误判

## Why

`scripts/control-plane-eval.py` 是 Phase 7 启动条件复评的**唯一权威自动化**。当前硬阻塞逻辑导致：
- Sprint 25+ U4 ship 后，复评结果仍是 "DescopeOrContinue"（C1 ✅ 但 C2 ❌ 阻塞）
- Solo Dev 永久 1 人（外部约束，无解药）= C2 永远 FAIL = 复评永远 FAIL
- alarm fatigue → 真 C5 FAIL（baseline 退化）也会被忽略
- Phase 7 启动决策退化为"看数字不听信号"

`--relaxed` 模式是治理机制的诚实表达：**C2 不是技术障碍，是组织约束；治理机制应区分"代码可解锁"和"组织不可控"**。

## What Changes

- **修改** `scripts/control-plane-eval.py` argparse 新增 `--relaxed` flag
- **修改** `scripts/control-plane-eval.py:evaluate_control_plane` 函数签名 + 阻塞逻辑
- **修改** `scripts/control-plane-eval.py:detect_c2_solo_dev_capacity` 输出 details 加标记
- **新增** `tests/test_control_plane_eval_relaxed.py` 4 类测试
- **修改** `docs/audits/2026-09-02-control-plane-eval-v1.md` 加 `--relaxed` 段

## Acceptance

- [ ] 4 类 pytest PASS（默认保持 + relaxed 3 case）
- [ ] `python3 scripts/control-plane-eval.py --dry-run`（默认）输出与原行为一致
- [ ] `python3 scripts/control-plane-eval.py --dry-run --relaxed` 输出 "Conditional" 而非 "DescopeOrContinue"
- [ ] C2 行 details 含 "(relaxed mode: not blocking)" 标记
- [ ] C1 仍阻塞（--relaxed 不能绕过 C1）
- [ ] `docs/audits/2026-09-02-control-plane-eval-v1.md` 加 `--relaxed` 段
- [ ] `openspec validate --strict` exit 0
- [ ] Oracle review 5/5 PASS（行为保持 + relaxed 语义 + C1 仍阻塞 + 决策树一致 + 测试覆盖）
- [ ] zero 业务逻辑改动（仅 CLI flag + 决策树 1 处 + 测试）
