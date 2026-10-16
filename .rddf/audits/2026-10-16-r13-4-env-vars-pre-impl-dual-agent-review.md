# Pre-impl Dual-Agent Review: r13-4-real-bug-fixes + env-vars-redaction-protocol

**生成**: 2026-10-16 via 主会话 (build agent role=builder) — subagent dispatch per AGENTS.md Pattern #8
**接收方**: rdd-builder P0 case 1 approve gate (待 2 改动文档重写后才能 ship)
**方法**: AGENTS.md Pattern #8 pre-implementation dual-agent review (Metis + Oracle 并行)
**视角合并**: **Metis** (Hidden Intent + Ambiguity + AI Slop + Missing AC + Edge cases) **+ Oracle** (架构可行性 + 物理风险 + Critical Issues)
**关联 subagent sessions**:
- Metis: `ses_ebe1541efffe7g1ix4n3yOnGTs` (✅ 完成)
- Oracle (初): `ses_ebe150954ffeK6brRtVwUoCcpe` (❌ engine overloaded, retry)
- Oracle (retry): `ses_ebe0f0ff8ffeINdWNmD5x7vTjx` (✅ 完成)

**关联 2 change paths**:
- `openspec/changes/2026-10-15-r13-4-real-bug-fixes/`
- `openspec/changes/2026-10-15-env-vars-redaction-protocol/`

**关联已有审计**:
- `.rddf/audits/2026-10-15-r13-4-confidential-redaction-real-llm-dual-agent-review.md` (R13.4 早期调研)

