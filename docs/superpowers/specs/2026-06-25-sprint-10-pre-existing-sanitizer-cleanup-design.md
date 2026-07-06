# Sprint 10 Design: Pre-existing Sanitizer Cleanup

> **设计日期**: 2026-06-25
> **作者**: Sisyphus-Junior (orchestrator)
> **状态**: 🟢 Approved (Sprint 10 brainstorm session)
> **承接 change**: `2026-06-25-pre-existing-sanitizer-findings` (12/50 tasks, P2.5 ship gate 复验已 ship)
> **目标 change**: 完成剩余 38 task + ship gate 闭环 (34/34 ASan + 34/34 TSan 零 pre-existing)

## 1. Context

### 1.1 起点状态 (2026-06-25)

- ✅ Phase 1 智能体层 100% 收官 (5 ADR Approved)
- ✅ 4-change archive chain 完整 ship (Sprint 10 起点 0 active change)
- ✅ engine-include-decoupling spec §sanitizer-revalidation Scenario 已触发 "独立 change 跟踪" 动作
- ✅ Pre-existing tracking change `2026-06-25-pre-existing-sanitizer-findings` 创建 (12/50 tasks done)
- ✅ Audit 报告 `docs/audits/2026-06-25-sanitizer-revalidation.md` 已 ship
- ⚠️ ASan 33/34 (1 失败: test_cognitive_worker)
- ⚠️ TSan 32/34 (2 失败: test_cognitive_worker + test_domain_worker_pool)

### 1.2 Sprint 10 范围锁定 (brainstorm session)

| 决策维度 | 选择 | 理由 |
|---|---|---|
| 主题范围 | A: pre-existing sanitizer cleanup | 治理债清理 + ship gate 闭环 → archive 后 Sprint 11 干净起跳 |
| Ship gate | A: 完全干净 34/34 ASan + 34/34 TSan | 零 pre-existing, 治理债彻底消化 |
| Time box | D: 没有 deadline | Quality 优先, 不赶进度 |
| 执行模式 | C: 分阶段串行 (P1→P2→P3) | 每阶段 context 完整, subagent 独立 |
| P1+P2 路径 | B: 调查优先 + 验证后修复 | P2 修复基于 TSan 详细堆栈数据 |

## 2. Architecture & Components

### 2.1 OpenSpec Change 拓扑

```
openspec/changes/2026-06-25-pre-existing-sanitizer-findings/
├── .openspec.yaml            ✅ ship (P2.5 复验阶段)
├── proposal.md               ✅ ship
├── specs/pre-existing-sanitizer-findings/spec.md   ✅ ship
└── tasks.md                  ✅ ship (50 task 跟踪结构)

Sprint 10 完成后:
  - tasks.md §2-4 全部 in_progress → completed
  - openspec validate 仍然 valid
  - openspec archive 2026-06-25-pre-existing-sanitizer-findings
```

### 2.2 三阶段架构

```
┌─────────────────────────────────────────────────────────────┐
│ Phase 1: P1 CognitiveWorker test 修复 (串行, 1-2h)         │
│                                                              │
│  INPUT:  tests/test_cognitive_worker.cpp:206-237            │
│          stack-use-after-scope at line 226 (worker ref)     │
│                                                              │
│  ACTION: std::vector<std::thread> → std::vector<std::jthread>│
│          + wait_until 强化 + worker.stop() 顺序调整         │
│                                                              │
│  OUTPUT: 1 atomic commit + ctest 34/34 + ASan 34/34          │
│          + TSan 33/34 (domain_worker_pool 仍 pre-existing)  │
└─────────────────────────────────────────────────────────────┘
                            ↓
┌─────────────────────────────────────────────────────────────┐
│ Phase 2: P2 DomainWorkerPool TSan 修复 (串行, 5-8h)         │
│                                                              │
│  P2.1 TSan 堆栈调查 (2-3h):                                  │
│    - cmake --preset tsan -DAGENTICDSL_BUILD_TESTS=ON         │
│    - ctest -R test_domain_worker_pool --output-on-failure    │
│    - 12 warnings 收集堆栈 + 分桶 (Catch2 resetAssertionInfo │
│      vs product code race)                                  │
│    - docs/audits/p2-tsan-investigation.md 创建              │
│                                                              │
│  P2.2 修复策略决策 (1-2h):                                   │
│    - 基于 P2.1 数据选择修复点                                │
│    - 决策树: jthread submitter / SECTION 隔离 / memory_order│
│                                                              │
│  P2.3 实施修复 (2-3h):                                       │
│    - 修改 tests/test_domain_worker_pool.cpp (按 P2.2 决策)  │
│    - commit + ctest 34/34 + TSan 34/34                       │
└─────────────────────────────────────────────────────────────┘
                            ↓
┌─────────────────────────────────────────────────────────────┐
│ Phase 3: P3 文档同步 + Ship Gate + Archive (串行, 1h)       │
│                                                              │
│  - docs/roadmap-status.md §ASan/TSan 验证表完整 (含 P2 修复)│
│  - AGENTS.md §Recent Changes 追加 Sprint 10 ship entry     │
│  - docs/audits/2026-06-25-sanitizer-revalidation.md 增强   │
│    §P2.5 (添加 P2 TSan 修复详情)                            │
│  - final ship gate: ctest + ASan + TSan 全部 34/34          │
│  - openspec archive 2026-06-25-pre-existing-sanitizer-findings│
└─────────────────────────────────────────────────────────────┘
```

