# Design: ADR 文档对齐修复 (fix-adr-doc-alignment-2026-07-08)

> **STATUS: ACTIVE** 🔵
> **关联 proposal**: `proposal.md` (本目录)
> **关联 spec**: `specs/adr-doc-alignment/spec.md` (本目录)
> **最后更新**: 2026-07-08

## Context

2026-07-08 Sisyphus 进行了项目架构文档专项审查 (对象: `docs/adversarial-reviews/` + `docs/adr-management/` + `docs/adr/` 关键 ADR 状态)。审查产出 3 P0 + 4 P1 + 3 P2 级别问题。

**当前状态**:
- **C14 实施层** (`lib/inference/engine.md` + `model.md`): 使用 SLASH 格式 (`inference/engine/init`)，与 ADR-0034 一致 ✅
- **C13 实施层** (`lib/inference/prefix_cache.md` 等 5 个 .md): 使用 DOT 格式 (`prefix_cache.configure`)，与 ADR-0034 矛盾 ❌
- **decisions 决策层** (`decisions-2026-07-07.md` D3): 使用 DOT 格式，与 ADR-0034 矛盾 ❌
- **STATUS-GLOSSARY 状态层**: 6 处状态与 ADR 文件实际冲突 ❌
- **C16 proposal 命名章节**: 使用 DOT 格式 (2 处)，与 ADR-0034 矛盾 ❌
- **ADR-0036 编号冲突**: renumber 到 ADR-0045/0046 后，原文件未归档 ❌

**约束条件**:
- ADR-0034 (Model Router, C7 已 ship) 是命名约定的"先例权威" — 其 §命名约定 段落明确"PDK tool names use slash (`/`) as hierarchy delimiter"
- ADR-0043 (PDK Naming, 2026-07-06 起草) 是命名约定的"规范权威" — §1 第 1 条"**唯一合法分隔符是 `/` (slash)**"
- STATUS-GLOSSARY.md 自身维护规则 #2: 任何 ADR 状态变更时，README、relationships.md、SPECS-ALIGNMENT.md **必须同步**
- STATUS-GLOSSARY.md 自身维护规则 #3: 不再创建新的状态标签

**stakeholders**:
- 即将启动 B2 实施 (C13/C14/C15) 的实施者
- C16 (ILLMProvider Call Chain V2) 文档作者
- 新 Session 阅读 `docs/adversarial-reviews/` 的 Sisyphus
- CI `adr_lint.py` 校验脚本

## Goals / Non-Goals

**Goals**:
- 修复 3 个 P0 阻塞问题（命名约定 + 状态同步 + decisions D3 映射）
- 修复 4 个 P1 关键问题（D5 step 编号、ADR-0036 归档、README 拼写、📋 双语义）
- 清理 3 个 P2 一般问题（relationships 重跑、C13 ship 验证、ADR-0021 状态同步）
- 形式化"ADR 文档对齐规范"为 `specs/adr-doc-alignment/spec.md`

**Non-Goals**:
- 不修改任何 C++/CMake 代码
- 不创建新 ADR 文件
- 不重启 B2 三 Change 实施
- 不修改 master plan 内容（仅修正漂移数字）
- 不增加新 status label（双语义扩展在 P1-4 范围内）
- 不修复 lib/inference/ 4 个未 ship 的 schema 实施（属于 C13 tasks 范围）
- 不解决 pdk/llama_engine/ 缺 `llama.h` 的 LSP 错误（pre-existing 状态，与本 change 无关）

## Decisions

### Decision 1: 命名约定以 SLASH 为准 (DOT 全部移除)

**选择**: 所有 PDK 工具名 MUST 使用 SLASH 分隔符

**理由**:
- ADR-0034 §命名约定 (line 340-346): "**PDK tool names use slash (`/`) as hierarchy delimiter.** Format: `{namespace}/{component}/{action}`"
- ADR-0043 §1 (line 55): "**分隔符: 唯一合法分隔符是 `/` (slash)。双 `/` 或尾随 `/` 不允许。**"
- C14 实施层 (`lib/inference/engine.md`) 已采用 SLASH，移除 DOT 不会破坏已 ship 代码
- vLLM/TGI/lit-gpt 等外部项目 SLASH 命名是行业事实标准

**替代方案**:
- 方案 A (DOT 优先): 拒绝采纳。DOT 与 ADR-0034/0043 明确冲突；与 C14 实施层矛盾；引入额外 dot/slash 双格式校验负担
- 方案 B (双格式兼容): 拒绝采纳。增加 `tools/adr_lint.py` 实现复杂度，且易产生 `prefix_cache.configure` vs `prefix_cache/configure` 语义歧义

