# Phase 6 — PDK Productionization + AgentForge MVP Master Plan (Sprint 24+)

> **目的**: Sprint 24-25 期间执行 PDK 生产化与 AgentForge MVP 验证, 取代原 Phase 6 服务化路径.
> **创建日期**: 2026-07-15
> **触发条件**: ADR-0050 §决策 Solo Developer 重新评估 (2026-07-15) + 用户规划 AgentForge 下游项目
> **估时**: Sprint 24 (2 周) + Sprint 25 (2 周) = 4-6 周日历时间 (Solo dev ~22 小时/周)
> **维护**: 每个 Sprint 末更新 §三/§四; Sprint 25 末重新评估 Phase 6 服务化是否重启
> **关联计划**: `docs/superpowers/plans/2026-07-03-phase5-self-bootstrapping.md` (Phase 5 收官档案) / `docs/active-status.md` (活跃看板)
> **关联 ADR**: ADR-0050 / ADR-0051 / ADR-0021 (PDK 设计) / ADR-0020 (线程模型隔离)

> 🚨 **Solo Dev 原则**: 减少管理开销. 不创建 OpenSpec change (Sprint 24-25), 不开 ship gate 仪式. 直接 commit + 简单推进. ADR 文档记录决策但不强行仪式化.

---

## 一、目标与边界

### 1.1 核心目标

> **"让 AgentForge 跑起来 1 个领域 agent, 验证 HydraForge PDK 是否可用"**

### 1.2 明确边界 (In-scope)

- ✅ PDK 生产化: SafeExec 重写 (高风险修复) + 真实 LLM 集成示例 + PDK README
- ✅ AgentForge MVP: 创建独立项目 + 1 个微型领域 agent (代码审查 / SQL 生成 / 任何领域都行)
- ✅ ADR-0051 §触发条件 #3 (Layer 3 dual memos) 满足后的 evidence 收集

### 1.3 明确边界 (Out-of-scope)

- ❌ Phase 6 服务化 (MCP server + OpenAI API) — 见 ADR-0050 §决策 Solo Dev 重新评估段
- ❌ C20 OpenSpec change 创建 — 服务化重新评估前不启动
- ❌ C16 §5 Cloud plugin — 独立顺延项
- ❌ C19 (fork-checkpoint) — Phase 7+ 自进化再评估
- ❌ 多 reviewer 仪式 (Layer 1/3 dual memos) — Solo dev 跳过
- ❌ ADR-0052 创建 — 等 Sprint 25 末评估

---

## 二、Solo Dev 关键约束

| 约束 | 影响 | 缓解策略 |
|------|------|---------|
| **单人容量** (~22 小时/周) | 估时 = 2-4 周**日历时间** / 单元任务 | 严格边界 (4 小时 MVP, 1 周 PDK 修复); 切割到 Sprint 周颗粒度 |
| **无 reviewer** | 缺早期反馈, 易陷入"完美主义陷阱" | 写测试代替评审 (TDD 红绿循环); 用 ADR 记录决策代替评审 |
| **关键人员风险** (bus factor=1) | 任何中断都打乱计划 | 关键子系统 (DSLEngine/PDK/Scheduler) 写最短记忆文件 (半页纯文本) |
| **"10x 工程师" 陷阱** | 试图同时推进 Phase 6 服务化 + AgentForge + PDK 生产化 | 一次只做一件事; 完整 Sprint 24 关闭再做 Sprint 25 |
| **动力可持续性** | 构建没人用基础设施 = 情感耗竭 | 优先 AgentForge MVP → dopamine 反馈循环 |
| **上下文切换税** (~20-30 分钟) | 跨项目切换 = 进度杀手 | 单日单任务; 切换前完成 commit |

---

## 三、Sprint 24 (2026-07-15 ~ 2026-07-29, 2 周)

### 任务 1: AgentForge 仓库 + 1 个微型领域 agent (Day 1-2, ≤4 小时)

> **最高优先级**. 建立"AgentForge 消费 HydraForge PDK" 的物理连接.