### 2.3 修复策略决策矩阵

| 修复点 | 方案 | 适用场景 | 风险 |
|---|---|---|---|
| `std::thread` submitter → `std::jthread` | A | 解决 std::thread 出 scope race | 低 (C++20 RAII) |
| `wait_until` 内 atomic polling 强化 | B | 解决 jthread handler race window | 中 (需严格同步语义) |
| Catch2 `SECTION` 隔离并发测试 | C | 避免 Catch2 framework race | 中 (test 拆分复杂) |
| Product code `memory_order` 强化 | D | Catch2 警告实际来自 product code | 高 (需深度 race 分析) |

## 3. Data Flow & Decision Logic

### 3.1 P1 CognitiveWorker 修复流程

```
ASan report:
  stack-use-after-scope
  at test_cognitive_worker.cpp:226 (worker.submit_task)
  reads from stack frame (worker stack local)

根因分析:
  1. worker 是 stack 局部变量 (line 219)
  2. submitter threads lambda [&] captures worker reference (line 224)
  3. submitter threads 完成 + join (line 231)
  4. wait_until(1000) 等异步 jthread handler 完成 (line 233)
  5. worker.stop() 停止 jthread (line 234)
  6. test scope 出 scope → worker 析构
  7. ⚠️ 如果 wait_until 提前 return 但 jthread handler 仍在跑
     → handler 访问 worker state (stack local) → ASan 报错

修复:
  ┌─────────────────────────────────────────────────┐
  │ std::vector<std::thread> threads;               │
  │   ↓                                              │
  │ std::vector<std::jthread> threads;              │
  │   (RAII auto-join on destruction)               │
  │                                                  │
  │ // 移除显式 join (line 231):                     │
  │ // for (auto& t : threads) t.join();            │
  │                                                  │
  │ // 强化 wait_until (lambda):                    │
  │ wait_until([&] {                                │
  │   return completed_count.load() == 1000;        │
  │ }, std::chrono::seconds(30));  // 加 timeout    │
  │                                                  │
  │ // worker.stop() 顺序保持                       │
  │ worker.stop();                                  │
  └─────────────────────────────────────────────────┘
```

### 3.2 P2 DomainWorkerPool 修复决策树 (P2.1 调查后填充)

```
P2.1 TSan 调查 → 12 warnings 分桶
  │
  ├── 桶 1: Catch2 framework race (resetAssertionInfo + jthread)
  │   - 出现频率: X/Y
  │   - 修复策略: test scope 隔离 + 严格 stop 顺序
  │   - 风险: 中
  │
  ├── 桶 2: DomainWorkerPool product code race
  │   - 出现频率: X/Y
  │   - 修复策略: memory_order 强化 / 锁顺序调整
  │   - 风险: 高 (需深度分析)
  │
  └── 桶 3: 其他 (bus subscribe / atomic ops)
      - 出现频率: X/Y
      - 修复策略: 按具体堆栈
      - 风险: 中

P2.2 决策:
  IF 桶 1 主导 (>50%) → 优先 test scope 隔离
  IF 桶 2 主导 (>30%) → 深度产品代码分析 + Oracle 咨询
  ELSE → 桶 1 + 桶 3 组合修复
```

### 3.3 Ship Gate 验收数据流

```
Phase 1 commit → ctest baseline 34/34 + ASan 34/34 (P1 已修)
                                       + TSan 33/34 (P2 仍 fail)
                                       ↓
Phase 2 commit → ctest baseline 34/34 + ASan 34/34
                                       + TSan 34/34 (P2 已修)
                                       ↓
Phase 3 docs   → git status clean + openspec archive
                                       ↓
Final ship gate = ✅ ALL GREEN
```

## 4. Error Handling & Rollback

### 4.1 P1 失败 Fallback

```
P1 commit 失败 (ASan 仍 fail):
  1. git log -1 看 commit hash
  2. git revert HEAD (撤销 P1 commit)
  3. git reset --hard HEAD~1 (回到 P1 前 baseline)
  4. systematic-debugging skill 重新诊断:
     - 看 ASan 详细堆栈 --track-origins=yes
     - 隔离 test_cognitive_worker 单跑
     - 假设: jthread auto-join timing 问题 / atomic memory_order
  5. 重新设计 P1 fix + commit
```

### 4.2 P2.1 调查超时 Fallback

```
P2.1 调查超 3h (未达成分桶):
  1. docs/audits/p2-tsan-investigation.md 记录 partial 结果
  2. 升级 — split P2 为:
     - P2a: 调查 + 决策 (深度, 单独 deep agent)
     - P2b: 实施 (基于 P2a 输出)
  3. Oracle 咨询 product code race 分析
```

