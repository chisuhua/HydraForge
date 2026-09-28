# Oracle SHIP-with-fixes Review Prompt Template (L2 finalization T2+T3)

## 触发条件
worker bg_93bdeeb2 完成 + 主会话 critical self-examination 通过

## Verdict 选项
- **SHIP** — 0 Critical + 0 Major, 直接 merge
- **SHIP-with-fixes** — N Major + M Minor, 主会话 apply 修复后 merge
- **BLOCK** — ≥1 Critical, 主会话退回 worker 重做

## Oracle 复评 prompt (待 worker commit 完成后填实际 commit hash)

```
你是 Oracle, 对 OpenSpec change `2026-09-26-l2-evolution-finalization` 的 T2 + T3 实施进行 SHIP-with-fixes 后台复评。

**Commits to review**:
- T2 commit: <T2_COMMIT_HASH> (phase3_mutation real wiring)
- T3 commit: <T3_COMMIT_HASH> (phase4_reload_rerun real wiring)
- Branch: feat/l2-evolution-finalization
- Worktree: /workspace/project/wt-l2-finalization-t2t3/

**Review focus** (按严重度优先):

1. **Gate semantics correctness** (T2 是 highest risk 6 SP):
   - MutationGateContext 6 fields 全部填充?
   - Gate 0/1/2/2.5/3 sequence 正确?
   - apply_harness_mutation persist-before-apply 不变量保持?
   - 失败零状态变更 (per Pattern #11 G4 case study)

2. **Hermetic HOME ordering** (per Pattern #10 / AGENTS.md §Reverse Indicator Rule):
   - IGenomeRegistry 创建在 MutationGateContext 之前?
   - 测试用 hermetic HOME fixture (隔离 HOME + unsetenv)?
   - 不污染 host 持久状态?

3. **chain-link semantics** (per Oracle M6 finding):
   - T3 reload == mutation.new_version (不是 baseline)?
   - VersionPairDiff 正确连接?

4. **IEvaluator API contract** (per Oracle C3):
   - compare() 返回 int (不是 struct)?
   - verdict 映射 {Attributed, Insufficient} (Confounded unreachable)?

5. **IDistillationWriter API** (per Oracle M2):
   - make_file_writer(output_dir, agent_id) factory?
   - write_record(DistillationRecord) 正确?

6. **Reverse Indicator 5-field block**:
   - new_up / old_down / failure_traces / ablation / context_ids 全部?
   - drop_ratio ≤ 5% (R8.1 红线)?

7. **Day-5 4-file integrity**:
   - git ls-files openspec/changes/2026-09-26-l2-evolution-finalization/ 6 文件完整?
   - .openspec.yaml + proposal + design + tasks + specs/* 都在?

8. **TDD 5-step evidence**:
   - 每 task commit message 含 "Write failing test" / "Verify fail" / "Implement" / "Verify pass" 步骤证据?
   - ctest -L l2-evolution 输出 ≥9 (含新 tests)?
   - 任何 NEW test 都实际跑过 (不只信 worker "all pass" 声明)?

9. **AGENTS.md §模式 #11 教训**:
   - worker final-report 不可信 (主会话已 git show 验证)
   - 不 amend baseline (每 commit 都是新 atomic commit)
   - SIGPIPE / waitpid / cv 通知 一致

**Required output**:
- Verdict: SHIP / SHIP-with-fixes / BLOCK
- Critical 列表 (按严重度排序, 含 commit:line 引用)
- Major 列表
- Minor 列表
- 推荐 fixes (具体改动建议, 不只是描述)

**Run these commands first** (不靠 worker 自报):
1. cd /workspace/project/wt-l2-finalization-t2t3/
2. git show --stat <T2_COMMIT> 看真实 diff
3. git show --stat <T3_COMMIT>
4. cd build && ctest -L l2-evolution -V 2>&1 | tail -30
5. git ls-files openspec/changes/2026-09-26-l2-evolution-finalization/
6. bash -n <any_new_script>.sh
7. grep -n "TODO\|FIXME\|XXX\|HACK" <new_files>

输出格式:
```
VERDICT: SHIP / SHIP-with-fixes / BLOCK
Critical: [N items, each with file:line]
Major: [N items, each with file:line]
Minor: [N items, each with file:line]
Recommended fixes: [list, sorted by severity]
```

duration: 30-60 min
```

## 历史 SHIP-with-fixes verdict 模板参考
- bg_8237a316 (G1 ship-with-fixes): 0 Critical + 2 Major + 4 Minor + 1 D3 deviation
- bg_ef5a0ca4 (G3 ship-with-fixes): 0 Critical + 1 Major + 3 Minor
- bg_e4eec567 (G4 ship-with-fixes): 3 Major + 1 Minor
- bg_f2b41e215 (Phase H ship-with-fixes): 1 Major + 2 Minor
- bg_2b41e215 (DECISION A ship): 0 Critical + 0 Major + 2 Minor

## 主会话 apply fixes 流程 (per Pattern #4)
1. 不 amend T2/T3 baseline commits
2. 新建 1 atomic commit "fix(L2-finalization): Oracle SHIP-with-fixes applied"
3. 包含所有 Critical + Major fixes
4. 重新派 Oracle 复评 (continuation session)
5. 拿 APPROVE 才能 merge