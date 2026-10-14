# .rddf/improvements/ 全面审计决策登记 (2026-10-14)

**生成**: 2026-10-14 via 主会话 (ses_ec6a3b1b4ffe2Jv7AqxsZ2WAza)
**触发**: 用户决策 "继续一一排查 33 OPEN 提案" (后扩到 51 个未标 SHIPPED)
**关联**: 9b5df59 (本日早 session 3 P0 提案) + 2c95fd4 (parser-string-template-args) + 本审计 26 个新 commit

## 1. 用户 3 项决策

### 决策 1: commit 粒度
> 1 commit per proposal (25 commits) — 粒度最细 (推荐)

### 决策 2: deliverables
> 全 deliverables: audit 报告 + 决策登记 + AGENTS.md entry (推荐)

### 决策 3: 执行模式
> 主会话手工进行 (本会话中已 60% 完成)

## 2. 实施路径

**Phase 1 (本 commit 包含)**:
- 1 atomic commit 含: audit 报告 (新建) + 决策登记 (新建) + AGENTS.md 2026-10-14 entry 段 (追加)

**Phase 2 (后续 25 个 commit)**:
- 每个 hygiene 修正 = 1 atomic commit
- 顺序: 20 SHIPPED + 5 PARTIAL + 4 CANCELLED = 29 commit
- (注: 4 CANCELLED 实际是 2 SHIPPED 误判 + 2 真 CANCELLED = 2 真 CANCELLED commit)
- 实际 commit 数 = 20 + 5 + 4 = 29 commits (本节前 4 项 CANCELLED 单独 commit)

## 3. Pattern 沉淀 (5 项)

### 3.1 5 维静态 ship 判定法
提案 self-declared → archive tracked → commit message → ADR status → fallback

### 3.2 tasks.md 数字 ≠ ship 状态
ship 证据 = 代码 ship + 测试 PASS + spec 同步, 非 OpenSpec 内部数字

### 3.3 AGENTS.md Pattern #10 drift 完整案例库
30 个案例覆盖 5 种状态 (✅/🟡/⛔/🔴/⏸)

### 3.4 `.gitignore` 对 archive 的影响
archive untracked ≠ 提案未 ship, 需多源验证

### 3.5 audit hygiene 修正的"1 commit per proposal" 模式
粒度最细, 便于回溯

## 4. 预期 ship gate

- 29 个 atomic commit
- 1 个 audit 报告 (新建)
- 1 个决策登记 (新建)
- 1 个 AGENTS.md entry 段 (追加)
- 总 ~30 commits
- 全量 ctest 零回归 (纯文档修改)

## 5. 后续依赖

完成 audit 后, 26 个 OPEN 提案可进入实际立项推进:
- P0: 4 个 (adr-0072-* 3 + control-plane-eval + harness-mutation-commit-real-llm + from-roadmap-phase-6c-control-plane-eval)
- P1: 7 个 (chat-async-io-* 3 + chat-slash-commands-migration + fix-markdown-parser-yaml + from-roadmap-6b-* 2)
- 等等
