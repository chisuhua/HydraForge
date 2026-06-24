## Context

**当前状态(2026-06-23 全量审计)**:

| 指标 | 值 | 状态 |
|---|---|---|
| `cd build && ctest` | 34/34 PASS | ✅ |
| `TopoScheduler::execute` 行数 | 54 (≤ 60 ✓) | ✅ Sprint 8 ship |
| `struct DagState` | `topo_scheduler.h:89` 引入 | ✅ Sprint 7 ship |
| `handle_node_completion` 完整函数体 | commit `bd936af` ship | ✅ Sprint 9 step 1 |
| 5 ADR (0019/0020/0021/0022/0023) Approved | 0/5 (Sprint 5 推迟) | 🟡 |
| `docs/roadmap-status.md` Phase 1 进度 | 80% | 🟡 |
| `examples/phase1_plugin_demo` 真实 .so 加载 | 不支持 (Sprint 0 mock only) | 🟡 |
| Sprint 9 step 1 backing change | 无 | 🟡 治理异常 |
| `openspec/changes/` 下 9 个 D 文件 | 未提交 | 🔴 工作区脏 |
| `docs/superpowers/` untracked | 1 个新 plan(已过期) | 🟡 自相矛盾 |
| `tech-debt-cleanup-sprint-6` 143 task backlog | 6.3.x 4 项未 ship | 🔴 |
| `engine.cpp` 跨模块/common include | 10 (commit `7cc4239` 自述 "10→8" 不准确) | 🔴 |
| TSan/ASan ship gate 自 Sprint 6 后 | 从未重跑 | 🔴 ship gate 缺失 |
| `docs/README.md` superpowers 段落 | 声明"已归档"但目录有新文件 | 🟡 |

**关联 ADR**:
- **ADR-0019 §1.4** (engine.h decoupling):✅ 已解决,`IProviderFactory` + `IToolRegistry` 接口已 ship (per AGENTS.md 2026-06-18 P1 全部 ship),本 change 推进到 `engine.cpp` 完全解耦(6.3.5)
- **ADR-0021 §7** (PDK Dual-Repo):Sprint 4 ship + Sprint 5 T5 待执行 `./scripts/sync-pdk.sh`
- **ADR-0022** (Plugin Loading):Sprint 5 in-flight,本 change 完成 S5.T3-T5 收官
- **ADR-0023** (ToolResult 标准化):Sprint 1a ship,本 change 仅状态标 ✅ Approved
- **Sprint 6 STATUS NOTE** (Oracle ses_112a9f9c5ffesqpYeefOBgMkjH):决议 4 commit 行为保持 + 不 archive 本 change + §6.3 follow-up 转 Sprint 7+。Sprint 7/8/9 已 ship §6.3.1/6.3.3/6.3.6,本 change 完成 §6.3.2/6.3.4/6.3.5(实际工作)+ §6.3.6 回归验证。

**利益相关方**:
- 自身技术债(143 task backlog 治理危机)
- Phase 1 收官的对外可见性(roadmap 100% 标记)
- 未来 Sprint 10 起点(零 OpenSpec backlog)

## Goals / Non-Goals

**Goals**:
1. **工作区零脏**:`git status` clean(Step 1-3,~35 min)
2. **Sprint 9 backing change 创建**:`2026-06-24-sprint-9-handle-node-completion` 跟踪 3 个已 ship commit
3. **Phase 1 智能体层 80% → 100%**:5 ADR → ✅ Approved + `docs/roadmap-status.md` 更新 + `sync-pdk.sh` 成功
4. **6.3.x 全部 4 项关闭**:6.3.2(删 factory)/6.3.4(15 测试)/6.3.5(include 10→≤3)/6.3.6(访问器一致,已 ship Sprint 7 `75ded94`,本 change 回归验证)
5. **P2.F TSan/ASan ship gate 复验**(自 Sprint 6 后首次)
6. **`tech-debt-cleanup-sprint-6` 干净 archive**(不留 limfall)
7. **`openspec list` ≤ 1 active change**(Sprint 9 回填可保留作 ship 跟踪)

**Non-Goals**:
- 不重做 Sprint 6 已 ship 的 4 commit (Oracle 决议 ship 行为保持)
- 不创建 `2026-07-30-sprint-8-...` 或 `sprint-7-tech-debt-followup` 续接 change(此二已 archive)
- 不修改 PDK / CognitiveWorker / DomainWorkerPool 公共 API
- 不引入新第三方依赖
- 不实现 ADR-0007 LLM 压缩 / ADR-0031/0033 实质化
- 不重构 `external/` vendor
- 不重写 `docs/proposals/` 内容

