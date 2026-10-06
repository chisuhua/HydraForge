# l2-evolution-real-llm-provider Design

## Context

L2 reference example (`examples/pdk_chat_demo_evolution/`) 在
`2026-09-26-l2-evolution-finalization` (Phase C ship) 时实施 mock 模式 +
real LLM 标志存储但 stub。当前状态:

- `EvolutionSession` ctor 接受 `opts.real_llm` 字符串,后续代码完全不使用
- `--real-llm <provider>` 在 main.cpp 解析为 `opts.real_llm`,传给 session 但无 effect
- 9/9 l2-evolution 测试全部 mock 模式,real LLM 路径无端到端验证
- `agenticdsl::LLMProviderFactory` 已 ship (per `src/common/llm/llm_provider_factory.h`),
  但未在 L2 EvolutionSession 中调用

约束:
- L2 是 autonomous evaluator (per AGENTS.md §Reverse Indicator Rule + L2 spec §R13),不能 hardcode 真实 API key
- API key 必须从 env var 读取 (`DEEPSEEK_API_KEY` / `MINIMAX_API_KEY`),helper 三态
- `--real-llm` 模式必须 fail-fast on 配置错误 (placeholder URL / missing key / unknown provider),不允许静默 fallback to mock
- 现有 mock 模式 (`--mock`) 行为完全不变,9/9 测试零回归
- hermetic HOME 兼容 (per finalization hermetic-home-composition requirement)

Stakeholders: Solo-Dev (实施 + 自审) + Oracle (post-impl review) + 用户 (L2 reference example 真实 LLM 闭环消费者)

## Goals / Non-Goals

**Goals:**
- L2 `--real-llm deepseek` 模式真实 LLM 端到端闭环 (6-phase demo + trace JSONL + drop_ratio)
- R8.1 红线生产验证 (真实 drop_ratio 计算 + >5% exit non-zero)
- 必须 fail-fast on placeholder URL / missing key / unknown provider
- 复用现有 LLMProviderFactory API,不重新发明
- 与 hermetic HOME 兼容,无 credential 文件冲突
- 测试覆盖: must_realllm + l2-evolution 双标签,与现有 helper 共享

**Non-Goals:**
- 不修改 mock 模式行为 (现有 9/9 测试零回归)
- 不重新设计 LLMProviderFactory (复用现有 API)
- 不引入新依赖 (OpenSSL / httplib 已 vendored)
- 不实现 LLM response cache / retry policy (后续 improvement)
- 不实施 R8.3 `--ablation-mode=full` real LLM (P2 deferred, 见 G8)
- 不补 Phase 9 training data 回流 (Wave 3 Phase 2 D4, 见 G8)

## Decisions

### Decision D1: LLMProviderFactory::create(provider, api_key_env) 直接调用

**Choice**: `EvolutionSession` ctor 接受 `real_llm` provider 名 → 调
`agenticdsl::LLMProviderFactory::create(provider, api_key_env)` 直接构造
unique_ptr<ILLMProvider>。

**Rationale**:
- 复用现有 `LLMProviderFactory` API (已 ship, deepseek + minimax provider 已注册)
- 不引入新抽象层 (Factory 已封装 create + register + initialize)
- 失败 fail-fast: factory 返回 nullptr 或抛异常时 → session 构造失败 → main.cpp 退出非零

**Alternatives considered**:
- A: 通过 `pdk/loop_agent` PluginLoader 加载 deepseek provider → 拒绝 (PluginLoader 是 PDK 框架层, L2 是 example 层, 不应跨层)
- B: 自实现 DeepseekProvider (HTTP client + JSON parser) → 拒绝 (重复造轮子, 现有 ProviderFactory 已有完整实现)
- C: 接受 unique_ptr<ILLMProvider> 由 main.cpp 注入 → 拒绝 (增加 main.cpp 复杂度, factory 模式更内聚)

### Decision D2: API key 来自 env var (NOT file / CLI flag)

**Choice**: API key 从 env var `DEEPSEEK_API_KEY` / `MINIMAX_API_KEY` 读取,通过 `real_llm_env.h::real_llm_config()` helper 集中管理。

**Rationale**:
- L2 spec §R13 约束: "L2 零 hardcode, 不通过 CLI flag 传 API key"
- 与 must_realllm 测试 helper 共享 (`require_real_llm_env()` + `real_llm_config()`)
- env var 是 12-factor app 标准,符合项目既有约定