**Deliverables**:
```bash
mkdir agentforge && cd agentforge && git init
# 拷贝 examples/agent_basic/ 作为种子
cp -r ../hydraforge/examples/agent_basic/. .
# 转化为 1 个领域 agent (任何领域都行, 推荐 code-reviewer 或 sql-generator 起步)
# 验证 DSLEngine::set_llm_provider() + DECLARE_TOOL 调用成功
git add -A && git commit -m "feat: bootstrap AgentForge with first domain agent via HydraForge PDK"
```

**成功标准**:
- [ ] AgentForge 仓库可独立编译 (`cmake -B build && cmake --build build`)
- [ ] 1 个微型 agent 通过 PDK 调用 `DSLEngine::register_tool()` + `DSLEngine::run()` 完成端到端流程
- [ ] 测试覆盖核心调用路径 (≥3 个测试用例)

**边界**: 不超过 4 小时. 失败 = 范围太大, 缩小.

**失败 fallback**: 至少写一个 `AGENTFORGE.md` 文档列出 3 个 PDK 改进需求, 提交. 不需要代码.

**📖 详细设计蓝图**: 见 [`docs/proposals/implementation/agentforge-mvp-blueprint.md`](../proposals/implementation/agentforge-mvp-blueprint.md) (Sprint 24 Day-by-Day 计划 + 5 个关键架构决策 + 6 个 Sprint 演进路径 + 风险评估). 任务 1 的具体实施细节（FTXUI vendor、7 工具清单、DEFINE_AGENT(React) 范式、DECLARE_TOOL + register_tool_function 混用、ApprovalHandler stdin transport 等）全部固化在该 blueprint 中.

**关键决策摘要**（详见 blueprint §五）：
- TUI 库 = **FTXUI v6** vendor 进 `vendor/ftxui/` (header-only, 与 ADR-0019 §5.3 设计一致)
- Agent Loop = **React** (`DEFINE_AGENT`, 沿用 `pdk/g1_coding_assistant/` 范式)
- LLM 接入 = **`LLMProviderFactory::create("openai")` + C16 Decorator 链** (CostTracking + RateLimit)
- 工具注册 = **DECLARE_TOOL + register_tool_function 混用** (简单工具用宏, 需 state 的用函数)
- 审批 = **ApprovalHandler stdin transport** (避免实现 ConfirmationDialog deferred 债)

**任务 1 → 12 (Sprint 24 Day-by-Day)**: blueprint §八. 估时 ~38h ≈ 1.5 周, 含 CMake 骨架 + FTXUI vendor + 7 工具 + TUI 基础 + 真实 LLM 测试 + ADR-AF-001/002 文档.

### 任务 2: SafeExec 重写 (Week 1, ~22 小时)

> **PDK 最高风险修复**. 当前 `std::async` + `wait_for` 在 `std::future` 销毁时泄漏线程.

**目标**:
- `std::async` + `wait_for` → `std::jthread` + `stop_token` (首选) 或复用 `DomainWorkerPool` (Sprint 3 已 ship)
- 完整超时支持 (`std::stop_token` propagation)
- 资源限制 (调用者可控的 worker 数量)

**TDD 流程** (Sprint 23 教训复用):
1. 写失败测试: SafeExec 在超时后必须 join 或 detach 线程 (无泄漏)
2. 验证测试 fail (用 `ps -eLf` 或 TSan 验证线程泄漏)
3. 实施 jthread 改造
4. 验证测试 pass
5. 跑全部 77 ctest 确保零回归

**位置**: `include/agenticdsl/pdk/safe_exec.h` + 实现位置待定

**估算**: 1 周 (含测试 + 文档 + 边界场景调试)

### 任务 3: 真实 LLM 集成示例 (Week 2 前期, ~12 小时)

> 验证 AgentForge 可调用真实 LLM, 不依赖 MockLLMProvider.

**Deliverables**:
- `examples/pdk_real_llm/` 示例: 通过 `DSLEngine::set_llm_provider()` 连接 C16 ILLMProvider v2 装饰器链 (CostTracking + Compliance + RateLimit)
- 文档 `include/agenticdsl/pdk/README.md` 起稿 (~2 页):
  - §1 PDK 是什么
  - §2 Quick Start (5 行代码 demo)
  - §3 3 种 Agent Loop (React/PlanExecute/ForkJoin) 选择指南
  - §4 错误处理模式
  - §5 与 AgentForge 的衔接示例

