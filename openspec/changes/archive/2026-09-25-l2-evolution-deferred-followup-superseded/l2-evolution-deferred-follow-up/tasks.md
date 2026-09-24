# l2-evolution-deferred-follow-up — Tasks

> **Status**: 🔍 Proposed Tasks
> **关联 Proposal**: [`./proposal.md`](./proposal.md)
> **关联 Design**: [`./design.md`](./design.md)
> **关联 Spec**: [`./specs/l2-evolution-deferred/spec.md`](./specs/l2-evolution-deferred/spec.md)

---

## Task Groups (3 阶段 + 1 research)

### Phase 1: R8 Full Metrics (Sprint 37+)

#### T1.1 [RED→GREEN] --release-metrics 从 stub 升级为真实 IEvaluator V2 调用

- **RED**: `test_reverse_indicators` 新 case "drop_ratio > 5% exit non-zero" 在 stub 模式下 FAIL (stub 恒返回 0%)
- **GREEN**: main.cpp `--release-metrics` 阶段真实调 IEvaluator V2 的 `BehavioralEquivalence::compare()`，收集 attribution_verdict 分布
- **GREEN**: 计算 `drop_ratio = (fail_count / total_contexts)`，> 5% == exit non-zero
- **GREEN**: 输出 metrics.json (new_up / new_down / old_up / old_down / drop_ratio)
- **REFACTOR**: 从 main.cpp inline 提取 `compute_release_metrics()` + `write_metrics_json()` 公共 helper

#### T1.2 [RED→GREEN] --ablation-mode=full 3 段对照

- **RED**: test 新 case 验证 ablation_report.json 3 segments
- **GREEN**: Segment 1 (same-task-diff-Harness) attribution_verdict 分布对比
- **GREEN**: Segment 2 (same-Harness-diff-task) 多 task_class 一致性矩阵
- **GREEN**: Segment 3 (failure-sample-retention) 使用 r8_failure_fixtures.jsonl
- **REFACTOR**: 提取 `compute_ablation_report()` + `write_ablation_report_json()`

#### T1.3 [RED→GREEN] --failure-event-format=v2

- **RED**: test 新 case 验证 v2 事件含 rule_shipped_commit + reproduce_in_new_task_demo
- **GREEN**: evolution_tracer 支持 failure_event chain 输出含 5 fields
- **GREEN**: 验证 `context_id` 链在 v2 模式下完整

---

### Phase 2: Wave 4 Sandbox (独立立项)

#### T2.1 [ADR] DockerBackendConfig.network_mode

- 创建 ADR 文档 `docs/adr/adr-0090-docker-backend-network-mode.md`
- 定义 `network_mode` 配置项 (默认 `"bridge"` 向后兼容)
- 审核 ADR ✅ Approved

#### T2.2 [GREEN] docker_backend net_mode impl

- `src/common/utils/docker_backend.cpp` + `.h`: 加 `network_mode` 字段到 `DockerBackendConfig`
- 容器创建时用 `config.network_mode` 替代硬编码 `"NetworkMode", "bridge"`
- 验证 `network_mode=none` 时容器无外网

#### T2.3 [RED→GREEN] test_anti_cheat_sandbox_escape 升级

- **RED**: 当前 keyword-rejection test 在 sandbox 环境应 FAIL (仅检查 keyword)
- **GREEN**: 添加真 sandbox 隔离验证 (容器内 curl/wget 应 timeout)
- **GREEN**: 保留 keyword-rejection 后退 (当 sandbox 不可用时继续降级)
- **REFACTOR**: `test_anti_cheat_sandbox_escape` 3 cases 从 "keyword-only" 升级为 "sandbox-first + keyword-fallback"

---

### Phase 3: Wave 3 Phase 2 D4-D7 (独立立项)

#### T3.1 [GREEN] workflow_patch V1 support

- 在 `harness_rsi::apply_harness_mutation` 中实现 `workflow_patch` 变体处理
- 移除 `return UnsupportedVariant` fallback
- evolution_session::phase3_mutation 额外路径：workflow_patch → 重写 DSL 子图

#### T3.2 [GREEN] ≥5 baseline samples 统计断言

- IEvaluator V2 BehavioralEquivalence: 使用 `kMinBaselineSamples=5` 阈值
- <5 samples → 返回 `Insufficient` (诚实标记)
- ≥5 samples → `compare()` 输出统计显著 verdict
- 配合 `run_6_phase_demo` 积累 baseline samples

#### T3.3 [GREEN] IDistillationWriter capture-mode=Training

- `--capture-mode=Training` 从 stub 升级：真实路径写 DistillationRecord 到 disk
- capture_mode_downgrade 事件触发时 fail-open (不 crash)

---

### Phase 4: S4 Research Path (indefinite)

#### T4.1 [Research] Agent-Agent 协同进化 survey

- 调研现有 Agent-Agent co-evolution literature
- 输出 research brief (不在此 change scope)
- 如获立项批准，创建独立 OpenSpec change

---

## Acceptance

- **AC1**: `ctest -L l2-evolution` 维持 9/9 PASS (向后兼容，不回归)
- **AC2**: `--release-metrics` 输出真实 metrics.json (非 stub)
- **AC3**: `--release-metrics` 遇到 drop_ratio > 5% 时 exit non-zero
- **AC4**: `--ablation-mode=full` 输出有效 3-segment ablation_report.json
- **AC5**: `--failure-event-format=v2` 输出含 5 fields 的 failure event
- **AC6**: sandbox `network_mode=none` 容器验证无外网
- **AC7**: workflow_patch mutation 返回成功 (非 UnsupportedVariant)
- **AC8**: BehavioralEquivalence ≥5 samples 输出有效 verdict
- **AC9**: N1/N2/N5 硬限制保持 (sandbox infra ADR 例外)
- **AC10**: 所有变更含 `[Reverse Indicator]` 5-field commit block