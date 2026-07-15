# AgentForge MVP Blueprint — TUI Programming Assistant (2026-07-15)

> **目的**: 详细设计文档. 回答 "AgentForge 第一版应该长什么样" + "演进路径是什么".
> **创建日期**: 2026-07-15
> **关联计划**: [`docs/superpowers/plans/2026-07-15-phase6-agentforge-mvp.md`](../superpowers/plans/2026-07-15-phase6-agentforge-mvp.md) (Sprint 24 Task 1 锚定本文档)
> **关联 ADR**: [ADR-0019 IInteractionBus + TUI Chat MVP](../adr/adr-0019-iinteraction-bus-mvp.md) (TUI 设计基础) / [ADR-0021 PDK Design](../adr/adr-0021-pdk-design.md) / [ADR-0020 Thread Model Isolation](../adr/adr-0020-thread-model-isolation.md) / [ADR-0031 Execution Policy](../adr/adr-0031-execution-policy.md) / [ADR-0043 PDK Tool Naming](../adr/adr-0043-pdk-tool-naming-convention.md)
> **前置决策**: ADR-0050 §决策 Solo Developer 重新评估 (2026-07-15) + ADR-0051 §后续 #9 注释 (AgentForge 同人项目约束)
> **状态**: 🚀 Active (Sprint 24 已启动)

---

## 一、Background

Solo 开发者 2026-07-15 确认两个核心约束：

1. **容量约束**：单人开发无法 commit "1-2 eng × 4-6 周无中断"，ADR-0050 §启动条件 #4 字面失效 → 服务化 (Candidate B) 结构性暂缓
2. **目标明确**：规划启动 **AgentForge** 项目，基于 HydraForge PDK 开发不同领域 Agent；首个领域 = **TUI 编程助手** (harness/TUI-style programming assistant)

**本文档目标**：把 "AgentForge TUI 编程助手 MVP" 从口头描述固化到 design doc，确保 Sprint 24-25 的实施有清晰架构边界。

---

## 二、Repo 决策

### 决策：**新建 `chisuhua/AgentForge`**

| 候选 | 接受/拒绝 | 理由 |
|------|:---------:|------|
| **新建 `chisuhua/AgentForge`** | ✅ 接受 | librararian 确认 hydraforge-pdk 是 PDK 分发仓库（4/4 standalone test pass，README 明确 "header-only independent library, no app code"）。AgentForge 是下游**消费者**角色，放进 PDK repo 违反 ADR-0021 §7 Dual-Repo Policy |
| 复用 `chisuhua/hydraforge-pdk` | ❌ 拒绝 | PDK repo 与 AgentForge 是反向关系（前者分发，后者消费）；生命周期不同（PDK 跟随 monorepo Sprint，AgentForge 跟随 LLM/工具变化） |
| 扩展 `HydraForge` monorepo 内 `examples/agent_chat/` | ❌ 拒绝 | monorepo = engine 职责，TUI app 是独立产品；与 Solo Dev 减少管理复杂度的诉求相违背 |
| 等 HydraForge Runtime install rules | ❌ 拒绝 | MVP 等不起，多 1-2 周；用 `FetchContent` 变通 |

### Repo 形态

- **平台**: GitHub (与 HydraForge 同 owner `chisuhua`)
- **可见性**: Public (与 HydraForge 一致；TUI app 用户友好)
- **命名**: `AgentForge` CamelCase (与 `HydraForge` 一致；`hydraforge-pdk` kebab-case 是发布 artifact 命名约定)
- **依赖 HydraForge 方式**: `FetchContent` (master branch pinned commit, 绕过 Runtime 无 install rules 的鸡生蛋)
- **依赖 hydraforge-pdk 方式**: `find_package(hydraforge_pdk 0.2 REQUIRED)` (消费 PDK 独立发布版)

---

## 三、MVP 范围与非目标

### 3.1 范围 (In-scope)

- ✅ 1 个 TUI binary (单进程, 1024×80 终端)
- ✅ 与真实 LLM（OpenAI-compatible API）对话
- ✅ 7 个内置工具 (fs/read, fs/write, fs/edit, fs/glob, fs/grep, shell/exec, + 1 个扩展工具)
- ✅ 1 个 domain agent (`coding_assistant`) — `DEFINE_AGENT(React)` 范式 (参考 `pdk/g1_coding_assistant/`)
- ✅ Token 流式显示（复用 `examples/phase5_yield_token_generator/` 的 `set_stream_tokens()` 范式 + `IInteractionBus` 事件订阅）
- ✅ 多轮会话 (内存 deque, 临时持久化到 `~/.agentforge/sessions/<uuid>.json`)
- ✅ 输入历史 + 基础 slash 命令 (`/help` `/reset` `/model` `/compact`)
- ✅ ApprovalHandler stdin transport 兜底审批 (避免实现 deferred 的 ConfirmationDialog, ADR-0004 §决策 6)

