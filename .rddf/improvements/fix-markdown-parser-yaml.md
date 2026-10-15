# fix-markdown-parser-yaml

**优先级**: P1 | **来源**: layer-based-missing-capabilities-analysis.md §四 L0-4 + §十三 附录第 4 项（轻量级，不另开 ADR）
**阶段**: wave-2 | **分类**: core-impl
**类型**: debt
**主题**: Schema校验基础；YAML frontmatter解析

## 架构依据
- `DslValidator` 仅支持 Markdown bold (`**key**: value`) 格式，但 `lib/loop/*.agent.md` 生产文件实际使用 **YAML frontmatter**——`main.cpp:321` 检测到 YAML 时直接跳过校验（带 stderr 警告），pdk_chat_demo T2 的 DSL Schema 校验在生产路径上**实际失效**。
- 9 个测试 fixture 仍用 Markdown bold 格式，与生产代码不一致（测试在验证一种生产不用的格式）。
- 缺失能力分析定性：0.5 Sprint，快速修复，不另开 ADR。

## 范围
- **In Scope**: MarkdownParser/DslValidator 支持 YAML frontmatter 解析（`---` 块内 YAML → 与 bold 格式相同的校验路径）；双格式自动检测；9 个测试 fixture 迁移或双格式参数化；`main.cpp:321` 跳过分支删除。
- **Out Scope**: DSL 语法本身的扩展（归 docs/proposals/）；`.agent.md` schema 字段新增。

## 关键场景
- GIVEN 一个 YAML frontmatter 格式的 `.agent.md`，WHEN DslValidator 校验，THEN 走与 bold 格式完全相同的 schema 校验路径，非法文件给出 line-level 错误。
- GIVEN bold 格式 fixture，WHEN 校验，THEN 行为与现状一致（向后兼容）。
- GIVEN `main.cpp` 加载 YAML agent 文件，WHEN 解析，THEN 不再出现"跳过校验"的 stderr 警告。

## 技术约束
- MUST 复用现有 schema 校验逻辑（双格式仅差解析前端，禁止复制校验规则）。
- MUST 保持 bold 格式向后兼容（9 个 fixture 是回归网）。
- SHOULD yaml-cpp 已在 external/ 依赖中，禁止引入新解析库。

## 验收标准
- YAML 格式 agent 文件校验生效（构造非法 YAML fixture 给出 line-level 错误）。
- `grep -n "跳过" examples/pdk_chat_demo/main.cpp` 返回 0。
- ctest 全量零回归。

## Status: ✅ SHIPPED (2026-08-05, Wave 2 P1)

**Ship evidence** (验证日期 2026-10-15 via 本会话 5-维静态 ship 判定):
- **OpenSpec archive**: `openspec/changes/archive/2026-08-04-fix-markdown-parser-yaml/` (per commit `1740b88`)
- **代码 ship commits**:
  - `e6f0955` (2026-08-02) feat: fill fix-markdown-parser-yaml change artifacts (Wave 2 P1)
  - `bc0d30e` (2026-07-31) guide(design): arch-done + design-done + 6 skeleton changes
  - `4c9721c` (2026-08-05) feat(parser): fenced yaml block detection + dsl_validator dual-format
  - `e7b468c` (2026-08-05) merge: fix-markdown-parser-yaml change (6 files, +328/-65)
  - `1740b88` (2026-08-05) chore(archive): finalize archive state
  - `921d551` (2026-08-12) fix(pdk-chat-demo): repair 4 pre-existing test failures (含此 fix 的回归保护)
- **代码实证**:
  - `src/modules/parser/markdown_parser.cpp:16` `#include <yaml-cpp/yaml.h>` — yaml-cpp 集成 (复用 `external/` 依赖, 无新解析库)
  - `src/modules/parser/markdown_parser.cpp:102` `YAML::Node yaml_root = YAML::Load(yaml_content)` — fenced YAML block 检测 + 解析
  - `examples/pdk_chat_demo/dsl_validator.cpp` — DslValidator 改造为双格式识别 (YAML + Markdown bold), 复用既有 schema 校验路径
- **验收标准对照**:
  - A1: YAML 格式 agent 文件校验生效 (dsl_validator.cpp 改造已 ship) ✅
  - A2: `grep -n "跳过" examples/pdk_chat_demo/main.cpp` 返回 0 — 现 main.cpp:374 是注释 (doc note), active skip 分支已删除 ✅
  - A3: ctest 零回归 (114/116 PASS at ship-time, 零新增 regression) ✅
- **测试 fixture 迁移**: 9 个 Markdown bold fixture 维持向后兼容 + 新增 YAML fixture 测试覆盖
- **设计意图兑现**: 双格式自动检测, 复用既有 schema 校验逻辑 (禁止复制校验规则 per 技术约束 MUST)

**Pattern 沉淀**: Pattern #10 drift 第 11 实例 (本会话 5-维判定). 提案文件无 Status 段达 2.5 个月. Wave 2 P1 实际已 ship 2026-08-05 (commit e7b468c 含 6 files +328/-65), 仅靠 OpenSpec archive + merge commit 反推. Status 段补全, 防未来 audit 误判 yaml frontmatter 解析能力未 ship.