**实施细节**:
- 5 个 `lib/inference/*.md` 文件: 全部 `find . -name "*.md" -exec sed -i 's|tool: prefix_cache\.configure|tool: prefix_cache/configure|g' {}` 风格
- `decisions-2026-07-07.md` D3 整章重写（DOT 映射表 → SLASH 映射表）
- `C16 proposal.md` 两处命名章节（"命名统一" + "文档修订"）DOT → SLASH
- `ADR-0043` §1 examples (line 71-80) 已是 SLASH，无需修改（这正是依据）

### Decision 2: STATUS-GLOSSARY 状态同步采用"双源对比"策略

**选择**: STATUS-GLOSSARY 状态表采用"以 ADR 实际状态字段为准"的同步策略

**理由**:
- ADR 文件 ## 状态 段是人工维护的"权威源"
- STATUS-GLOSSARY 状态表自身声称是"权威词汇表"，但实际是从 ADR 文件派生
- 修复方式: 在 STATUS-GLOSSARY 维护规则 #2 追加"同步方向"说明（"从 ADR 文件 → STATUS-GLOSSARY 单向同步"）

**替代方案**:
- 方案 A (从 STATUS-GLOSSARY → ADR): 拒绝。STATUS-GLOSSARY 已严重过时，单向同步会污染 6 个已 ship ADR
- 方案 B (autogen 工具): 接受。`tools/adr_relationships.py` 可扩展为状态同步工具，但本次仅手动修复

**实施细节**:
- 修改 STATUS-GLOSSARY.md 维护规则 #2: 追加 "From ADR ## 状态 → STATUS-GLOSSARY 单向同步" 说明
- 6 处状态值修正（ADR-0021/0022/0023/0030/0034/0036）
- 表格示例行更新到引用最新已 ship ADR（ADR-0034 替代 ADR-0002 作为 ✅ Approved 例）
- 📋 双语义（Decision 4）

### Decision 3: ADR-0036 归档策略 — "软归档" (DEPRECATED 横幅 + 移至 archive/)

**选择**: ADR-0036 原文件保留在 `docs/archive/adr/` 目录 + 顶部加 DEPRECATED 横幅

**理由**:
- ADR-0035/0038-0044 状态字段均声明 "renumber: 兄弟 ADR-0036 → ADR-0045"，但原 ADR-0036 文件仍存在
- "硬删除"会破坏 git blame 链，且 AGENTS.md 引用了"12 个已废弃 ADR 已归档"
- "软归档"在 archive/ 目录加 DEPRECATED 横幅，新 Session 不会误读

**替代方案**:
- 方案 A (硬删除): 拒绝。破坏 git history 且违反"归档而非删除"的项目约定
- 方案 B (原地加横幅 + 留原目录): 拒绝。`docs/adr/` 是活跃 ADR 目录，混合归档会污染 CI `adr_lint.py` 扫描
- 方案 C (仅重命名 + 留 docs/adr/): 拒绝。同一目录存在 2 个 ADR-0036 编号是 confusable

**实施细节**:
- `git mv docs/adr/adr-0036-three-layer-service-protocol.md docs/archive/adr/`
- 在归档文件头部追加:
  ```markdown
  > **⛔ DEPRECATED (2026-07-08)** — 本 ADR 已被 ADR-0045 (renumbered from 0036) 替代。
  > 详见: `docs/adr-management/relationships.md` (自动生成) + `openspec/changes/fix-adr-doc-alignment-2026-07-08/`
  ```
- 在 `docs/adr/plugin/README.md` 追加 ADR-0036 归档注记
- `docs/README.md` 删除 `adr-0036-three-layer-service-protocol.md` 行（保持现状表格只列活跃 ADR）

### Decision 4: STATUS-GLOSSARY 📋 双语义扩展

**选择**: 增补 📋 标签的"审计补充"语义，与"Reserved"并列

**理由**:
- README.md 12 个 `adr-*-impl-scope.md` 已使用 📋 标记为"审计补充"
- STATUS-GLOSSARY 当前定义 📋 = "Reserved"，与 README 用法冲突
- 维护规则 #3 禁止"创建新标签"，但本扩展是"增补子语义"，不创建新标签

**替代方案**:
- 方案 A (用 🔍 Proposed 替代): 拒绝。impl-scope-audit 文档不是"待实施提案"，语义不准
- 方案 B (新标签 ⭐ 或 🪧): 拒绝。维护规则 #3 禁止
- 方案 C (删除所有 📋 审计补充标记): 拒绝。破坏 docs-code-drift-audit 12 个审计文档的语义标签

**实施细节**:
- STATUS-GLOSSARY.md 表格 📋 行扩展为:
  ```
  | 📋 Reserved | Reserved | 编号预留，无内容 | (无活跃 ADR，未来用于占位) |
  | 📋 Audit    | 审计补充 | impl-scope-audit 文档专用 (与 docs-code-drift-audit 配套) | ADR-0002/0004-impl-scope-audit 等 12 个 |
  ```
