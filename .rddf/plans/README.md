# .rddf/plans/ — Implementation Plans Index

> **最后更新**: 2026-08-29
> **Owner**: Solo Dev（Sisyphus）
> **治理**: `.rddf/plans/<change-name>-<phase>.md` 命名约定，Phase 0/1/2/3 分别独立文件
> **TDD 纪律**: 每个 plan 严格 TDD 5 步（Write failing test → Verify fail → Implement → Verify pass → Commit）

---

## 活跃 Plans（与 OpenSpec changes 对应）

| Change | Plan 文件 | Phase | 估时 | 状态 | 最近 ship |
|--------|----------|-------|------|------|----------|
| `capture-mode-and-distillation-writer-v1` | `capture-mode-and-distillation-writer-v1-phase-0-CORRECTED.md` (655 行, Oracle/Metis 9 项修正后版本) | Phase 0 | 0.5 sprint | ✅ **ship 2026-08-29** (commit `11d3515`) | 4 files / +267 lines / 4 cases PASS |
| `capture-mode-and-distillation-writer-v1` | `capture-mode-and-distillation-writer-v1-phase-1.md` (909 行) | Phase 1 | 0.5 sprint | 📋 **待启动** (Sprint 24 W2) | (Phase 0 ship 后启动) |
| `capture-mode-and-distillation-writer-v1` | (待创建) | Phase 2 | 0.3 sprint | 📋 待 Phase 1 ship 后创建 | — |
| `capture-mode-and-distillation-writer-v1` | (待创建) | Phase 3 | 0.2 sprint | 📋 待 Phase 2 ship 后创建 | — |

## 历史 Plans（已 ship + archived）

### `capture-mode-and-distillation-writer-v1` Phase 0（原始版，已废弃）

| 文件 | 状态 | 备注 |
|------|------|------|
| `capture-mode-and-distillation-writer-v1-phase-0.md` (425 行) | ⛔ **已废弃** | Oracle + Metis 双审查发现 9 项致命错误（reward_signal.h 路径错误 / 2 虚函数 vs ADR 3 虚函数 / 零编译覆盖等），由修正版 `phase-0-CORRECTED.md` 替代 |

### Phase 6c 系列

| Change | Plan 文件 | 状态 |
|--------|----------|------|
| `from-roadmap-phase-6c-execution-baseline` | `from-roadmap-phase-6c-execution-baseline.md` | ✅ ship 2026-08-18 |
| `from-roadmap-phase-6c-evidence-gate` | `from-roadmap-phase-6c-evidence-gate.md` | 📋 Wave 2 待启动 |
| `from-roadmap-phase-6c-execution-dsl` | (待创建) | 📋 Wave 3 待启动 |

### Phase 3-A / Phase 5

| Change | Plan 文件 | 状态 |
|--------|----------|------|
| `chat-async-io-cancellation-chain` | `chat-async-io-cancellation-chain.md` | ✅ ship 2026-08-09 |
| `chat-async-io-queue-infra` | `chat-async-io-queue-infra.md` | ✅ ship 2026-08-08 |
| `chat-slash-commands-migration` | `chat-slash-commands-migration.md` | (历史) |
| `chat-streaming-slash-tui` | `chat-streaming-slash-tui.md` | ✅ ship 2026-08-07 |
| `cli-args-cxxopts` | `cli-args-cxxopts.md` | (历史) |
| `fix-tool-registry-signal-handler-shutdown` | `fix-tool-registry-signal-handler-shutdown.md` | ✅ ship 2026-08-08 |
| `from-roadmap-phase-6c-evidence-gate` | `from-roadmap-phase-6c-evidence-gate.md` | 📋 Wave 2 待启动 |
| `pkgm-temporal-agent` | `pkgm-temporal-agent.md` | (历史) |
| `provider-dynamic-discovery` | `provider-dynamic-discovery.md` | (历史) |
| `2026-08-10-pdk-safe-exec-tests` | `2026-08-10-pdk-safe-exec-tests.md` | ✅ ship 2026-08-10 |

## 命名约定

- **Phase 0 / 1 / 2 / 3**: 大型 change 拆分实施，每 Phase 独立 plan 文件
- **-CORRECTED 后缀**: Oracle/Metis 审查后修正版本（替代原 plan）
- **归档**: ship 后无需删除，保留作为历史参考

## 与 OpenSpec change 关系

每个 plan 文件**直接对应** `openspec/changes/<change-name>/`：
- Phase 0 详细任务 → `openspec/changes/<change>/tasks.md` Phase 0
- 验收标准 → `openspec/changes/<change>/specs/<change>/spec.md`
- 设计依据 → `openspec/changes/<change>/design.md`

plan 文件**不重复** OpenSpec change 内容，而是**实施指令**：
- 完整 TDD 5 步命令
- 完整代码片段（已 ship 后的可复制粘贴版本）
- ship gate 验证清单
- MUST DO / MUST NOT DO 列表
- 风险 mitigation 表格
- Oracle/Metis 审查 session 追溯

## 维护规则

| 事件 | 操作 |
|------|------|
| Phase ship 后 | 立即更新本 README（change 状态 + ship commit hash）|
| Phase 0 ship 触发 phase-1 plan 创建 | 在新 commit 中同步新增 phase-1.md |
| Phase ship 后 24h | 检查 commit hash + ship gate 文档化 |
| Plan 修正（Oracle 审查发现错误）| 创建 `-CORRECTED.md` 替代，原 plan 标 ⛔ 已废弃 |
| OpenSpec change archive | 同步将对应 plan 文件移入"历史 Plans"段 |

## 相关治理工具

- `openspec validate <change> --strict` — 验证 change artifacts
- `tools/adr_lint.py` — ADR-TRACKING-01 WARNING 检查（含跳过 impl-scope 审计补充文档）
- `tools/docs_drift_audit.py` — 文档 drift 检测
- `scripts/sprint-closeout.sh` — Sprint 收官验证

---

**索引最后验证**: 2026-08-29（capture-mode-and-distillation-writer-v1 Phase 0 ship 后）