**成功标准**:
- [ ] `examples/pdk_real_llm/` 编译通过 + 至少 1 个端到端测试
- [ ] `PDK/README.md` 起稿完成 (>= 100 行有效内容)

---

## 四、Sprint 25 (2026-07-29 ~ 2026-08-12, 2 周)

> **Sprint 24 末决策点**: 若任务 1/2 全部完成 → 启动 Sprint 25; 若有任何阻断 → 重新评估范围.

### 任务 4: PDK 开发者指南完整化 (Week 1, ~22 小时)

**Deliverables**:
- `include/agenticdsl/pdk/README.md` 完整版
  - §6 工具注册模式 (DECLARE_TOOL + register_tool_function)
  - §7 SafeExec 实战
  - §8 Agent loop 自定义 (DEFINE_AGENT 模板使用)
  - §9 调试与诊断 (含 ASan/TSan 集成)
  - §10 迁移指南 (从 MockLLMProvider 到 ILLMProvider 真实集成)
- `examples/pdk_real_llm/` 升级为完整 demo project

### 任务 5: AgentForge 第 2 个领域 agent (Week 2 前期, ~16 小时)

> 验证 PDK 复用性. 如果第 2 个 agent 比第 1 个快 50% 完成, 说明 PDK 设计成功.

**目标**: 与第 1 个 agent **不同领域** (避免重复领域偏见).

### 任务 6: Phase 6 重新评估 (Week 2 后期, ~8 小时)

> **硬决策点**: Sprint 25 末, 评估 Phase 6 服务化是否重启.

**评估清单**:
- [ ] AgentForge MVP 验证 (≥2 个不同领域 agent 通过 PDK 调用成功)?
- [ ] PDK 生产化进度 (SafeExec 重写 + 文档 + 真实 LLM 全部完成)?
- [ ] 服务化范围文档 ≤1 周可完成 (Solo dev 适配)?
- [ ] AgentForge 是否触发具体 HTTP 集成诉求?

**3 个可能动作**:
- **路径 X (重启服务化)**: 若上述 4 项全部 ✅, 启动 C20 OpenSpec change, 进入 Phase 6 v1 实施
- **路径 Y (继续延迟)**: 若 AgentForge 反馈 PDK 还有阻塞, 优先继续 PDK 完善, 等 Sprint 26-27 再评估
- **路径 Z (暂停服务化)**: 若 AgentForge 反馈"PDK 已经够用, 暂不需要服务", 正式将 Phase 6 服务化降级为 Phase 7+ 候选

**记录**: 评估结果写入 `docs/active-status.md` §六 + 新 plan (如有需要)

---

## 五、风险与缓解

### 风险 1: Sprint 24 任务 1 失败 (AgentForge 仓库无法落地)

| 影响 | 🟠 中 — PDK 价值无法验证 |
|------|----------------------|
| 概率 | 🟢 低 — examples/ 已 ship, 拷贝即可 |
| 缓解 | 失败 fallback = 写 AGENTFORGE.md 文档列出 3 个 PDK 改进需求 |

### 风险 2: SafeExec 重写破坏现有行为

| 影响 | 🔴 高 — 影响所有 Agent loop |
|------|----------------------|
| 概率 | 🟡 中 — jthread 与 std::async 语义差异 |
| 缓解 | TDD 5 步严格 (先写失败测试 → 验证 fail → 实施 → 验证 pass → 零回归 ctest 77/77) |

### 风险 3: Solo dev 失去动力 / 中断

| 影响 | 🔴 高 — 单人计划无 redundancy |
|------|----------------------|
| 概率 | 🟡 中 — 大幅中断风险 |
| 缓解 | 关键子系统写最短记忆文件 (.md 半页); Sprint 24/25 末必有可工作 demo (AgentForge MVP); 避免在 Sprint 中启动新工作 |

### 风险 4: 全部"Sprint 24 完成 → Sprint 25 启动" 决策路径延误

| 影响 | 🟡 中 — 触发 Phase 6 服务化重新评估延迟 |
|------|----------------------|
| 概率 | 🟢 低 — Sprint 24 任务都是物理可执行项 |
| 缓解 | 任务 1/2 任何一个失败立即 raise 范围评估 |

