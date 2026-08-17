## Context

ADR-0072 DSL 节点扩展含 D2 (`$var` 变量引用)、D3 (declarative style `exec: [...]`)、D5 (双语法共存期) 三件决策，按 ADR-0074 §决策 D4 三层阈值**条件性** ship。本 change 是 ADR-0072 的 C5+C6+C7 实施层落地，由 `from-roadmap-phase-6c-evidence-gate` 提案决议**直接触发**——Evidence Gate PASS → 全 descope；CONDITIONAL → ship C6；FAIL → ship C5+C6。

当前 DSL parser 缺口（per `src/modules/parser/markdown_parser.{h,cpp}` 现状）：
- `$var` token 解析器不存在——baseline DSL 节点引用走完整 `node_output_b` 字符串路径，无 `$` 前缀 compact syntax；
- `exec: [...]` declarative style 解析器不存在——并行节点组全部需手写 fork/join，无语法糖；
- 双语法共存期基础设施不存在——一旦 D2/D3 ship，无 lint 工具区分 legacy 与新语法，仅能靠人工 review；
- `lib/prompts/fewshot/`（由 `from-roadmap-phase-6c-execution-baseline` 提案建立）schema 集成路径未定义。

依赖链（per roadmap.md line 275-277）：
```
W2 → C1 → C2 → C3 → C4 (Evidence Gate, 独立 change) ──┐
                                                       ├── 分支: <85% →C5 ──┐
                                                       ├──── 85≤x<90 →C6 ──┤──→ C7
                                                       └──── ≥90% → descope 全空
```

C5+C6+C7 总 ~16h conditional（按 Evidence Gate 决议分支）：
- ≥90% → descope 全空（任务清零）；
- ≥85% → ship C6+C7（8h）；
- <85% → 三件全 ship（16h）。

## Goals / Non-Goals

**Goals:**

1. **C5** (ADR-0072 D2 `$var` 解析): `src/modules/parser/markdown_parser.{h,cpp}` 新增 `$var` token 识别 + node-reference 替换规则 + 与 `lib/prompts/fewshot/` schema 集成（few-shot example 内 `$var` 也需正确解析）。
2. **C6** (ADR-0072 D3 declarative style): `src/modules/parser/declarative_style.{h,cpp}` 实施 `exec: [...]` 语法解析 + 自动 fork/join DAG 包装（与手写 fork/join 语义等价）。
3. **C7** (ADR-0072 D5 双语法共存期): `src/modules/parser/dual_syntax_lint.cpp` lint 工具（双语法检测 + line-level 警告 + `# lint:disable dual-syntax` 行级豁免）。
4. **测试**: `tests/test_dsl_extensions.cpp` 3 类扩展测试（`$var` 等价性 / `exec:` fork/join / lint 警告），≥5 case / ≥10 assertion 覆盖。
5. **文档**: `docs/specs/dsl.md` §6 新章节（变量引用规范 + declarative style 规范 + 共存期迁移说明 + lint disable 注释规范）。
6. **向后兼容性**: baseline DSL 100% 兼容——所有现有 `.agent.md`（含 7 个 examples + 全部 examples/ 目录）在 C5/C6/C7 ship 后**无修改**通过解析（lint 警告可接受，parse error 不可接受）。
7. **架构合规性 + 零回归**: `ctest --output-on-failure` 全量零回归（147/147 baseline + 新增 `test_dsl_extensions` PASS）。

**Non-Goals:**

- C1-C4 baseline prompt 实施（C4 Evidence Gate 决议由 `from-roadmap-phase-6c-evidence-gate` 提案实施）。
- 条件触发的判定逻辑（D2/D3 是否 ship 由 C4 决议直接决定，不在本提案权限内）。
- 旧 syntax 废弃时间表（ADR-0072 §不变量 3 强制新语法使用率 ≥50% 才评估废弃时机，超前废弃视为违规）。
- 复杂表达式支持（仅变量引用 + literal，不支持函数调用或算术运算）。
- LLM fine-tune 衔接（Phase 8b gated by ADR-0074 D6）。

## Decisions

### D-1. `$var` 严格匹配 output 节点命名空间

**决策**: `$var` 严格匹配 ParsedGraph 节点的 `name` 字段（output 节点），不与 input 字段或 local variable 重名冲突。冲突时抛 `ParseError` 而非隐式选择。

