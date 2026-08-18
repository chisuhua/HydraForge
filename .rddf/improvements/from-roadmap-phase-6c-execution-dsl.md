# from-roadmap-phase-6c-execution-dsl

**优先级**: P0* | **来源**: from-roadmap (phase-6c/execution-dsl, ADR-0072 D2/D3/D5)
**阶段**: phase-6c | **分类**: execution-dsl
**类型**: functional
**主题**: $var变量；declarative语法糖；双语法共存

## 架构依据

ADR-0072 DSL 节点扩展含 D2/D3/D5 三件，按 Evidence Gate（C4）决议**条件性** ship——本提案覆盖 C5+C6+C7 的实施层落地：

- **D2 `$var` 变量引用**：`node_a → $output_b` 紧凑语法，降低 LLM 输出节点引用时的认知负载。
- **D3 declarative style**：`exec: [shell/exec, fs/read]` 语法糖，自动 fork/join 包装，让并行节点组更易读。
- **D5 双语法共存期**：D2/D3 任一触发后即强制 ship，提供 lint + 警告基础设施，让新旧语法并存而不破坏现有 `.agent.md`。

**条件触发逻辑**（per ADR-0072 §决策 D2/D3 / ADR-0074 §决策 D4 三层阈值）：
- `parse-valid ≥90%` → **C5 + C6 跳过**（baseline 已足够，避免过早抽象）
- `85% ≤ x < 90%` → C5 跳过 + **C6 ship**（临界带——LLM 已能生成 baseline，仅部分场景需 declarative 语法糖）
- `parse-valid <85%` → **C5 ship** + C6 可选（LLM 无法稳定生成 baseline，D2 `$var` 显著降低引用复杂度）

D2/D3 在 ADR-0074 baseline DSL 之上叠加（D5 在 D2/D3 触发后强制 ship——lint 警告而非报错，迁移归用户责任，ADR-0072 §不变量 3 强制）。

**依赖链**（per roadmap.md line 275-277）：`W2 → C1 → C2 → C3 → C4 (Gate)`，分支：`<85% →C5 ──┐`、`85≤x<90 →C6 ┴─→ C7`、`≥90% skip both`。总 ~16h conditional。≥90% → descope 全空；≥85% → ship C6+C7（8h）；<85% → 三件全 ship（16h）。

**本提案不** 覆盖 Evidence Gate 决议——该决策由独立 `evidence-gate` 提案实施（C4）。

## 范围

- **In Scope**:
  - C5: ADR-0072 D2 `$var` 解析——`src/modules/parser/markdown_parser.{h,cpp}` 新增 `$var` token + node-reference 替换规则 + `lib/prompts/` few-shot schema 集成。
  - C6: ADR-0072 D3 declarative style——`src/modules/parser/declarative_style.{h,cpp}` `exec: [...]` 语法解析 + 自动 fork/join DAG 包装。
  - C7: ADR-0072 D5 双语法共存期——`src/modules/parser/dual_syntax_lint.cpp` lint 工具（双语法检测 + line-level 警告 + `# lint:disable dual-syntax` disable 注释）。
  - `tests/test_dsl_extensions.cpp` 3 类扩展测试（`$var` 等价性 / `exec:` fork/join / lint 警告）。
  - `docs/specs/dsl.md` §6 新章节（变量引用规范 + declarative style 规范 + 共存期迁移说明）。
- **Out of Scope**:
  - C4 Evidence Gate 决议（独立 `evidence-gate` 提案）与 C1-C3 baseline prompt 实施。
  - 条件触发的判定逻辑（D2/D3 是否 ship 由 C4 决议直接决定，不在本提案权限内）。
  - 旧 syntax 废弃时间表、复杂表达式支持（仅变量引用 + literal）、LLM fine-tune（Phase 8b gated）。

## 关键场景

- GIVEN C4 Evidence Gate 决议 parse-valid ≥90% + task-success L1 ≥70%（PASS）
  WHEN 本提案 scope 评估
  THEN **descope C5 和 C6**——C7 仅在 D2/D3 任一 ship 时才有意义；两者皆跳则本提案 active 任务清零。

