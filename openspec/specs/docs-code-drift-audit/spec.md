# docs-code-drift-audit Specification

## Purpose
文档与代码系统性 drift 全面审计 + 修正(0 代码修改):4 类修正 — (1) `examples/agent_simple/simple.cpp` + `agent_loop.cpp` DEPRECATED 注释误标(澄清 `LlamaAdapter`/`InjaTemplateRenderer`/`extract_pathed_blocks` 仍存在),(2) `src/core/engine.h` 文件头自相矛盾(Task 19 部分完成,非 3/3), (3) plan F1-F4 self-audit 误标(输出仍为 `[N/N]` 字面占位),(4) 新增 ADR-0002/0004 impl-scope 澄清(EventBus/SecureToolRegistry 在 archive ADR,InMemoryBus/execution_policy 在当前实现)。
## Requirements
### Requirement: drift-detection-checklist

项目 SHALL 提供 `tools/docs_drift_audit.py` 脚本，自动检测下列 drift 类型并输出报告。每个 drift 项 MUST 包含：文件路径、行号、当前文本、建议修正。

#### Scenario: 检测 DEPRECATED 注释与实际 API 状态不符

- **WHEN** 脚本扫描 `examples/**/*.cpp`
- **THEN** 对每个 `⚠️ DEPRECATED API NOTE` 注释块
- **AND** 解析注释中提到的 API 类名
- **AND** 在 `src/` 下查找该类定义是否仍存在
- **AND** 若存在，标记为 "DEPRECATED 注释错误：API 仍存在"
- **AND** 若不存在，标记为 "DEPRECATED 注释准确"

#### Scenario: 检测 plan F1-F4 self-audit 误标

- **WHEN** 脚本读取 `.omo/plans/project-organization.md`
- **THEN** 解析 F1/F2/F3/F4 段落
- **AND** 检测每个段落是否同时存在 `Agent: <name>` + `(replaced by self-audit)` 字样
- **AND** 若存在，警告 "self-audit 不能视为真实 verification"
- **AND** 检测输出行是否含未填充占位符（如 `[N/N]`）

#### Scenario: 检测 engine.h includes 与声明不符

- **WHEN** 脚本扫描 `src/core/engine.h`
- **THEN** 解析 `#include` 行
- **AND** 对每个 `modules/` 或 `common/` include，验证文件头注释是否一致（line 7/12 vs line 22/32）
- **AND** 若不一致（自相矛盾），标记为 "engine.h 注释内部矛盾"

#### Scenario: 检测 ADR 声称实现但实际不存在

- **WHEN** 脚本扫描 `docs/adr/adr-NNNN-*.md`
- **THEN** 对每个 ADR，解析 "## 状态" 行的 ✅ 标记
- **AND** 若 ADR 描述了具体类（如 `EventBus`, `SecureToolRegistry`），在 `src/` 下 grep 该类定义
- **AND** 若 `✅ Approved` 但类不存在，标记为 "ADR 实现 drift：声称实现但类不存在"

### Requirement: drift-correction-template

修正 DEPRECATED 注释 MUST SHALL 使用下列模板结构：先列出 git history 证据（commit hash + stat 输出），再列出当前文件状态，最后给出建议保留/重起草。

#### Scenario: 修正 examples DEPRECATED 注释

- **WHEN** 需修正 `examples/<name>/<file>.cpp` 的 DEPRECATED 注释
- **THEN** 替换注释块内容为：
  ```
  // ⚠️ ACTUAL STATE NOTE (YYYY-MM-DD):
  // 本文件使用下列 API 的实际状态（基于 git history 审计）：
  // - <API1>: [EXISTS|DELETED] in <commit_hash>
  // - <API2>: [EXISTS|DELETED] in <commit_hash>
  // 实际编译错误: <具体错误>
  // 迁移路径: <未来 OpenSpec change 名称或链接>
  ```
- **AND** 不得删除实际编译错误的描述
- **AND** 不得使用 "removed in commit X" 这种未经验证的断言

### Requirement: adr-impl-scope-companion

存在 implementation drift 的 ADR SHALL 配对一个 `<adr-NNNN>-impl-scope.md` 文件，明确指出文档描述与代码现实的差异，但不修改原 ADR。

#### Scenario: 创建 adr-0002-impl-scope.md

- **WHEN** 检测到 ADR-0002 (EventBus) 描述的 `EventBus` 类在 `src/` 中不存在
- **THEN** 创建 `docs/adr/adr-0002-impl-scope.md`
- **AND** 包含下列章节：
  - "ADR 描述": 引用 ADR-0002 原文
  - "代码实际状态": grep 验证结果
  - "决策需求": 保留作为设计历史 OR 重新起草
  - "PENDING DECISION" 显式状态字段
- **AND** 不得修改 ADR-0002 原文

#### Scenario: 创建 adr-0004-impl-scope.md

- **WHEN** 检测到 ADR-0004 (ToolRegistry Security) 描述的 `PathPolicy`/`ShellGuard`/`SecureToolRegistry`/`ApprovalPolicy`/`ToolCategory` 在 `src/` 中不存在
- **THEN** 创建 `docs/adr/adr-0004-impl-scope.md`
- **AND** 结构同 adr-0002-impl-scope.md

### Requirement: self-audit-marking

plan 的 self-audit 段（F1-F4 风格）SHALL 用明确状态标记区分真实 verification 与自我声明。

#### Scenario: plan F-section 真实完成

- **WHEN** F-section 实际执行了 `cmake --build` / `ctest` / `grep` 等命令并附上输出
- **THEN** 可标 `[x]` + 引用输出文件路径

#### Scenario: plan F-section 自审声明

- **WHEN** F-section 仅声明 "self-audit" 而无执行命令或输出
- **THEN** MUST 标 `[ ]` + 注脚 "structural self-attestation; 需独立 agent 执行"
- **AND** 不得使用 `[x]` 标记

#### Scenario: 占位符检测

- **WHEN** F-section 输出行含字面占位符（如 `[N/N]`、`[APPROVE/REJECT]`）
- **THEN** MUST 标 `[ ]` + 警告 "output template not filled"

### Requirement: tech-debt-cleanup-modification

本 change SHALL 修改 `tech-debt-cleanup` capability，明确 Task 4 与 Task 19 的部分完成状态。

#### Scenario: tech-debt-cleanup spec delta

- **WHEN** 本 change 归档后
- **THEN** `tech-debt-cleanup` spec 新增 requirement: "Task 4 部分完成" 与 "Task 19 部分完成"
- **AND** 描述每个 task 的已完成子项与未完成子项