### 3.2 非目标 (Out-of-scope)

- ❌ MCP server 暴露 (ADR-0050 服务化暂缓)
- ❌ 多模型路由 (ModelRouter PDK 已 ship, MVP 用不上)
- ❌ Git ops 工具 / LSP 集成 (Phase 3+)
- ❌ Sqlite 会话持久化 (JSON dump 够了)
- ❌ TUI ConfirmationDialog (ADR-0004-impl-scope.md:34 deferred 债, 不填)
- ❌ Plugin 生命周期管理 / 用户插件加载 (Phase 2)
- ❌ 多 Panel TUI (edit + chat + diff 三栏) (Phase 2+)
- ❌ PlanExecute / ForkJoin loop (MVP 仅 React, Sprint 25+ 升级)
- ❌ 自适应预算 / Cost collector 集成 (Phase 3+)

### 3.3 演进价值判断标准

> "为什么这版 MVP 有演进价值" — 答案：每次加 1 个 Sprint，**新增 1 维度能力** 而非**重写**。

| 维度 | MVP 起点 | 演进终点 |
|------|---------|---------|
| 工具集 | 7 个 fs/* + shell/exec | 用户定义工具 plugin |
| Agent loop | React 单轮 | PlanExecute 多步 / ForkJoin 并行 |
| LLM 提供方 | OpenAI-compatible 1 个 | 多模型 + ModelRouter plugin |
| TUI 面板 | 单 chat panel | edit + chat + diff 三栏 |
| 审批 | stdin transport | TUI ConfirmationDialog |
| 会话持久化 | JSON dump | sqlite + 向量检索 |

---

## 四、Repo 布局

```
chisuhua/AgentForge/
├── CMakeLists.txt                        # find_package(hydraforge_pdk 0.2) + FetchContent(HydraForge) + vendor ftxui
├── README.md                              # 启动说明 + ASCII mock + 截图 (Sprint 25)
├── LICENSE                                # Apache 2.0 (与 HydraForge 一致)
├── .gitignore
├── vendor/
│   └── ftxui/                             # git subtree 或 cp -r (header-only) — vendor 而非 submodule
├── src/
│   ├── main.cpp                           # CLI 入口 + sigaction + 异常边界
│   ├── tui/
│   │   ├── app.{h,cpp}                    # FTXUI 主循环 + 事件路由
│   │   ├── stream_panel.{h,cpp}           # 订阅 assistant.token → 渲染增量字符
│   │   ├── input_panel.{h,cpp}            # 多行输入 + 历史 (↑/↓)
│   │   ├── status_panel.{h,cpp}           # 模型/费用/工具调用计数器
│   │   └── tool_card.{h,cpp}              # 工具调用卡片 (折叠/展开)
│   ├── client/
│   │   ├── hydraforge_client.{h,cpp}      # DSLEngine + InMemoryBus 封装
│   │   └── session_state.{h,cpp}          # 多轮会话持久化 (~/.agentforge/sessions/<id>.json)
│   ├── agents/
│   │   └── coding_assistant.cpp           # DEFINE_AGENT(React) — 沿用 g1 模式
│   └── tools/                             # 7 个内置工具
│       ├── fs_read.cpp                    # ReadOnly
│       ├── fs_write.cpp                   # WriteFile
│       ├── fs_edit.cpp                    # WriteFile (字符串替换 + uniqueness check)
│       ├── fs_glob.cpp                    # ReadOnly
│       ├── fs_grep.cpp                    # ReadOnly (ripgrep wrapper 或 fallback)
│       ├── shell_exec.cpp                 # Execute (复用 ShellGuard from secure_tool_registry.h)
│       └── tool_registry_init.cpp         # DECLARE_TOOL 7 个集中注册点
├── tests/
│   ├── test_tools.cpp                     # 7 工具单元 (Catch2, 无 TUI 依赖)
│   ├── test_agent.cpp                     # React 端到端 (MockLLMProvider)
│   ├── test_session.cpp                   # 多轮会话持久化
│   └── test_tui.cpp                       # FTXUI 渲染快照 (input/output 比对)
├── docs/
│   ├── README.md
│   ├── ADR-AF-001-design.md               # AgentForge 总体架构 (本次 MVP 决策固化)
│   ├── ADR-AF-002-tool-protocol.md        # 工具协议契约 (Phase 2 插件兼容性)
│   └── screenshots/                       # Sprint 25+ 补充
└── examples/
    └── workflow.agent.md                  # DSL 工作流示例 (Sprint 25+ 补充)
```

### 文件数估算

| 类别 | 数量 | 行数估算 |
|------|:---:|:--------:|
| C++ 源码 (src/) | 14 | ~1100 |
| C++ 测试 (tests/) | 4 | ~400 |
| 文档 (docs/) | 3 | ~600 |
| 总计 | 21 | ~2100 |

---

## 五、关键架构决策 (5 个)

### 决策 1: TUI 库 = FTXUI (vendor 进 `vendor/ftxui/`)

| 选项 | 接受/拒绝 | 理由 |
|------|:---------:|------|
| **FTXUI v6 vendor** | ✅ | header-only；与 ADR-0019 §5.3 既有 FTXUI 伪代码设计一致；事件循环天然适配 `IInteractionBus::subscribe()` token 流；功能齐全（多 panel, color, mouse, flex layout） |
| raw termios | ❌ | 调试 I/O 时间估算 2.5 周；窗口尺寸自适应难；多 panel 难 |
| Inkwell / ncurses | ❌ | 增加学习成本；与 PDK 风格不匹配 |

**演进路径**: Sprint 27 升级多 Panel (edit + chat + diff) 时，FTXUI 的 `Renderer` 抽象让 panel 添加是 copy-paste + 事件分发 2 行变更；如未来需 web TUI，FTXUI 5 行替换为 `httplib::Server` 即可。

### 决策 2: Agent Loop = React (`DEFINE_AGENT(React)`)

| 选项 | 接受/拒绝 | 理由 |
|------|:---------:|------|
| **React** | ✅ | 沿用 `pdk/g1_coding_assistant/` 范式（2-step ReAct 编排 G3 + MockLLMProvider）；`SimpleCognitiveOrchestrator` 已有 6/6 测试覆盖；PDK `ReactLoop` 已 ship（Sprint 20）；返回 `LoopResult` 类型统一 |
| PlanExecute | ❌ (MVP) | 5 状态机 + retry 机制对 MVP 过于复杂；Phase 2 升级路径清晰（5 行替换 `LoopType` 模板参数） |
| ForkJoin | ❌ (MVP) | 依赖 `DomainWorkerPool` + 事件订阅同步，需多 agent 协作场景，MVP 单一领域不合适 |

**演进路径**: Sprint 25 升级 `PlanExecute` 仅替换 `AgentLoopType` 模板参数 + 添加规划 prompt 模板；Sprint 28 多 agent 协作时加 `ForkJoin`。

### 决策 3: LLM 接入 = `LLMProviderFactory::create("openai")` + C16 Decorator 链

| 选项 | 接受/拒绝 | 理由 |
|------|:---------:|------|
| **`LLMProviderFactory` + Decorator** | ✅ | 复用 HydraForge C16 ship 的 CostTracking + Compliance + RateLimit 装饰器链；用户配置 `OPENAI_API_KEY` env var 即用；装饰器链可独立加（如 Sprint 27 加 ModelRouter） |
| 直连 OpenAI HTTP API | ❌ | 绕过 HydraForge ILLMProvider 抽象，浪费 Sprint 17-21 投资 |
| llama.cpp 本地推理 | ❌ (MVP) | 需要 `pdk/llama_engine/` 12 工具全部实现（当前 🟡 stub），不适合 MVP |

**演进路径**: Sprint 27 加 `ModelRouter` 装饰器实现成本/质量/延迟多策略路由（PDK 已 ship 3 .so）；Sprint 28+ 加 Claude / Gemini provider 仅 1 个新 ILLMProvider 子类。

### 决策 4: 工具注册 = DECLARE_TOOL (简单) + register_tool_function (需 state)

| 选项 | 接受/拒绝 | 理由 |
|------|:---------:|------|
| **DECLARE_TOOL + register_tool_function 混用** | ✅ | 简单工具（7 个 fs/* + shell/exec）走 `DECLARE_TOOL` 宏（5 行领域逻辑）；需访问 plugin state（如未来的 SessionStore）走 `register_tool_function`（参考 g3_knowledge_base 范式） |
| 仅 DECLARE_TOOL | ❌ | 不能访问 state，未来扩展阻塞 |
| 仅 register_tool_function | ❌ | 失去 PDK 宏的 Schema + 权限自动生成能力 |

**演进路径**: Phase 2 用户自定义工具 plugin 仍可二选一；Phase 3+ 工具 manifest 用 g1 范式（spec 声明依赖）。

### 决策 5: 审批流 = ApprovalHandler stdin transport

| 选项 | 接受/拒绝 | 理由 |
|------|:---------:|------|
| **ApprovalHandler stdin transport** | ✅ | ADR-0031 §决策 5 已 ship 3 种 transport（stdin/event_bus/test_auto），stdin 模式适合 TUI 阻塞提示；避免实现 ConfirmationDialog（ADR-0004 deferred 债） |
| TUI ConfirmationDialog | ❌ (MVP) | 实现成本高（多 panel + 焦点 + Esc 取消）；不解决任何 MVP 阻塞问题 |
| 默认 auto-approve | ❌ | ADR-0031 §决策 6 "YOLO 切换需用户确认"，自动批准破坏安全模型 |

**演进路径**: Sprint 28+ 实现 TUI ConfirmationDialog 填补 ADR-0004 债；ApprovalHandler 切换 transport 零代码。

---

## 六、演进路径 (6 个 Sprint 增量)

| Sprint | 加什么 | 关键依赖 | 估时 |
|--------|--------|---------|:----:|
| **24** | MVP 本体 (TUI + React + 7 工具 + MockLLMProvider 测试) | FTXUI vendor, hydraforge-pdk sync | ~35h (1.5 周) |
| **25** | 真实 LLM 接入 + 第 2 个 agent (`sql_assistant` 或 `doc_writer`) | C16 ILLMProvider v2, OpenAI API key | ~22h (1 周) |
| **26** | 用户插件加载 (`engine.load_plugin()` 公开方法 per ADR-0051 D5) | ADR-0051 §决策 7, PluginLoader V2 | ~22h (1 周) |
| **27** | 多 Panel TUI (edit + chat + diff 三栏) + ModelRouter plugin | FTXUI Renderer 复制, ModelRouter .so | ~30h (1.5 周) |
| **28** | 流式 Token 渲染优化 + TUI ConfirmationDialog 填 deferred 债 | `examples/phase5_yield_token_generator/` 范式 + ADR-0004 §决策 6 | ~22h (1 周) |
| **29+** | Session 持久化升级 (sqlite) + 向量检索 + 多 agent 协作 (ForkJoin) | ADR-0033 三层会话模型 | TBD |

---

## 七、风险与缓解

| 风险 | 等级 | 缓解 |
|------|:---:|------|
| **PDK 同步滞后** (`hydraforge-pdk` 2026-06-24 last push, monorepo 已 ship Sprint 20 PlanExecute/ForkJoin) | 🟠 中 | Sprint 24 Day 1 先跑 `scripts/sync-pdk.sh`；CI 加版本冲突检测；MCP server 强制 `find_package(hydraforge_pdk 0.2)` |
| **HydraForge Runtime 无 install rules** | 🟠 中 | `FetchContent` 变通；同时在 HydraForge 侧起草 ADR-AF-001 同步提案推动 install rules 落地 |
| **FTXUI v5 vs v6 API 差异** (ADR-0019 代码是 v5 时代) | 🟡 中 | Day 1 spike：写 50 行 hello-world 跑通再 commit；vendor 时选 v6 stable tag |
| **`DECLARE_TOOL` token-pasting 限制** (`coding_assistant/edit_file` 这种含下划线的名字可能失败) | 🟡 中 | Day 1 测试 7 个工具名一次性验证；如失败改 `DECLARE_SERVICE` workaround (g1 plugin 已用) |
| **Solo dev 单点风险** | 🟠 中 | 关键子系统（client + agent + tools）每天 commit；不要攒；agent_basic 失败 fallback 路径（4h 内完不成则缩范围） |
| **`examples/agent_chat/` 缺失** (设计存在代码不存在) | 🟢 低 | 不依赖，本 MVP 自己造 TUI |
| **77 ctest 基线** 破坏 | 🔴 高 | AgentForge 是独立 repo 不修改 HydraForge；但 FetchContent 拉取后必须 `cmake --build && ctest --output-on-failure` 验证；本仓库内 ctest 100% pass 才算 sprint end |

---

## 八、Sprint 24 详细计划 (Day-by-Day)

| Day | 任务 | 估时 |
|-----|------|:----:|
| 1 | **创建 `chisuhua/AgentForge` repo + CMake 骨架 + FetchContent(HydraForge)** | 2h |
| 1 | **`hydraforge-pdk` sync** (确保 PDK 头文件最新, 跑 `scripts/sync-pdk.sh` from HydraForge side) | 30min |
| 1 | **FTXUI v6 vendor spike** (验证 API 可用 + commit `vendor/ftxui/`) | 1h |
| 2 | **HydraForgeClient 封装** (`DSLEngine` + `InMemoryBus` + `LayeredContext` 三件套) | 3h |
| 3-4 | **2 个核心工具** (`fs/read` + `fs/write`) + 测试 + DECLARE_TOOL 验证 | 6h |
| 5-6 | **React agent loop 接入** (`DEFINE_AGENT(CodingAssistant, React)` 范式沿用 g1) | 6h |
| 7-8 | **5 个扩展工具** (`fs/edit` `fs/glob` `fs/grep` `shell/exec` + 1 个扩展) + TUI 基础布局 | 8h |
| 9 | **真实 LLM 端到端测试** (`OPENAI_API_KEY` env var + C16 Decorator 链验证) | 4h |
| 10 | **docs 章节** (`README.md` + `ADR-AF-001` + `ADR-AF-002`) | 3h |
| 11 | **commit + push + 写 ADR 提交 HydraForge 侧提案 (Runtime install rules)** | 2h |
| 12 | **缓冲 / 验收测试** | 2h |
| **合计** | | **~38h ≈ 1.5 周** |

---

## 九、关联文档

| 文档 | 关联 |
|------|------|
| [`docs/superpowers/plans/2026-07-15-phase6-agentforge-mvp.md`](../superpowers/plans/2026-07-15-phase6-agentforge-mvp.md) | 总体 Sprint 24+ 计划 (本文档是 Task 1 详细蓝图) |
| [`docs/adr/adr-0019-iinteraction-bus-mvp.md`](../adr/adr-0019-iinteraction-bus-mvp.md) | TUI Chat 设计基础 (本文档 §五决策 1 引用) |
| [`docs/adr/adr-0021-pdk-design.md`](../adr/adr-0021-pdk-design.md) | PDK 总体设计 (§五决策 4 引用) |
| [`docs/adr/adr-0020-thread-model-isolation.md`](../adr/adr-0020-thread-model-isolation.md) | 线程模型隔离 (主线程设置 bus, worker 读取) |
| [`docs/adr/adr-0031-execution-policy.md`](../adr/adr-0031-execution-policy.md) | ToolCoordinator + YOLO 切换 (§五决策 5 引用) |
| [`docs/adr/adr-0043-pdk-tool-naming-convention.md`](../adr/adr-0043-pdk-tool-naming-convention.md) | 工具命名 `<domain>/<verb>` (§四布局引用) |
| [`docs/adr/adr-0050-phase6-strategic-evaluation.md`](../adr/adr-0050-phase6-strategic-evaluation.md) | §决策 Solo Developer 重新评估 (本文档 §一前提) |
| [`docs/adr/adr-0051-phase6-pdk-composition-spike.md`](../adr/adr-0051-phase6-pdk-composition-spike.md) | §后续 #9 (AgentForge 同人项目约束, §一前提) |
| HydraForge `pdk/g1_coding_assistant/` | DEFINE_AGENT(React) 范式参考 (§四 §五决策 2) |
| HydraForge `pdk/g3_knowledge_base/` | register_tool_function + SessionStore 范式参考 (§五决策 4) |
| HydraForge `examples/phase5_yield_token_generator/` | YIELD/STREAM token 流范式 (§四 TUI 渲染) |
| HydraForge `examples/agent_simple/simple.cpp` | MockLLMProvider + 单次 run() 范式 (§四入门模板) |

---

## 十、决策日志

| 日期 | 决策 | 依据 |
|------|------|------|
| 2026-07-15 | Repo = 新建 `chisuhua/AgentForge` (非 hydraforge-pdk) | librararian 验证 + ADR-0021 §7 字面判读 |
| 2026-07-15 | TUI 库 = FTXUI v6 vendor | ADR-0019 §5.3 设计一致 + header-only |
| 2026-07-15 | Agent Loop = React | g1 plugin 范式 + Sprint 20 PDK ship |
| 2026-07-15 | LLM 接入 = `LLMProviderFactory` + C16 Decorator 链 | 复用 Sprint 21 ship 投资 |
| 2026-07-15 | 工具注册 = DECLARE_TOOL + register_tool_function 混用 | g1/g3 双范式覆盖 |
| 2026-07-15 | 审批 = ApprovalHandler stdin transport | 避免实现 ConfirmationDialog deferred 债 |
| 2026-07-15 | HydraForge Runtime 接入 = `FetchContent` 变通 | 绕过无 install rules 鸡生蛋 |

---

**最后更新**: 2026-07-15 (蓝图创建)
**计划状态**: 🚀 Active (Sprint 24 Day 1 已启动)
**下一决策点**: 2026-07-29 (Sprint 24 末验收) + 2026-08-12 (Sprint 25 末 Phase 6 服务化重新评估)