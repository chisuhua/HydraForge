# Proposal: Fix Flatten Layers Comment Drift

> **STATUS: PLACEHOLDER** ⚠️
> **触发**: F1 `fix-react-decide-empty-response` SHIPPED 后 (2026-09-18) 残留 drift
> **追溯范围**: `docs/roadmap/2026-09-16-pdk-chat-demo-evolution-roadmap.md` §四 F1 子节 + §十 Drift Log + 各种 code comments
> **优先级**: P3 (cosmetic drift, 低风险)
> **估时**: 30 min
> **关联 ADR**: 无 (纯 drift fix)

---

## Why（背景概要）

**F1 SHIP 期间** (2026-09-18, Oracle `ses_f4d05cdb0ffe0BhMdEADyfsdTz` 评审纠正初判):

**初判根因 (错)**: `flatten_layers` 嵌套导致 `{{llm_response}}` 顶层访问失效。

**真实根因 (Oracle 纠正)**: `DSLEngine::run(LayeredContext)` 走 `scheduler.execute(ctx.working)` 透传 flat 顶层 keys; `flatten_layers` 不在 react 路径. 真实根因 = think 节点 (`llm_call`) 写入 output_key 的值为空字符串 → inja 静默渲染为空 → `decide_react` 收到空 → `Missing 'response' argument`。

**残量 drift**:
1. **Code comments**: 部分源码注释可能仍引用 `flatten_layers` 嵌套作为原因（特别是 `node_executor.cpp` write-keys 处）
2. **Plan docs**: master plan §十 Drift Log 第 N 行初判描述保留 "flatten_layers" 字样（仅作为审计追溯 OK，但 cross-reference 应明示 "初判错"）
3. **AGENTS.md AGENTS pattern**: `pattern #1 step 4` 提"系统性记录同类潜伏站点"，未明确区分初判与确认根因
4. **Test comments**: `tests/test_dsl_engine_ctx_bridge.cpp` 测试注释可能保留初判描述

**没有此 fix 的后果**:
- 未来维护者 grep `flatten_layers` + react 会找到错关联
- 知识传递成本升高（误传"flatten_layers 是 react 路径 bug"）
- DRIFT 累积（虽然 low risk，但数量起来影响 docs_drift_audit 0 DRIFT gate）

---

## What Changes（具体变更范围）

### In Scope

- **Code comments**: 修 `src/modules/executor/node_executor.cpp` 任何仍引用 `flatten_layers` + react 路径的注释（如有）
- **Plan docs**: master plan §十 Drift Log 初判描述追加 "【初判错】" 标签
- **AGENTS.md**: Pattern #1 step 4 追加明确说明 "初判与确认根因分离"（借鉴 Oracle 评审实战）
- **Test comments**: 复审 `tests/test_dsl_engine_ctx_bridge.cpp` + `tests/test_react_loop_real_llm.cpp` 注释，移除初判描述（如有）
- **active-status.md**: 检查无 flatten_layers drift（应该已同步）

### Out of Scope

- `flatten_layers` 函数本身的语义/实现（语义正确，无 bug）
- F1 main fix（已 ship）
- 新功能/重构

---

## Acceptance（验收标准）

### D1 Code comment audit
- [ ] grep -rn "flatten_layers" src/ — 仅在 `include/agenticdsl/types/context_flatten.h` + `src/core/types/context.cpp` 等定义/使用处出现
- [ ] react/loop 相关路径无 "flatten_layers" 引用
- [ ] 任何注释引用 flatten_layers + react 应明确标记 "误关联/初判错"

### D2 Plan docs alignment
- [ ] master plan §十 Drift Log 初判描述加 "【初判错】" 标签
- [ ] active-status.md F1 节无 flatten_layers 字样（已 ship 状态）

### D3 AGENTS.md pattern refinement
- [ ] AGENTS.md Pattern #1 step 4 追加 "初判与确认根因分离" 子条目
- [ ] 引用 Oracle `ses_f4d05cdb0` 作为正例

### D4 Test comment audit
- [ ] tests/test_dsl_engine_ctx_bridge.cpp 无 flatten_layers 引用（已 ship 的 fix-related 代码不引用初判）
- [ ] tests/test_react_loop_real_llm.cpp 注释无 flatten_layers 引用

### D5 Docs drift gate
- [ ] tools/docs_drift_audit.py 0 DRIFT items
- [ ] openspec validate --strict "Change is valid"

---

## Capabilities（能力影响）

无新能力/接口变更。仅文档/comments drift fix。

---

## Impact（影响面）

- **代码层**: 0 行代码变更，仅 comments
- **测试层**: 0 行测试变更
- **文档层**: master plan + AGENTS.md + active-status 微调（< 50 lines）
- **风险**: 极低（comments 不影响运行时行为）
- **回归**: 无（purely cosmetic）

---

## Tasks（执行步骤，~30 min 估时）

### 1. Code comment audit (~10 min)
- [ ] grep `flatten_layers` src/ — 列出所有引用
- [ ] 验证 react/loop 路径无引用（如有，标记为"初判错" 或移除）
- [ ] 验证 tests/ 无引用（如有，移除）

### 2. Plan docs alignment (~5 min)
- [ ] master plan §十 Drift Log 检索 "flatten_layers" 行 → 加【初判错】标签
- [ ] active-status.md F1 节核对

### 3. AGENTS.md pattern refinement (~10 min)
- [ ] Pattern #1 step 4 子条目 "初判与确认根因分离" 追加
- [ ] 引用 Oracle `ses_f4d05cdb0` 作为正例

### 4. Docs drift gate (~5 min)
- [ ] docs_drift_audit.py 0 DRIFT
- [ ] openspec validate --strict clean

---

## Reference

- F1 archived: `openspec/changes/archive/2026-09-18-fix-react-decide-empty-response/`
- Oracle session: `ses_f4d05cdb0ffe0BhMdEADyfsdTz` (corrected root cause)
- AGENTS.md Pattern #1: 测试驱动发现生产 bug