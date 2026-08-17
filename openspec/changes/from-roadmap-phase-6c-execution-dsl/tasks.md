## 1. 前置依赖验证 (ship gate)

- [ ] 1.1 验证 `from-roadmap-phase-6c-execution-baseline` change 已 ship（`docs/audits/<date>-execution-baseline-v1.md` 存在）
- [ ] 1.2 验证 `from-roadmap-phase-6c-evidence-gate` change 已 ship 且决议文档存在
- [ ] 1.3 读取 Evidence Gate 决议状态（PASS / FAIL / CONDITIONAL / ABORT）
- [ ] 1.4 若决议 ABORT → 整体 descope，本 change 仅保留任务清单作为未来 reference
- [ ] 1.5 决议 PASS → 仅 ship C7（如 D2/D3 已 ship）；否则 descope 全空

## 2. C5: ADR-0072 D2 `$var` 解析（仅 FAIL 分支 ship）

- [ ] 2.1 `src/modules/parser/markdown_parser.h` 新增 `$var` token 声明（regex: `\$[a-zA-Z_][a-zA-Z0-9_]*`）
- [ ] 2.2 `src/modules/parser/markdown_parser.cpp` 新增 `parse_var_reference()` 函数，将 `$var` 替换为 output node name
- [ ] 2.3 `lib/prompts/fewshot/` schema 集成：few-shot example 内 `$var` 也需正确解析（与 markdown_parser 共享 token 解析器）
- [ ] 2.4 命名冲突检测：`$var` 不与 input 字段或 local variable 重名，冲突抛 `ParseError` 含字段名

## 3. C6: ADR-0072 D3 declarative style 解析（FAIL 或 CONDITIONAL 分支 ship）

- [ ] 3.1 创建 `src/modules/parser/declarative_style.{h,cpp}`
- [ ] 3.2 实施 `exec: [shell/exec, fs/read]` 语法解析，识别 `exec:` key + 数组形式的子节点列表
- [ ] 3.3 自动 fork/join DAG 包装：解析后生成 fork 节点 + N 个子节点 + join 节点，边集合与手写等价
- [ ] 3.4 嵌套支持：仅一层 `exec: [exec: [...]]`（深度限制由 `max_exec_depth=1` 常量定义）

## 4. C7: ADR-0072 D5 双语法共存期（FAIL 或 CONDITIONAL 分支 ship）

- [ ] 4.1 创建 `src/modules/parser/dual_syntax_lint.cpp` lint 工具（独立可执行 `tools/dual_syntax_lint`）
- [ ] 4.2 实现双语法检测：legacy 语法（缺 `$var` + `exec:`）在新语境下使用 → warning 含行号 + 修复建议
- [ ] 4.3 实现 `# lint:disable dual-syntax` 行级豁免注释解析（行首注释 grep 命中即跳过该行警告）
- [ ] 4.4 实现 D-4 heuristic `is_new_file(path)`：mtime 晚于 D2/D3 ship 时间戳 + git log 显示为新提交
- [ ] 4.5 `--include-historical` flag 手动覆盖（默认 off）

## 5. 测试用例 (test_dsl_extensions.cpp)

- [ ] 5.1 创建 `tests/test_dsl_extensions.cpp` Catch2 测试文件
- [ ] 5.2 `$var` 等价性测试：手写 `→ node_output_b` vs `$var` 展开后 DAG 边集合相等（≥5 case）
- [ ] 5.3 `exec:` fork/join 等价性测试：手写 fork/join vs `exec:` 展开后 DAG 边集合相等（≥3 case）
- [ ] 5.4 lint 警告测试：legacy 语法 → warning 含行号；豁免注释 → 无 warning；新文件 → 检测 + 报告
- [ ] 5.5 嵌套 `exec: [exec: [...]]` → 深度超限抛 `ParseError`
- [ ] 5.6 命名冲突：`$var` 与 input 字段同名 → 抛 `ParseError`

## 6. 向后兼容性验证 (CRITICAL)

- [ ] 6.1 创建 `scripts/verify_dsl_backward_compat.sh`：遍历 `examples/**/*.agent.md` + `lib/**/*.agent.md` 全部解析一遍
- [ ] 6.2 所有现有 `.agent.md` 在 C5/C6/C7 ship 后必须无修改通过解析（lint warning 可接受，parse error 不可接受）
- [ ] 6.3 失败 → ship gate 阻断，强制 revert + 重新实施
- [ ] 6.4 7 个 examples (agent_basic / agent_simple / agent_loop / slice_01_tool_call / phase1_* / pdk_chat_demo) 全部 baseline fixture 通过

## 7. 文档更新 (docs/specs/dsl.md §6)

- [ ] 7.1 `docs/specs/dsl.md` §6.1 新章节：`$var` 变量引用规范（语法 + 命名空间 + 冲突处理）
- [ ] 7.2 `docs/specs/dsl.md` §6.2 新章节：`exec: [...]` declarative style 规范（语法 + fork/join 展开 + 嵌套限制）
- [ ] 7.3 `docs/specs/dsl.md` §6.3 新章节：双语法共存期迁移说明 + `# lint:disable dual-syntax` 注释规范
- [ ] 7.4 §6.4 新章节：C7 lint 工具使用说明（CLI + flag + 与 CI 集成建议）

## 8. 架构合规性 + ctest 零回归

- [ ] 8.1 grep 验证 `src/modules/parser/markdown_parser.cpp` 新增 `$var` 解析但**不修改** baseline parser 路径（additive-only）
- [ ] 8.2 grep 验证 `src/modules/parser/declarative_style.cpp` 仅在 `exec:` key 触发，baseline 解析路径零变化
- [ ] 8.3 `ctest --output-on-failure` 全量零回归（baseline 147/147 不变；新增 `test_dsl_extensions` ≥8 case 全 PASS）
- [ ] 8.4 `scripts/verify_dsl_backward_compat.sh` exit 0（所有现有 `.agent.md` 解析通过）

## 9. 元数据 + 文档同步

- [ ] 9.1 更新 `docs/active-status.md` §一 Phase 6c 状态行（C5/C6/C7 按 Evidence Gate 分支 ship 状态）
- [ ] 9.2 如 C5 ship → ADR-0072 状态字段保持 🟡 Partial（D2 已 ship）；如 C5+C6 全 ship → ADR-0072 翻牌 ✅ Approved
- [ ] 9.3 ship commit message 引用 Evidence Gate 决议文档 URL（evidence-based ship gate 强制）

## 10. ship gate 综合验证

- [ ] 10.1 `scripts/sprint-closeout.sh` Step 6 验证 `verify_dsl_backward_compat.sh` 全部通过
- [ ] 10.2 ADR-0072 状态字段同步（按 D-5 §分支决策）
- [ ] 10.3 phase-6c-evidence-gate change 已 ship + Evidence Gate 决议文档 git-tracked + 24h 内 active-status 同步
- [ ] 10.4 整体 ship 顺序：execution-baseline → evidence-gate → execution-dsl（本 change）
