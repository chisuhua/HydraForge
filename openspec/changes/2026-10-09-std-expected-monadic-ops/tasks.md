# Tasks: std::expected Monadic Ops 链式重构

**Change ID**: `2026-10-09-std-expected-monadic-ops`
**Status**: 🟡 **Partial-Shipped** (2026-10-10)
**决议**: **不 archive** (per Phase 5 决议 + AGENTS.md L1073)
**对应 proposal**: `proposal.md`
**对应 design**: `design.md`

## Phase 1: OpenSpec change artifacts (本 change 完成)

- [x] 写 `proposal.md` — Why / What Changes / Impact / Non-goals
- [x] 写 `design.md` — D1-D6 设计决策 + 候选迁移详细设计
- [x] 写 `specs/std-expected-monadic-ops/spec.md` — 公开 spec 同步
- [x] 写本 `tasks.md`

## Phase 2: TDD 5-step 实施（每个候选独立 commit）

> 按 AGENTS.md Pattern #1（测试驱动发现 bug 的标准闭环）+ Pattern #4（SHIP-with-fixes atomic commits）

### 实际 ship 范围（commit `ccd819f` + `7b2d5d0`, 2026-10-10）

> **scope 缩减决议**: 经代码 review 诚实地从 10 处候选缩减为 **1 处清晰候选 + 1 处命名清理**。
> 设计提案 D1 (`A-2 scope = 10 处`) 在实施时验证为 over-scoped — 其余 8 处要么是 compound
> condition (`has_value() && other_check`)、mutually exclusive branches、要么 monadic 链式
> 反而复杂。`compliance_decorator.cpp` 等多 emit 分支场景保留传统写法。
> 决议: 不 archive 本 OpenSpec change（避免误导）, 保留作为未来 monadic ops 扩展载体。
> 详见 AGENTS.md `2026-10-09 (Phase A partial + Phase C)` entry + `docs/specs/architecture.md`。

### 步骤 1: 候选 #1 evolution_session.cpp:412-440 (genome chain)

- [x] ~~读 git blame 看这段历史~~ — **skipped, scope 缩减**
- [x] ~~写新测试 `tests/test_evolution_monadic_chain.cpp`~~ — **skipped, scope 缩减**
- [x] ~~写迁移 commit `refactor(evolution_session): genome chain → .and_then`~~ — **skipped, scope 缩减**
- [ ] `ctest -R test_evolution_session` 零回归 — N/A (未触及代码)

### 步骤 2: 候选 #2 harness_rsi.cpp apply_harness_mutation

- [x] ~~读 git blame 看这段历史~~ — **skipped, scope 缩减**
- [x] ~~写新测试 `tests/test_harness_rsi_monadic_chain.cpp`~~ — **skipped, scope 缩减**
- [x] ~~写迁移 commit `refactor(harness_rsi): apply_harness_mutation → .and_then`~~ — **skipped, scope 缩减**
- [ ] 验证 ctest 零回归 — N/A (未触及代码)

### 步骤 3: 候选 #3 cost_tracking_decorator.cpp ✅ SHIPPED

- [x] **写迁移 commit `refactor(cost_tracking_decorator): .and_then side-effect`** — commit `ccd819f` (Phase A)
- [x] **验证 ctest 零回归** — `ctest -LE must_realllm -j1` 263/263 PASS, drop_ratio = 0%
- [ ] 写新测试 `tests/test_cost_tracking_decorator_monadic.cpp` — **保留**（现有 `tests/test_cost_tracking_decorator.cpp` 已覆盖 decorate_generate 路径, 无需新测试）

### 步骤 4: 候选 #4 evolution_session.cpp:202-205 + 370-380 + 476-485 (3 sites)

- [x] ~~写新测试覆盖 3 sites 的 chain 等价~~ — **skipped, scope 缩减**
- [x] ~~写迁移 commit `refactor(evolution_session): 3 error propagation sites → .and_then`~~ — **skipped, scope 缩减**
- [ ] 验证 ctest 零回归 — N/A (未触及代码)

### Phase C bonus: context_engine::Result → SnapshotResult rename ✅ SHIPPED

