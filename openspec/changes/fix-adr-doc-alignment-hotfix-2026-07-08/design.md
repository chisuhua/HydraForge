# Design: ADR 文档对齐 Hotfix (fix-adr-doc-alignment-hotfix-2026-07-08)

> **STATUS: ACTIVE** 🔵
> **关联 proposal**: `proposal.md` (本目录)
> **关联 spec**: `specs/adr-doc-alignment-hotfix/spec.md` (本目录)
> **关联 Metis 审查**: session `ses_0bf414b4affe5zB7zN06vHudKN`
> **最后更新**: 2026-07-08

## Context

2026-07-08 架构文档对齐审查发现 10 处对齐问题 (3 P0 + 4 P1 + 3 P2)。原 mega-change `fix-adr-doc-alignment-2026-07-08` 47 tasks 经 Metis 审查后拆分为 3 个 change:
- **本 hotfix (A)**: 15 tasks, 30 min, 立即 ship
- **Change B**: 命名政策 (P0-2/3 SLASH 统一), 2h, 待与 B2 协调
- **Change C**: P2 清理, 2h, 推到 Sprint 21 follow-up

**当前状态**:
- STATUS-GLOSSARY 5 处 ADR 状态与 ADR 实际冲突 (P0-1, 部分)
- `decisions-2026-07-07.md` D5 实施步骤 step 2/3 重复 (P1-3)
- `docs/adversarial-reviews/README.md` line 84 文件名拼写错 (P1-1)
- ADR-0036 文件未归档但 renumber 决策已 ship (P1-2)
- ADR-0030/handoff 等文档保留对 ADR-0036 的旧引用 (需同步)

**约束条件**:
- Metis 审查 (Q4): ADR-0036 标 `⛔ Superseded` + 从活跃表移除
- Metis 审查 (Q5): spec.md 修正"📋 Audit"语义 — README 表格用 Audit, ADR 文件保留实际状态
- Metis 审查: C16 proposal `inference.*` 是合法事件 topic notation, **不修改**
- Metis 审查: P0-2 SLASH 统一需同步改 PDK 代码, **不属本 hotfix 范围**
- STATUS-GLOSSARY 维护规则 #2: "从 ADR ## 状态 → STATUS-GLOSSARY 单向同步"
- 编号重定义: ADR-0036 → ADR-0045/0046 (renumber 后原 ADR-0036 软归档)

**stakeholders**:
- 立即启动 B2 实施的 C13/C14/C15 owner
- `decisions-2026-07-07.md` 决策追溯者 (D5 签字归属待确认)
- `tools/adr_lint.py` CI 维护者 (验证本 hotfix 不引入 lint 错误)
- 新 Session 阅读 `docs/adversarial-reviews/` 的 Sisyphus

## Goals / Non-Goals

**Goals**:
- 修复 STATUS-GLOSSARY 5 处状态错误 + ADR-0036 Superseded 标注
- 修复 README line 84 拼写
- 修复 D5 step 编号 + 签字状态
- 软归档 ADR-0036 + 同步所有旧链接引用
- 形式化本 hotfix 为 `specs/adr-doc-alignment-hotfix/spec.md` (2 个 Requirements)

**Non-Goals**:
- 不修改 PDK 工具名 (P0-2 SLASH 统一 → Change B)
- 不修改 `lib/inference/*.md` (DOT→SLASH → Change B)
- 不修改 `decisions-2026-07-07.md` D3 决策内容 (P0-3 → Change B)
- 不修改 C16 proposal (经 Metis 审查, `inference.*` 是合法事件 topic notation)
- 不扩展 STATUS-GLOSSARY 📋 双语义 (P1-4 → Change C)
- 不重跑 `tools/adr_relationships.py` (P2-1 → Change C)
- 不验证 C13 4 个 schema ship 状态 (P2-2 → Change C)
- 不修复 `pdk/llama_engine/` 缺 `llama.h` LSP 错误 (pre-existing, 非本 change 责任)
- 不修改任何 C++/CMake 代码

## Decisions

### Decision 1: STATUS-GLOSSARY 5 处状态同步采用"ADR → STATUS-GLOSSARY"单向

**选择**: STATUS-GLOSSARY 状态表 5 处修订基于 ADR 文件实际 `## 状态` 字段

**理由**:
- ADR 文件 `## 状态` 段是人工维护的"权威源"
- STATUS-GLOSSARY 自身声称是"权威词汇表", 但实际从 ADR 派生
- 同步方向单向后, 避免 STATUS-GLOSSARY 污染已 ship ADR 状态

**替代方案**:
- 方案 A (双向同步): 拒绝。已 ship ADR 状态不应被 STATUS-GLOSSARY 改写
- 方案 B (autogen 工具): 接受。Change C 将添加 `tools/adr_relationships.py` 状态同步支持

