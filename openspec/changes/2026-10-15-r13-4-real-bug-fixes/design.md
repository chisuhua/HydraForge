# Design: R13.4 confidential redaction 2 真 bug 修复

**Change ID**: `2026-10-15-r13-4-real-bug-fixes`
**Status**: draft
**对应 proposal**: `proposal.md`

## Context

`r13-4-confidential-redaction-real-llm` 提案（commit `7321154` 2026-10-15 关闭 Path 1）的 dual-agent 调研（Metis `ses_ec1545fcdffe79LuLYcKsBH38D` + Oracle `ses_ec1540a00ffeoqYH8dp7b32Zcx` 独立交叉命中）识别 R13.4 mock 之外的 2 个真 bug：

1. **`expected_eval_quality` 漏脱敏** — spec 409 显式要求，impl 582-602 缺字段
2. **跨 context 泄漏** — `chat_session_` 跨 context 共享，messages history 累积

需独立 OpenSpec change 修复 2 bug + TDD 5-step 实施 + Oracle SHIP-with-fixes 复评。

## 1. 设计决策 D1-D6

### D1: per-context 隔离策略（Bug 2 修复方案）

**选项**:
- **A (推荐)**: per-context `chat_session_` reset（`phase2_baseline` 入口）
  - 优点: 严格隔离 + 最小风险 + 不动 ChatSession 公开 API
  - 缺点: 每 context 创建新实例（mock mode 性能影响可忽略，real LLM mode 重新连接可接受）
- B: 调 `chat_session_->clear_history()` 公开 API
  - 优点: 性能更优
  - 缺点: 需 ChatSession 公开新 API（**BREAKING，跨 PDK 边界**）
- C: 在 `loop_args` 构造时清空 `history` 字段
  - 优点: 最小改动
  - 缺点: 治标不治本，`ChatSession::impl_->messages` 仍在累积
- D: 改 ChatSession API 增加 `start_new_session()` 方法
  - 优点: 长期更通用
  - 缺点: 范围扩大

**决策**: A.

**理由**:
- 最小风险：不动 ChatSession 公开 API（避免 BREAKING 跨 PDK 边界）
- 严格隔离：彻底切断 cross-context 污染路径
- 性能可接受：mock mode setup 一次 / real LLM mode setup_hermetic_home 已有隔离机制
- 范围最小化：只改 `examples/pdk_chat_demo_evolution/evolution_session.cpp` 一处

**实施位置** (`examples/pdk_chat_demo_evolution/evolution_session.cpp:290` 入口):
```cpp
void EvolutionSession::phase2_baseline(const ContextRequest& ctx) {
    // Bug 2 fix (R13.4 cross-context isolation): per-context ChatSession reset
    // 避免 public context 1 content 污染 confidential context 2 LLM call payload
    chat_session_ = std::make_unique<ChatSession>(
        engine_.get(), bus_, &engine_->get_tool_registry(),
        agent_cfg_, SessionConfig{},
        nullptr,  // cancellation_registry
        // ... per phase1_init 模式
    );
    // ... rest of phase2_baseline
}
```

**风险**: 重复 `make_unique` 模板代码 → 抽 helper `recreate_chat_session()` 复用

### D2: Bug 1 修复字段位置

**选项**:
- **A (推荐)**: 在 `redact_trace_fields` 函数内加 `expected_eval_quality` 脱敏分支
  - 优点: 与现有 `turn_input` + `response` + `meta.tags` + `meta.domain` 处理模式一致
  - 缺点: top-level field 脱敏（与 `meta.*` 不同）
- B: 在 event 构造前 caller 端脱敏 `expected_eval_quality`
  - 优点: caller 决定 redact 策略
  - 缺点: 散落多个 caller，易遗漏

**决策**: A.

**理由**:
- 与现有 `redact_trace_fields` 设计一致（centralize redaction logic）
- 防止 future caller 遗漏脱敏
- 易测试（直接 unit test `redact_trace_fields` 函数）