- GIVEN C4 Evidence Gate 决议 parse-valid <85%（FAIL）决议文档 §行动项 同步生效
  WHEN 决议生效
  THEN ship **C5**（D2 `$var`，8h），可同时 ship C6。

- GIVEN C4 决议 parse-valid = 87.5%（85% ≤ x <90% 临界带，CONDITIONAL）
  WHEN 决议文档 §临界带说明 记录
  THEN 仅 ship **C6**（D3 `exec:` declarative style，4h），不 ship C5。

- GIVEN C5（`$var`）和 C6（`exec:`）已 ship 进入共存期
  WHEN 用户 `.agent.md` 同时使用新旧两种语法
  THEN parser 两种语法均正确解析 + 等价语义；lint 对 legacy 部分（`$var` 缺失 / `exec:` 缺失）发警告（含行号 + 修复建议），不报错。

- GIVEN C7 lint 工具 ship + CI 流水线对新增/修改 `.agent.md` 运行 lint
  WHEN 启发式检测匹配"新语境下的 legacy 语法"
  THEN 对 legacy 部分发警告（含行号 + 修复建议）；既有 shipped `.agent.md` 不重报。

## 技术约束

- MUST C5/C6 ship 严格 gated by C4 Evidence Gate 决议——本提案仅在决议文档明确指向 C5/C6 ship 后才落地实施，无条件启动视为违反 ADR-0072 §决策 D2/D3 conditional 语义。
- MUST 保留 baseline DSL 100% 向后兼容——所有现有 `.agent.md` 文件在 C5/C6/C7 ship 后必须无修改通过解析（lint 警告可接受，parse error 不可接受）。
- MUST `$var` 严格匹配 output 节点命名空间——不与 input 字段或 local variable 重名冲突；冲突时抛 parse error 而非隐式选择。
- MUST `exec:` 自动 fork/join 包装保持原有边语义——并行子节点全部 exit 时 join 节点才 fire，与手写 fork/join 等价。
- MUST C7 lint 发警告（warning）而非错误（error）——共存期内不阻断 commit；用户通过 `# lint:disable dual-syntax` 行级豁免。
- MUST NOT 自动重写用户 `.agent.md` 文件——migration 是用户责任，工具仅 lint + 提示，不做 in-place 改写。
- SHOULD C7 检测 heuristic 为"新语境下的 legacy 语法"——文件 mtime 晚于 D2/D3 ship 时间戳 + git log 显示为新提交，避免对历史 shipped 文件重报。
- SHOULD 新语法使用率 ≥50%（post-ship 测量）才评估完全废弃旧语法的时机，超前废弃视为违反 ADR-0072 §不变量 3。

## 验收标准

- [ ] C5 完成：`markdown_parser.{h,cpp}` 新增 `$var` 解析；`test_dsl_extensions.cpp` 含 `→ node_output_b` 等价性测试（≥5 case / ≥10 assertion）。
- [ ] C6 完成：`declarative_style.{h,cpp}` 实施 `exec: [...]` → fork/join DAG；`test_dsl_extensions.cpp` 含与手写 fork/join 等价性比对。
- [ ] C7 完成：`dual_syntax_lint.cpp` lint 工具；输出含行号 + 修复建议；支持 `# lint:disable dual-syntax` 行级豁免。
- [ ] baseline DSL 向后兼容性验证：现有所有 `.agent.md`（含 7 个 examples）解析零变化，lint 警告（不错误）可接受。
- [ ] 双语法共存期工作：混合 `$var` + `exec:` + legacy 的 `.agent.md` sample 测试通过；lint 对 legacy 部分发警告，对新语法不报。
- [ ] `ctest --output-on-failure` 全量零回归（147/147 baseline 不变；新增 `test_dsl_extensions` 测试 PASS）。
- [ ] `docs/specs/dsl.md` §6 新章节文档已更新（$var / declarative style / 共存期迁移 / lint disable 注释规范）。
- [ ] 元数据：`docs/active-status.md` §一 Phase 6c 状态行同步更新 C5/C6/C7 ship 状态（P0* conditional 标注保留）。
