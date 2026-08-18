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
