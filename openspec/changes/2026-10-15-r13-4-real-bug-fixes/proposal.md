# R13.4 confidential redaction 2 真 bug 修复

## Why

`r13-4-confidential-redaction-real-llm` 提案（commit `7321154` 2026-10-15 关闭 Path 1）的 dual-agent 调研（Metis + Oracle 独立交叉命中）识别出 2 个真 bug，需独立 OpenSpec change 修复：

### Bug 1: `expected_eval_quality` 漏脱敏（spec 409 vs impl 582-602）

**Oracle dual-agent review finding (B4 critical, 2026-10-16)**: 早 session proposal claim "expected_eval_quality 在 event top-level" 是 **事实错误**。实测 `examples/pdk_chat_demo_evolution/evolution_session.cpp:570` `build_meta` 函数:

```cpp
{"expected_eval_quality", ctx.expected_eval_quality.value_or("")},
```

**expected_eval_quality 实际在 `event["meta"]["expected_eval_quality"]` 内**, 不是 top-level。早 session proposal/design/spec/Case 5 断言路径 `event["expected_eval_quality"]` 全部错设计 — 照做 = dead code (branch 永远不触发, bug 未修)。

**spec 显式要求** (`openspec/changes/archive/2026-09-24-pdk-chat-demo-evolution-reference-example/specs/pdk-chat-demo-evolution/spec.md:409`):
> `confidential` | redact `turn_input` + `response` + **`expected_eval_quality`** + `metadata.tags` + `metadata.domain` (replace with `[REDACTED-confidential]`); preserve only `context_id` + `task_class` + `metadata.sensitivity` for audit trail

**实装缺失** (`examples/pdk_chat_demo_evolution/evolution_session.cpp:582-602`):
```cpp
nlohmann::json redact_trace_fields(nlohmann::json event, const std::string& sensitivity) {
    if (sensitivity.empty() || sensitivity == "none" || sensitivity == "public") {
        return event;
    }
    std::string level = (sensitivity == "confidential") ? "confidential" : "internal";
    if (event.contains("turn_input") && event["turn_input"].is_string()) {
        event["turn_input"] = "[REDACTED-" + level + "]";
    }
    if (event.contains("response") && !event["response"].is_null()) {
        event["response"] = "[REDACTED-" + level + "]";
    }
    if (sensitivity == "confidential" && event.contains("meta") && event["meta"].is_object()) {
        auto& m = event["meta"];
        if (m.contains("tags")) {
            m["tags"] = "[REDACTED-confidential]";
        }
        if (m.contains("domain")) {
            m["domain"] = "[REDACTED-confidential]";
        }
        // Bug 1 修复点 (per Oracle B4): expected_eval_quality 在 meta 内, 非 top-level
        if (m.contains("expected_eval_quality") && m["expected_eval_quality"].is_string()) {
            m["expected_eval_quality"] = "[REDACTED-confidential]";
        }
    }
    if ((sensitivity == "confidential" || sensitivity == "internal")
        && event.contains("meta") && event["meta"].is_object()) {
        // internal 场景 (per spec 408 隐含 + 与 turn_input/response 一致):
        // expected_eval_quality 同样脱敏 (per Oracle B4: meta 分支 guard 扩展到 internal)
        auto& m = event["meta"];
        if (m.contains("expected_eval_quality") && m["expected_eval_quality"].is_string()) {
            m["expected_eval_quality"] = "[REDACTED-" + level + "]";
        }
    }
    return event;
}
```

**漏** (per Oracle B4): `expected_eval_quality` 字段在 trace JSONL 输出时未脱敏。`build_meta` (line 570) 把 `expected_eval_quality` 写入 `event["meta"]["expected_eval_quality"]` (per `value_or("")` 永远 string)，但 `redact_trace_fields` 只处理 `meta.tags` / `meta.domain`，漏 `expected_eval_quality`。当 `metadata.sensitivity == "confidential"` 时，trace emit 含完整 expected_eval_quality 明文（如 `"Poor"` 表示评估期望），违反 R13.4 `confidential 0% leak` 红线。

