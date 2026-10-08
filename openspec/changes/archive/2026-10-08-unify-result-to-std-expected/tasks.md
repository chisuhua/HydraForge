# unify-result-to-std-expected Tasks

## 1. Setup & Audit Baseline

- [ ] 1.1 Verify C++23 + gcc-14 + std::expected 基线: `cmake --preset release` + `cmake --build build -j$(nproc)` 确认全量 build 零 error (基线 per `be0600e` 验证)
- [ ] 1.2 全量 ctest baseline 验证: `ctest --test-dir build -LE must_realllm --output-on-failure` 100% PASS (基线 ~215/215)
- [ ] 1.3 **grep audit (按模板分类计数, 非 magic 129)**: 列出 4 类 Result 使用点, 写入 `openspec/changes/unify-result-to-std-expected/audit-baseline.md` (per AGENTS.md Pattern #1 step 4):
  - `agenticdsl::Result<T,E>` 调用点 (LLM provider 链, 99 处) — MUST replace
  - `agenticdsl::genome::Result<T,E>` 调用点 (Genome registry, 30 处) — MUST replace
  - `ToolResult::success/failure` (46 处) — MUST NOT replace
  - `AgentResult<T>` 模板实例化 (iagent_composition.h, 数处) — out of scope
  - 简写形式 `Result::success/failure` (无模板实参, ~5 处) — MUST grep 单独清零
  - 文档引用 `Result<...>` (signatures + 描述, 多个文档位置) — 列入 6.2 清理范围
- [ ] 1.4 触发 24h cooling-off 周期 (per AGENTS.md Pattern #4); 起点 = `proposal.md` 完成时间 (2026-10-08T23:16Z), 到期 = 2026-10-09T23:16Z

## 2. LLM Result 替换 (Phase 1)

- [ ] 2.1 `src/common/llm/llm_types.h`: 删除 28 行 `template <typename T, typename E> class Result { ... }` (L83-110), 加 `#include <expected>`, 删除 `Result<...>` 类型别名
- [ ] 2.2 99 处 `Result<X, Y>::success(v)` → `std::expected<X, Y>{std::in_place, std::move(v)}` 批量替换 (主要在 `src/common/llm/cloud_adapter.cpp` / `src/common/llm/finetune_provider.cpp` / `src/common/llm/mock_llm_provider.cpp` / `src/common/governance/cross_cutting/decorator_pattern.cpp` / `include/agenticdsl/contract/i_llm_provider_decorator.h` / `include/agenticdsl/pdk/agent_loops/orchestration_illm_provider.h` / `include/agenticdsl/evolution/harness_rsi.h`)
  - **MUST use 精确 sed 模式** (避免 ToolResult::success 误伤, per M3-M): `s/Result<\([^>]*\), \([^>]*\)>::success/std::expected<\1, \2>{std::in_place, std::move(/g` (锚定 `Result<` 前缀)
  - **MUST NOT 用宽泛 grep** `\.success\(` (会命中 `ToolResult::success` 46 处 + `Result::success` 简写形式)
  - **MUST separately grep 简写形式** `Result::success` (无模板实参) — 数量与位置来自 1.3 audit
- [ ] 2.3 99 处 `Result<X, Y>::failure(e)` → `std::unexpected(std::move(e))` 批量替换 (同样 MUST 精确 sed 模式, MUST NOT 误伤 `ToolResult::failure`)
- [ ] 2.4 编译验证: `cmake --build build -j$(nproc)` 核心 tree 零 error
  - **[[nodiscard]] warning 检查** (per m3-M): `std::expected::value()` 是 `[[nodiscard]]` 而旧 `Result::value()` 不是, 任何忽略返回值的 `result.value();` 调用会新增 -Wunused-result warning. 若 -Werror 配置则变 error. MUST 验证 build 无新增 warning (or 显式标注已知 warning)
  - 仅允许 std::expected 自身的 libstdc++ deprecation / 模板诊断 warning
- [ ] 2.5 LLM provider 链测试: `ctest -R "test_provider|test_llm|test_cost_tracking" -LE must_realllm` 全 PASS

## 3. Genome Result 替换 (Phase 2)

- [ ] 3.1 `include/agenticdsl/genome/genome.h`: 删除 25 行 `template <typename T, typename E> class Result { ... }` (L32-56), 加 `#include <expected>`, 更新 6 个虚函数签名
- [ ] 3.2 30 处 `genome::Result<X, Y>::success(v)` → `std::expected<X, Y>{std::in_place, std::move(v)}` 批量替换 (主要在 `src/core/genome/registry_filesystem.cpp` / 测试文件)
  - **同样 MUST use 精确 sed 模式**: `s/genome::Result<\([^>]*\), \([^>]*\)>::success/std::expected<\1, \2>{std::in_place, std::move(/g`
  - **同样 MUST separately grep 简写形式** `genome::Result::success`
- [ ] 3.3 30 处 `genome::Result<X, Y>::failure(e)` → `std::unexpected(std::move(e))` 批量替换
- [ ] 3.4 编译验证: `cmake --build build -j$(nproc)` 核心 tree + `FilesystemGenomeRegistry` 零 error
- [ ] 3.5 Genome test 验证: `ctest -R "test_genome|test_registry_filesystem" -LE must_realllm` 全 PASS (预期 ≥13/13 + 10/10 + 22/22, 至少 45 cases)

## 4. Test 回归验证 (Phase 3)

- [ ] 4.1 **17+ 个涉及 Result 的 test binary** 全量重编 + 单独跑验证 100% PASS (per M1-O, 数量从 8 修正为 17+):
  - core tree (per `grep -l "Result<" tests/*.cpp`):
    - `test_provider_factory` / `test_provider_factory_concurrent` / `test_provider_register_dynamic_tool`
    - `test_loop_agent_plugin` (mock e2e)
    - `test_harness_rsi_pilot` (含 `Result<AppliedMutation, MutationError>`)
    - `test_genome_registry` (13 cases) / `test_genome_walk_ancestors` (10 cases)
    - `test_skill_interpreter` / `test_serializing_decorator` / `test_decorator_pattern`
    - `test_llm_streaming` / `test_gepa_phase2` / `test_credit_assignment`
    - `test_pdk_chat_session` / `test_context_compactor` / `test_simple_orchestrator` / `test_agent_composition`
    - `test_e2e_real_llm` (must_realllm, 仅在 API key 存在时跑)
  - examples tree (5 处): `mock_blocking_provider.h` 等, 含 Wave 4.7 的 `unique_ptr<Result<...>>` heap 包装
- [ ] 4.2 全量 ctest: `ctest --test-dir build -LE must_realllm --output-on-failure` 100% PASS (目标 ≥215/215, 零回归)
- [ ] 4.3 **函数级行为审计** (per M2-M + M2-O 双 agent 合并):
  - 审计全部 .value() 调用点 (≥99 处) 控制流路径上 has_value() 已保证 — 行级 grep "前一行有 check" **不足**, 必须函数级推理
  - 审计全部 .error() 调用点 (~30+ 处) 控制流路径上 !has_value() 已保证 — `std::expected::error()` 在 has_value()==true 时是 **UB** (vs 旧 Result 返回默认构造, 方向相反, per M2-O)
  - 抽查 12 处已通过 (per Oracle pre-impl): node_executor.cpp / harness_rsi / tracing_decorator / pdk_entry / registry_filesystem — 全部有 guard
  - **重点 audit `harness_rsi.cpp:218`** `fork_res.value().version` — Gate 3 顶部 has_value() 检查后, 函数中段再次 .value(), 多行间隔, 行级 grep 漏判; MUST 显式补 `if (!fork_res.has_value()) return failure(...)` 守卫
  - 若发现裸 .value() / .error() 无 check, 补 if-check (首选) 或加函数注释说明"失败时抛 std::bad_expected_access" (per result-type-contract spec)
- [ ] 4.4 **drop_ratio 验证 = 0%** (per AGENTS.md Reverse Indicator Rule R8.1 红线, 纯类型替换, 无行为变化)
  - **具体化验证手段** (per m4): 对比 baseline (1.2 实测 215/215) vs post-ship ctest 数, 数字相等即证据
  - 在 ship commit message `[Reverse Indicator]` 段的 `old_down: drop_ratio=0%` 字段填这个数, 保证可追溯

## 5. PDK 集成 (Phase 4)

- [ ] 5.1 **clean rebuild + 6 个 PDK .so 重编** (per M4-O ABI 静默错配风险):
  - **MUST clean rebuild** (返回值类型不参与符号 mangling, 增量 build 漏重编会 runtime 静默读错内存, 不产生 link error):
    - 选项 A (强推荐): `rm -rf build && cmake --preset release -DAGENTICDSL_BUILD_TESTS=ON && cmake --build build -j$(nproc)`
    - 选项 B (次选): 至少 `find build -name '*.so' -newer <commit_ts>` 验证 14 个 .so 全部刷新, 任何 mtime 未更新者手动 `rm` 后重 link
  - 6 个 PDK .so: loop_agent / provider_agent / chat_session / budget_agent / g1_coding_assistant / g3_knowledge_base, 零 link error
  - **mtime 验证**: `find build/pdk -name '*.so' -printf '%T@ %p\n' | sort` 应全部晚于 src/common/llm/llm_types.h 修改时间
- [ ] 5.2 PDK ctest 验证: `ctest --test-dir build/examples/pdk_chat_demo -LE must_realllm --output-on-failure` 100% PASS
- [ ] 5.3 (可选) examples tree 全量: `ctest --test-dir build/examples -LE must_realllm --output-on-failure` 100% PASS

## 6. 注释 + Spec + 文档收尾 (Phase 5)

- [ ] 6.1 更新 `include/agenticdsl/policy/path_policy.h:144` 注释, 删除"不使用 std::expected(C++23 才标准化)"过时说明, 改写为 **准确 rationale** (per m4-M):
  - 正确 rationale: `SecureToolRegistry::Result` 是 **bool + json + SecurityError 三态结果** (非纯错误容器), `std::expected` 单错误语义不匹配三态 (成功有 payload + 失败有 SecurityError + 需区分"检查通过但 payload 为空"), 故保留本地结构体
  - **不**写 "std::expected 单值模式与 SecurityError 多字段语义不完全对齐" — 这是错误 rationale (std::expected<T, SecurityError> 完全支持 4 字段, 实际三态的 bool 才是根因)
- [ ] 6.2 **扩展文档漂移审计** (per M5-M, 不只清理 ::success/failure):
  - 验证 1: `grep -rn "Result<.*>::success\|Result<.*>::failure" --include="*.cpp" --include="*.h"` 全工作区 = 0
  - 验证 2: `grep -rn "Result::success\|Result::failure" --include="*.cpp" --include="*.h"` 全工作区 = 0 (简写形式)
  - 验证 3: `grep -rn "Result<GenerationResult" docs/` 列出文档引用, 至少必须更新 `docs/specs/dsl.md:1410` (公开 spec 引用 `virtual Result<GenerationResult, LLMError>` 虚函数签名)
  - 验证 4: `grep -rn "agenticdsl::Result" docs/ architecture/ research/` 列出 ADR / plan / audit 引用, 评估哪些需同步
- [ ] 6.3 验证 `path_policy.h:144` 注释已更新: `grep -A 3 "SecurityError" path_policy.h | head -10` 输出新版本 (验证 6.1 实际写入)
- [ ] 6.4 更新 genome-registry spec: L5 Purpose 填写 (per m2, 当前是 TBD placeholder) + L128 variants 计数从 6 同步为 7 (per m1, 当前 spec 内部不一致)
- [ ] 6.5 更新 `proposal.md` / `design.md` 状态: 添加 ✅ ship 标记 + commit hash (archive 前)

## 7. Ship & Oracle Review (Phase 6)

- [ ] 7.1 atomic commit (per AGENTS.md 模式 #4): 1 个 commit, message 包含 `[Reverse Indicator]` 段 5 字段 (`new_up` / `old_down: drop_ratio=0%` / `failure_traces` / `ablation` / `context_ids`)
- [ ] 7.2 派 Oracle post-impl SHIP-with-fixes 审查 (session `subagent_type=oracle`, `run_in_background=true`), 收集 verdict (SHIP / SHIP-with-fixes / BLOCK) + 修正清单
- [ ] 7.3 应用 SHIP-with-fixes 修正 (若 Oracle 给出): 独立 atomic commit, 不 amend baseline
- [ ] 7.4 验证 ship gates: `cmake --build build` + `ctest -LE must_realllm` + `git log --oneline -1` + `git status --short` 干净 + `find build -name '*.so' -newer <commit_ts>` 14 个 .so 全更新
- [ ] 7.5 archive change: `openspec archive unify-result-to-std-expected` (自动 git mv 到 `archive/`, 保留 5 文件完整性, per AGENTS.md Day 5 4-file lesson)
- [ ] 7.6 更新 AGENTS.md Recent Changes 条目: 标注 ship 状态 + commit hash + drop_ratio = 0% + Reverse Indicator 5 字段

## 任务规模自检 (per instructions 2h 粒度)

| Task Group | 子任务数 | 估算总时长 | 验证 |
|---|---|---|---|
| 1. Setup | 4 | ~2h | 24h cooling-off 不可压缩 |
| 2. LLM 替换 | 5 | ~4h | 99 处替换主要靠精确 grep + sed (避免 ToolResult 误伤), 编译验证 ~30 min/cycle |
| 3. Genome 替换 | 5 | ~2h | 30 处比 99 处少, 编译验证相同 |
| 4. Test 回归 | 4 | ~4h | 17+ binary × 单独跑 + 全量 + 函数级 .value/.error 审计 + drop_ratio |
| 5. PDK 集成 | 3 | ~1.5h | clean rebuild (10 min) + 6 .so 重编 + ctest + mtime 验证 |
| 6. 注释 + 文档收尾 | 5 | ~1.5h | grep 4 套 (cpp/h/docs) + 注释 + spec 修订 + 状态更新 |
| 7. Ship + Oracle | 6 | ~3h | commit + Oracle 审查 + apply + archive + AGENTS |
| **总计** | **32** | **~18h** | 1 个工作日 + 24h cooling-off + 0.5 天 review |
