## ADDED Requirements

### Requirement: sprint-9-shipped-commits-tracked

Sprint 9 step 1 的 3 个 ship commit(`40008a5` `ce4358b` `bd936af`)MUST 有对应的 backing OpenSpec change 进行治理跟踪。本 change 是治理回填,**无新代码变更**,仅 spec 跟踪。

#### Scenario: 3 commit hash 在 tasks.md 中引用

- **WHEN** 阅读 `openspec/changes/sprint-9-handle-node-completion/tasks.md`
- **THEN** MUST 引用 `40008a5` / `ce4358b` / `bd936af` 三个 commit hash
- **AND** 3 个 commit 在 tasks.md 全部标 [x]

#### Scenario: 无新代码变更

- **WHEN** git diff 本 change vs main
- **THEN** MUST 无 src/ 或 include/ 下任何代码变更
- **AND** 仅有 proposal.md + tasks.md + specs/ 等治理文件

#### Scenario: 治理一致性

- **WHEN** `openspec list` 执行
- **THEN** 本 change MUST 出现在 active changes 列表
- **AND** ship 后立即 archive(Sprint 9 治理回填是一次性)
