# Tasks: Phase 4.5 — Implementation Scope Audit (C9)

> **STATUS: ACTIVE** 🟡
> **关联 proposal**: `proposal.md`
> **关联 spec**: `specs/phase4-5-impl-scope-audit/spec.md`
> **前置依赖**: C8 ship ✅ (2026-07-03)
> **预估工时**: 2-3 天
> **最后更新**: 2026-07-03

---

## 1. 数据收集 — 跑 drift 工具 + 人工验证

- [ ] 1.1 跑 `python3 tools/docs_drift_audit.py` 获取 11 项 DRIFT 列表
- [ ] 1.2 对每个 DRIFT 项执行 `grep -r "<class_name>" src/ include/ pdk/ tests/` 找实际位置
- [ ] 1.3 按 Shipped / Evolved / Deferred 三类对每个缺失类分类
- [ ] 1.4 输出 11 个 ADR 的 audit 表 (`docs/adr/_audit/adr-drift-analysis-2026-07-03.md` 临时文件)

---

## 2. 11 个 ADR impl-scope audit 文档创建

为每个 ADR 创建 `docs/adr/adr-XXXX-*-impl-scope.md` 文档, 包含:

```markdown
# ADR-XXXX Implementation Scope Audit

> **生成时间**: 2026-07-03
> **基础**: tools/docs_drift_audit.py 报告
> **状态**: ✅ Approved / 🟡 Partial (依据 audit 结果调整)

## 原始描述 vs 实际实施

| ADR 描述的类 | 实际状态 | 实际位置 (如有) | 备注 |
|--------------|---------|----------------|------|
| ClassA | ✅ Shipped | src/foo/bar.h:10 | 命名一致 |
| ClassB | 🔁 Evolved | src/baz/qux.h:5 | 被 CreatorFunc 替代 |
| ClassC | 📅 Deferred | — | Phase 5 阶段 1 任务 |

## 决策

- ADR 状态: ✅ Approved (X/Y 类已 ship, K 个 deferred 不影响核心契约)
- (或 🟡 Partial, 视情况)

## 后续行动

- (如有 deferred 类) 列入 Phase 5 backlog
- (如有 evolved 类) 更新 ADR 主文档的演进说明
```

- [ ] 2.1 `docs/adr/adr-0001-illm-provider-streaming-interface-impl-scope.md`
- [ ] 2.2 `docs/adr/adr-0003-dslengine-thread-safety-impl-scope.md`
- [ ] 2.3 `docs/adr/adr-0004-toolregistry-security-impl-scope.md`
- [ ] 2.4 `docs/adr/adr-0005-llm-backend-config-factory-impl-scope.md`
- [ ] 2.5 `docs/adr/adr-0007-context-compression-impl-scope.md`
- [ ] 2.6 `docs/adr/adr-0008-structured-context-impl-scope.md`
- [ ] 2.7 `docs/adr/adr-0019-iinteraction-bus-mvp-impl-scope.md`
- [ ] 2.8 `docs/adr/adr-0020-thread-model-isolation-impl-scope.md`
- [ ] 2.9 `docs/adr/adr-0022-plugin-loading-impl-scope.md`
- [ ] 2.10 `docs/adr/adr-0023-tool-result-standard-impl-scope.md`
- [ ] 2.11 `docs/adr/adr-0033-session-hierarchy-impl-scope.md`

---

## 3. ADR 状态校准

- [ ] 3.1 更新 `docs/README.md` ADR 状态表格 (与 impl-scope audit 一致)
- [ ] 3.2 重跑 `python3 tools/adr_relationships.py` 重新生成 `docs/adr-management/relationships.md`
- [ ] 3.3 如状态有变化, 更新各 ADR 主文档的 `## 状态` 章节

---

## 4. 文档同步

- [ ] 4.1 `docs/roadmap-status.md` §一 添加 Phase 4.5 → Phase 5 过渡说明
- [ ] 4.2 `docs/roadmap-status.md` §四 实施日志追加 C9 ship 记录
- [ ] 4.3 `AGENTS.md` § Recent Changes 追加 C9 ship 记录

---

## 5. 验证

- [ ] 5.1 `python3 tools/docs_drift_audit.py` 输出 `0 DRIFT items` (原本 11)
- [ ] 5.2 `python3 tools/adr_lint.py` exit 0
- [ ] 5.3 `python3 tools/adr_relationships.py` 成功生成
- [ ] 5.4 `ctest --output-on-failure` ≥ 61/61 PASS (零回归, 纯文档)
- [ ] 5.5 `openspec validate 2026-07-03-phase4-5-impl-scope-audit` exit 0
- [ ] 5.6 `git status` clean (working tree 仅预期文件)

---

## 6. 同步与归档

- [ ] 6.1 提交 + push 所有修改
- [ ] 6.2 `openspec archive 2026-07-03-phase4-5-impl-scope-audit`
- [ ] 6.3 写 master plan `2026-07-XX-phase5-self-bootstrapping.md` 草稿 (预备, 不在本 change)

---

## 验证检查清单 (C9 ship gate)

- [ ] 1. 11 个 `*-impl-scope.md` 文档全部创建
- [ ] 2. `docs_drift_audit.py` 0 drift
- [ ] 3. `adr_lint.py` exit 0
- [ ] 4. `docs/README.md` ADR 表格与 audit 一致
- [ ] 5. `adr_relationships.md` 已重新生成
- [ ] 6. `docs/roadmap-status.md` 同步
- [ ] 7. ctest 61/61 全绿 (零回归)
- [ ] 8. openspec validate exit 0
- [ ] 9. `git status` clean
- [ ] 10. change 已 archive