**附带 vacuous AC 修正** (per Oracle B4): 早 session AC-5 断言 `event["expected_eval_quality"]` 是 null scenario，物理不可达 (因 `value_or("")` 永远 string)。修正后 AC-5 改为**断言 `event["meta"]["expected_eval_quality"]` 是 string 而非 null** (覆盖 Oracle B4 的 vacuous AC 风险)。

**数据流** (修正后):
- `phase2_baseline` / `phase4_reload_rerun` / `phase5_compare` 调 `redact_trace_fields(event, sensitivity)`
- `event` 含 8 top-level fields (per spec scenario trace-4-events-with-8-top-level-and-meta-context-id)
- `event["meta"]` 7 fields: `context_id` / `task_class` / `is_hidden` / `sensitivity` / `eval_quality` / **`expected_eval_quality`** (per B4 修复点) / 其他
- 修复: meta 分支内加 `expected_eval_quality` 脱敏, confidential + internal 都覆盖

**风险场景**:
- confidential context 评估期望 (`"Poor"`) 永久写入 trace JSONL 磁盘
- audit 工具（`event_log_query_perf` 等）读取 trace 暴露
- CI 失败输出时 print 整个 trace 行

### Bug 2: 跨 context 泄漏（共享 ChatSession）

**问题**:
- `phase1_init` 一次性 `chat_session_ = std::make_unique<ChatSession>(...)` （`evolution_session.cpp:252`）
- `run_6_phase_demo` 6 phase 循环处理多 context (line 144-149: `for (const auto& ctx : contexts_)`)
- `phase2_baseline` 每 context 调 `chat_session_->chat(ctx.turn_input)` (line 301)
- **没有 per-context reset**

**ChatSession 历史管理** (`pdk/chat_session/src/chat_session.cpp`):
- `ChatSession::chat()` 每次调 `impl_->messages.push_back(user_msg)` (append-only)
- `loop_args["history"] = nlohmann::json(impl_->messages).dump()` 透传给 LLM

**跨 context 泄漏场景**:
```
context_1 (sensitivity=public):
  turn_input: "Explain CAP theorem"
  LLM response: "CAP theorem means Consistency, Availability, Partition tolerance..."

context_2 (sensitivity=confidential):
  turn_input: "Diagnose server room breach"
  LLM call payload 含 history = [
    {user: "Explain CAP theorem"},
    {assistant: "CAP theorem means Consistency..."},
    {user: "Diagnose server room breach"}  ← 当前
  ]
  LLM 响应可能: "Based on the earlier discussion about CAP theorem, your server room breach is..."
  
context_2 trace emit:
  redact_trace_fields 脱敏 turn_input + response + meta.tags + meta.domain
  → 实际未脱敏的部分: 之前 contexts 的内容**通过 LLM response 间接出现**
```

虽然 `response` 字段被整体替换 `[REDACTED-confidential]`，**但 LLM response 自身可能含 cross-context 内容**（即便被 redact 也是 "redacted 字符串"，没"cross-context 内容"标记）。**真正问题是**:
- public context 1 的内容通过 LLM 续写**进入** confidential context 2 的 history
- LLM 在 confidential context 看到"前面用户在问什么"（含 public 信息），增加 cross-context pollution 风险
- 用户期望每个 context 独立隔离（L2 是 6 phase demo，不应跨 context 续写）

**对比提案**:
- `r13-4-confidential-redaction-real-llm` 提案中 Case 2 提到"多轮对话 turn 1 confidential → turn 2 LLM 不能复述 turn 1 cleartext" — 这其实是**多 context 隔离**（EvolutionSession 设计上不是多 turn 而是多 context 循环，但 ChatSession 单实例让它们**变成事实上的多 turn**）