- 维护规则 #3 追加"例外: 现有标签的子语义扩展允许"

### Decision 5: decisions-2026-07-07.md D5 step 编号修正方案

**选择**: 重写 D5 实施步骤 5 步序列，明确每步任务边界

**理由**:
- 当前 line 99-105 step 2 与 step 3 完全重复（"新增 DSLEngine::load_plugin(...) 公开方法" 出现 2 次）
- 决策签字状态标注 "待签字确认" 与文件头 "✅ 已定稿" 矛盾

**替代方案**:
- 方案 A (仅修正 step 编号): 拒绝。签字状态矛盾未解决
- 方案 B (改 D5 整章): 接受。同时修正 step + 签字状态 + 添加 references

**实施细节**:
- D5 实施步骤重写为 5 步清晰序列:
  1. 删除 `DSLEngine` 构造中默认注入逻辑 (line 200-220)
  2. 新增 `DSLEngine::load_plugin(const std::string& name)` 公开方法
  3. 添加 `test_load_plugin.cpp` 单元测试 (4 test cases)
  4. 迁移现有测试/示例 (添加 `load_plugin("pdk/llama_engine")` 调用)
  5. 更新 `lib/dsl-reference.md` §3.2 记录 API 变更
- 签字状态: 标注 `🟡 待签字 (2026-07-08)` 或更新为 `✅ 已签字 (2026-07-08 by [signer])`

## Risks / Trade-offs

