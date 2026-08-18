# from-roadmap-phase-6b-execution-dsl

**优先级**: P0 | **来源**: from-roadmap (phase-6b/execution-dsl, ADR-0072 D2/D3/D5)
**阶段**: phase-6b | **分类**: execution-dsl
**类型**: feature
**主题**: $var变量；declarative语法糖；双语法共存

## 架构依据

ADR-0072 DSL 节点扩展含 3 件，根据 Evidence Gate 决议条件性 ship：

- D2 `$var` 变量引用：`node_a → $output_b` 语法，让节点引用更紧凑。
- D3 declarative style：`exec:` 语法糖，让并行节点更易读。
- D5 双语法共存期：旧样式 + 新样式并行 + lint 警告。
- 条件触发：parse-valid < 85% → D2 ship；85% ≤ x < 90% → D3 ship；D5 强制 ship（D2 或 D3 触发后）。

## 范围

- **In Scope**:
  - `src/modules/parser/markdown_parser.{h,cpp}` 新增 `$var` 解析 + 节点引用替换。
  - `src/modules/parser/declarative_style.{h,cpp}` `exec:` 语法解析 + 转换为标准 DAG。
  - `src/modules/parser/dual_syntax_lint.cpp` lint 工具（双语法检测 + 警告）。
  - `tests/test_dsl_extensions.cpp` 3 类测试（$var 解析 / declarative 转换 / lint 警告）。
  - `docs/specs/dsl.md` § 6 新章节文档（变量引用 + declarative style + 共存期规范）。
- **Out of Scope**:
  - LLM 生成的语法偏好（依赖 prompt 训练，留 follow-up）。
  - 复杂表达式（仅简单变量引用 + literal）。
  - 旧 syntax 完全废弃（共存期内兼容）。

## 关键场景

- GIVEN DSL 节点定义含 `$output_b` 引用
  WHEN parser 解析
  THEN 替换为 `output_b` 节点的引用 ID，等价语义。

- GIVEN DSL 节点定义 `exec: [shell/exec, fs/read]`
  WHEN parser 解析
  THEN 转换为标准 DAG：`fork → [shell/exec, fs/read] → join`（自动 fork/join 包装）。

- GIVEN 同时存在 `$var` 与 `exec:` 风格
  WHEN parser + lint 验证
  THEN 双语法生效，lint 输出警告（旧风格 deprecated）。

- GIVEN parse-valid = 88%（临界带）
  WHEN C5+C6 ship 决议
  THEN D2 + D3 + D5 同时 ship，触发双语法共存期。

## 技术约束

- MUST `$var` 解析严格匹配 output 节点命名空间（不与 input 字段冲突）。
- MUST `exec:` 自动 fork/join 包装保持原有边语义。
- MUST 双语法共存期 lint 警告可关闭（`# lint:disable dual-syntax`）。
- MUST NOT 修改现有节点定义语法（向后兼容）。
- SHOULD 新语法使用率 ≥ 50% 后（基线 measurement）才考虑完全废弃旧语法。

## 验收标准

- $var + exec: 解析测试通过（与 baseline 语义等价）。
- lint 工具输出警告（含行号 + 修复建议）。
- DSL spec 文档更新。
- ctest 全量零回归。
- 阻塞 ADR-0072 D2+D3+D5 ship 决议（依赖 Evidence Gate）。