## What Changes

### Bug 1 修复: `redact_trace_fields` 加 `expected_eval_quality` 脱敏 (per Oracle B4)

**修改**: `examples/pdk_chat_demo_evolution/evolution_session.cpp:582-602` `redact_trace_fields` 的 meta 分支
- 在 `if (sensitivity == "confidential" && event.contains("meta"))` 分支内 (line 593 附近) 加 `expected_eval_quality` 脱敏
- 在新独立 `if ((sensitivity == "confidential" || sensitivity == "internal") && event.contains("meta"))` 分支内也加 expected_eval_quality 脱敏 (覆盖 internal 场景, per Oracle B4 "meta 分支 guard 扩展到 internal" 修复建议)
- **NOT top-level** (per Oracle B4: `expected_eval_quality` 在 meta 内, 不是 top-level)
- 若 `sensitivity == "confidential"`, 替换为 `[REDACTED-confidential]`
- 若 `sensitivity == "internal"`, 替换为 `[REDACTED-internal]` (per spec 408 隐含, 虽未显式列但与 `turn_input` + `response` 一致)
- 保留 `sensitivity == "public"` 或空 或 `none` (无 redaction)

**Test 新增**: `examples/pdk_chat_demo_evolution/tests/test_reverse_indicators.cpp` 加 Case 5 (修正 per Oracle B4)
- 构造 ContextRequest `metadata.sensitivity = "confidential"`, `expected_eval_quality = "Poor"`
- 跑 `run_6_phase_demo`, grep 整个 stdout
- 断言: stdout 中**任何位置**不出现字符串 `"Poor"`
- 断言: trace emit 中 **`event["meta"]["expected_eval_quality"]` == `"[REDACTED-confidential]"`** (per Oracle B4 修正, NOT top-level `event["expected_eval_quality"]`)
- 附带 assertion: `event["meta"]["expected_eval_quality"]` 是 string (覆盖 vacuous AC 风险, per Oracle B4 finding)

### Bug 2 修复: 跨 context 隔离（per-context reset + 白盒断言方案, per Oracle B5）

**设计决策 D1**: per-context 隔离策略
- **选项 A (推荐)**: `chat_session_` 在 `phase2_baseline` 入口重置 (`chat_session_.reset(); chat_session_ = std::make_unique<...>(...)`)
  - 优点: 严格隔离, 不依赖 ChatSession 内部 API
  - 缺点: 每次 reset 创建新实例, mock mode 性能影响可忽略, real LLM mode 每 context 重新连接 (但 l2-evolution-real-llm-provider 已有 setup_hermetic_home 隔离)
- **选项 B**: 调 `chat_session_->clear_history()` 公开 API
  - 优点: 性能更优
  - 缺点: 需 ChatSession 公开新 API (BREAKING, 跨 PDK 边界)
- **选项 C**: 在 `loop_args` 构造时清空 `history` 字段
  - 优点: 最小改动
  - 缺点: 治标不治本, ChatSession::impl_->messages 仍在累积
- **选项 D**: 改 ChatSession API 增加 `start_new_session()` 方法
  - 优点: 长期更通用
  - 缺点: 范围扩大

**决策**: A (per-context reset) + **白盒断言方案** (per Oracle B5 推荐选项 b). 理由: 最小风险 + 严格隔离 + 不动 ChatSession 公开 API (避免 BREAKING 跨 PDK 边界)。

**Oracle B5 finding (2026-10-16 critical)**: 早 session design D3 计划"mock 工具 lambda capture `loop_args['history']`"验证跨 context 隔离, 但:
- (a) `phase1_init` 无条件注册 `loop/run` (`evolution_session.cpp:175+`)
- (b) `ToolRegistry` duplicate registration 抛 `invalid_argument` (`src/common/tools/registry.cpp:101/103`)
- (c) `run_6_phase_demo` monolithic (line 143-149) 无 mock handler 注入窗口
- (d) mock handler (line 234) 完全忽略 args → **mock 模式跨 context 泄漏不可观测**

