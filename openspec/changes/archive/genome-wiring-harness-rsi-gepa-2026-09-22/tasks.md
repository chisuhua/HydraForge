# Genome Wiring — Tasks (v2, post dual-agent review)

## 1. Setup / 准备
- [ ] 1.1 确认 `harness-rsi-remove-governance` 已 ship + archive（前置硬依赖）；在其 commit 上 rebase
- [ ] 1.2 读取 `harness_rsi.h/.cpp` + `genome.h` + `registry_filesystem.h/.cpp` + `gepa_loop.h/.cpp` 确认当前签名
- [ ] 1.3 确认 `MutationGateContext` 构造点：**仅 `test_harness_rsi_pilot.cpp`**（gepa_loop.cpp 用 governor 的 MutationContext，不构造 MutationGateContext）
- [ ] 1.4 确认 ADR-0068 Appendix A 当前版本（v2.2）与 `genome.*` 主题未登记；确认无可搭车 active follow-up（`2026-09-20-adr-0068-appendix-a-evolution-themes` 已 archive）——本 change 直接修订

## 2. RED — 测试先行
- [ ] 2.0 **test_harness_rsi_pilot.cpp 顶部加 hermetic env fixture**：`setenv("HYDRAFORGE_GENOME_KEY", "test_key_wiring_...", 1)`（复制 `test_genome_registry.cpp:30-41` 模式）——防依赖宿主机 key（C1 掩盖机制复现）
- [ ] 2.1 Case 7a: registry 注入（真实 FilesystemGenomeRegistry + tmpdir + seed root v1）+ mutation 成功 → `committed_genome_version == fork 返回值`（fresh lineage 下 == 2，用返回值断言非写死）+ `genome.committed` 事件 + `load(name, N)` 可回读
- [ ] 2.2 Case 7b: registry 注入 + `parent_version` 指向不存在版本（如 99）→ `RegistryRejected` + prompt/tools/registry 零变更 + `genome.persist_failed`（error="NotFound"）
- [ ] 2.3 Case 7c: `genome_registry == nullptr` → 行为与 V1 逐字节一致（既有 9 cases 零修改通过，`committed_genome_version == 0`）
- [ ] 2.4 Case 7d: tools 最终态断言（MockRegistry 或真实 FS）— `tools={A,B}` + add C + remove A → `load().spec.tools == {B,C}`（整体替换回归守卫）
- [ ] 2.5 Case 7e: 最终态 tools 为空（remove 全部）→ `InvalidMutation` + 零状态变更（V1 边界）
- [ ] 2.6 Case 7f: `parent_version == 0` → `InvalidMutation` + 零状态变更
- [ ] 2.7 Case 7g: workflow_patch + registry 注入 → `UnsupportedVariant` + 磁盘零新增版本（Gate 0 顺序守卫）
- [ ] 2.8 Case 7h: 缺 `genome_name`（registry 非空）→ `InvalidMutation` + 零事件发射（含 readiness.denied 亦不发射）
- [ ] 2.9 Case 8: `undo_applied_mutation` 后 system_prompt/tools 等于 AppliedMutation 快照；`registry.has_tool(removed) == false`（V1 限制断言）
- [ ] 2.10 Case 8b: undo 后 `committed_genome_version` 保持原值（锚点保持）
- [ ] 2.11 Case 9 (GEPA, 扩 test_gepa_phase2): registry 注入 + fork 成功 + commit 成功 → `version_id == "gepa_skill@N"` + `load().spec.harness == candidate.compiled_content` + `gepa.commit.committed.genome_version == N` + `mutation.committed` 与 `gepa.commit.committed` 版本一致（persist-then-commit 守卫）
- [ ] 2.12 Case 9b (GEPA): fork 失败 → `gepa.commit.denied(genome_persist_failed)` + `success == false` + candidate_skills 空
- [ ] 2.13 Case 9c (GEPA): `Config.genome_registry == nullptr` → `version_id == reflection_id`（V1 回归）
- [ ] 2.14 L4 emit-then-throw 回归守卫：mutation_kind="L4_weights" → `mutation.denied(l4_forbidden_v1)` 先于 throw + 无 genome.* 事件（镜像 test_mutation_governance.cpp 既有 L4 case）
- [ ] 2.15 运行 RED: 确认新测试 FAIL（功能未实现）