**实施细节**:
- 5 处状态值修正 (ADR-0021/0022/0023/0030/0034)
- 维护规则 #2 追加 "From ADR ## 状态 → STATUS-GLOSSARY 单向同步" 说明
- ADR-0036 不在此 5 处之列 (由 Decision 4 软归档处理)

### Decision 2: README 拼写修正采用单行 edit

**选择**: `docs/adversarial-reviews/README.md` line 84 单行 edit

**理由**:
- 拼写错误是 1 字符 (`oo` → `o`), 影响 1 行
- 用 Edit 工具精确替换, 避免 sed 全局误伤

**替代方案**:
- 方案 A (sed 全局替换): 拒绝。`oopenspec` 字符串仅在 README line 84 出现 1 次, sed 会扫描整个文件
- 方案 B (Read + Edit 精确): 接受。Read line 81-85 上下文, Edit 单行

**实施细节**:
- Read `docs/adversarial-reviews/README.md` line 81-87
- Edit `ref-1-b2-oopenspec-arch.md` → `ref-1-b2-openspec-arch.md`

### Decision 3: D5 step 编号去重采用"拆分重复 step"策略

**选择**: step 2/step 3 重复内容拆分为 2 个独立 step (新增 API + 添加单测)

**理由**:
- 现状: step 2 与 step 3 内容完全相同 ("新增 DSLEngine::load_plugin(...) 公开方法" 出现 2 次)
- 拆分后形成完整 5 步序列: 删除默认注入 / 新增 API / 添加单测 / 迁移示例 / 文档更新
- 逻辑上 step 3 应是"验证", 缺失该 step 会让 reader 误以为"添加 API = 完事"

**替代方案**:
- 方案 A (删重复 step 保留 4 步): 拒绝。逻辑不完整, 缺测试
- 方案 B (重命名为 step 2 = API / step 3 = 单测): 接受。明确每步任务边界

**实施细节**:
- step 2: 新增 `DSLEngine::load_plugin(const std::string& name)` 公开方法
- step 3: 添加 `test_load_plugin.cpp` 单元测试 (4 test cases)
- step 4: 迁移现有测试/示例 (保留原 step 3 内容)
- step 5: 更新 `lib/dsl-reference.md` §3.2 (保留原 step 4 内容)
- 删除原 step 5

### Decision 4: ADR-0036 软归档采用"git mv + DEPRECATED 横幅 + 旧链接同步"

**选择**: 软归档至 `docs/archive/adr/`, 头部加 DEPRECATED 横幅, 同步所有旧引用

**理由**:
- ADR-0035/0038-0044 状态字段均声明 "renumber: 兄弟 ADR-0036 → ADR-0045", 但原文件仍存在
- "硬删除"会破坏 git blame 链
- "软归档"在 archive/ 目录加 DEPRECATED 横幅, 新 Session 不会误读

**替代方案**:
- 方案 A (硬删除): 拒绝。违反"归档而非删除"项目约定
- 方案 B (原地加横幅 + 留 docs/adr/): 拒绝。`docs/adr/` 是活跃目录, 触发 `tools/adr_lint.py` 误报
- 方案 C (仅 git mv, 不更新旧链接): 拒绝。旧链接断裂影响其他文档

**实施细节**:
- `git mv docs/adr/adr-0036-three-layer-service-protocol.md docs/archive/adr/`
- 归档文件头部追加:
  ```
  > **⛔ DEPRECATED (2026-07-08)** — 本 ADR 已被 ADR-0045 (renumbered from 0036) 替代。
  > 详见: `docs/adr-management/relationships.md` + `openspec/changes/fix-adr-doc-alignment-hotfix-2026-07-08/`
  ```
- `docs/README.md` 删除该 ADR 行
- `docs/adr/plugin/README.md` 追加 renumber 注记
- `docs/adr/adr-0030-async-runtime-v2.md:318` 旧链接更新: 删除或指向 ADR-0045
- `docs/handoff/2026-07-06-architecture-completion.md:51` 旧引用更新

### Decision 5: ADR-0036 状态值标 `⛔ Superseded` (非 `🔍 Proposed`)

**选择**: STATUS-GLOSSARY 中 ADR-0036 状态标 `⛔ Superseded` (被 ADR-0045 替代), 从活跃表移除

**理由**:
- Metis 审查 (Q4) 明确指示: 归档后的 ADR 应标 Superseded, 不应再列为活跃 ADR
- ADR-0036 renumber 到 ADR-0045/0046 后, 旧编号已被替代, 不是 "active but unused"
- `⛔ Superseded` 状态标签与 STATUS-GLOSSARY 现有词汇表一致

