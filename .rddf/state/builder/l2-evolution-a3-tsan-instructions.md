# A3 TSan Baseline Scan Instructions

## 触发条件
A1+A2 (T2+T3) merge → main 完成

## 目标
填补 Sprint 33+ (2026-09-16) 以来 4+ sprints 的 TSan 验证空窗 (per AGENTS.md §Reverse Indicator Rule + §模式 #7 Concurrent ctest race detection)

## 执行方案

### 方案 A: CI 触发 (推荐, 机器性能不受限)

```bash
# GitHub Actions 触发
gh workflow run tscan.yml  # 如果 workflow 配置

# 或本地 CI 镜像
docker run --rm -v $(pwd):/workspace hydraforge/ci:tsan \
  bash -c "cmake --preset tsan -B build-tsan -DAGENTICDSL_BUILD_TESTS=ON && \
           cmake --build build-tsan -j\$(nproc) && \
           ctest --test-dir build-tsan -j\$(nproc) --output-on-failure"
```

### 方案 B: 本地高配机

```bash
cmake --preset tsan -DAGENTICDSL_BUILD_TESTS=ON
cmake --build build-tsan -j$(nproc)
ctest --test-dir build-tsan -j$(nproc) --output-on-failure
```

### 方案 C: 最小 subset (机器受限)

如果机器不够强,优先覆盖 Sprint 35-36 期间改动最大的模块:
```bash
ctest -L "l2-evolution|domain_worker_pool|cognitive" -j$(nproc)
ctest -R "test_evolution_session|test_gepa_phase2|test_harness_rsi_pilot|test_genome_registry|test_credit_assignment" -j$(nproc)
```

## 验收标准

### Pass Criteria
- **0 TSan warnings** (新 baseline)
- 与 Sprint 10 baseline 对齐 (Sprint 10 = 34/34 ✅)
- 与 Sprint 21 baseline 对齐 (72/72 ✅ per C16 后)

### Fail 处理 (per AGENTS.md §模式 #7 + Pattern #11)
1. **写 fuzz test 复现** (不能直接 fix)
2. 修复 (按 race 类型):
   - data race → 加 mutex / std::atomic<bool>
   - dtor race → RAII guard + barrier (per Pattern #9 contract drain API)
   - lock-order-inversion → 拆分锁区间 (per fix-session-manager-lock-order-inversion 案例)
3. **加 regression guard test** (防止未来回归)
4. **SHIP-with-fixes commit** (per Pattern #4, 不 amend)

### Success 后的更新
- AGENTS.md §Recent Changes 添加 "TSan baseline reset 2026-09-28" entry
- drop_ratio 重新基线 (per R8 reverse indicator)
- 解锁 Wave 3 D4 LoRA training pipeline 数据路径 (per ADR-0078)

## 历史 TSan 状态参考

| Sprint | TSan 状态 | 来源 |
|--------|----------|------|
| Sprint 10 (2026-06-26) | 34/34 ✅ | pre-existing-sanitizer-findings |
| Sprint 21 (2026-07-09) | 72/72 ✅ | C16 (Phase 5 ILLMProvider) |
| Sprint 33+ (2026-09-16) | NOT-VERIFIED | KI 1 |
| Sprint 35-36 (2026-09-22~26) | NOT-VERIFIED | KI 2-4 (多个 commit) |
| **A3 后 (2026-09-28)** | **TBD - 待验证** | 本 change |

## 已知 KI 监控

- **KI-1 TimerService dtor hang** ✅ RESOLVED (2026-09-12, commit `897b147`)
- **test_skill_interpreter 7.S29-1** ⚠️ Pre-existing flaky (MockToolRegistry sync return, AGENTS.md 早记录, inherent limitation)
- **TSan #191 lock-order-inversion** ✅ FIXED (`04f8351~051dc84`)
- **TSan #192 EventLogWriter file_ race** ✅ FIXED (`be2f103`)

## post-merge 顺序

A1+A2 merge → A3 TSan → A4 T5 dispatch → A5 T6 archive

A3 必须在 A4 之前完成 (T5 capture-mode=Training 涉及 IDistillationWriter, 可能引入新 race)

## evidence_required

```bash
# 跑完 TSan 后记录
1. ctest --test-dir build-tsan -j$(nproc) 2>&1 | tail -30 > a3-tsan-results.txt
2. cmake --build build-tsan --target _run_tests 2>&1 | tail -10 >> a3-tsan-results.txt
3. 验证 0 warnings, file commit 到 feat/l2-evolution-finalization (or main if merged)
```

## 时间估算

- 方案 A (CI): 30-60 min (网络 + Docker)
- 方案 B (本地高配): 1-2 hours
- 方案 C (subset): 30-60 min

## risk

- TSan 性能开销 ~5-10x, 跑 266 tests 可能超时 → 优先 subset
- 机器负载高 → CI 优先
- TSan 与现有 pre-existing KI 冲突 → 已知 KI 排除后判断