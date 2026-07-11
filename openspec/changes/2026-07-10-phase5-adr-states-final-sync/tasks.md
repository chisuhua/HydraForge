# Tasks: Phase 5 — ADR States Final Sync (C17)

> **STATUS: ACTIVE** 🟡
> **关联 proposal**: `proposal.md`
> **关联 spec**: `specs/adr-states-final-sync/spec.md`
> **前置依赖**: 无
> **预估工时**: 0.5-1 天 (Metis 审查后范围缩减 12 → 5 ADR)
> **最后更新**: 2026-07-10 (Metis 审查后修正)

---

## 1. P0 ADR 状态变更 (Day 1 上午, Metis 审查后确认可安全翻转)

对每个 P0 ADR 执行:
1. 找到当前 `## 状态` 行 (格式: `🔍 Proposed (YYYY-MM-DD ...)`)
2. 替换为: `✅ Approved (YYYY-MM-DD — <关联 change 名> ship)`
3. 追加 1 行 ship 证据段:
   ```
   > **实施依据**: `<change 名>` 已 ship + archived (commit hash), 验证: <ship gate 证据>
   ```
4. 提交 1 commit per ADR (便于 git log 追溯)

- [x] 1.1 ADR-0035 (`docs/adr/adr-0035-inference-engine-plugin-spec.md`) — C14 ship 证据 (per `openspec/changes/archive/phase5-llama-engine-plugin/`, ctest 65/65, 12 工具注册)
- [x] 1.2 ADR-0041 (`docs/adr/adr-0041-pluginloader-lifecycle-extension.md`) — C14 + C16 证据 (5 符号查找 + lifecycle 钩子)
- [x] 1.3 ADR-0043 (`docs/adr/adr-0043-pdk-tool-naming-convention.md`) — C13/C14 D3 证据 (D3 决策应用)
- [x] 1.4 ADR-0044 (`docs/adr/adr-0044-inference-plugin-security-model.md`) — C14 证据 (三层安全模型应用于推理插件工具)

---

## 2. P1 ADR 状态变更 (Day 1 上午)

- [x] 2.1 ADR-0040 (`docs/adr/adr-0040-inference-plugin-build-strategy.md`) — C14 证据 (Dual-Repo + SHARED 库策略已应用)

---

## 3. 7 个 ADR 排除原因文档化 (Day 1, Metis 审查要求)

> **目的**: 防止后续 Sprint 误判这 7 个 ADR 为"待翻转"而重复劳动

排除列表 (保持 `🔍 Proposed`):
- **ADR-0030 V2** — P1-P4 退出条件未满足 (P2 FleetOrchestrator 已 Oracle 延迟)
- **ADR-0037** — 纯规范, 因果排序机制零实施 (`grep` `causal_order`/`EventReorderBuffer`/`sequence_number` → 0 matches)
- **ADR-0038** — BatchingQueue 增量决议延迟至第二个推理 backend 出现时
- **ADR-0039** — ADR 规范的是 JSON 查询工具, C16 的 `available_models()` C++ 接口 ≠ JSON 工具
- **ADR-0042** — 主文档第 10 行硬性 banner "ADR 整体状态仍保持 🔍 Proposed"
- **ADR-0045** — 实施顺序 5 步仅 step 2 (ILLMProvider 包装层) 部分交付
- **ADR-0046** — 4 通道架构仅通道 ① 完成基础设施

- [x] 3.1 追加排除原因到 `docs/superpowers/plans/2026-07-10-phase5-remainder-adr-sync.md` §十一 Adjustment Log
- [x] 3.2 追加排除原因到 `docs/active-status.md` §一 或 §六 (顺延项)
- [x] 3.3 **不修改** 这 7 个 ADR 主文档 (保留 `🔍 Proposed` + 现有 banner)

---

## 4. 文档同步 (Day 1)

- [x] 4.1 更新 `docs/README.md` §adr/ 表格: 新增 5 行 (ADR-0035/0040/0041/0043/0044, 当前 0035-0046 范围未列出)
- [x] 4.2 更新 `docs/active-status.md` §一: ADR Approved 计数 `14 → 19` (+5), Partial 计数 `2 → 2` (不变)
- [x] 4.3 **不修改** master plan `2026-07-03-...` §一 Phase 2 行 (因 ADR-0030 V2 保持 Proposed)
- [x] 4.4 重跑 `python3 tools/adr_relationships.py` 生成 `docs/adr-management/relationships.md`
- [x] 4.5 AGENTS.md § Recent Changes 追加 C17 ship 记录 (2026-07-10)

---

## 5. 验证 (Day 1)

- [x] 5.1 `git grep "🔍 Proposed" docs/adr/` 计数: **12 → 7** (12 个 ADR 文件含 `🔍 Proposed`, 5 FLIP 后剩 7 个排除 ADR 文件; ADR-0031 Partial 不含 `🔍 Proposed`, 不计入)
- [x] 5.2 `python3 tools/adr_lint.py` exit 0
- [x] 5.3 `python3 tools/docs_drift_audit.py` 0 DRIFT items
- [x] 5.4 `git diff docs/README.md docs/active-status.md` 显示 5 行 README 新增 + Approved 计数 +5
- [x] 5.5 `openspec validate 2026-07-10-phase5-adr-states-final-sync` exit 0

---

## 6. 收尾 (Day 1)

- [x] 6.1 `git add . && git commit -m "docs(adr): C17 — 5 个 ADR 状态从 🔍 Proposed 同步至 ✅ Approved (Metis 审查后范围修正: 12 → 5)"` (主 commit)
- [x] 6.2 `openspec archive 2026-07-10-phase5-adr-states-final-sync --yes`
- [x] 6.3 验证: `openspec list` 显示 C17 已 archived + C18 active (待 C17 ship 后启动)
- [x] 6.4 通知用户 C17 ship + archived, 准备 C18 启动

---

## 备注

- 每个 ADR 状态变更独立 commit, 5 个翻转 ADR 共 5 commits
- 若 ADR 主文档含 §决策 / §增量决议 / §实施细节段, 不要触碰, 仅改 `## 状态` 行 + 追加 ship 证据
- ADR-0031 (Execution Policy) 不在本 change 范围, 保持 `🟡 Partial`
- 7 个排除 ADR (0030 V2/0037/0038/0039/0042/0045/0046) 不修改, 仅在 master plan/active-status.md 文档化排除原因
- 本 change 范围缩减自 12 → 5 ADR, 估时从 1-2 天 → 0.5-1 天 (Metis 审查成果)