**Case 6 验证方案修正** (per Oracle B5 推荐选项 b - 白盒断言):
- **NOT** mock lambda capture history (物理不可达 per Oracle B5)
- **YES** 验证 `chat_session_` 指针 per-context 变化 (白盒, 物理可达)
- 测试实施: 通过 friend class / protected accessor / 测试专用 API (e.g. `get_chat_session_for_test()`) 暴露 `chat_session_` 指针/唯一 ID
- 跑 `run_6_phase_demo` 处理 2 contexts (ctx1 public + ctx2 confidential)
- 断言: 进入 ctx1 时 `chat_session_` 指针 (或 unique ID) = P1; 进入 ctx2 时 = P2; P1 != P2 (per-context reset 实证)

**实施位置**: `examples/pdk_chat_demo_evolution/evolution_session.cpp:phase2_baseline` 入口 (line 290 之前)
```cpp
void EvolutionSession::phase2_baseline(const ContextRequest& ctx) {
    // Bug 2 fix (R13.4 cross-context isolation): per-context ChatSession reset
    // 避免 public context 1 content 污染 confidential context 2 LLM call payload
    chat_session_ = std::make_unique<ChatSession>(
        engine_.get(), bus_, &engine_->get_tool_registry(),
        agent_cfg_, SessionConfig{},
        nullptr,  // cancellation_registry
        ... // per phase1_init 模式
    );
    // ... rest of phase2_baseline
}
```

**Test 新增**: `examples/pdk_chat_demo_evolution/tests/test_reverse_indicators.cpp` 加 Case 6
- 构造 2 个 ContextRequest: ctx1 (sensitivity=public) + ctx2 (sensitivity=confidential)
- 跑 `run_6_phase_demo`
- 断言: ctx2 的 trace event meta 或 history 中**不出现** ctx1 的 turn_input 明文
- (注: 实际 ChatSession history 在 LLM 接收前清空, 难以直接验证, 可用 mock 工具的 lambda capture 验证)

### 关联 R13.4 mock 测试保持

- `examples/pdk_chat_demo_evolution/tests/test_reverse_indicators.cpp:265-301` 现有 R13.4 mock case (turn_input + response 脱敏) 必须保持 PASS
- 加 Case 5 (expected_eval_quality 脱敏) + Case 6 (跨 context 隔离)
- 总 6 cases / 6+ assertions PASS

## Out of Scope

- **Bug 3 (env_vars secrets redaction)**: 独立 OPEN 提案 `adr-0072-d4-env-vars-redaction` (per Metis 独占发现 E), 范围正交, 独立 OpenSpec change 处理
- **api_key 字段本身**: 已在 ADR-0068 §3.1 ship, 本 change 不触及
- **trace JSONL 持久化机制**: 不变 (只修改 redact 逻辑, 不改写入流程)
- **ChatSession 公开 API 扩展**: D1 选项 B/D 涉及 BREAKING, 本 change 不引入
- **wire-level redaction** (LLM call payload): per spec R13.4 NOTE `:417` by design 不在 L2 范围, 属 provider 责任

## Impact

| 指标 | 预期 |
|------|------|
| 改动行数 | ~20 行净增 (Bug 1: ~8 行 + Bug 2: ~12 行) |
| 行为变化 | **是** (Bug 修复, 非纯 refactor) |
| 测试改动 | +2 cases (Case 5 + Case 6) |
| 公开 ABI | 0 变化 (D1 选项 A, 不动 ChatSession API) |
| ctest PASS | 必须 305+ 保持 + 2 new cases |
| drop_ratio | 必须 0% (R8 红线) |
| L2 spec R13.4 compliance | 提升 0% → 100% (per 5-维判定 0% → 100% redaction 覆盖) |