## Decisions

### Decision 1: P0.A 范围限定(只删归档,docs/superpowers 走 P3.A git mv)

**问题**:`git status` 显示 9 个 D + 1 个 untracked。两种处理路径:
- 方案 A:P0.A commit 全部(含 `docs/superpowers/`),P3.A 再 git mv
- 方案 B:**P0.A 仅 commit 9 个 D**,`docs/superpowers/` 整个走 Step 3 `git mv` 一气呵成

**选 B 原因**:
- 方案 A 是"先 commit-in-place 再 move"= 2 个 commit 做 1 个逻辑动作,纯浪费
- `docs/superpowers/plans/2026-06-22-*.md` 是过期 plan(已 ship 至 Sprint 9),正确归宿是 `docs/archive/superpowers/`,不是留 `docs/superpowers/`
- 一次 `git mv` 既 stage 移动又 record 改名为单一 commit,原子性强

**实施**:
```bash
# Step 1 - P0.A
git add -u openspec/changes/2026-07-30-sprint-8-scheduler-pipeline-followup/ \
         openspec/changes/sprint-7-tech-debt-followup/
git commit -m "chore(openspec): finalize archive deletions for sprint-7/8 followups"

# Step 3 - P3.A (单步,git mv)
git mv docs/superpowers/plans/2026-06-22-sprint7-scheduler-pipeline-tightened.md \
       docs/archive/superpowers/plans/
# (同时 edit docs/README.md superpowers 段落)
git add docs/README.md
git commit -m "docs: archive stale sprint-7 plan, reconcile superpowers README"
```

### Decision 2: P2.A 删 factory.{h,cpp}(非补 Config 参数)

**问题**:`src/modules/scheduler/factory.{h,cpp}` 在 Sprint 6 commit `6c5557c` 引入但零调用(per STATUS NOTE)。两种处理:
- 方案 A:**删除** factory + `engine.cpp` 直接构造
- 方案 B:补 `Config` + `initial_budget` 参数,让 `engine.cpp:188` 调用 factory

**选 A 原因**(Oracle 强烈推荐):
- 零调用 = 死代码 = 该删
- 方案 B 是为不存在的需求添功能,违反 over-engineering discipline
- 删除副作用红利:engine.cpp 跨模块 include 顺势 -1,为 P2.C 铺路
- 承重假设(零调用)若不成立 → Step 8 前 `grep -rn` 二次确认;若不成立改走方案 B

**实施**:
```bash
# Step 8 - P2.A 二次确认(承重假设验证)
grep -rn "create.*SchedulerConfig\|namespace.*scheduler::create" src/ include/
# 期望:0 命中(零调用确认)

# 删除 factory
git rm src/modules/scheduler/factory.h src/modules/scheduler/factory.cpp
# CMakeLists.txt 移除注册
# engine.cpp 改直接构造

git add -A
git commit -m "refactor(scheduler): remove dead NodeFactoryRegistry, inline engine construction"
```

### Decision 3: P2.B 必须在 P2.C 之前(TDD 顺序硬约束)

**问题**:Sprint 6 limfall 头号教训是无测试锁就开始大规模 include 重构。本 change 涉及 P2.C(include 10→≤3)与 P2.B(15 测试)两步。两种顺序:
- 方案 A:**先 P2.B 后 P2.C**(TDD 顺序,行为锁 → 安全重构)
- 方案 B:先 P2.C 后 P2.B(传统"先重构后补测试")

**选 A 原因**:
- 方案 B 是 Sprint 6 `7cc4239`/`6c5557c` 翻车的根因(声称改了 include,实测 10→10)
- TDD 顺序:test first → lock behavior → refactor with safety net
- P2.B 的 15 测试覆盖 P2.C 重构后必然经过的代码路径

**实施硬约束**:`tasks.md` 中 P2.B 任务必须在 P2.C 之前出现且 [x] 完成;`tasks.md` 验收清单明确"P2.C 启动前必须 `ctest` 显示 15 新 case 全 PASS"。

### Decision 4: P2.C 分批提交(2-4 commit,每批 ctest 验证)

**问题**:P2.C (engine.cpp 跨模块 include 10→≤3) 是 Sprint 6 limfall 重灾区(`7cc4239` 一次性大改,自述 10→8 实测 10→10)。两种提交策略:
- 方案 A:一次性 commit,1 个 commit 完成 10→3
- 方案 B:**分批 commit**,每批替换 2-3 个 include,每批跑 ctest 验证