### 4.3 P2.3 实施超时 Fallback

```
P2.3 实施超 3h:
  1. split 为 P2.3a (jthread submitter 替换, 1h)
           + P2.3b (SECTION 隔离, 1h)
           + P2.3c (memory_order 强化, 1h)
  2. 每个 P2.3x 独立 commit + 验证
```

### 4.4 全局 Rollback Strategy

```
每 commit 前 baseline check:
  git status --short           # 必须 clean
  ctest --test-dir build       # 必须 34/34 PASS

失败 → git reset --hard HEAD 回滚到上一个 green commit
ASan/TSan build artifacts 隔离:
  build/asan/   (独立 build dir)
  build/tsan/   (独立 build dir)
不影响 baseline build
```

## 5. Testing & Acceptance

### 5.1 Unit Tests (locked pre-existing)

| Test Case | Phase | Expected | Lock |
|---|---|---|---|
| `CognitiveWorker concurrent submit 10x100 TSan clean` | P1 | ASan 0 + TSan 0 | ✅ ASan pre-existing → fixed |
| `DomainWorkerPool 1000x concurrent submit TSan clean` | P2 | TSan 0 | ✅ TSan pre-existing → fixed |
| 32 其他测试 | baseline | 零回归 | ✅ 已有 34/34 baseline |

### 5.2 Ship Gate 验收清单

```
[ ] baseline:  ctest --test-dir build  → 34/34 PASS
[ ] ASan:      ctest --test-dir build/asan  → 34/34 PASS
[ ] TSan:      ctest --test-dir build/tsan  → 34/34 PASS
[ ] Lint:      .clang-tidy 无新 warning (diff vs main)
[ ] Git:       git status clean
[ ] OpenSpec:  openspec list 显示 0 active change
[ ] 文档同步:
    [ ] AGENTS.md §Recent Changes 追加 2026-06-25 (Sprint 10 ship)
    [ ] docs/roadmap-status.md §ASan/TSan 验证表含 P2 修复详情
    [ ] docs/audits/2026-06-25-sanitizer-revalidation.md §P2.5 含 P2 TSan 详情
[ ] Commit history:
    [ ] P1: 1+ atomic commit
    [ ] P2.x: 1-3 atomic commits (按 P2.2 决策)
    [ ] P3: 1 batch commit (文档同步)
```

### 5.3 Non-goals

- ❌ 重构 CognitiveWorker / DomainWorkerPool 产品代码 (除必要 race fix)
- ❌ 添加新测试 case (除 lock 现有测试)
- ❌ Catch2 框架升级 (超 Sprint 10 范围)
- ❌ Phase 2/3/4 ADR 启动 (Sprint 11+ 范围)

## 6. Communication Plan

### 6.1 Status Updates

- 每次 commit 后: 1 行 summary (commit hash + 关键变更)
- P1 完成: 中等状态更新 (ASan 34/34 达成)
- P2 完成: 大状态更新 (TSan 34/34 达成, ship gate 干净)
- Sprint 10 ship: 最终 status report (1 commit + archive)

### 6.2 Documentation Updates

| 文档 | Phase | 内容 |
|---|---|---|
| `docs/audits/p2-tsan-investigation.md` | P2.1 | TSan 12 warnings 调查分桶 + 决策依据 |
| `docs/audits/2026-06-25-sanitizer-revalidation.md` | P3 | §P2.5 增强 (P2 TSan 修复详情) |
| `docs/roadmap-status.md` | P3 | §ASan/TSan 验证表完整 (含 P2 修复日期/commit) |
| `AGENTS.md` | P3 | §Recent Changes 追加 Sprint 10 ship entry |

## 7. Open Questions

- [ ] P1 cognitive_worker 修复是否需要改 `wait_until` 实现 (vs 仅调用点 timeout 参数)?
- [ ] P2 domain_worker_pool 修复涉及产品代码变更时, 是否需要新 ADR?
- [ ] P2.1 调查是否需要 Oracle 咨询 product code race 分析?
- [ ] Sprint 10 plan 文档是否需要 commit (按 commit `41440c8` precedent)?

## 8. References

- **OpenSpec change**: `openspec/changes/2026-06-25-pre-existing-sanitizer-findings/`
- **Spec**: `engine-include-decoupling` §sanitizer-revalidation Scenario
- **Audit 报告**: `docs/audits/2026-06-25-sanitizer-revalidation.md`
- **Plan precedent (归档于 2026-07-06)**: `docs/archive/superpowers/plans/2026-06-24-engine-include-final-decoupling.md` (895 lines)
- **Commit precedent**: `454fdfc` (P2.5 ship gate documentation)
- **Systematic debugging skill**: `/home/ubuntu/.config/opencode/skills/superpowers/systematic-debugging/SKILL.md`
- **Verification before completion skill**: `/home/ubuntu/.config/opencode/skills/superpowers/verification-before-completion/SKILL.md`