### Risk 1: B2 实施层已部分采用 SLASH，C13 重写可能不一致
- **影响**: 5 个 C13 .md 文件 SLASH 化后，与已 ship 的 ADR-0034 命名规范一致 ✅，但若 C13 实施时未察觉 SLASH 变更，可能仍按旧 DOT 写
- **缓解**: 在 C13 tasks.md §1 顶部加 "**命名规则: 工具名 MUST 使用 SLASH 分隔符 (per ADR-0034 §命名约定)**" 提醒
- **检测**: C13 实施后由 `tools/adr_lint.py` 校验 lib/inference/*.md 工具名格式

### Risk 2: C16 proposal 命名章节修订可能影响在途 OpenSpec change
- **影响**: C16 当前 active，proposal-v2.md 2026-07-07 18:03 更新；本次 SLASH 修订是 minor 文字调整
- **缓解**: 修订前与 C16 author 同步 (待 Oracle confirmation)；修订后 C16 仍可继续实施
- **检测**: C16 tasks.md 实施时引用更新后的 proposal

### Risk 3: STATUS-GLOSSARY 6 处状态修正可能与 README 表格冲突
- **影响**: 6 处状态在 STATUS-GLOSSARY、ADR 文件、README.md、relationships.md 四方需同步
- **缓解**: 先修 STATUS-GLOSSARY，再跑 `tools/adr_relationships.py` 重生成 relationships.md，最后更新 README.md 表格
- **检测**: `tools/adr_lint.py` 检查四源一致性

### Risk 4: ADR-0036 归档后, AGENTS.md 中"12 个已废弃 ADR 已归档"数字需更新
- **影响**: AGENTS.md 引用是 "12 个", 本次新增 1 个 → 13 个
- **缓解**: 同步更新 AGENTS.md "12 个已废弃 ADR" → "13 个已废弃 ADR" (含 ADR-0036)
- **检测**: `scripts/sprint-closeout.sh` drift 检查

### Risk 5: relationships.md 重跑可能引入新的 ADR 引用关系
- **影响**: P2-1 重跑 `tools/adr_relationships.py` 可能发现新的"depends-on/supersedes"边
- **缓解**: 重跑前备份当前 `relationships.md`; 重跑后人工 review mermaid 图变化
- **检测**: git diff `relationships.md` 后人工确认

### Risk 6: `tools/adr_lint.py` 是否过滤 docs/archive/adr/
- **影响**: 若 lint 工具扫描 archive/ 目录，DEPRECATED 横幅可能触发 false positive
- **缓解**: 检查 `tools/adr_lint.py` 实现；如未过滤，提交 patch 添加 archive/ 排除
- **检测**: 修复后跑 `tools/adr_lint.py` exit 0

### Risk 7: 📋 双语义扩展可能被 `tools/adr_lint.py` 拒绝
- **影响**: 若 lint 工具硬编码 6 个 status label，双语义扩展会触发"未知标签"错误
- **缓解**: 修改 `tools/adr_lint.py` 接受 📋 双语义
- **检测**: 修复后跑 `tools/adr_lint.py` exit 0

## Migration Plan

### Phase 0: 准备 (5 min)
- 检查 `tools/adr_lint.py` 和 `tools/adr_relationships.py` 是否存在
- 备份当前 `docs/adr-management/STATUS-GLOSSARY.md` 和 `relationships.md`
- 确认 git branch 是干净的

### Phase 1: P0 修复 (3 h)

#### Step 1.1: 修复 STATUS-GLOSSARY (30 min)
- 修改 `docs/adr-management/STATUS-GLOSSARY.md` 6 处状态值
- 追加维护规则 #2 "From ADR ## 状态 → STATUS-GLOSSARY 单向同步" 说明
- 表格示例行更新到引用最新已 ship ADR

#### Step 1.2: SLASH 化 5 个 C13 .md 文件 (50 min)
- 5 个文件各自 `sed` 替换 2-3 处工具名
- 每个文件验证 `grep "tool: " <filename>` 输出 SLASH 风格

#### Step 1.3: 重写 decisions-2026-07-07.md D3 + D5 (1 h)
- D3 整章重写：删 DOT 映射表 + 删 "C13 架构工具命名边界" 小节 + 改 D3 描述
- D5 step 编号 + 签字状态修正

#### Step 1.4: 修正 C16 proposal 命名章节 (15 min)
- "命名统一" 段: DOT → SLASH
- "文档修订" 段: DOT → SLASH

#### Step 1.5: 验证 (15 min)
- `git grep "inference\\.\\|inference/" -- 'docs/**' 'openspec/**' 'lib/**'`
- `tools/adr_lint.py` exit 0 (若工具存在)
- `openspec validate fix-adr-doc-alignment-2026-07-08` exit 0

### Phase 2: P1 修复 (1 h)

#### Step 2.1: 修正 README 拼写 (1 min)
- `docs/adversarial-reviews/README.md` line 84 拼写

#### Step 2.2: STATUS-GLOSSARY 📋 双语义 (15 min)
- 表格 📋 行扩展
- 维护规则 #3 例外条款

#### Step 2.3: 修正 D5 step 编号 (5 min)
- 重复 step 去重

#### Step 2.4: ADR-0036 软归档 (30 min)
- `git mv` 归档
- 头部加 DEPRECATED 横幅
- 更新 `docs/README.md` 删除该 ADR 行
- 更新 `docs/adr/plugin/README.md` 加注记

### Phase 3: P2 修复 (2 h)

#### Step 3.1: 重跑 `tools/adr_relationships.py` (30 min)
- 验证脚本覆盖 `docs/adr/plugin/`
- 验证脚本覆盖 `docs/adr/adr-0035/0038-0046`
- 重生成 `relationships.md`
- 人工 review git diff

#### Step 3.2: 验证 C13 4 个 schema ship 状态 (30 min)
- `git log --oneline lib/inference/prefix_cache.md` 检查 commit
- 确认 ship 状态后更新 master plan §十六.5 数字
- 若未 ship，记录在 tasks §9.2

#### Step 3.3: 同步 ADR-0021 状态字段 (5 min)
- 追加 D1 决策注记

#### Step 3.4: 更新 AGENTS.md (15 min)
- "12 个已废弃 ADR" → "13 个已废弃 ADR"

### Rollback Strategy

若 Phase 1 验证失败，回滚步骤:
```bash
git restore docs/adr-management/STATUS-GLOSSARY.md
git restore 'lib/inference/*.md'
git restore docs/adversarial-reviews/decisions-2026-07-07.md
git restore openspec/changes/phase5-illmprovider-call-chain-v2/proposal.md
```

若 P1/P2 失败，按文件回滚即可。**全部回滚** 不影响 B2 实施 (B2 仍可启动但有文档风险)。

## Open Questions

1. **D5 签字状态实际归属**: decisions-2026-07-07.md 头部"已定稿" 与 D5 内部"待签字确认"矛盾。需确认 D5 是否已签字 (向 C14 author `ses_0cb1027ccffeN7BmCaOQTpQl1Y` 询问)

2. **C13 实际 ship 状态**: 2026-07-06 ref-3 报告 0/32 tasks complete，但 2026-07-07 后 lib/inference/ 出现 4 个新 .md 文件。需 git log 确认 4 个文件 commit hash + author + 是否完整 (含 tool 工具名注册)

3. **ADR-0043 §3 "Plugin 命名 vs Tool 命名" 区分是否需要补充到 STATUS-GLOSSARY**: ADR-0043 §1.5 区分 `plugin_name` (POD char[]) 与 `plugin_namespace` (工具名前缀)。STATUS-GLOSSARY 是否需要追加命名相关规范

4. **`tools/adr_lint.py` 是否存在**: 审查发现 `docs/adr-management/relationships.md` 标注"由 `tools/adr_relationships.py` 自动生成"，但本仓库 linter 是否独立存在未知

5. **是否同步更新 handoff 文档**: `docs/handoff/2026-07-05-week1-day1-day2-completion.md` §5.1-5.2 B2 决策可能引用旧 DOT 命名，需 review
