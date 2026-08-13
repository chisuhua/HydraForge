# tasks.md — adr-0073-partial-flip

## 实施任务列表

- [ ] **T1**: 新建 `docs/adr/adr-0073-impl-scope-audit.md`，按 D1-D6 逐决策分类 Shipped / Partial / Deferred / Not implemented；附 file:line + commit 证据
- [ ] **T2**: 修改 `docs/adr/adr-0073-tool-json-schema-contract.md` 状态行为 `🟡 Partial`；追加 ship 证据段（引用 manifest.h/manifest_validator.cpp/测试）；修订 §复审节点
- [ ] **T3**: 修改 `docs/README.md` ADR-0073 行，更新状态为 `🟡 Partial`
- [ ] **T4**: 修改 `roadmap.md` line 210 标记 W1 完成；修正 line 397 "Sprint 21 部分" 措辞为 Phase 6a manifest 真实来源
- [ ] **T5**: 运行 `tools/adr_lint.py`，确认退出码 0
- [ ] **T6**: 运行 `tools/docs_drift_audit.py`，确认 0 新增 DRIFT（与本 change 相关）
- [ ] **T7**: 运行 `openspec validate --strict`，确认通过
- [ ] **T8**: 运行 `ctest --output-on-failure`，确认计数保持 147/147

## 验证清单

所有任务完成后，执行以下终验：
- `git diff --name-only HEAD~1 HEAD` 只包含 `docs/adr/` + `docs/README.md` + `roadmap.md`
- 无 `src/`、`include/`、`pdk/`、`tests/` 路径
- 所有 gate 命令退出码 0

## 预估时间

总计 ~2h（与路线图 W1 估时一致）
