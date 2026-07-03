# Tasks: Phase 4.5 — MVP Cleanup

> **STATUS: ACTIVE** 🟡
> **关联 design**: `design.md` (5 Decisions)
> **关联 proposal**: `proposal.md`
> **前置依赖**: C3 + C4 + C5 + C6 全部 ship ✅
> **预估工时**: 1 天
> **最后更新**: 2026-07-03

---

## 1. SimpleCognitiveOrchestrator @internal 标记

- [ ] 1.1 `include/agenticdsl/cognitive/simple_orchestrator.h` 行 1-11: 重写文件头注释
  - 删除 "MVP 阶段：仅单轮，标 TODO(mvp)；多轮 + 状态机留待后续 Phase 1" (行 5)
  - 新增 "@internal Phase 0 单轮 ReAct 编排器 — Phase 1+ 由 CognitiveWorker + ReactLoop 封装"
  - 更新最后修改日期为 2026-07-03
- [ ] 1.2 `src/modules/cognitive/simple_orchestrator.cpp` 行 109: 替换 TODO(mvp) 注释
  - "TODO(mvp): 多轮 + 真实 prompt 模板留待 Phase 1"
  - → "@internal: 多轮循环由 CognitiveWorker 在上层管理；prompt 模板已迁移至 llm_config.json"
  - 更新最后修改日期为 2026-07-03

---

## 2. examples/ 目录梳理

- [ ] 2.1 新建 `examples/README.md` — 8 个 entry 的用途说明
  - 6 个 C++ 示例 (编译运行, `-DAGENTICDSL_BUILD_EXAMPLES=ON`)
  - 2 个参考文档 (`.md`/`.agent.md`, 非构建目标)
- [ ] 2.2 确认 `examples/CMakeLists.txt` 中 `AGENTICDSL_BUILD_EXAMPLES` flag 覆盖所有 6 个 C++ 示例

---

## 3. 文档同步

- [ ] 3.1 `docs/roadmap-status.md` §一:
  - Phase 4 行: `0% ░░░░░░░░░░` → `100% ██████████ ✅ 已完成 (C7, 2026-07-02)`
  - Phase 4.5 行: `0% ░░░░░░░░░░` → `100% ██████████ ✅ 已完成 (C8, 2026-07-03)`
  - §四 实施日志: 追加 Phase 4.5 ship 记录
- [ ] 3.2 `AGENTS.md` § Recent Changes:
  - 追加 Phase 4.5 ship 记录
  - 内容: SimpleCognitiveOrchestrator @internal 标记 + TODO(mvp) 清理 + examples/ 梳理
  - 附注: 52/52 ctest 零回归

---

## 4. 验证

- [ ] 4.1 `ctest --output-on-failure` ≥ 52/52 PASS (零回归)
- [ ] 4.2 `grep -r "TODO(mvp)" src/ include/` 0 results (除 C8 自身 docs)
- [ ] 4.3 `openspec validate 2026-06-26-phase-4-5-mvp-cleanup` exit 0
- [ ] 4.4 `lsp_diagnostics` on changed files: 0 new errors

---

## 5. 同步与归档

- [ ] 5.1 `openspec archive 2026-06-26-phase-4-5-mvp-cleanup --yes`
- [ ] 5.2 更新 master plan C8 行: ⚪ placeholder → ✅ archived (2026-07-03)
- [ ] 5.3 Phase 5 启动评估 (远期, 非阻塞 C8 ship gate)

---

## 验证检查清单 (C8 ship gate)

- [ ] 1. SimpleCognitiveOrchestrator 标记为 @internal (2 文件)
- [ ] 2. TODO(mvp) 全部移除 (src/ 和 include/ 中 0 matches)
- [ ] 3. examples/README.md 已创建
- [ ] 4. docs/roadmap-status.md Phase 4 + 4.5 → 100%
- [ ] 5. AGENTS.md Recent Changes 已追加
- [ ] 6. ctest 52/52 全绿 (零回归 — 仅注释+文档变更)
- [ ] 7. `openspec validate` exit 0
- [ ] 8. `git status` clean (仅预期文件变更)
- [ ] 9. Phase 0-4.5 全部 100%
- [ ] 10. master plan C8 状态更新