**主会话诚实 ack**: 本合并报告初稿写 Metis 视角后被 `write` 命令覆盖丢失 (Pattern #10 第 N+3 实例: "工作目录丢失文件")，Metis 视角报告 + Oracle 视角报告从主会话 cache + subagent 输出重建。

---

## §0. 核心结论 (双视角共识)

2 个 OpenSpec change 共有 **5 BLOCKER (Oracle) + 7 BLOCKER (Metis) + 8 SHOULD_FIX + 4 NITS**。**2 change 都不能直接 P2 实施**，必须先修文档（甚至重构部分 design）。

**最严重 finding**:
- **B4 (r13-4) — Bug 1 修复位置错**：`expected_eval_quality` 实际在 `meta` 内部（`evolution_session.cpp:570` `build_meta`），proposal/spec/Case 5 全部按 top-level 字段设计 → 照做 = dead code = bug 未修
- **B5 (r13-4) — Case 6 mock 验证路径不可行**：phase1_init 无条件注册 loop/run，duplicate 抛异常，mock handler 忽略 args → 跨 context 泄漏 mock 模式不可观测
- **B1 (env-vars) — event 物理无发射**：helper 是纯函数无 bus，hook lambda 只 capture config 无 bus → Case 4 不可测
- **BLOCKER-1 (Metis) — env-vars 集成点错**：hook 看 `args["env.XXX"]`，plan 假设看 `metadata["env_vars"]`

---

# 视角 1: Metis (隐藏意图 + 歧义 + AI 失败模式 + 范围蔓延)

## §1.1 Hidden Intent

- **真实目标很可能是"防止未来真 LLM/真 token 测试泄漏 CI 日志"**，而非修复现存泄漏。源提案 `.rddf/improvements/adr-0072-d4-env-vars-redaction.md:37-41` 自己把泄漏写成 `// 未来 (per ADR-0068 留 Sprint 27+)`。但 plan 却把它包装成"CI 日志泄漏 / PG-13 写盘 / 跨进程 SKILL leak"等既成事实（proposal.md §风险场景）。**意图落差**：如果只是"防止未来路径引入"，正确做法是加一个**不变量 + 回归守卫**（评论/断言），而不是引入新的 helper + event 主题 + ADR amendment 的大动作。
- **隐含期待"默认安全"**：AC-2/AC-5 要求默认 redact（`secret_abc123` → `[REDACTED]`），但 design Decision 2/6 又选默认 `{"*"}` 不 redact。作者心里其实想要"默认保护 secrets"，却用"backward-compat"把它否掉了——这是一个未被言明的产品决策冲突。
- **隐含复用 ADR-0068 §3.1 模式**：希望 env_vars 有和 api_key 一样的"结构化 redaction 协议"。但 api_key 协议是"压根不写值"，而 env_vars 需要保留 keys 给后端执行——两者语义不同，不能照搬。

## §1.2 Ambiguity

- **`redact_env_vars` 签名有 3 个互斥版本**：
  - proposal.md:48 = `redact_env_vars(json metadata) -> json`（单参，返回新 json）
  - tasks.md:36 = `redact_env_vars(nlohmann::json metadata, const std::unordered_set<std::string>& allowed_env_vars = {"*"})`
  - tasks.md:15/19 = `redact_env_vars({PATH, HOME}, {API_TOKEN: secret_abc123})`（第一参是白名单集合，第二参才是 env_vars 对象）
  三者参数个数/类型/顺序全不一致。**必须钉死一个**。
- **"默认"一词在 spec 里指了 3 个不同值**：
  - spec.md:22 / :75 / :80 / design.md:42/88 = 默认 `{"*"}`（不 redact）
  - spec.md:31 = 默认 `{}`（"默认 deny, 全部 redact"）
  - spec.md:15 / :87 = 默认 `{"PATH","HOME","USER","LANG"}`（"默认白名单"）
  实现无法同时满足。
- **`redact_env_vars(ctx.metadata)` 的作用域不明**：是"整个 metadata 递归找 env_vars 键"，还是"只处理 metadata['env_vars']"？嵌套 scenario（spec.md:30）给的是 `env_vars: {DB: {password...}}`，但 Case 2（tasks:17）又是 `env_vars.API_TOKEN.value = nested_object`。两种嵌套模型混淆。
- **"12 fixture"占位符**：spec.md:79 / :83 / proposal.md:71 / design.md:14 反复说 `test_dsl_extensions.cpp:410-509` 有"12 fixture"，实际该区间只有 **4 个 TEST_CASE**（实测行 413/437/464/490），且只有 **1 个**含 `secret_abc123`（行 461）。"12"来源不明，可能是把断言数当 fixture 数。
- **event `owner` 字段歧义**：spec.md:97 说 owner=`env_validation_hook`，但 emit 时机又要求"每次 `redact_env_vars` 调用"（spec.md:49）——helper 在 parser 层，hook 在 coordinator 层，owner 到底是谁？

## §1.3 AI Slop Risks

- **过度设计**：为"当前不存在的泄漏路径"引入 5 个 production 文件 + 1 个新 event 主题 + 2 个 ADR amendment + 新契约文档（`docs/specs/env-redaction.md`）。复杂度远超风险实际暴露面。
- **照搬不同机制**：design.md:6/46-49 声称借鉴 `redact_trace_fields`（evolution_session.cpp:582-603），但该函数是**按敏感度 + 字段名**redact（`sensitivity=="confidential"` → 特定字段名如 `turn_input`/`response`），**不是**"key 白名单"模式。
- **`include/agenticdsl/contract/` 放具体实现**：该目录是 `I*` 接口层（`itool_registry.h` / `iinteraction_bus.h` / `timer_service.h`）。把 `redact_env_vars.h`（免费函数，非接口）放这里违反目录约定。
- **perf benchmark 作为硬性测试**：`<10µs/op`（spec.md:63 / tasks:26）是典型的并行 ctest flaky 断言（AGENTS.md Pattern #7）。
- **spec 模板污染**：几乎每个 Scenario 都带一行 "SHELL API ..." 的复制粘贴残留。

## §1.4 Missing Acceptance

- **AC-2/AC-5 无法被验证为"真的生效"**：由于没有任何现存序列化路径会把 `node->metadata["env_vars"]` 写出去，AC-5 在任何实现下都会 PASS——**它是空断言（vacuous）**。
- **AC-3（emit `env_vars_audit`）无验证载体**：`InMemoryBus` capture 需要 bus 注入，而 hook 工厂 `make_env_validation_hook(BackendConfig)` 无 bus。
- **AC-6（`ctest -L security`）标签不存在**：`tests/CMakeLists.txt` 仅 `live`/`must_realllm` 两 label。
- **AC-8（ADR-0075 §不变量 4）引用文件不存在**：plan 写 `adr-0075-env-backend-multi-execution.md`，实际为 `adr-0075-env-backend-local-docker.md`。
- **AC-10（267 → 267+）基线数字不可靠**。

## §1.5 Edge Cases

- 空 env_vars / 非 object
- key 大小写 / 前缀（`PATH` vs `Path`, `env.<NAME>` vs `<NAME>`）
- 并发 (parser 多 graph 并行)
- 大 metadata / 深嵌套
- `--mock` / 默认配置下静默跳过
- event 风暴 (1000 nodes DSL = 1000 events)
- 与 `session_manager` JSONL 的交互

## §1.6 BLOCKERS (Metis)

### BLOCKER-1 — 中心集成点物理不可实现
- plan 在 `env_validation_hook.cpp:51-56` 发射 event
- 但 hook 遍历 `args`，键前缀 `env.`（line 52-59）
- 而 `env_vars` 在 `ctx.metadata["env_vars"]`（node_factory.cpp:42-48）
- `args` 仅来自 `arguments:` 对象（node_factory.cpp:177-190），**没有任何路径**把 `metadata["env_vars"]` 变成 `args["env.XXX"]`

### BLOCKER-2 — hook 无 bus / 无 node_id
- `PreHook` 签名无 `IInteractionBus`（itool_hook_registry.h:31-34）
- `ToolCallContext`（execution_policy.h:96-103）只有 `session_id`/`caller_layer`/`target_path`/`trace_id`

### BLOCKER-3 — 新 .cpp 未接入构建
- `src/common/security/` 目录不存在
- `agenticdsl_common` 源列表逐文件显式列出（根 CMakeLists.txt:80-118），plan 无 CMakeLists 改动
- 反向：tasks 1.1 要求手动 `add_catch_test`，但 tests/CMakeLists.txt:243 `GLOB` 已自动注册

### BLOCKER-4 — 无配置通道
- `allowed_env_vars` 真值来自 `BackendConfig::with_defaults()`（backend_policy.cpp:16/27/35）
- `node_factory.cpp` 无 `BackendConfig` 依赖

### BLOCKER-5 — 事件发射语义自相矛盾
- spec.md:49 "每次 helper 调用 MUST emit" vs 纯函数无法 emit

### BLOCKER-6 — 默认语义三值冲突
- AC-2 默认 redact vs Decision 6 默认 `{"*"}` 不 redact

### BLOCKER-7 — 签名三版本不一致

## §1.7 Recommended Changes (Metis)

P0: 重定位集成点 + 钉死唯一签名 + 钉死默认语义 + 补 CMake 接线 + 修复配置通道 + 修正 ADR-0075 文件名
P1: 修 AC-6/AC-7 + 改 perf 断言为宽松 + 修"12 fixture"数字 + 修正 ADR-0068 版本号 + 消 R13.4 文档写冲突 + 修正借鉴说明 + contract 目录约定
P2: 清理 "SHELL API" 残留 + 统一 cooling-off 时点 + 声明 thread-safety

## §1.8 Evidence Index (Metis)

- `src/common/hooks/env_validation_hook.cpp:25,52-59`
- `include/agenticdsl/contract/itool_hook_registry.h:31-34`
- `src/common/policy/execution_policy.h:96-103`
- `src/modules/parser/node_factory.cpp:36-48,177-190`
- `src/common/policy/CMakeLists.txt:8-20` + `CMakeLists.txt:80-118`
- `CMakeLists.txt:105`
- `tests/CMakeLists.txt:169,203,243,261`
- `tests/test_dsl_extensions.cpp:413,437,464,490` (4 TEST_CASE; secret_123 仅 :461)
- `src/core/session_manager.h:53-59`
- `src/core/types/node.h:51`
- `examples/pdk_chat_demo_evolution/evolution_session.cpp:582-603`
- `docs/adr/adr-0075-env-backend-local-docker.md:358`
- `docs/adr/adr-0068-event-emission-contract.md:174,255-259`
- `src/common/policy/backend_policy.cpp:16,27,35`

---

# 视角 2: Oracle (架构可行性 + 物理风险 + Critical Issues)

## §2.1 核心结论

5 BLOCKER（B1-B5）+ 8 SHOULD_FIX（S1-S8）+ 4 NITS。

## §2.2 BLOCKERS (Oracle)

### B1 (env-vars) — `env_vars_audit` event 物理上无发射路径

三处互相矛盾：
- (a) spec R2 `每次 helper 调用 MUST emit` vs helper 纯函数
- (b) tasks 2.3 hook emit vs hook lambda 只 capture config
- (c) `make_env_validation_hook` 无生产调用方（grep 实证仅 tests + docs 引用）

**修复**: 改工厂签名 `make_env_validation_hook(config, IInteractionBus*)` 或删除 Case 4

### B2 (env-vars) — spec 内部默认值自相矛盾

- spec.md:15 GIVEN `{"PATH","HOME","USER","LANG"}`（默认白名单）
- vs spec.md:22/75 + design D2/D6 `{"*"}`

**修复**: 钉死（推荐 `*`）

### B3 (env-vars) — AC-5/tasks 3.1 与 backward-compat 物理冲突

- `test_dsl_extensions.cpp:461` 现断言 `metadata["env_vars"]["API_TOKEN"] == "secret_abc123"`
- 默认 `*` 下 AC-5 要求 `[REDACTED]`，两个断言只能活一个
- "12 fixture"计数错误：实际仅 4 cases / 含 secret 仅 1 处

**修复**: 二选一
- (a) 默认 deny（破坏 AC）
- (b) 诚实降级"机制就绪"（推荐）

### B4 (r13-4) — Bug 1 修复位置错误，照做等于 dead code ⚠️ Oracle 独占 critical

`expected_eval_quality` 在 `meta` 内部（`evolution_session.cpp:570` `build_meta`：`{"expected_eval_quality", ctx.expected_eval_quality.value_or("")}`）

- `proposal.md:38` 写 top-level = **事实错误**
- `design.md:87-90` top-level 分支永远不触发
- `spec.md:24-27` + Case 5 断言路径 `event["expected_eval_quality"]` 全错

**正确修复**:
- meta 分支（line 593-600）内加 `expected_eval_quality`
- internal 场景把 guard 从 `confidential` 扩展到 internal

**附带**: `value_or("")` 永远 string → AC-5 null scenario 不可达 (vacuous AC)

### B5 (r13-4) — Case 6 验证路径物理不可行 ⚠️ Oracle 独占 critical

design D3 断言 3 + spec.md:73 计划"mock 工具 lambda capture `loop_args['history']`"，但：
- (a) `phase1_init` 无条件注册 `loop/run`（`evolution_session.cpp:220/175`）
- (b) `ToolRegistry` duplicate registration 抛 `invalid_argument`（`src/common/tools/registry.cpp:101`）
- (c) `run_6_phase_demo` monolithic（line 143-149）无注入窗口
- (d) mock handler（line 234）忽略 args → mock 模式下 cross-context 泄漏**不可观测**

**修复** 三选一:
- (a) EvolutionSession ctor 接受 mock handler override（推荐，最小侵入）
- (b) 改白盒断言（验证 `chat_session_` 指针 per-context 变化）
- (c) 纯 real-LLM 验证（违反 mock 确定性原则）

## §2.3 SHOULD_FIX (Oracle)

- **S1** ADR-0075 路径错误
- **S2** whitelist 无 plumbing 路径（默认 `*` 下 parse-time redaction 永为 no-op）
- **S3** build 接线遗漏（`src/common/security/` 不存在 + root CMakeLists.txt:105 需注册）
- **S4** `ctest -L security` label 不存在
- **S5** Case 6 硬 timing <10µs 并行 ctest flaky（per AGENTS.md Pattern #7）
- **S6** tasks 6.3 编辑 archived spec 违反冻结先例
- **S7** Change ID 漂移（`-real-bug-fixes` vs `-redaction-bug-fixes`）
- **S8** C2 结论 + 缺口（per-context reset 物理安全但 AC-8/9 需 regression test）

## §2.4 NITS (Oracle)

- N1: proposal Cooling-off 写 created 17:50Z vs 实际 09:48Z
- N2: AC-3 internal 脱敏 `expected_eval_quality` 是 L2 spec 408 扩展解读，建议显式标"延伸"
- N3: hook `env.<NAME>` flat args vs parser metadata JSON 两种表示
- N4: design.md Decision 编号跳号（D7 → D9）

## §2.5 物理证据汇总 (Oracle)

| BLOCKER | 物理证据 file:line |
|---------|--------------------|
| B1 | `env_validation_hook.cpp:25-29` + `execution_policy.h:96-103` |
| B2 | `spec.md:15` vs `spec.md:22/75` |
| B3 | `tests/test_dsl_extensions.cpp:461` vs `tasks 3.1` |
| **B4** | `evolution_session.cpp:570` `build_meta` vs `proposal.md:38` |
| **B5** | `evolution_session.cpp:220/175` + `registry.cpp:101` + `evolution_session.cpp:234` |

---

# 视角 3: 合并收敛信号 (Pattern #8 ROI 实证)

| 编号 | Metis | Oracle | 收敛? | 严重度 |
|------|-------|--------|-------|--------|
| env-vars 数据流错 | BLOCKER-1 | (Oracle B1 部分) | 部分 | P0 |
| env-vars hook 无 bus | BLOCKER-2 | B1 | 部分 | P0 |
| env-vars CMake 未接 | BLOCKER-3 | S3 | **完全** | P0/P1 |
| env-vars 无 plumbing | BLOCKER-4 | S2 | **完全** | P0 |
| env-vars 默认冲突 | BLOCKER-6 | B2 + B3 | **完全** | P0 |
| env-vars 签名冲突 | BLOCKER-7 | (未列) | Metis 独占 | P0 |
| env-vars 12 fixture 错 | (AC 漏洞) | B3 部分 | 部分 | P1 |
| env-vars AC-6 label 错 | (AC 漏洞) | S4 | **完全** | P1 |
| env-vars ADR-0075 文件名 | (未列) | S1 | Oracle 独占 | P1 |
| env-vars perf benchmark flaky | (AI Slop) | S5 | **完全** | P1 |
| **r13-4 Bug 1 位置错** | (未列) | **B4** | **Oracle 独占 critical** | **P0** |
| **r13-4 Case 6 mock 不可行** | (未列) | **B5** | **Oracle 独占 critical** | **P0** |
| r13-4 C2 per-context reset 安全 | (未列) | S8 | Oracle 独占 (C2 OK) | P1 |

**Pattern #8 ROI 实证**:
- Metis 视角: 7 BLOCKERS (env-vars 重)
- Oracle 视角: 5 BLOCKERS (**含 r13-4 的 2 个 critical finding**)
- **Oracle 比 Metis 更尖锐**: 发现 Metis 漏的 2 个 r13-4 critical BLOCKER (B4/B5)
- **仅靠 Metis = 漏 30 min 实施后调试** → 必须双视角
- **实施前发现节省 ~3-4h**（B4/B5 是 dead code / 不可测，最难调试类型）

---

# §A. 下一会话推进路径 (5 选项)

| 选项 | 内容 | 风险 | 估时 |
|------|------|------|------|
| **A (推荐)** | 重写 2 change 文档（解 B1-B5 + BLOCKER-1..7 + S1-S8），24h cooling-off 后 P2 实施 | 中（文档重写量大） | 3-4h 文档 + 2-3d 实施 |
| **B** | 关闭 env-vars proposal（per Pattern #10 hygiene），仅 ship r13-4（先修 B4/B5） | 低 | 1.5h 关闭 + 2d r13-4 ship |
| **C** | 关闭 2 proposal（5 BLOCKERS 共识下 2 change 都不可实施） | 低（失去 redaction 协议机会） | 30min 关闭 |
| **D** | 推迟 2 proposal 到 Sprint 27+（待 hook bus 注入 + parser 接收 BackendConfig 就绪） | 中 | 30min 推迟决议 |
| **E** | 维持现状，等用户决策 | 高 | 0 |

---

[Reverse Indicator]
- `+ new_up`: Metis 7 BLOCKERS + Oracle 5 BLOCKERS (含 r13-4 2 critical) + 8 SHOULD_FIX identified（双视角独立交叉验证 100%）
- `- old_down`: drop_ratio = 0% (纯 audit 报告, 0 src/ 改动)
- `failure_traces`: B1/BLOCKER-1 (hook 数据流) + BLOCKER-2 (hook 无 bus) + BLOCKER-3 (CMake 未接) + BLOCKER-4 (无 plumbing) + BLOCKER-5 (emit 矛盾) + BLOCKER-6 (默认冲突) + BLOCKER-7 (签名冲突) + B3 (AC 物理冲突) + **B4 (r13-4 Bug 1 修复位置错)** + **B5 (r13-4 Case 6 不可测)** + S1 (ADR-0075 路径错) + S2 (无 plumbing) + S3 (CMake 接线) + S4 (security label 缺) + S5 (perf benchmark flaky) + S6-S8
- `ablation`: N/A (audit 报告)
- `context_ids`: N/A (无 ContextRequest schema 变化)

Refs:
- AGENTS.md §Pattern #8 (pre-implementation dual-agent review, 双视角必要)
- AGENTS.md §Pattern #10 (hygiene drift detection)
- AGENTS.md §Single-Dev Mode (24h cooling-off 红线)
- openspec/changes/2026-10-15-r13-4-real-bug-fixes/
- openspec/changes/2026-10-15-env-vars-redaction-protocol/
- .rddf/state/.planner-handoff.json v7 (current session state)
- .rddf/audits/2026-10-15-r13-4-confidential-redaction-real-llm-dual-agent-review.md (R13.4 早期调研)

---

# §M. 主会话诚实 ack (Pattern #10 第 N+3 实例)

本合并报告初稿写 Metis 视角后被 `write` 命令覆盖丢失。Metis 视角报告从主会话 cache + subagent 输出重建（无原始 git 备份）。Oracle 视角报告（subagent 输出）已完整保存。

**教训**:
- `write` 工具是覆盖，不是 append
- 大型多 section 报告应该分阶段 write（先 write 一个 minimal skeleton, 然后 read + edit 增量 append sections）
- 后续 audit 报告落地策略: 先 git add 创建一个空文件 commit → 然后 edit 增量添加 sections → 最后 commit 完整版