**替代方案**:
- 方案 A (标 `🔍 Proposed`): 拒绝。Metis 审查已否决 (与"归档"语义矛盾)
- 方案 B (标 `📋 Audit`): 拒绝。Audit 用于 impl-scope-audit 文档, 不适用
- 方案 C (保留原 `❌ Not Implemented` + 标 superseded): 拒绝。状态标签应选最强语义

**实施细节**:
- STATUS-GLOSSARY 状态表 5 处修订**不含** ADR-0036
- ADR-0036 状态在"已废弃 ADR"小节列出, 标 `⛔ Superseded`
- "已废弃 ADR" 数量从 13 → 14 (含 ADR-0036)

### Decision 6: D5 签字状态采用"查询 git log + 默认 🟡 待签字"

**选择**: 查 git log 找 D5 决策 author; 找不到则保持 `🟡 待签字 (2026-07-08)`

**理由**:
- Metis 审查指出 AI 不知道谁签的字, 不能随便填 signer
- 决策签字状态属于"待用户确认"范畴, 保持透明

**替代方案**:
- 方案 A (随机填 signer): 拒绝。审计风险
- 方案 B (保持 `✅ 已定稿` + 删 line 105 标注): 接受。简化但失真
- 方案 C (默认 🟡 待签字): 接受。诚实

**实施细节**:
- 跑 `git log --follow docs/adversarial-reviews/decisions-2026-07-07.md` 找 author
- 若 author 存在 + 有 D5 决策 commit → 标 `✅ 已签字 (2026-07-08 by [author])`
- 否则 → 标 `🟡 待签字 (2026-07-08)`

## Risks / Trade-offs

### Risk 1: ADR-0036 软归档后 `tools/adr_lint.py` 失败

- **影响**: 现有 lint 工具可能不识别 archive/ 目录文件, 触发 "duplicate ADR-0036 节点" 错误
- **缓解**: 先跑 `tools/adr_lint.py` 验证当前状态; 若失败, 提交 patch 让 lint 排除 archive/
- **检测**: `tools/adr_lint.py` exit 0

### Risk 2: 旧链接同步遗漏

- **影响**: `git grep "adr-0036-three-layer-service-protocol"` 可能命中未在本 hotfix 任务中的文件
- **缓解**: 实施时跑 git grep 全面扫描; 一次性更新所有命中
- **检测**: `git grep "adr-0036-three-layer-service-protocol" -- 'docs/' 'openspec/'` 输出仅 `docs/archive/adr/` 路径

### Risk 3: D5 step 拆分后, 上游引用 D5 的文档未同步

- **影响**: `openspec/changes/phase5-llama-engine-plugin/` 等可能引用 D5 的 step 编号, 拆分后引用失效
- **缓解**: 跑 `git grep -E "D5.*step" -- 'docs/' 'openspec/'` 验证
- **检测**: 仅有 1 处 D5 step 引用 (decisions 文件自身)

### Risk 4: STATUS-GLOSSARY 状态表修订触发 `tools/adr_lint.py` 误报

- **影响**: 若 lint 工具硬编码旧状态值, 修订后触发"未知状态"错误
- **缓解**: 先验证 lint 工具支持 6 个标准标签; 否则先 patch lint
- **检测**: `tools/adr_lint.py` exit 0

### Risk 5: D5 签字状态默认 `🟡 待签字` 可能被误读

- **影响**: 上游 Sprint 21 实施者可能误以为 D5 未生效
- **缓解**: 决策描述段 (line 84-89) 内容不变, "影响" 段保持原内容, 仅签字标注变更
- **检测**: 人工 review line 84-89 内容

### Risk 6: ADR-0036 旧引用 `adr-0030-async-runtime-v2.md:318` 修订后可能与新 ADR-0045 内容不一致

- **影响**: ADR-0030 引用的 ADR-0036 内容可能与 ADR-0045 不同 (renumber 时通常合并/调整)
- **缓解**: 检查 ADR-0030 line 318 上下文, 修订时同步更新引用文段
- **检测**: 引用文段与 ADR-0045 实际内容 match

## Migration Plan

### Phase 0: 准备 (2 min)
- 检查 `tools/adr_lint.py` 是否存在 + 是否支持 6 个状态标签
- 跑 `git status` 确认工作区干净
- 备份 `docs/adr-management/STATUS-GLOSSARY.md`

### Phase 1: 修订 STATUS-GLOSSARY (15 min)

#### Step 1.1: 修订 5 处状态值
- ADR-0021: 🔍 Proposed → ✅ Approved
- ADR-0022: 🔍 Proposed → ✅ Approved  
- ADR-0023: 🟡 Partial → ✅ Approved
- ADR-0030: ❌ Not Implemented → 🔍 Proposed
- ADR-0034: ❌ Not Implemented → ✅ Approved