**实施位置** (`examples/pdk_chat_demo_evolution/evolution_session.cpp:582-602`):
```cpp
nlohmann::json redact_trace_fields(nlohmann::json event, const std::string& sensitivity) {
    if (sensitivity.empty() || sensitivity == "public") return event;
    std::string level = (sensitivity == "confidential") ? "confidential" : "internal";
    if (event.contains("turn_input") && event["turn_input"].is_string()) {
        event["turn_input"] = "[REDACTED-" + level + "]";
    }
    if (event.contains("response") && !event["response"].is_null()) {
        event["response"] = "[REDACTED-" + level + "]";
    }
    // Bug 1 fix (R13.4 spec 409 compliance): expected_eval_quality 脱敏
    if (event.contains("expected_eval_quality") && event["expected_eval_quality"].is_string()) {
        event["expected_eval_quality"] = "[REDACTED-" + level + "]";
    }
    if (sensitivity == "confidential" && event.contains("meta") && event["meta"].is_object()) {
        auto& m = event["meta"];
        if (m.contains("tags")) m["tags"] = "[REDACTED-confidential]";
        if (m.contains("domain")) m["domain"] = "[REDACTED-confidential]";
    }
    return event;
}
```

### D3: 跨 context 隔离测试设计

**难点**: ChatSession::impl_->messages 在 LLM 接收 payload 前清空，难以直接验证

**测试策略**:
- 构造 2 contexts (public + confidential)
- 跑 `run_6_phase_demo`
- 断言 1: confidential context trace emit 中 `event["response"]` 是 `[REDACTED-confidential]` (整字段替换)
- 断言 2: confidential context trace emit 中**不出现** public context turn_input 明文
- 断言 3: 用 mock 工具 lambda capture 验证 LLM call payload 中 `history` 字段不包含前 context turn_input

**可行性**: 既有 `tests/test_l2_real_llm_e2e.cpp` Case 1 (real trace) + Case 2 (real drop_ratio) 有 `run_demo` helper，可复用

### D4: 不扩展 redact 协议主题

**决策**: 不新增 ADR-0068 Appendix A 主题

**理由**:
- `redact_trace_fields` 函数本身已实现 redaction 协议
- 缺字段是 implementation bug，不是协议变更
- 修复后 100% spec R13.4 compliance，无需新协议主题

### D5: 不动 ChatSession 公开 API

**决策**: D1 选项 A (per-context reset)，不引入 ChatSession 新 API

**理由**:
- 避免 PDK 公开 API BREAKING
- 现有 `phase1_init` 已 make_unique 模式，新增 `recreate_chat_session()` helper 复用

### D6: 既有 9/9 mock tests 零回归

**决策**: 实施前快照 `l2-evolution` 测试套件 baseline

**具体测试**:
- `test_l2_real_llm_e2e` 5 cases (real LLM drop_ratio)
- `test_l2_ship_validation` (per `l2-evolution-finalization`)
- `test_l2_audit` (R8 reverse indicators 4 cases)
- `test_l2_redaction_static_guard` (Bug 1 + Bug 2 修复后新增)
- 既有 9/9 + 新 2 cases = 11/11 PASS

## 2. 候选迁移详细设计

### 步骤 1: Bug 1 修复 (TDD RED → GREEN)

**RED**:
1. 在 `tests/test_reverse_indicators.cpp` 加 Case 5 (构造 confidential context + expected_eval_quality = "Poor")
2. 跑测试，断言 `event["expected_eval_quality"] == "[REDACTED-confidential]"` 失败 (因为 impl 不处理)
3. 验证失败信息显示是 `redact_trace_fields` 函数问题

**GREEN**:
1. 在 `redact_trace_fields` 函数 line 590-592 之后加 `expected_eval_quality` 脱敏分支
2. 跑测试 PASS
3. 验证 `test_reverse_indicators` 既有 4 cases 零回归

### 步骤 2: Bug 2 修复 (TDD RED → GREEN)

**RED**:
1. 在 `tests/test_reverse_indicators.cpp` 加 Case 6 (2 contexts: public + confidential)
2. 跑测试，断言 confidential context trace emit 不含 public context 明文失败 (因为 chat_session_ 共享 + history 累积)
3. 验证失败

