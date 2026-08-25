# Tasks — capability-application-map v1.3 drift 修复

> **估时总工**: ~45 分钟（计划）→ 实际含 4 项扩展修复约 55 分钟
> **预期 commit 数**: 1 (`docs(cap-map): v1.3.1 drift fix`)

## 1. Pre-flight Verification (Setup)
- [x] 1.1 `git status --short` 干净
- [x] 1.2 目标文件存在性 (`capability-application-map-2026-08.md` + 2 ADR)
- [x] 1.3 archive 路径 (T14/T16)
- [x] 1.4 ctest baseline 185 tests
- [x] 1.5 ADR-0071/0074 状态字段

## 2. Phase A — P0 严重 Drift 修复

### drift-1: capability-map §七/§八 章节顺序互换
- [x] 2.1 读 line 430-460 (§八 start)
- [x] 2.2 在 line 432 后插入 §七 内容 (从 §六结束 → §七 → §八)
- [x] 2.3 删除原 §七 block (line 583-612)
- [x] 2.4 grep 验证: `^##` 顺序 一/二/三/四/五/六/七/八
- [x] 2.5 grep Oracle session 引用 ≥ 7

### drift-2: §六.6 章节位置 + 路径修复
- [x] 2.6 移动 §六.6 (§六.5 之后 via Edit)
- [x] 2.7 §六.5 line 421 路径修正: 加 `archive/` + `-2026-08-25-` 前缀
- [x] 2.8 验证 §六章节完整性
- [x] 2.9 archive 路径验证

### drift-3: §二 词汇表补充 ✅ Closed
- [x] 2.10 Edit §二 line 90-91 + 91 (性质标记合并)
- [x] 2.11 grep ✅ Closed ≥ 6

### drift-4: §二 标题措辞更新
- [x] 2.12 Edit line 87 标题
- [x] 2.13 grep 标题不含"未 ship"

### drift-5: §八 闭环 1 + 闭环 2 G15/G10/G12 同步
- [x] 2.14-2.15 Edit 闭环 1 G15 row 7 (line 463 → 修复)
- [x] 2.16-2.17 grep 验证
- [x] 2.14b-2.17b Edit 闭环 1 rows 1-3 (G12 + ADR-0074 + G10) 同步
- [x] 2.14c Edit 闭环 2 row 3 G10 同步

### drift-6: §八.5 重复排期块删除
- [x] 2.18-2.20 删除 line 538-547 (含"下个 Sprint" 块)

### drift-7: ADR-0071/0074 footer 同步
- [x] 2.21-2.22 ADR-0071 line 5 + line 689
- [x] 2.23-2.24 ADR-0074 line 5 + line 595
- [x] 2.25-2.26 grep 验证 Promotion 评审通过 (4 处)

## 3. Phase B — P1 中等 Drift 修复

### drift-8: ctest 计数 184 → 185
- [x] 3.1-3.2 Edit §六.1.2 line 325 + §六.3 line 385
- [x] 3.3-3.4 grep 验证

### drift-9: §一 覆盖范围 + 标题
- [x] 3.5 Edit §一 line 31 + line 28

### drift-10: §三 "零工程"段计数
- [x] 3.7-3.8 Edit §三 line 124 + 131

## 4. Phase C — P2 轻微 Drift 修复

### drift-11: §二 词汇对齐 (与 drift-3 同 Edit 合并)
- [x] 4.1-4.2 已在 Phase A 完成

### drift-12: §一 L4 表头加注
- [x] 4.3-4.4 Edit §一 L4 表头

## 5. Verification (10 gate)
- [x] 5.1 `openspec validate --strict`: ✅ valid
- [x] 5.2 `tools/adr_lint.py`: ✅ 80 ADR PASS
- [x] 5.3 `tools/docs_drift_audit.py`: ✅ 0 DRIFT 增加
- [x] 5.4 `grep "184/184"`: capability-map 0 hits
- [x] 5.5 `grep "下个 Sprint|新 ADR 需求"`: 仅 G11 1 处合法
- [x] 5.6 章节顺序 ✓
- [x] 5.7 ctest 185/185 PASS (parallel)
- [x] 5.8 src/include/pdk/tests/examples 0 变化 (Non-Goals)
- [x] 5.9 archive/ 0 变化
- [x] 5.10 git stat ~70+/84-

## 6. Commit
- [x] 6.1-6.4 `git commit` 1 commit (commit hash 854f13b 后被 reset 替换)

## 7. Post-ship Cleanup
- [ ] 7.1 `openspec archive 2026-08-25-cap-map-v1-3-drift-fix --yes`
- [ ] 7.2 更新 AGENTS.md Recent Changes
- [ ] 7.3 `openspec list` 验证
- [ ] 7.4 `scripts/sprint-closeout.sh` Step 8 交叉检查
