# tasks.md — adr-0068-appendix-a

## 实施任务列表

- [ ] **T1**: 验证 14 个 `📡` 主题的 evidence 引用（grep 确认 source file:line 存在 emit 调用）
- [ ] **T2**: 修改 `docs/adr/adr-0068-event-emission-contract.md` Appendix A，14 个 `📡` → `✅ registered`，附 evidence 引用
- [ ] **T3**: 检查 `docs/README.md` ADR-0068 行是否需要同步（保持 ADR-0068 状态一致）
- [ ] **T4**: 检查 `docs/active-status.md` W6 行是否需要标记完成
- [ ] **T5**: 运行 `tools/adr_lint.py`，确认退出码 0
- [ ] **T6**: 运行 `tools/docs_drift_audit.py`，确认 0 新增 DRIFT
- [ ] **T7**: 运行 `openspec validate --strict`，确认通过
- [ ] **T8**: 运行 `ctest --output-on-failure`，确认计数保持 147/147

## 验证清单

所有任务完成后，执行以下终验：
- `git diff --name-only` 只包含 `docs/adr/` + `docs/README.md`（如有）+ `docs/active-status.md`（如有）
- 无 `src/`、`include/`、`pdk/`、`tests/` 路径
- 所有 gate 命令退出码 0

## 预估时间

总计 ~2-4h（与路线图 W6 估时一致）