---

## 六、关联文档

| 文档 | 关联 |
|------|------|
| [`docs/adr/adr-0050-phase6-strategic-evaluation.md`](../adr/adr-0050-phase6-strategic-evaluation.md) | §决策 Solo Dev 重新评估段 (本计划决策来源) |
| [`docs/adr/adr-0051-phase6-pdk-composition-spike.md`](../adr/adr-0051-phase6-pdk-composition-spike.md) | §后续 #9 (本计划后续评估入口) |
| [`docs/adr/adr-0021-pdk-design.md`](../adr/adr-0021-pdk-design.md) | PDK 设计基础 |
| [`docs/adr/adr-0020-thread-model-isolation.md`](../adr/adr-0020-thread-model-isolation.md) | SafeExec 重写时考虑的线程隔离不变量 |
| [`docs/active-status.md`](../active-status.md) | §一/§二/§四/§六 已同步本计划 |
| [`docs/service-composition/spike-onboarding.md`](../service-composition/spike-onboarding.md) | ADR-0051 Spike 兼容性参考 (虽暂缓但契约仍有效) |
| `examples/agent_basic/` | AgentForge 仓库种子 |

---

## 七、决策日志

| 日期 | 决策 | 依据 |
|------|------|------|
| 2026-07-15 | 创建本计划 | ADR-0050 §决策 Solo Dev 重新评估 + 用户规划 AgentForge |
| 2026-07-15 | 不创建 OpenSpec change | Solo Dev 减少管理开销原则 |
| 2026-07-15 | 不重命名 master plan | `2026-07-03-phase5-self-bootstrapping.md` 仅加 closeout banner |
| 2026-07-15 | ADR-0050 §启动条件 #4/#5 修正但保持 🔍 Proposed | Phase 6 服务化决策保留, 不强行翻转 |
| 2026-07-15 | Q3 实施路线 = **Route B+** (Oracle 推荐, Medium-High 置信度) | Oracle session `ses_097e891c3ffeDhQaBnl1yuFnX2`: SafeExec 不在 Day 1-6 critical path, dopamine feedback 优先 |
| 2026-07-15 | Repo = 新建 `chisuhua/AgentForge` (Q1 已接受, librararian + Oracle 验证) | hydraforge-pdk = PDK 分发仓库, 角色反向, 生命周期不同 |
| 2026-07-15 | TUI 库 = FTXUI v6 vendor (Q2 已接受) | ADR-0019 §5.3 设计一致 + header-only |

## 八、执行日志 (Sprint 24 进度)

### Day 1 (2026-07-15) ✅ Done

**Commit**: `chisuhua/AgentForge@3f189e3` (root commit on main, forced update replaced auto-generated README)
**URL**: https://github.com/chisuhua/AgentForge
**实际时长**: ≤1h (远低于 Oracle 4h STOP 条件)

**Deliverables (6 files, 308 insertions)**:
- `CMakeLists.txt` — `find_package` stub + `FetchContent(HydraForge)` (Day 2+ 接入 targets)
- `README.md` — purpose + ASCII mock + Sprint 24 状态 + 5 决策摘要
- `docs/ADR-AF-001-design.md` — 5 个关键决策固化 (TUI/Loop/LLM/工具/审批), 标记 DRAFT (Sprint 24 末升级)
- `.gitignore` — build/ vendor/build/ .agentforge/ 等完整覆盖
- `LICENSE` — Apache 2.0 (与 HydraForge 一致)
- `src/main.cpp` — 5 行 hello world