## 3. GREEN — 实现
- [ ] 3.1 `harness_rsi.h`: `MutationGateContext` 增加 `trace_id`（若 remove-governance 未带）+ `genome::IGenomeRegistry* genome_registry = nullptr` + `std::string genome_name` + `uint64_t parent_version = 0`（**末尾追加，顺序 MUST 为 trace_id → genome 字段，不重排既有字段**）
- [ ] 3.2 `harness_rsi.h`: `AppliedMutation` 增加 `uint64_t committed_genome_version = 0` + `std::string prompt_snapshot` + `std::vector<std::string> tools_snapshot`；声明 `undo_applied_mutation()`
- [ ] 3.3 `harness_rsi.cpp`: workflow_patch 检查上移 Gate 0（纯校验，与 InvalidMutation 并列）
- [ ] 3.4 `harness_rsi.cpp`: Gate 3 实现 — `genome_name` 空检查 + `parent_version==0` 检查（→ InvalidMutation）→ 构造最终态 `GenomeSpec`（harness 拼接 + tools 最终集；空 tools → InvalidMutation）→ `fork()` → 失败 `RegistryRejected` / 成功记录 version
- [ ] 3.5 `harness_rsi.cpp`: apply 时填充 `prompt_snapshot`/`tools_snapshot` + `undo_applied_mutation()` 实现（从 AppliedMutation 读快照恢复 + 文档化 V1 限制）
- [ ] 3.6 `harness_rsi.cpp`: 发射 `genome.committed` / `genome.persist_failed`（meta.trace_id 来自 `ctx.trace_id`）
- [ ] 3.7 `gepa_loop.h`: `Config` 增加 `std::shared_ptr<genome::IGenomeRegistry> genome_registry = nullptr` + `genome_name` + `parent_version`
- [ ] 3.8 `gepa_loop.cpp`: **persist-then-commit** — fork 在 `:181` 前，成功后改写 `proposal.version_id = name@N` 再调 `governor_->commit`；fork 失败走 `gepa.commit.denied`；`gepa.commit.committed` payload 增加 `genome_version`
- [ ] 3.9 运行 GREEN: 全部新测试 + 既有 `test_harness_rsi_pilot` 9 cases / 43 assertions + `test_gepa_phase2` PASS

## 4. REFACTOR / 收尾
- [ ] 4.1 检查 persist-before-apply 后"失败路径零状态变更"不变量仍成立（Gate 3 失败不污染 prompt/tools/registry；Gate 0 的 workflow_patch 检查先于 Gate 3）
- [ ] 4.2 注释更新: `harness_rsi.h` Gate 顺序说明（含 Gate 0 workflow_patch）+ `undo_applied_mutation` 的 V1 registry 限制 + `gepa_loop.cpp` persist-then-commit 分支 + GenomeSpec.harness 语义（系统提示词全文，非路径）
- [ ] 4.3 `lsp_diagnostics` 全部改动文件零错误

## 5. VERIFY — 验证
- [ ] 5.1 focused ctest: `ctest --test-dir build -R "test_harness_rsi_pilot|test_gepa_phase2|test_genome_registry|test_genome_walk_ancestors" --output-on-failure` → 100% PASS
- [ ] 5.2 全量 `ctest -N` 计数确认（预期无新增 binary，测试并入既有文件）
- [ ] 5.3 回归确认: 相关测试 100% PASS，0 新回归（pre-existing failures 不变）
- [ ] 5.4 `openspec validate genome-wiring-harness-rsi-gepa --strict` → "Change is valid"
- [ ] 5.5 ADR-0084 红线断言全绿: revert audit-only / L4 emit-then-throw（可测 case 2.14）/ 门禁顺序 / 白名单 fail-closed

## 6. DOCS / 治理
- [ ] 6.1 **直接修订** `docs/adr/adr-0068-event-emission-contract.md` Appendix A (Amendment v2.3): 登记 `genome.committed` + `genome.persist_failed`；验收 `grep -c "genome.committed" docs/adr/adr-0068-event-emission-contract.md >= 1` + `grep -c "genome.persist_failed" ... >= 1`
- [ ] 6.2 `docs/architecture/self-evolution-architecture-2026-08.md`: §五 加 Genome Registry 接线行 + §三 闭环第 7 环标 ✅；验收 `grep -c "第 7 环.*✅" docs/architecture/self-evolution-architecture-2026-08.md >= 1`
- [ ] 6.3 `docs/research/rsi-three-operators-hydraforge-mapping-2026-09.md`: 闭环第 7 环断裂说明改为"✅ 已接线"
- [ ] 6.4 `docs/guides/agent-collaboration-patterns.md` §10: ADR-0086 状态翻牌 + Genome Registry 提及
- [ ] 6.5 `docs/active-status.md` + AGENTS.md Recent Changes 同步（change ship + archive）
- [ ] 6.6 archive change（4 文件完整 per AGENTS.md Day 5 lesson）