**选 B 原因**:
- 方案 A 的"声称改了"是 Sprint 6 limfall 失败模式;分批可立即发现哪一批出问题
- 借力已存在的 `IProviderFactory` + `IToolRegistry` 接口(per ADR-0019 §1.4),每批替换都是"已验证接口"非"发明接口",风险比 Sprint 6 低一个量级
- `mcp__code-review-graph__get_hub_nodes --top_n 5` 复验分批可观察 hub out_degree 收敛过程

**实施时间盒**:**1.5 day** (Oracle 建议)。超时未达 ≤3 → 触发 handoff 变体:创建新 change `2026-07-xx-engine-include-final-decoupling` 正式 handoff,把 6.3.5 移交过去(带完整 proposal/tasks),然后 archive `tech-debt-cleanup-sprint-6`。

**实施**:
```bash
# Step 12 - P2.C 分批(3 commit 示意)
# Commit A: 替换 ToolRegistry include (2 个 include 去除)
git add src/core/engine.cpp
git commit -m "refactor(core): factory-inject ToolRegistry, reduce includes 10→8"

# Verify
ctest --output-on-failure  # 必须 49/49

# Commit B: 替换 MockLLMProvider include (3 个 include 去除)
git commit -m "refactor(core): factory-inject MockLLMProvider, reduce includes 8→5"

# Verify
ctest --output-on-failure  # 必须 49/49

# Commit C: 替换 BudgetController (若需 IBudgetController 抽象则同时引入) (2-3 个 include 去除)
git commit -m "refactor(core): introduce IBudgetController + factory-inject, reduce includes 5→3"

# Verify
ctest --output-on-failure  # 必须 49/49
grep -c '#include.*\(modules/\|common/\)' src/core/engine.cpp  # 必须 ≤ 3
```

### Decision 5: P2.E "严格全路径"归档姿态(不重复 Sprint 6 limfall)

**问题**:`tech-debt-cleanup-sprint-6` archive 前,§6.3 follow-up 列表 4 项(6.3.2/6.3.4/6.3.5/6.3.6 — 6.3.6 已 ship,本 change 仅对账+回归验证)需关闭。两种姿态:
- 方案 A:Sprint 6 模式 — ship-as-is + STATUS NOTE + 留 backlog(反模式,本 change 杜绝)
- 方案 B:**严格全路径** — Step 8/9/10/11/12 全部 ship 后 archive `tech-debt-cleanup-sprint-6`(无 backlog)

**选 B 原因**(Oracle 强意见):
- Sprint 6 模式演化为本次 audit 的根因(143 task backlog 持续污染 Sprint 7+)
- "严格全路径"是 Sprint 7/8 模式(close everything → archive clean)的延续
- 唯一变体:若 P2.C 1.5 day 时间盒超时 → 触发 Decision 4 的 handoff 变体(正式移交新 change),**仍非 ship-as-is**

**实施**:
```bash
# Step 13 - P2.E + archive
# 1. 更新 tech-debt-cleanup-sprint-6/tasks.md §6.1 表格:6.3.1/2/3/4/5/6 全部 ✅
# 2. 更新 STATUS NOTE 引用本 change + Sprint 7/8/9 全部 ship commit
# 3. openspec archive
openspec archive tech-debt-cleanup-sprint-6 --yes

# Verify
openspec list  # 期望:≤ 1 active change(Sprint 9 回填)
```

## Risks / Trade-offs

| 风险 | 等级 | 缓解 |
|---|---|---|
| [RISK-1] P2.C (engine.cpp includes) 1.5 day 时间盒超时 | 🔴 高 | (1) 分批提交(Decision 4) (2) Oracle 推荐 handoff 变体:创建 `2026-07-xx-engine-include-final-decoupling` 正式 handoff (3) handoff 变体仍是"严格全路径"姿态(非 ship-as-is) |
| [RISK-2] P2.A 删 factory 假设(零调用)不成立 | 🟡 中 | Step 8 启动前 `grep -rn` 二次确认;若不成立改走 Decision 2 方案 B(补 Config) |
| [RISK-3] P2.B → P2.C 顺序颠倒(违反 Decision 3) | 🟠 Major | `tasks.md` 验收清单明文硬约束;P2.C 启动前 ctest 必须显示 15 新 case 全 PASS |
| [RISK-4] Sprint 9 回填 change 时间错配 | 🟢 低 | Step 2 简单 proposal/tasks 回溯,3 commit hash 引用,无新代码 |
| [RISK-5] `docs/superpowers/` git mv 后 `docs/README.md` 索引未更新 | 🟢 低 | Step 3 commit 同时 edit `docs/README.md` superpowers 段落 |
| [RISK-6] P1.C sync-pdk.sh 外部阻塞(GitHub 组织存在性) | 🟡 中 | (1) `sync-pdk.sh` 失败时记录 STATUS NOTE (2) `P1.D archive` 不强依赖 external push 成功 (3) AGENTS.md 已记录"外部阻塞: GitHub 组织存在性" |
| [RISK-7] 5 ADR Approved 同步与代码不同步 | 🟢 低 | 5 ADR 状态变更 commit 引用本 change;`tools/adr_lint.py` exit 0 验证 |
| [RISK-8] TSan/ASan 复验发现历史 race/leak(非本 change 引入) | 🟡 中 | (1) 记录为"pre-existing"(非本 change 引入) (2) 创建独立 OpenSpec change 跟踪修复 (3) 本 change 仍 archive(ship gate 不阻塞) |
| [RISK-9] P2.B 15 测试发现新 bug(非技术债,而是真 bug) | 🟡 中 | (1) 测试驱动发现新 bug 是 TDD 价值 (2) Step 10 同步报 bug 跟踪系统 (3) 评估后决定 fix-in-this-change vs defer |

