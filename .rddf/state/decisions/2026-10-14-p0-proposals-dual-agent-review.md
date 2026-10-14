# P0 OPEN 4 提案 Dual-Agent 调研决策登记 (2026-10-14)

**生成**: 2026-10-14 via 主会话 (ses_ec5f47664ffesw8JNHgAwvAe1C)
**触发**: 用户决策 "oracle + metis 调研P0 4个提案，给出后续推进建议"

## 1. 用户 4 项决策

### 决策 1: Phase A 范围
> 全 Hygiene 修正: 3 提案 (A1+A2+A3)

### 决策 2: Phase B 立 improvement
> 全 4 个新立 (B1 真流式 + B2 redaction + B3 UTF-8 + B4 sink UAF)

### 决策 3: Phase C 立项
> 启动 rdd-arch for 提案 4 (Standard Path)

### 决策 4: 报告文件交付
> 入 git-track (推荐)

## 2. 实施路径

**Step 1 (本 commit)**: 报告 + 决策登记 + AGENTS.md entry 段 — 1 atomic commit
**Step 2 (Phase A)**: 3 hygiene 修正 (A1+A2+A3) — 3 atomic commits
**Step 3 (Phase B)**: 4 新立 improvement (B1+B2+B3+B4) — 4 文件 + 1 批量 commit
**Step 4 (Phase C)**: 启动 rdd-arch for 提案 4 — 写 `.rddf/state/.arch-handoff.json`

## 3. Pattern #8 调研结论

**4 项强交叉命中 (置信度 100%)**:
- A: 1/2/3 已 ship (Oracle + Metis 独立识别)
- B: 1 V1 切片回放 ≠ 真流式 (双向命中)
- C: 4 Case 4 drop_ratio 物理不可行 (双向命中)
- D: 3 闭环缺口 (ToolCoordinator 未读) (双向命中)

**4 项独占发现**:
- E (Metis): env_vars secrets redaction
- F (Metis): UTF-8 切片 bug
- G (Oracle): sink pointer RAII 生命周期
- H (Oracle): hermetic HOME 强制

## 4. 关联 commit

- `7227e8d` (本日 audit 报告)
- `8a83e90 ~ 1fe848b` (本日 25 atomic hygiene commits)
- 本 commit (本 step 1)
- 后续 Phase A/B/C commits