**理由**: 隐式选择（隐式 fallback 到 input 字段）会引入 LLM 生成歧义，proposal Capabilities §MUST 强制显式拒绝。

### D-2. `exec: [...]` fork/join 语义等价

**决策**: `exec: [shell/exec, fs/read]` 语法糖展开后 DAG 与手写 fork/join 节点**逐边等价**——并行子节点全部 exit 时 join 节点 fire。

**替代方案拒绝**:
- 独立 thread pool 执行（破坏 DAG 调度一致性）
- 异步 + future 包装（增加调度复杂度，无收益）

### D-3. C7 lint 发警告而非错误

**决策**: C7 lint 在共存期内仅发 warning（exit code 0，stderr 含行号 + 修复建议），不阻断 commit。用户通过 `# lint:disable dual-syntax` 行级豁免。

**理由**: ADR-0072 §不变量 3 强制新语法使用率 ≥50% 才评估废弃；提前 error 级别会强制迁移违反渐进共存原则。

### D-4. C7 检测 heuristic 为"新语境下的 legacy 语法"

**决策**: C7 仅对 mtime 晚于 D2/D3 ship 时间戳 + git log 显示为新提交的 `.agent.md` 文件检测，避免对历史 shipped 文件重报。heuristic 实现为 `src/modules/parser/dual_syntax_lint.cpp` 内 `is_new_file(path)` helper。

**理由**: 历史 shipped `.agent.md` 不应被强制 lint（与 ADR-0072 §不变量 3 一致）；新提交的文件才有迁移义务。

### D-5. baseline DSL 100% 向后兼容

**决策**: 现有所有 `.agent.md` 文件（含 7 个 examples + examples/ 目录全部 fixture）在 C5/C6/C7 ship 后必须**无修改**通过解析。验证方法：CI 阶段跑 `scripts/verify_dsl_backward_compat.sh`（grep 现有 `.agent.md` 全部解析一遍）。

**理由**: proposal Capabilities §MUST 强制；breaking change 触发 ADR-0072 状态回退至 🔍 Proposed。

## Risks / Trade-offs

- **[Risk: `$var` 命名冲突导致 parse error 雪崩]** → Mitigation: 实施期附友好错误消息（含冲突字段名 + 修复建议）；CI 跑现有 `.agent.md` 验证零 regression。
- **[Risk: `exec:` fork/join 包装与手写 fork/join 不等价]** → Mitigation: 单元测试对比展开前后 DAG 边集合（≥10 节点对比），失败立即 ship gate 阻断。
- **[Risk: C7 lint 误报历史文件]** → Mitigation: D-4 heuristic 限制仅新文件检测；附 `--include-historical` flag 手动覆盖（默认 off）。
- **[Risk: 双语法共存期导致 code review 负担]** → Mitigation: C7 lint 集成 pre-commit hook（可选，不强制）；CI 阶段统一 lint。
- **[Risk: ship 时机错误（C4 决议未生效就启动 C5/C6）]** → Mitigation: 本 change tasks §10 强制 Evidence Gate ship gate 验证，未通过则 descope。
- **[Risk: 新语法使用率 <50% 导致 D5 共存期无限延长]** → Mitigation: 提议 follow-up Sprint 28+ 引入使用率测量 + 废弃评估（不在本 change 范围）。

## Migration Plan

1. `from-roadmap-phase-6c-execution-baseline` + `from-roadmap-phase-6c-evidence-gate` 两 change 必须先 ship。
2. Evidence Gate 决议：PASS → 本 change descope（任务清零）；CONDITIONAL → 仅 ship C6+C7；FAIL → ship C5+C6+C7。
3. 按分支实施相应 C5/C6/C7 任务，CI 阶段验证 baseline DSL 100% 向后兼容。
4. ship 后 `docs/specs/dsl.md` §6 章节更新 + `docs/active-status.md` §一 Phase 6c C5/C6/C7 行标记。

回滚策略：C5/C6/C7 各自独立 commit，可逐个 revert。revert 后 baseline DSL 仍 100% 兼容（无破坏性变更）。

## Open Questions

1. `exec:` 嵌套是否支持（`exec: [exec: [...]]`）？当前决策仅支持一层嵌套，递归留 Sprint 28+。
2. `$var` 是否支持跨文件引用（multi-file DSL）？当前仅单文件内节点引用，跨文件留 follow-up。
3. C7 lint 是否集成 clang-tidy 基础设施？当前独立二进制，集成 clang-tidy 留 follow-up。