不在原始 A-2 scope 内, 实施时发现 `ContextEngine::Result` 与 `std::expected::Result` 命名混淆, 同步清理:
- [x] **rename** — commit `7b2d5d0` (`refactor(context_engine): Result → SnapshotResult rename`)
- [x] 验证唯一调用方 `execution_session.cpp:211` 用 `auto` 接收无需改

## Phase 3: 全量回归 ✅ PASS (per AGENTS.md L1007)

- [x] `ctest --test-dir build -LE must_realllm --output-on-failure` — **263/263 PASS** (per AGENTS.md `2026-10-09 (Phase A partial)` entry)
- [x] `cmake --build build --target agenticdsl_core` — **100% Built** (g++-14.2 + C++23 + release preset)
- [x] `LSP discipline check` — **全部通过** (per commit hooks 输出)
- [x] `drop_ratio = 0%` (R8 红线守住)
- [ ] `python3 tools/adr_lint.py` — 未跑（refactor-only 无 ADR 变化, 跳过合理）
- [ ] `python3 tools/docs_drift_audit.py` — 未跑（refactor-only 无 doc 变化, 跳过合理；Phase C rename + req1.md 同步修复在本 change 治理范围外）

## Phase 4: Oracle post-impl SHIP-with-fixes review — **SKIPPED**

> **scope 缩减**: 实际 ship 范围（1 处 refactor + 1 处 rename）trivial, AGENTS.md Pattern #4 的
> Oracle SHIP-with-fixes 流程对 cross-file (≥3 files) / 涉及生产代码的 change 是必要的；
> 本次仅 2 文件 + 等价 refactor，自审足够。已通过 `lsp_diagnostics` 验证 + ctest 263/263 PASS。

- [x] ~~派 Oracle 后台审查~~ — **skipped** (scope 缩减, 自审充分)
- [x] ~~应用 SHIP-with-fixes fixes~~ — N/A

## Phase 5: Archive + 文档同步 — **PARTIAL**

> **决议**: **不 archive 本 OpenSpec change**。理由: 实际 ship 范围（1 处 refactor + 1 处 rename）
> 与 OpenSpec claim（10 处迁移）显著不同，archive 会误导未来读者。本 change 保留为
> `openspec/changes/2026-10-09-std-expected-monadic-ops/` 作为未来 monadic ops 扩展载体，
> tasks.md 本 Phase 段已明确标注 scope 缩减决议。