**Alternatives considered**:
- A: CLI flag `--real-llm-key <key>` → 拒绝 (违反 L2 §R13 零 hardcode,容易泄漏到 shell history)
- B: credential file `~/.hydraforge/credentials` → 拒绝 (与 hermetic HOME 冲突,见 D3)
- C: 注入测试 fixture 提供 → 接受 (用于 unit test,但 binary 必须 fail-fast if missing)

### Decision D3: hermetic HOME 兼容 + API key env var 双轨

**Choice**: hermetic HOME 隔离 HMAC key path (per finalization hermetic-home-composition),但 API key 从 env var 读取 (NOT hermetic HOME 内的 file)。两套机制正交不冲突。

**Rationale**:
- hermetic HOME 解决 HMAC key 路径冲突 (避免污染 `~/.hydraforge/genome/`)
- API key env var 解决凭证管理 (避免硬编码 / 文件冲突)
- 两者正交: hermetic HOME 改 `HOME` env var, API key 直接读 `DEEPSEEK_API_KEY` (priority 不冲突)

**Risk**: 如果 `DEEPSEEK_API_KEY` 在 hermetic HOME setup 后被 unset (test fixture 误操作),real LLM 会 fail-fast → 显式错误,优于静默 fallback。

### Decision D4: real LLM drop_ratio 必须写入 metrics.json

**Choice**: `--release-metrics` 在 real LLM 模式下 MUST 写入真实 drop_ratio 到 `/tmp/l2-metrics.json` (默认) 或 `--metrics-output <path>` 指定路径。

**Rationale**:
- 当前 mock 模式输出 `-1.0` 哨兵 (semantic hollow)
- real LLM 模式必须计算 `mutated_failures / max(1, mutated_passes + mutated_failures)` per finalization C4 公式
- R8.1 红线 `drop_ratio > 5% → exit non-zero` 在 real LLM 模式必须真实触发
- 写入 `/tmp/l2-metrics.json` 默认避免 CWD pollution (per finalization housekeeping)

**Risk**: real LLM mode 跑失败 (network error / provider 不可用) → metrics.json 不写入 → 用户需 stderr log 排查。Mitigation: real_llm_provider 失败时 stderr 输出 traceback + partial state。

### Decision D5: 必须 fail-fast on 4 类配置错误

**Choice**: 以下 4 类配置错误 MUST 立即 exit non-zero,不允许静默 fallback to mock:
1. Unknown provider name (e.g., `--real-llm gpt-4`) → "ERROR: unknown provider"
2. Missing API key env var → "ERROR: real LLM requires DEEPSEEK_API_KEY or HYDRAFORGE_SKIP_REAL_LLM=1"
3. Placeholder URL (MINIMAX_API_URL = api.minimax.chat) → "ERROR: MINIMAX_API_URL placeholder"
4. `--real-llm` flag without value → parse error

**Rationale**:
- AGENTS.md §Reverse Indicator Rule: silent fallback = misleading "mock PASS" 掩盖真实问题
- Pattern #10 hygiene: 配置错误必须在源头拦截,不依赖下游 catch
- 与 must_realllm 测试 helper 三态语义对齐 (HYDRAFORGE_SKIP_REAL_LLM=1 only for test framework, NOT binary mode)

**Alternatives considered**:
- A: 静默 fallback to mock → 拒绝 (违反 fail-fast 原则, 隐藏配置错误)
- B: warn to stderr + continue with mock → 拒绝 (用户使用 `--real-llm` = 显式意图, warn 不足)
- C: exit non-zero (selected) → 接受 (显式 fail-fast, 用户立即知道配置问题)

## Risks / Trade-offs

[Risk R1: Real LLM mode 增加 CI token 成本] → Mitigation: real LLM 测试 binary 必须 `[must_realllm]` 标签,`HYDRAFORGE_SKIP_REAL_LLM=1` 时 SUCCEED + return 短路,不跑实际 LLM 调用 (per AGENTS.md §REAL LLM TESTING §3)

[Risk R2: LLMProviderFactory::create() 失败抛异常,EvolutionSession ctor 析构异常] → Mitigation: ctor 用 try-catch 包装 factory 调用,失败时设 `real_llm_error_` 字段,run_6_phase_demo() 开头检查立即 exit non-zero (RAII 友好)