**GREEN**:
1. 抽 helper `EvolutionSession::recreate_chat_session()` 复用 `phase1_init` make_unique 模式
2. 在 `phase2_baseline` 入口调 `recreate_chat_session()`
3. 跑测试 PASS
4. 验证 `test_l2_real_llm_e2e` Case 5 (hermetic HOME + real LLM dual-track) 仍 PASS

### 步骤 3: 实施验证

- `ctest --test-dir build/examples/pdk_chat_demo_evolution/tests -L l2-evolution` 11/11 PASS
- `ctest --test-dir build -LE must_realllm` 全 PASS (含 must_realllm 标签)
- `cmake --build build --target agenticdsl_core` 100% Built
- `ctest -R test_skill_interpreter -j1` 单独 PASS (避免 pre-existing flaky)

## 3. 风险与回滚

### 风险 1: per-context reset 破坏 real LLM mode 性能

**缓解**: real LLM mode 每 context 重新连接 setup_hermetic_home，但 `--real-llm deepseek` 模式 API call 本就每次新建连接（l2-evolution-real-llm-provider 设计）。实测 drop_ratio 应仍 0%。

**回滚**: D1 切到选项 C (loop_args 清空 history 字段)，最小改动。

### 风险 2: 既有 9/9 mock tests 因 chat_session reset 行为变化失败

**缓解**: 既有 mock tests 不依赖跨 context history (单 context 跑)。实测应零回归。

**回滚**: D1 切到选项 D (ChatSession 公开新 API)，需多会话。

### 风险 3: redact expected_eval_quality 影响 现有 spec scenario

**缓解**: spec R13.4 显式要求。修复是合规化，零风险。

**回滚**: N/A (修复即合规)。

## 4. 测试设计 (14 AC, per proposal §Acceptance)

### Bug 1 (5 AC, test_reverse_indicators Case 5)
- AC-1: redact_trace_fields 含 expected_eval_quality 分支
- AC-2: confidential → `[REDACTED-confidential]`
- AC-3: internal → `[REDACTED-internal]`
- AC-4: public/empty → 保留原值
- AC-5: null → 跳过

### Bug 2 (5 AC, test_reverse_indicators Case 6)
- AC-6: phase2_baseline 入口 chat_session_ reset
- AC-7: 2 contexts trace emit 跨 context 不泄漏
- AC-8: per-context reset 不影响 phase1_init 接线
- AC-9: per-context reset 不破坏 hermetic HOME 双轨
- AC-10: 9/9 既有 l2-evolution mock 零回归

### Reverse Indicator (4 AC)
- AC-11: commit message 含 5 字段
- AC-12: 5 字段具体值
- AC-13: 既有 spec R13.4 schema 保持
- AC-14: Day-5 archive 5-file integrity

## 5. 实施时间线

| 阶段 | 任务 | 估时 |
|------|------|------|
| P0 | dual-agent pre-impl review (已 done, commit 7321154) | 0h |
| P1 | proposal/design/tasks/spec 创建 (本 change) | 1h |
| P1.5 | cooling-off 24h | 24h |
| P2 | TDD 5-step 实施 (Bug 1 + Bug 2) | 1-2 day |
| P2.5 | Oracle post-impl SHIP-with-fixes review | 30 min |
| P3 | git mv archive + AGENTS.md Recent Changes 登记 | 15 min |
| **总** | 24h cooling-off + 2 day 实施 + 1h review | **2-3 day** |

## 6. 参考

- `.rddf/audits/2026-10-15-r13-4-confidential-redaction-real-llm-dual-agent-review.md` (dual-agent 调研)
- `examples/pdk_chat_demo_evolution/tests/test_reverse_indicators.cpp:265-301` (R13.4 mock 现有)
- `examples/pdk_chat_demo_evolution/evolution_session.cpp:144-149, 252, 290-301, 582-602` (修改位置)
- `openspec/changes/archive/2026-09-24-pdk-chat-demo-evolution-reference-example/specs/pdk-chat-demo-evolution/spec.md:409, 417` (spec source)
- `pdk/chat_session/src/chat_session.cpp` (ChatSession::chat 历史管理)
- AGENTS.md §Reverse Indicator Rule + §ENGINEERING PATTERNS Pattern #4
