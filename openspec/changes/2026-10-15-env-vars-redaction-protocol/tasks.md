# Tasks: ADR-0072 D4 env_vars 字段 secrets redaction 协议

> **关联 proposal/design**: `proposal.md` (5-segment) + `design.md` (8 decisions + 风险回滚 + 1-2 day timeline)
> **实施阶段**: TDD 5-step (per AGENTS.md Pattern #1)
> **Cooling-Off**: 24h 至 2026-10-16T17:50:00Z (per ADR-0049 default)
> **关联 OpenSpec**: R13.4-redaction-bug-fixes (parallel, commit dbd01d6) — env_vars 与 R13.4 redaction 正交, R13.4 修 turn_input/response, 本 change 修 env_vars

## Phase 1: RED (test-only) — 失败测试先写

### 1.1 测试 binary 注册
- [ ] `tests/CMakeLists.txt` 添加 `add_catch_test(test_redact_env_vars)` (per `tests/test_helpers/real_llm_env.h` 模式)
- [ ] `tests/test_redact_env_vars.cpp` 创建 (空文件, 仅 header include + Catch2 test case 框架)

### 1.2 6 cases RED 验证
- [ ] Case 1 happy path: `redact_env_vars({PATH, HOME}, {API_TOKEN: secret_abc123})` → `API_TOKEN: [REDACTED]`
  - assert: `result.contains("PATH")` + `result["PATH"] == "/usr/bin"` + `result["API_TOKEN"] == "[REDACTED]"`
- [ ] Case 2 nested: `redact_env_vars({PATH}, {DB: {password: secret}})` → `DB: {password: [REDACTED]}`
  - assert: `result["DB"]["password"] == "[REDACTED]"` (recursive)
- [ ] Case 3 mixed: `redact_env_vars({PATH, HOME}, {PATH: /usr/bin, API_TOKEN: secret, DB_PASSWORD: pwd})` → PATH 保留 + 其他 redact
  - assert: keys 列表完整 + 白名单 value 保留 + 非白名单 value redact
- [ ] Case 4 audit event: `redact_env_vars` 触发 `env_vars_audit` event emit (per ADR-0068 Appendix A v2.5)
  - assert: InMemoryBus capture event `env_vars_audit` + payload 含 keys 列表不含 values
- [ ] Case 5 backward-compat: `redact_env_vars({"*"}, {API_TOKEN: secret})` → value 保留 (默认 `*` 行为)
  - assert: `result["API_TOKEN"] == "secret"` (默认 `*` 不 redact)
- [ ] Case 6 performance: 1000 iter × 平均耗时 <10µs/op
  - assert: `chrono::high_resolution_clock::duration < 10µs` (per AC-1 Case 6)

### 1.3 RED 验证 (per Pattern #1 step 1)
- [ ] `cmake --build build` exit 0 (空 test binary 可编译)
- [ ] `ctest -R test_redact_env_vars` FAIL 6/6 (per RED 验证, helper 还不存在)

## Phase 2: GREEN (impl-only) — 最小实现通过测试

### 2.1 公开 API + 实现
- [ ] `include/agenticdsl/contract/redact_env_vars.h` 新建
  - 公开函数: `nlohmann::json redact_env_vars(nlohmann::json metadata, const std::unordered_set<std::string>& allowed_env_vars = {"*"})`
  - 注释: "Per ADR-0068 §3.1 spirit, env_vars 字段整字段替换为 [REDACTED]"
- [ ] `src/common/security/redact_env_vars.cpp` 新建 (~40 lines)
  - recursive nlohmann::json 遍历 (decision 4)
  - 整字段替换 `value = "[REDACTED]"` (decision 3)
  - `allowed_env_vars` 白名单 (decision 2)

### 2.2 集成到 node_factory.cpp
- [ ] `src/modules/parser/node_factory.cpp:41-47` 修改 (~5 lines)
  - 解析 `env:` / `env_vars:` 后调 `redact_env_vars(ctx.metadata)` (decision 1 early binding)
  - 默认 `allowed_env_vars = {"*"}` (backward-compat, decision 6)

### 2.3 集成到 env_validation_hook.cpp
- [ ] `src/common/hooks/env_validation_hook.cpp:51-56` 修改 (~10 lines)
  - 现有白名单检查通过后 emit `env_vars_audit` event (decision 5)
  - payload 含 keys 列表不含 values (per ADR-0068 §3.1 协议)

### 2.4 GREEN 验证 (per Pattern #1 step 2)
- [ ] `cmake --build build --target test_redact_env_vars` exit 0
- [ ] `ctest -R test_redact_env_vars` PASS 6/6 cases / 18+ assertions
- [ ] RED 6 cases → GREEN 6 cases (per TDD 5-step)

## Phase 3: REFACTOR (production code) — 优化与集成

### 3.1 既有 fixture 改造 (AC-5)
- [ ] `tests/test_dsl_extensions.cpp:410-509` 改造 (~5 lines)
  - `secret_abc123` fixture 验证为 `[REDACTED]`
  - 不在 stderr / trace JSONL 输出 (verify via grep)
- [ ] `ctest -R test_dsl_extensions` PASS 验证 (零回归)

### 3.2 audit event 协议登记 (AC-7)
- [ ] `docs/adr/adr-0068-event-emission-contract.md` Appendix A v2.5 amendment
  - 新增 1 主题 `env_vars_audit` (owner=`env_validation_hook`, payload `keys: list<string>` + `node_id: string`)
  - 同步 commit msg 标注 v2.5 amendment

### 3.3 ADR-0075 §不变量 4 扩展 (AC-8)
- [ ] `docs/adr/adr-0075-env-backend-multi-execution.md` §不变量 4 扩展
  - 引用 `redact_env_vars` 协议 (env_vars 仅透传白名单, 不入日志/trace)
  - 同步 commit msg 标注 §不变量 4 扩展

### 3.4 契约文档 (AC-4)
- [ ] `docs/specs/env-redaction.md` 新建 (~80 lines)
  - §白名单机制 (per ADR-0075 backend policy)
  - §audit event schema (per ADR-0068 Appendix A v2.5)
  - §迁移指南 (现有 12 fixture 改造步骤)
  - §性能预算 (per decision 7 <10µs/op)

### 3.5 全量回归验证
- [ ] `ctest --test-dir build -LE must_realllm -j1` 267 → 267+ PASS (drop_ratio=0%)
- [ ] `ctest --test-dir build -L security -j1` 新增 binary + 既有 security tests 零回归 (AC-6)
- [ ] `tools/adr_lint.py` exit 0 (ADR v2.5 amendment 通过 lint)
- [ ] `tools/docs_drift_audit.py` SUMMARY 增量 < 3 (v2.5 amendment + env-redaction spec)

## Phase 4: ARCHIVE (Day-5 5-file integrity) — OpenSpec change archive

### 4.1 完整 ship gate 验证 (per AGENTS.md Reverse Indicator Rule)
- [ ] `openspec validate 2026-10-15-env-vars-redaction-protocol --strict` PASS (per Day-5 5-file integrity)
- [ ] 5 file 文件 integrity: `.openspec.yaml` + `proposal.md` + `design.md` + `tasks.md` + `specs/env-vars-redaction-protocol/spec.md`
- [ ] `git status --short` clean (post-commit)

### 4.2 Archive
- [ ] `git mv openspec/changes/2026-10-15-env-vars-redaction-protocol/ openspec/changes/archive/`
- [ ] archive commit msg 含 [Reverse Indicator] 5 字段:
  - `+ new_up`: env_vars redaction 协议 ship + 6 cases / 18+ assertions PASS + 9 files +335/-5 lines
  - `- old_down`: drop_ratio=0% (默认 `*` 行为 backward-compat, 既有 fixture 零行为变化)
  - `failure_traces`: 5 pre_issues (Case 1-5 RED/GREEN/REFACTOR 验证 + audit event emit + nested values + mixed + backward-compat + performance)
  - `ablation`: N/A (本 change 不涉及 Harness 变化, 仅新 security helper + 集成 2 处)
  - `context_ids`: N/A (本 change 不涉及 ContextRequest schema, 仅 env_vars redaction 协议)

### 4.3 AGENTS.md Recent Changes 同步 (per 早 session 模式)
- [ ] AGENTS.md Recent Changes 段本 ship 登记 + 关联 Pattern #13 (must_realllm 误标反模式避免 — env_vars redaction 是 LLM-independent helper)
- [ ] 关联 commit hash 验证 (`git log -1 --format="%h %s"`)
- [ ] ADR-0068 Appendix A v2.5 amendment 状态翻牌

## 升级触发条件 (per AGENTS.md Pattern #2)

实施期间如出现以下情况, 立即暂停 + 升级:

- ⚠️ **C1**: redact_env_vars 性能 >10µs/op (per Case 6 benchmark FAIL)
- ⚠️ **C2**: 现有 fixture 改造 >20 个 (远超 12 预期, 默认 `*` 行为可能不 backward-compat)
- ⚠️ **C3**: audit event emit 频率过高 (每次解析 emit 触发性能问题)
- ⚠️ **M1**: oracle SHIP-with-fixes 评审发现 >3 Major issues (需重新设计)

## 关联 subagent sessions

- `ses_ec5f36ffaffeT3KEsoFqAUPq43` (Metis 2026-10-14 dual-agent 调研, 意图 D 隐藏缺口识别)
- 早 session R13.4 调研审计: `.rddf/audits/2026-10-15-r13-4-confidential-redaction-real-llm-dual-agent-review.md` (Metis + Oracle 调研 R13.4 redaction 借鉴模式)

Per AGENTS.md Pattern #1 (TDD 5-step) + Pattern #4 (SHIP-with-fixes flow) + Pattern #8 (pre-impl dual-agent review) + Day-5 5-file integrity lesson.