#### Step 1.2: 追加维护规则 #2 同步方向说明
- 在维护规则 #2 段后追加 "**同步方向**: From `## 状态` 字段 → STATUS-GLOSSARY 状态表 (单向)"

### Phase 2: 修订 README + D5 (6 min)

#### Step 2.1: 修订 README line 84 拼写 (1 min)
- `ref-1-b2-oopenspec-arch.md` → `ref-1-b2-openspec-arch.md`

#### Step 2.2: 修订 D5 step 编号 (3 min)
- step 2/step 3 拆分为 step 2 (新增 API) + step 3 (添加单测)
- 删除原 step 5

#### Step 2.3: 修订 D5 签字状态 (2 min)
- 跑 `git log --follow docs/adversarial-reviews/decisions-2026-07-07.md` 找 author
- 修订 "待签字确认" 为 "🟡 待签字 (2026-07-08)" 或 "✅ 已签字 (2026-07-08 by [author])"

### Phase 3: 软归档 ADR-0036 (10 min)

#### Step 3.1: git mv 归档 (1 min)
- `git mv docs/adr/adr-0036-three-layer-service-protocol.md docs/archive/adr/`

#### Step 3.2: 追加 DEPRECATED 横幅 (2 min)
- 归档文件头部追加 `> **⛔ DEPRECATED (2026-07-08)**` 横幅

#### Step 3.3: STATUS-GLOSSARY ADR-0036 状态标注 (2 min)
- 在"已废弃 ADR"小节添加 ADR-0036, 标 `⛔ Superseded`

#### Step 3.4: README.md 同步 (1 min)
- `docs/README.md` 删除 ADR-0036 行

#### Step 3.5: plugin/README.md 同步 (1 min)
- `docs/adr/plugin/README.md` 追加 ADR-0036 renumber 注记

#### Step 3.6: 旧链接同步 (3 min)
- `docs/adr/adr-0030-async-runtime-v2.md:318` 更新引用
- `docs/handoff/2026-07-06-architecture-completion.md:51` 更新引用
- 跑 `git grep "adr-0036-three-layer-service-protocol"` 验证

### Phase 4: 验证 + Ship Gate (5 min)

#### Step 4.1: 跨文件 lint 验证
- `tools/adr_lint.py` exit 0 (若存在)
- `git grep -E "inference\\.[a-z_]+" -- 'docs/adr-management/'` 输出仅含 STATUS-GLOSSARY 文档本身 (已修订) + AGENTS.md
- `git grep "adr-0036-three-layer-service-protocol" -- 'docs/' 'openspec/'` 输出仅 `docs/archive/adr/`

#### Step 4.2: OpenSpec 验证
- `openspec validate fix-adr-doc-alignment-hotfix-2026-07-08` exit 0

#### Step 4.3: Commit
- `git add -A`
- `git status` 验证变更文件清单
- `git diff --stat` 验证 +X/-Y 行数
- `git commit -m "docs: hotfix ADR document alignment (STATUS-GLOSSARY + ADR-0036 archive)"`

### Rollback Strategy

若 Phase 4 验证失败, 回滚所有变更:
```bash
git restore docs/adr-management/STATUS-GLOSSARY.md
git restore docs/adversarial-reviews/README.md
git restore docs/adversarial-reviews/decisions-2026-07-07.md
git restore docs/README.md
git restore docs/adr/plugin/README.md
git restore docs/adr/adr-0030-async-runtime-v2.md
git restore docs/handoff/2026-07-06-architecture-completion.md
# ADR-0036 归档回滚
git mv docs/archive/adr/adr-0036-three-layer-service-protocol.md docs/adr/adr-0036-three-layer-service-protocol.md
```

**回滚影响**: 仅恢复文档到本 hotfix 前的状态, 无代码影响, 无 ctest 影响。

## Open Questions

1. **D5 实际签字 author**: `git log --follow decisions-2026-07-07.md` 找出的 author 是否真的是 D5 决策签字人? 若 author 实际仅 ship 文件而非 D5 决策, 仍需标 `🟡 待签字`
2. **ADR-0036 归档后 STATUS-GLOSSARY 表格行结构调整**: "已废弃 ADR" 段是否应作为新独立 section, 还是合并到现有 "Superseded" 行? 当前 STATUS-GLOSSARY 没有 "已废弃" section, 需新增
3. **ADR-0030 line 318 引用文段修订**: 原引用是"详见 ADR-0036"还是更具体内容? 修订时需保持引用上下文完整性
4. **`tools/adr_lint.py` 是否存在**: 仓库 `tools/` 目录是否有该脚本, 本 hotfix 是否需要 patch
5. **AGENTS.md 是否需要追加本 hotfix ship 记录**: AGENTS.md 维护"最近变更"段, 文档-only 修复是否值得记录