**关键决策落地**:
- ✅ gh auth `chisuhua` 账号已配置 (Oracle STOP 条件 #1 不触发)
- ✅ force push 成功 (`0e2fdfc...3f189e3 main -> main`)
- ✅ LSP discipline 不适用 (AgentForge 是新 repo, HydraForge pre-existing 错误与本任务无关)
- ✅ 0 代码逻辑 (符合 Day 1 "skeleton only" 边界)
- ✅ 工作目录 `/tmp/opencode/agentforge/` (避免 HydraForge monorepo 污染)

**下一步 (Day 2)**:
1. vendor/ftxui/ v6 git subtree 或 cp -r (~1h)
2. HydraForgeClient 封装 (DSLEngine + InMemoryBus) (~3h)
3. 验证 `cmake -B build && cmake --build build` 通过 (≤1h)
4. Day 2 commit + push

**Oracle B+ STOP 条件触发监控**:
- Day 1 ≤ 1h elapsed ✅ (远低于 6h 上限)
- FetchContent 未执行 (Day 2 才用) ⏳
- FTXUI vendor 未执行 (Day 2 才用) ⏳

### Day 2 (2026-07-15) ✅ Done

**Commits**:
- `c123858` (fix: Day 1.1 — resolve Threads + gate HydraForge FetchContent)
- `82fb139` (feat: vendor FTXUI v6.1.9 + HydraForgeClient skeleton)

**URL**: https://github.com/chisuhua/AgentForge/commits/main

**Deliverables (4 files new, 2 modified, +42562 insertions)**:
- `vendor/ftxui/` — v6.1.9 tarball vendor (2.1MB, 308 files, header-only 用法)
  - blueprint 写 v6.0.0 但该 tag 不存在, 改用最新 v6.1.9 (v7.0.0 风险过高, v6.x 末班车)
  - `add_subdirectory` 留 Day 3+ (本期 header-only 不用构建 FTXUI 库)
- `include/agentforge/hydraforge_client.h` — PIMPL interface (75 行)
  - 5 公开方法: `initialize` / `start_session` / `stop_session` / `session_active` / `instance`
  - 决策固化 per ADR-AF-001 §五决策 3 (LLM 接入 + Decorator 链)
- `src/hydraforge_client.cpp` — Day 2 stub Impl (50 行)
  - 所有方法返回 false / no-op 满足 linker; PIMPL Impl 仅含 `mutex` + `atomic<bool>` 占位
  - Day 3+ 填 DSLEngine + IInteractionBus + LLMProviderFactory
- `src/main.cpp` — 调用 `HydraForgeClient::instance()` (Day 2 dry-run)
- `CMakeLists.txt` — `target_include_directories(agentforge PUBLIC include)` + 条件 PRIVATE FTXUI include path + 加入 `hydraforge_client.cpp` 到 target

**构建验证**:
```bash
cmake -B build -S .   # 0.7s
cmake --build build   # [100%] Built target agentforge
./build/agentforge    # → "AgentForge MVP — Sprint 24 Day 2"
```

**Oracle B+ STOP 条件 Day 2 监控**:
- Day 1.1 ≤ 30min (修复 Threads::Threads + 加 option gate) ✅
- Day 2 vendor FTXUI ≤ 5min (curl tarball 比 git clone 快 4x) ✅
- cmake configure 0.7s ✅
- 实际 ≤2h elapsed (远低于 Oracle 6h 上限)

**网络/构建观察**:
- github.com curl OK (200), git clone 慢 (120s timeout for shallow) — tarball 路径更快更稳定
- vendor/ftxui 全量提交 2.1MB, 308 files — 体积可接受 (header-only 必要文件)
- HydraForge FetchContent (cmake -DAGENTFORGE_FETCH_HYDRAFORGE=ON) 仍 Day 3+ 验证

**下一步 (Day 3+)**:
1. 验证 `cmake -DAGENTFORGE_FETCH_HYDRAFORGE=ON` + cmake configure 通过
2. 检查 HydraForge main 分支 DSLEngine 构造签名 (是否需要 `set_interaction_bus()` 前置)
3. 替换 `HydraForgeClient::initialize()` stub 为真实 `LLMProviderFactory::create("openai")` + C16 Decorator 链
4. DECLARE_TOOL × 2 起步 (fs/read + fs/write)
5. 跑通 MockLLMProvider 端到端测试

### Day 3 (2026-07-15) 🟡 STOP 条件触发 + Pivot

**Commit**: `dc33384` (docs(adr): Day 3 lessons learned — HydraForge FetchContent blocked, pivot to MockLLMProvider)
**URL**: https://github.com/chisuhua/AgentForge/commit/dc33384

**事件**: 尝试 `cmake -DAGENTFORGE_FETCH_HYDRAFORGE=ON` 验证 HydraForge FetchContent 路径

**Oracle B+ STOP 条件 #2 触发**: "FetchContent >2h fail → commit ADR-AF-001 lessons learned + pivot to alternative path" (实际 <2h 即明确阻塞)

**根因**:
```
CMake Error at build/_deps/hydraforge-src/CMakeLists.txt:46 (add_subdirectory):
  add_subdirectory given source "external/taskflow" which is not an existing
  directory.
```
- HydraForge monorepo `CMakeLists.txt:46` 引用 `external/taskflow`
- 该目录不在 git tree 中（不是 submodule, `.gitmodules` 中也没有）
- HydraForge 内部 vendoring 不完整 (nlohmann_json/inja/yaml-cpp 是 submodule, taskflow 不是)
- **不是 AgentForge 配置问题, 是 HydraForge 仓库问题**

**Pivot 决策** (per Oracle B+ STOP + Solo Dev 减少管理原则):
- ❌ **不**等待 HydraForge 修复 taskflow (阻塞 AgentForge 进度)
- ❌ **不**vendor HydraForge 整个 monorepo (100s MB 超 MVP 合理体积)
- ✅ **Day 4 改用 MockLLMProvider-only 路径** (复用 HydraForge `examples/agent_simple/` 模式, 但不 fetch 整个 Runtime)
- ✅ **起草 HydraForge 侧 ADR-AF-001 镜像提案** (推动 Runtime install rules + `find_package(HydraForge ...)` consumer entry, 这是 FetchContent 路径长期解锁的唯一办法)

**对 §蓝图的影响**:
- 决策 1 (TUI = FTXUI v6) ✅ 不变 (vendor 完成)
- 决策 2 (Agent Loop = React) ✅ 不变 (DEFINE_AGENT 不依赖 HydraForge Runtime)
- 决策 4 (工具 = DECLARE_TOOL + register_tool_function) ✅ 不变 (宏 + 函数注册不依赖 Runtime)
- 决策 5 (审批 = stdin transport) ✅ 不变 (ApprovalHandler 已 ship 在 hydraforge-pdk)
- 决策 3 (LLM 接入 = LLMProviderFactory + Decorator 链) 🟡 **暂时降级为 MockLLMProvider**, 等 install rules ship 后升级

**Day 4 调整后路径**:
| 步骤 | 原计划 | Day 3 pivot |
|------|--------|-------------|
| Day 4 | HydraForgeClient 真实 LLM | MockLLMProvider 模式 + DEFINE_AGENT 端到端 |
| Day 5-8 | 7 个 fs/* + shell/exec 工具 | **不变** (DECLARE_TOOL 不依赖 Runtime) |
| Day 9 | 真实 LLM 端到端 | **推迟** 到 HydraForge install rules ship |
| Day 10 | docs ADR-AF-001 final | **不变** (Day 3 已写 lessons learned 段) |

**HydraForge 侧紧急事项** (Sprint 24 末决策点):
1. 修复 `external/taskflow` vendoring (add as submodule 或 vendored 目录)
2. 起草 Runtime install rules + `find_package(HydraForge ...)` consumer entry (推动 AgentForge FetchContent 路径)
3. 这 2 项完成 → Sprint 25+ 可重新评估 Phase 6 服务化 (ADR-0050 §决策 Solo 重新评估段触发条件)

**Oracle B+ STOP 条件 Day 3 监控**:
- FetchContent fail < 2h ✅ (实际 <2h, 立即触发 pivot)
- Day 3 实际 ≤ 30min elapsed
- ADR-AF-001 lessons learned 段已添加 ✅
- Pivot 决策明确记录 ✅

---

**最后更新**: 2026-07-15 (Day 1 + Day 2 + Day 3 完成, Day 3 STOP 触发)
**计划状态**: 🚧 Active (Sprint 24 Day 1-3 ✅, Day 4 pivot to MockLLMProvider, Day 5+ 待定)
**下一决策点**: 2026-07-29 (Sprint 24 末验收) — 评估 4 项 (HydraForge install rules / taskflow 修复 / Mock-only 路径 / Sprint 25 启动)