[Risk R3: deepseek API 暂时不可用 → 测试 flaky] → Mitigation: 测试用例设计参考 AGENTS.md Pattern #3 (断言强度分层),契约用 `≥1/3 ok`,能力断言用严格 `==expected`;real LLM 测试 binary 加 `[must_realllm]` 标签,CI 上 `HYDRAFORGE_SKIP_REAL_LLM=1` 短路

[Risk R4: hermetic HOME + real LLM API key 路径冲突 (e.g., test fixture 把 HOME 改了, DEEPSEEK_API_KEY 被 unset)] → Mitigation: hermetic HOME setup 不修改 env var 中其他 key (per finalization hermetic-home-composition scenario); real LLM fail-fast on missing key (per D5)

[Risk R5: R8.1 红线 real LLM 触发 → binary exit non-zero → CI 红] → Mitigation: 测试用例必须构造 drop_ratio ≤ 5% 场景,避免 flaky red line; release-metrics 测试用 `--real-llm deepseek --release-metrics` + 已知低 drop_ratio fixture

[Risk R6: `--real-llm minimax` URL placeholder NXDOMAIN 误判 → 测试 INFRASTRUCTURE_FAIL 而不是 PRODUCT_FAIL] → Mitigation: MINIMAX 路径独立测试 binary,跳过 URL placeholder 检查 (per `real-llm-provider-construction` Scenario 2 接受)

## Migration Plan

### 实施步骤 (tasks.md 详细分解)

1. **EvolutionSession real LLM 接线** (核心, ~30 行):
   - ctor 接受 `real_llm` → 调 `LLMProviderFactory::create()` → 存 `unique_ptr<ILLMProvider> real_provider_`
   - mock_provider_ 与 real_provider_ 双轨: real_provider_ 存在时优先使用
   - fail-fast on factory exception / nullptr return

2. **新增 test binary** `test_l2_real_llm_e2e.cpp`:
   - 复用 `tests/test_helpers/real_llm_env.h`
   - Case 1: `--real-llm deepseek` 6-phase demo + trace JSONL 验证 (real response 字段)
   - Case 2: `--real-llm deepseek --release-metrics` drop_ratio 真实计算
   - Case 3: `--real-llm <unknown>` exit non-zero
   - Case 4: 缺 `DEEPSEEK_API_KEY` + `HYDRAFORGE_SKIP_REAL_LLM` 未设 → exit non-zero
   - Case 5: hermetic HOME + real LLM 双轨兼容

3. **CMakeLists 注册**:
   - 新增 binary `test_l2_real_llm_e2e` 链接 `pdk_chat_session` + `agenticdsl_common` (含 LLMProviderFactory)
   - 设 `LABELS "must_realllm;l2-evolution"` 双标签
   - 设 `[must_realllm]` Catch2 tag (按 `a21c92e` precedent)

4. **dual Oracle review**:
   - Pre-impl: Metis + Oracle 并行 (per Pattern #11)
   - Post-impl: Oracle SHIP-with-fixes verdict
   - 应用 Major 修正 → 新 atomic commit (per Pattern #4)

### Rollback 策略

如 ship 后发现 critical bug:
1. revert `EvolutionSession` ctor real LLM 接线 (保留 mock 模式)
2. `git revert` 实施 commit
3. 9/9 mock 测试仍 PASS (mock 路径不变)
4. 重新 OpenSpec change 修复 (按 Pattern #10 hygiene 登记)

## Open Questions

1. **Q1**: real LLM 测试 binary 应该在 `examples/pdk_chat_demo_evolution/tests/` (新增) 还是 `tests/` (核心树)? 当前决策: examples 树 (符合 L2 reference example 归属),`examples/pdk_chat_demo_evolution/tests/test_l2_real_llm_e2e.cpp`
2. **Q2**: real LLM mode 是否需要 `--api-url <url>` flag 覆盖默认 URL? 当前决策: 不需要,env var `DEEPSEEK_API_URL` / `MINIMAX_API_URL` 足够 (减少 flag 数量, 避免与现有 9 flag 冲突)
3. **Q3**: real LLM drop_ratio > 5% 测试 case 如何构造? 当前决策: 用 mock fixture 注入 baseline 高失败率 (mutated 总数 100, fail 8 = 8% drop_ratio),与 must_realllm 真实 LLM 测试解耦
4. **Q4**: real LLM 测试 binary 是否需要 `IF_NOT_REALLM` 跳过? 当前决策: 是,按 `[must_realllm]` + `require_real_llm_env()` + `real_llm_env_skipped()` SUCCEED+return 模式 (per AGENTS.md §REAL LLM TESTING §4)