## Non-goals

- 不引入新 redact 协议主题
- 不迁移 std::expected monadic ops (独立 Phase B-1)
- 不引入 std::move_only_function (独立 Phase B-2)
- 不动 std::expected 公开 ABI (保持 ship 后契约)
- 不重写 ChatSession::chat() (范围最小化)

## Dependencies

- **前置 ship**: 无 (spec 409 显式 + impl 582-602 缺字段)
- **关联提案**: `r13-4-confidential-redaction-real-llm` (Path 1 关闭, commit 7321154) + `adr-0072-d4-env-vars-redaction` (parallel OPEN, 独立 change)
- **R13.4 mock 测试**: `test_reverse_indicators.cpp:265-301` 必须保持 PASS (零回归)

## Acceptance Criteria (TDD 5-step, 14 AC)

### Bug 1 (expected_eval_quality 脱敏, 5 AC)
- [ ] AC-1: `redact_trace_fields` 显式处理 `expected_eval_quality` 字段 (per spec 409 完整覆盖)
- [ ] AC-2: `sensitivity == "confidential"` 时 `event["expected_eval_quality"]` → `"[REDACTED-confidential]"`
- [ ] AC-3: `sensitivity == "internal"` 时 `event["expected_eval_quality"]` → `"[REDACTED-internal]"` (per spec 408 隐含)
- [ ] AC-4: `sensitivity == "public"` 或空时 `event["expected_eval_quality"]` 保留原值
- [ ] AC-5: `expected_eval_quality` 是 `null` 时不处理 (现有 is_string() 守卫已覆盖)

### Bug 2 (跨 context 隔离, 5 AC)
- [ ] AC-6: `phase2_baseline` 入口 `chat_session_` reset
- [ ] AC-7: 2 contexts (public + confidential) trace emit 中 confidential context 不含 public context turn_input 明文
- [ ] AC-8: per-context reset 不影响 phase1_init 接线 (real_provider_ borrow, loop/run register)
- [ ] AC-9: per-context reset 不破坏 hermetic HOME 双轨 (real LLM mode Case 5)
- [ ] AC-10: 既有 9/9 mock tests 零回归 (含 9 个 l2-evolution + 跨 context 多 phase emit)

### Reverse Indicator 5 字段 (per AGENTS.md)
- [ ] AC-11: commit message 含 `[Reverse Indicator]` 5 字段
- [ ] AC-12: `new_up: 2 bug 修复` + `old_down: drop_ratio=0%` + `failure_traces: 2 bugs per dual-agent` + `ablation: 3-segment baseline/mutated/rerun` + `context_ids: 2 fixture context_ids`
- [ ] AC-13: 既有 spec R13.4 公开 spec 保持 (无 schema breaking)
- [ ] AC-14: `day-5 archive 5-file integrity` 验证 (本 change 实施后 git mv 到 archive/)

## Cooling-off

`r13-4-confidential-redaction-real-llm` Path 1 关闭 (commit 7321154, 2026-10-15)
本 change created at 2026-10-15T17:50:00Z
Cooling-off 24h: 2026-10-16T17:50:00Z ✅ (期间可派 Oracle pre-impl review)

## Related

- `.rddf/audits/2026-10-15-r13-4-confidential-redaction-real-llm-dual-agent-review.md` (dual-agent 调研审计)
- `.rddf/audits/2026-10-15-rsi-anti-cheat-real-llm-dual-agent-review.md` (姐妹审计)
- `openspec/changes/archive/2026-09-24-pdk-chat-demo-evolution-reference-example/` (L2 reference example)
- ADR-0068 §3.1 (api_key redaction 协议模板, 本 change 类似思路)
- AGENTS.md §Reverse Indicator Rule 5 字段
- AGENTS.md §ENGINEERING PATTERNS Pattern #4 (SHIP-with-fixes atomic commits)