## Migration Plan

**阶段 A — 工作区清场(Step 1-3,~35 min,3 commits)**
- W1D1 上午:Step 1 (P0.A 仅 9 D)+ Step 2 (Sprint 9 回填)+ Step 3 (P3.A docs/superpowers git mv)

**阶段 B — Phase 1 收官(Step 4-7,~4-6h,4-6 commits)**
- W1D1 下午:Step 4 (S5.T3 demo flags) + Step 5 (5 ADR Approved) + Step 6 (sync-pdk) + Step 7 (archive plugin-loader)

**阶段 C — 技术债关闭(Step 8-13,~3-4d,9-12 commits)**
- W1D2:Step 8 (P2.A 删 factory) + Step 9 (P2.D 访问器)
- W1D3:Step 10 (P2.B 15 测试,3 commit)
- W1D4:Step 11 (P2.F TSan/ASan 复验)
- W1D5-W2D1:Step 12 (P2.C engine includes 分批,2-4 commit,1.5 day 时间盒)
- W2D2:Step 13 (P2.E STATUS NOTE 对账 + archive)

**回滚策略**:
- 每 Step 独立 commit,`git revert HEAD` 即回滚
- P2.A 删 factory 后若发现真调用 → `git revert` 恢复 + 走方案 B 路径
- P2.C include 重构分批 → 失败时只 revert 失败的那一批
- Sprint 9 回填 change 可独立 archive,不影响本 change 主体

**部署检查**:
- W1D1 结束:`git status` clean
- W1D1 结束(Sprint 5 S5.T3 后):`./phase1_plugin_demo --load-plugin` 3 模式实跑
- W1D1 结束(Sprint 5 S5.T4 后):`grep "✅ Approved" docs/adr/` 5 命中
- W1D3 结束(P2.B 后):`ctest` ~49/49
- W1D4 结束(P2.F 后):ASan + TSan 0 error
- W1D5-W2D1 结束(P2.C 后):`grep -c '#include.*\(modules/\|common/\)' src/core/engine.cpp` ≤ 3
- W2D2 结束(全路径 ship):`openspec list` ≤ 1 active change

## Open Questions

1. **P2.C 1.5 day 时间盒超时,是否直接走 handoff 变体**?或者允许延长到 2 day 再决策?Oracle 建议 1.5 day,但项目节奏通常 1-2 day 弹性。决策点:Step 12 启动时确定。
2. **`IBudgetController` 抽象是否本 change 引入**?Sprint 6 design.md Open Question 1 已记录;若 P2.C 需要则本 change 引入(增量工作),若不需要则继续用 BudgetController 具体类。决策点:P2.C Commit C 时确定。
3. **Sprint 9 回填 change ship 后是否 archive**?回填是为治理一致性,本质是 ship+archive 一次性 change(类似 docs-code-drift-audit 类)。决策点:Step 2 proposal 阶段确认(本设计默认:ship 后立即 archive)。
4. **P1.C sync-pdk.sh 外部阻塞(standalone `hydraforge-pdk` repo push 失败)时,如何归档**?3 选 1:(a) 阻塞 archive 直到 external 成功 (b) archive + STATUS NOTE 记录 (c) 跳过 T5,等后续 change 补。Oracle 推荐 (b) — 不阻塞主路径。决策点:Step 6 启动时确认。
5. **P2.F TSan/ASan 复验发现历史 race/leak,是否阻塞 archive**?Oracle 建议 ship gate 不阻塞(记录为 pre-existing)。决策点:Step 11 跑完后确认。