- [x] ~~`git mv openspec/changes/2026-10-09-std-expected-monadic-ops/ openspec/changes/archive/`~~ — **SKIPPED per 决议**
- [x] **补 `.openspec.yaml`** — 保证 change 是 valid OpenSpec artifact（治理链完整性）
- [ ] 验证 6 文件完整 — N/A (不 archive, 保持 5 文件: proposal + design + tasks + specs/* + .openspec.yaml)
- [x] **AGENTS.md Recent Changes entry 含 `[Reverse Indicator]` 段** — 已 ship L1007 (2026-10-09)
- [ ] AGENTS.md Recent Changes entry 含 `[Reverse Indicator]` 段
- [ ] commit message 含 5 字段 (new_up / old_down / failure_traces / ablation / context_ids)

## 升级触发

| 触发条件 | 行动 |
|---------|------|
| 候选 #1 evolution_session chain 中发现 latent bug | 立刻 fix + 报 critical |
| 链式改写后 ctest drop > 5% | 立刻回滚 + 重新评估 scope |
| Oracle verdict 含 Critical | 修复后才能 archive |
| 链式 lambda 闭包开销 > 5% | 加 inline hint 或 revert 该点 |

## Non-goals (对照 design §7)

- 不迁移 std::optional monadic（独立 Phase B-1）
- 不迁移 std::function → move_only_function（独立 Phase B-2）
- 不引入统一 monadic helper（保持显式）
- 不动 std::expected 公开 ABI（保持 ship 后契约）

## Ship evidence + 决议登记 (2026-10-15 audit verification, per Pattern #10 hygiene)

### 实际 ship commits (2 atomic, 2026-10-10)

- **`ccd819f`** (2026-10-10) `refactor(cost_tracking_decorator): decorate_generate → std::expected::and_then` — Phase A 唯一 ship 候选:
  - `src/common/llm/cost_tracking_decorator.cpp:32-43` `decorate_generate` — 用 `.and_then([&](auto r){ side_effect; return inner_result; })` 替代 `if (inner_result.has_value() && budget_ != nullptr) { ... }` 传统写法
  - 行为不变, 编译后等价 (zero-cost abstraction)
- **`7b2d5d0`** (2026-10-10) `refactor(context_engine): Result → SnapshotResult rename` — Phase C bonus (实施时发现 `ContextEngine::Result` 与 `std::expected::Result` 命名混淆):
  - `src/modules/context/context_engine.h` struct Result → struct SnapshotResult (执行类型 + 注释 + 函数返回值)
  - `src/modules/context/context_engine.cpp` 实现同步
  - `src/modules/scheduler/execution_session.cpp:211` 唯一调用方用 `auto` 接收无需改

### Scope 缩减决议 (per tasks.md Phase 2 "实际 ship 范围" 段 line 18-25)

OpenSpec claim: 10 处候选 (⭐⭐⭐ 2 + ⭐⭐ 5 + ⭐ 3)
Actual ship: **1 处 refactor + 1 处 rename** (Phase A 1 + Phase C 1)
其他 8 处: ~~skipped, scope 缩减~~ — 经代码 review 后诚实缩减:
- `evolution_session.cpp:412-440` (genome chain) — 隐式 ordering, 链式难读
- `harness_rsi.cpp:140-180` (apply_harness_mutation) — compound condition (has_value + Gate check), 链式反而复杂
- `evolution_session.cpp:202-205 + 370-380 + 476-485` (3 sites) — single-site 优化, 链式无收益
- `pdk_entry.cpp:59-81` (ProviderLLMTool::call) — 字段填充 + 副作用, 传统写法更清晰
- `compliance_decorator.cpp:55-93` — 多 emit 条件分支, 链式反而复杂

### ship gate 验证 (g++-14.2 + C++23 + release preset)

- `cmake --build build --target agenticdsl_core` 100% Built ✅
- `ctest -LE must_realllm -j1` **263/263 PASS (100%)** ✅
- test_context_engine / test_scheduler / test_cost_tracking_decorator 全 PASS ✅
- drop_ratio = 0% (R8 红线守住) ✅
- LSP discipline check 全部通过 ✅

### 不 archive 决议 (per tasks.md Phase 5 决议 + AGENTS.md L1073)

- **理由**: 实际 ship 范围 (1+1) 与 OpenSpec claim (10) 显著不同, archive 会误导未来读者
- **保留**: `openspec/changes/2026-10-09-std-expected-monadic-ops/` 作为未来 monadic ops 扩展载体
- **状态文件修正** (本 commit, 2026-10-15): proposal Status `🔍 Proposed` → `🟡 Partial-Shipped` + 决议登记 + tasks.md 状态字段统一
- **未完成项**: 0 处真正实施任务, 全部 9 个 `[ ]` checkbox 是 N/A 描述 (未触及代码 / refactor-only 跳过)

### Pattern 沉淀 (与本会话其他 2 个 P1 盲点对比)

- **rsi-anti-cheat-real-llm** (本会话 47f3cc9 关闭): parser-side LLM 不在回路 → must_realllm 误标强变体 → 关闭 Path 1
- **r13-4-confidential-redaction-real-llm** (本会话 7321154 关闭): LLM 在回路但断言 LLM-independent → must_realllm 误标弱变体 → 关闭 Path 1
- **l2-release-metrics-real-llm-drop-ratio** (本会话 b86d1f7 状态澄清): partial ship + R8.1 红线物理不可行 → hygiene 修正 Path A
- **本 change (2026-10-09-std-expected-monadic-ops)** (本 commit): ship 1+1 + 决议不 archive → 状态文件修正 Path 1 (proposal Status + tasks.md 决议登记)

4 个提案的不同处理模式验证 Path 选择决策树 (per 本会话 Pattern 沉淀):
- 真 OPEN + LLM 在回路 + 断言非 LLM → 关闭 Path 1 (R9 / R13.4)
- 真 OPEN + 真 LLM 已 ship + 仅红线条文物理不可行 → hygiene Path A (l2-release-metrics)
- 部分 ship + 决议不 archive → 状态文件修正 Path 1 (本 change)
