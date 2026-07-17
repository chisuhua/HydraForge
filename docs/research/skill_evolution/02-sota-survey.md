# Skill Evolution SOTA 调研报告

**日期**: 2026-07-16
**来源**: Librarian 后台调研（Task `bg_90d2ea6d`，2m17s）
**范围**: 2024-2026 学术论文（arXiv / ICLR / ACL / AAAI / NeurIPS / Microsoft Research / Stanford / NVIDIA）+ 工业实践（Anthropic / Cline / LangChain / Continue.dev / Microsoft AutoGen）

---

# HydraForge ADR-0061 调研报告：Agent 进化管线（Skill.md → .agent.md → C++ → Wasm）

> **范围**：2024-2026 学术论文（arXiv / ICLR / ACL / AAAI / NeurIPS / Microsoft Research / Stanford NLP / NVIDIA 等）+ 工业实践（Anthropic / Cline / LangChain / Continue.dev / Microsoft AutoGen）。
> **目标**：把"自动/半自动 Skill → 结构化 Agent 图 → 本地码 → WebAssembly"这条链路的最新 SOTA 全景提供给 ADR-0061 决策者。

---

## 📘 Topic 1 · Skill 进化（NL → 结构化 Agent/工作流）

### 学术

| # | 论文 | ID | 一句话贡献 | URL |
|---|------|----|-----------|-----|
| 1 | **AFlow: Automating Agentic Workflow Generation** | arXiv:2410.10762 (ICLR 2025 Oral) | MCTS 在代码表示的工作流空间内自动发现 workflow，6 个 benchmark 上比手设计 +5.7%、比自动化基线 +19.5% | https://arxiv.org/abs/2410.10762 |
| 2 | **ADAS: Automated Design of Agentic Systems** | arXiv:2408.08435 | "Meta Agent Search" 在代码空间迭代编程新 agent，发现的 agent 跨 domain / model 迁移鲁棒 | https://arxiv.org/abs/2408.08435 |
| 3 | **Meta-Agent: From Task Descriptions to Verified Multi-Agent Systems** | arXiv:2605.25233 | 自然语言→DAG+每个 agent 的 I/O 契约和验证条件；构造期 + 执行期双层验证；三级错误归因 | https://arxiv.org/html/2605.25233v1 |
| 4 | **GEPA: Reflective Prompt Evolution Can Outperform Reinforcement Learning** | arXiv:2507.19457 (ICLR 2026 Oral) | Genetic-Pareto 反思式 prompt 进化，比 GRPO 平均 +6%、最多 +20%，用 35× 更少 rollout | https://arxiv.org/abs/2507.19457 |
| 5 | **DSPy: Compiling Declarative Language Model Calls into Self-Improving Pipelines** | arXiv:2310.03714 (ICLR 2024) | 把 prompt template 提升为带类型的 signature，teleprompter（编译器）自动 bootstrap demo + fine-tune | https://arxiv.org/abs/2310.03714 |
| 6 | **AutoFlow: Automated Workflow Generation for LLM Agents** | arXiv:2407.12821 | 用自然语言 program 作 workflow 表示，RL + in-context 双方法，相比 CoRE +40% | https://arxiv.org/abs/2407.12821v1 |
| 7 | **CapFlow: Learning to Compose for Cross-domain Agentic Workflow Generation** | arXiv:2602.11114 | decompose-recompose-decide：训练单 LLM 一次性组合 workflow 替代 20 轮搜索 | https://arxiv.org/pdf/2602.11114 |
| 8 | **FlowSteer: Agents Designing Agentic Workflows via Reinforced Progressive Canvas Editing** | arXiv:2602.01664 | RL policy 在可执行 Workflow Canvas 上做 atomic edit，引入 graph-level 执行反馈 | https://arxiv.org/html/2602.01664v4 |

补充：FUSIONFLOW（ACL 2026, https://aclanthology.org/2026.acl-long.1278.pdf）、A²Flow（AAAI 2026, https://ojs.aaai.org/index.php/AAAI/article/view/40240）、RobustFlow（arXiv:2509.21834, https://arxiv.org/html/2509.21834v2）分别聚焦**深度结构探索**、**自动抽象算子**、**扰动鲁棒性**，值得作为旁支参考。

### 工业实践

| 项目 | 一句话贡献 | URL |
|------|-----------|-----|
| **Anthropic Agent Skills** (2025-10 开放为开放标准) | "SKILL.md 协议 + 三级 progressive disclosure"：metadata 始终常驻、SKILL.md body 命中后载入、scripts/references 链上按需；已支持 Claude.ai / Claude Code / API / Foundry；agentskills.io 开放标准 | https://www.anthropic.com/news/skills ; https://platform.claude.com/docs/en/agents-and-tools/agent-skills/overview ; https://github.com/anthropics/skills |
| **LangChain Skills** (2026-03) | 首批 11 个 skill（LangChain / LangGraph / Deep Agents），用 `npx skills add langchain-ai/langchain-skills` 安装；在 Claude Code eval 上 29%→95% | https://www.langchain.com/blog/langchain-skills ; https://github.com/langchain-ai/langchain-skills |
| **Cline Skills System** | `.clinerules/skills/<name>/SKILL.md`，三层加载；CLI/VSCode/JetBrains 通用；SDK skill 单独仓库 | https://docs.cline.bot/customization/skills ; https://github.com/cline/sdk-skill |
| **Continue.dev config.yaml** | YAML 声明 model / context / rules / prompts / mcpServers，类似 `.agent.md` 的 v0 形态 | https://docs.continue.dev/reference |
| **LangGraph (BSP/Pregel runtime)** | 严格 DAG + 循环执行算法，channel version 保证 concurrency 无 data race；LinkedIn/Uber/Klarna 落地 | https://www.langchain.com/blog/building-langgraph |
| **Magentic-One / AutoGen** (Microsoft, 2024-11, arXiv:2411.04468) | "Orchestrator + WebSurfer + FileSurfer + Coder + ComputerTerminal" 5 agent 通用 multi-agent 模板，GAIA/WebArena/AssistantBench SOTA 附近 | https://arxiv.org/abs/2411.04468 ; https://www.microsoft.com/en-us/research/articles/magentic-one-a-generalist-multi-agent-system-for-solving-complex-tasks/ |
| **Semantic Kernel Magentic orchestration** | 把 Magentic-One 思想带到 .NET / Python SK 框架，标准 manager (LLM) + 多角色 agent 协奏 | https://learn.microsoft.com/en-us/semantic-kernel/frameworks/agent/agent-orchestration/magentic |
| **llms.txt + Skills** | Jeremy Howard 提出的渐进式文档注入协议，被 LangChain Skills 借鉴 | (LangChain docs 引用) |

**对 HydraForge 的直接含义**：
- **SKILL.md 已是事实标准**。HydraForge 的 `.agent.md` 与 Anthropic Skills / Cline Skills 高度兼容，**ADR-0061 应明确 v0 = Markdown + YAML frontmatter + 三级加载**（metadata / body / script-reference）。
- **AFlow / ADAS / CapFlow 提供了三种 cost-quality trade-off**：(a) 重搜索但慢 (AFlow)、(b) 重学习但通用 (ADAS)、(c) 一次性模型推理 (CapFlow)。HydraForge 可从 CapFlow 起步（小模型内嵌），再用 GEPA 做持续 fine-tune。

---

## 📗 Topic 2 · Skill 优化（hot path / 节点替换 / 加速）

> 这是 ADR-0061 中"用更快实现替换热点节点"对应的 SOTA 维度。

| # | 论文 | ID | 一句话贡献 | URL |
|---|------|----|-----------|-----|
| 1 | **PASTE: Pattern-Aware Speculative Tool Execution** | arXiv:2603.18897 (Microsoft Research, 2026) | 发现 agent 工具调用模式稳定，从历史 trace 推断未来调用并 speculatively 执行；平均任务时长 ↓48.5%，工具吞吐 1.8× | https://arxiv.org/html/2603.18897v3 ; https://www.microsoft.com/en-us/research/publication/act-while-thinking-accelerating-llm-agents-via-pattern-aware-speculative-tool-execution/ |
| 2 | **Speculative Actions: A Lossless Framework for Faster Agentic Systems** | arXiv:2510.04371 (ICLR 2026, Columbia) | 类 speculative decoding：fast speculator 预跑 k 个可能 action，权威 actor 验证 commit；棋盘 / 电商 / 搜索多场景 20% 延迟下降 | https://www.cs.columbia.edu/~kkaffes/papers/speculative-actions-iclr26.pdf |
| 3 | **DSP: Dynamic Speculative Planning** | arXiv:2509.01920 (2025) | 在线 RL 决定 speculation 步数；ISP 的 cost-latency 折中通过单参数连续可调 | https://arxiv.org/pdf/2509.01920 |
| 4 | **Agent.xpu: Efficient Scheduling of Agentic LLM Workloads on Heterogeneous SoC** | arXiv:2506.24045 (2025) | prefill/decode 解耦调度到 NPU+iGPU；reactive-proactive 异质 flow；91% reactive 延迟 ↓ | https://arxiv.org/pdf/2506.24045 |
| 5 | **SPAgent: Speculation-based Algorithm-System Co-Design for LLM Search Agent** | arXiv:2511.20048 (2025) | 自适应两阶段 speculation（早期省略 verification）+ 两级调度器；端到端 1.65× 加速 | https://arxiv.org/html/2511.20048 |
| 6 | **AgentSight: System-Level Observability for AI Agents Using eBPF** | arXiv:2508.02736 (2025-10) | eBPF 在 TLS + syscall 双边注入，"intent-action" 因果关联，3% 开销；可检测 reasoning loop / 协调瓶颈 | https://arxiv.org/pdf/2508.02736 |
| 7 | **Demystifying the System Bottlenecks of Agentic AI Workloads** | arXiv:2511.00739 (2025) | 系统级剖析 5 个 agentic 工作负载（Haystack/Toolformer/ChemCrow/LangChain/SWE-Agent）；发现 **CPU tool processing 占延迟 90.6%** | https://arxiv.org/html/2511.00739v1 |
| 8 | **AgentDiet: Reducing Cost of LLM Agents via Trajectory Reduction** | arXiv:2509.23586 (2025) | 滑动窗口移除 trajectory 冗余/过期 token，节省 40-60% input token、22-36% 实际成本；保持性能 | https://arxiv.org/html/2509.23586v2 |
| 9 | **ProfilingAgent: Profiling-Guided Agentic Reasoning for Adaptive Model Optimization** | arXiv:2509.05584 (2025) | 多 agent 读 profiling metrics 自动调剪枝/量化策略，ResNet/ViT/Swin/DeiT 上量化内存省 74% | https://arxiv.org/html/2509.05584 |
| 10 | **Cost-Aware Speculative Execution for LLM-Agent Workflows** | arXiv:2606.07846 (2026) | 5 维 method（D1-D5）：speculation 美元成本建模 + Bayesian Beta-Binomial 估计成功率 + 漂移 kill-switch | https://doi.org/10.48550/arxiv.2606.07846 |
| 11 | **NVIDIA: Small Language Models are the Future of Agentic AI** (position paper, 2025-08) | blog | 论证 SLM 接管 agent 中 80% 的常规子任务（parse / 抽取 / summarization）；Llama 3.1B 比 3.3-405B **便宜 10-30×** | https://developer.nvidia.com/blog/how-small-language-models-are-key-to-scalable-agentic-ai/ |

**对 HydraForge 的直接含义**：
- **"hot path 替换"的本质是用 SLM / 缓存 / 推测执行代替昂贵 LLM 调用**——这条链路 SOTA 已经收敛于 (a) tracing-based hot path detection（AgentSight 7、Demystifying 7）+ (b) speculative parallel（PASTE 1、Speculative Actions 2）+ (c) trajectory compaction（AgentDiet 8）+ (d) SLM 替换（NVIDIA 11）。HydraForge 的 ADR-0061 应采用**AgentSight-style eBPF / TraceExporter**已有基础设施（与 Sprint 18 P1.T3 `execution_session.cpp` move 兼容）+ **SLM 候选库**决策。
- **CPU 工具链是瓶颈，不是 LLM**——7 这一发现支持 ADR-0061 "把 tool call 推到本地二进制"的论断。
- **PASTE 的应用模式**（trace mining → 模式匹配 → speculative 预执行）天然契合 HydraForge 的 DAG 调度器，可在 ExecutionSession 层加一个"speculative tool fork"。

---

## 📙 Topic 3 · Skill 编译（Agent 定义 → 本地码 / Wasm）

### 3A. WebAssembly / 浏览器端 Agent 运行时

| # | 项目 | ID | 一句话贡献 | URL |
|---|------|----|-----------|-----|
| 1 | **WebLLM: A High-Performance In-Browser LLM Inference Engine** | arXiv:2412.15803 (MLC.AI, 2024) | WebGPU + WebAssembly 双栈，Apache TVM/MLC-LLM 编译 WGSL/WASM；Llama-3.1-8B 解码保持 native **71-80%** | https://arxiv.org/html/2412.15803v2 ; https://github.com/mlc-ai/web-llm |
| 2 | **3W Stack: WebLLM + WASM + WebWorkers** | Mozilla AI blog, 2025-08 | 浏览器内多运行时（Rust/Go/Python-Pyodide/JS）每语言独立 worker + WASM runtime + WebLLM 实例，全离线运行 | https://blog.mozilla.ai/3w-for-in-browser-ai-webllm-wasm-webworkers/ |
| 3 | **WasmEdge / WASIX for LLM sandboxing** | (LLM Edge, 2024-2025) | WASI-NN / WASIX 把 ONNX / llama.cpp 装进 wasm 沙箱；Plug-in 模型 | (WasmEdge docs) |

### 3B. Agent → 本地码 / DSL 编译器

| # | 论文 / 项目 | ID | 一句话贡献 | URL |
|---|-----------|----|-----------|-----|
| 4 | **Compiled AI: Deterministic Code Generation for LLM-Based Workflow Automation** | arXiv:2604.05150 (2026) | LLM 一次性"编译"出 narrow business-logic functions 嵌入预验证模板，然后纯本地 deterministic 执行；token amortized 到 0（per-transaction）| https://arxiv.org/html/2604.05150v1 |
| 5 | **Agint: An Agentic Graph Compiler, Interpreter, and Runtime** | arXiv:2511.19635 (2025-11) | NL → typed effect-aware code DAG，**type floors** TEXT→TYPED→SPEC→CODE；hybrid LLM/function JIT runtime，dagent runtime 用 effect monad 安全回滚 | https://arxiv.org/pdf/2511.19635 |
| 6 | **A Declarative Language for Building and Orchestrating LLM-Powered Agent Workflows** | arXiv:2512.19769 (PayPal, 2025) | PayPal 上百万/日交互验证的 DSL，Java/Python/Go 多 backend，DSL→JSON IR；A/B test 内置；vs imperative 编码减少 67% 开发时间 | https://arxiv.org/pdf/2512.19769 |
| 7 | **λ_A: A Typed Lambda Calculus for LLM Agent Composition** | arXiv:2604.11767 (2026) | 给 LangGraph / CrewAI / AutoGen / OpenAI SDK / Dify 5 个主流框架一阶抽象，Coq 1519 行 42 定理 0 Admitted；835 真实 config 的 lint 工具 | https://arxiv.org/pdf/2604.11767v2 |
| 8 | **The New Compiler Stack: A Survey on the Synergy of LLMs and Compilers** | arXiv:2601.02045 (2026) | 系统性 survey LLM 用于 transpilation / IR optimization / agentic pass selection / LLM-as-compiler | https://arxiv.org/html/2601.02045v1 |
| 9 | **CompileAgent: Automated Real-World Repo-Level Compilation with Tool-Integrated LLM-based Agent System** | arXiv:2505.04254 (2025-05) | 5 个工具 + flow-based agent 策略自动 repo 编译，Claude-3.5 上比 baseline +71% | https://arxiv.org/html/2505.04254v1 |
| 10 | **AgentIR — A Compiler Infrastructure for Agentic Trajectories** ("LLVM for agent traces") | github.com/WhitzardAgent/agentir | LLVM/MLIR-style multi-level IR（RawIR→ParsedIR→Canonical），5 内建 framework frontends + 用户 YAML DSL，pass pipeline，backends 落 SFT/RL/eval/observability | https://github.com/WhitzardAgent/agentir |
| 11 | **Semantic Router DSL: Declarative Policy Compilation with Cross-Layer Verification** | arXiv:2603.27299 (2026) | 同一 .sr 源 → LangGraph decision nodes + Kubernetes/YANG/ConfigMap + MCP/A2A protocol gates；single-source 多 target 防 policy drift | https://arxiv.org/abs/2603.27299v1 |
| 12 | **LangGraph Pregel Runtime** | (LangChain blog) | channel version + BSP deterministic 并发，LangGraph Platform 自动编译并部署为 LangGraph API | https://www.langchain.com/blog/building-langgraph |
| 13 | **DeepWisdom MetaGPT Repository (AFlow 仓库示范)** | github.com/FoundationAgents/AFlow | AFlow 的开箱即用代码表示 + MCTS 实现，可参照 HydraForge 调度器 | https://github.com/FoundationAgents/AFlow |
| 14 | **HydraForge PDK `pdk/llama_engine/`** | (本仓库) | Phase 5 Llama Engine plugin (12 工具：engine/model/arch)；PDK 接口 + PluginLoader V2 验证完整 | (项目内部) |

**对 HydraForge 的直接含义**：
- **HydraForge 的"DAG → C++ → WASM"路径已经有清晰 SOTA 模板**：Compiled AI (4) 的"编译一次、永久 deterministic" + Agint (5) 的"type floor 渐进编译" + AgentIR (10) 的"trajectory IR 多 backend"三条路并行。**建议 ADR-0061 采用 AgentIR-style 的轨迹 IR 作为 v1 中间表示**（与 Runtime track 兼容，6 个 modules 已有 IR 设计基础）。
- **WebAssembly 路径** 1-3 已成熟。MLC-LLM 链（HydraForge Sprint C16 当前用的 llama.cpp）已是事实标准，**但 WebLLM/WasmEdge 提供 container 化 portable 二进制的可能性**——ADR-0061 可标注为 **Phase 4 长期目标**，而非 v1 必选。
- **λ_A 提供 formalism**——直接可借鉴给 HydraForge `.agent.md` 节点类型系统加 subtype 检查（与 Sprint ADR-0019 §1.4 PIMPL-lite 解耦兼容）。

---

## 📕 Topic 4 · Skill 等价验证（Evolution 前后行为一致性）

| # | 论文 / 项目 | ID | 一句话贡献 | URL |
|---|-----------|----|-----------|-----|
| 1 | **AgentAssay: Token-Efficient Regression Testing for Non-Deterministic AI Agent Workflows** | arXiv:2603.02601 (2026-03) | first **token-efficient** regression framework；三值 verdict (Pass/Fail/Inconclusive) + 行为指纹 (Hotelling T²) + adaptive budget + trace-first offline；5 模型 × 3 场景 × 7605 trial 验证；cost ↓78-100% | https://arxiv.org/html/2603.02601 |
| 2 | **ATA: Agent-Testing Agent** | arXiv:2508.17393 (2025) | meta-agent：static code analysis + designer interrogation + 文献 mining + persona-driven 对话生成（difficulty posterior 自适应）；20-30 分钟 vs 10 人 expert 数天 | https://arxiv.org/html/2508.17393v1 |
| 3 | **Neo: Configurable multi-agent framework for scalable testing of LLM agents** | arXiv:2507.14705 (2025) | Q&A agent + eval agent + probabilistic state model；10-12× throughput（180 questions in 45 min vs 16h 人工）；3.3% break vs 5.8% expert | https://arxiv.org/pdf/2507.14705 |
| 4 | **Automated structural testing of LLM-based agents** | arXiv:2601.18827 (2026) | OpenTelemetry traces + mocking + assertions = "agents 的 unit test/integration test"；adapt 测试金字塔 + TDD + regression | https://www.arxiv.org/pdf/2601.18827 |
| 5 | **An Empirical Study of Testing Practices in Open Source AI Agent Frameworks** | arXiv:2509.19185 (2025-09) | 实证研究 39 frameworks + 439 applications；发现 testing effort 在确定性组件（tools）占 70%+，但**prompts（trigger）只占 1%**——核心盲点 | https://arxiv.org/pdf/2509.19185 |
| 6 | **Rethinking Testing for LLM Applications: AICL protocol** | arXiv:2508.20737 (2025) | 3 层架构 × testing paradigm shifts；提出 **Agent Interaction Communication Language** schema-driven + replayable | https://arxiv.org/html/2508.20737v1 |
| 7 | **AgentLTL: A Trace-Verification Framework with FO-LTL** | arXiv:2607.02599 (2026) | 把 process 约束写成 First-Order LTL，deterministic judge-free compliance score；可作 pretraining reward；grounding constraint 抗 hallucination | https://arxiv.org/pdf/2607.02599 |
| 8 | **Oroboro: Checking Correctness for Agentic Systems with Temporal Expressions** | arXiv:2509.20364 (2025-08) | 借鉴硬件验证的 LTL，关注**行为轨迹**而不是文本匹配；3-agent 系统替换小 LLM 后能自动捕获偏差 | https://arxiv.org/pdf/2509.20364 |
| 9 | **Lean4Agent: Formal Modeling and Verification for Agent Workflow and Trajectory** | arXiv:2606.06523 (2026-06) | 首个 Lean4 形式化 agent 工作流验证；pass 的 workflow 比 fail 的 +11.94%；**LeanEvolve** 自动修订 workflow 又 +7.47% | https://arxiv.org/html/2606.06523v1 |
| 10 | **Verified Code Reasoning by LLMs** | arXiv:2509.26546 (Microsoft, 2025) | LLM 推理 → 抽取 AgentClaims（formal predicates）→ Soufflé Datalog 验证；20 个 uninit + 20 个 equivalence queries 上**捕错 6/8 个 LLM 错误判断** | https://arxiv.org/pdf/2509.26546 |
| 11 | **AgentBoard: Analytical Evaluation Board of Multi-turn LLM Agents** | arXiv:2401.13178 (2024-01) | fine-grained progress rate metric 而非最终成功率；可作为 HydraForge sprint-level regression baseline | https://arxiv.org/pdf/2401.13178 |
| 12 | **λ_A's empirical finding** | (同 3B-7) | 835 真实 GitHub config 中 **94.1% 在 λ_A 语义上结构不完整**——证明生产 agent 配置 brittle 度极高 | (同上) |

**对 HydraForge 的直接含义**：
- **NoSprint 14 C5 已 ship 行为契约（AgentAssert）**，但 **Prompts 维度测试覆盖率仅 1%** 是工业实证盲点（5）。ADR-0061 应将 **"prompt regression suite"** 作为 v1 必选项。
- **AgentAssay (1) 的 "behavioral fingerprint + Hotelling T² + adaptive budget + trace-first offline"** 几乎是 ADR-0061 应直接借鉴的模板——它在 token/cost 上比 binary pass/fail **多 5-20× 效率**，与 HydraForge cost-sensitive Sprint 19 cost-tracking decorator 兼容。
- **LTL / Lean4 形式化（7-9）是 v2 长期方向**，但可作为"演化前后等价证明"的最强工具；Oroboro 的 temporal assertions 适合 HydraForge ExecutionSession trace regression baseline。
- **dynamic differential testing 仍稀缺**——ATA / Neo 是少数覆盖 paper，但都属于**生成测试**而非**对比新旧版本**。这是 ADR-0061 可以做出 contrib 的差异化点。

---

## 📊 交叉对比表：研究主题 × 谁最强

| 维度 | 学术最强代表 | 工业最强代表 | **HydraForge 借鉴优先级** |
|------|-------------|--------------|-------------------------|
| **1.1 Skill 编译成图** (NL→DAG) | **AFlow (MCTS) + ADAS (Meta Agent Search)** + Stanford DSPy (signature/teleprompter) | **Anthropic Skills + LangChain Skills + Cline Skills** | **P0**：v1 直接采用 SKILL.md-compatible Markdown |
| **1.2 Skill 进化 / 优化** (迭代 / 进化) | **GEPA (Genetic-Pareto reflection) + Meta-Agent (DAG + 三级错误归因) + CapFlow** | AutoGen Magentic-One + LangGraph Pregel | **P1**：参考 GEPA 给 HydraForge 加 prompt evolution loop |
| **2.1 Hot path 检测** (profiling) | **AgentSight (eBPF) + Demystifying-bottleneck paper** | **NVIDIA NeMo Agent** + DeepEval/Promptfoo | **P0**：扩展 HydraForge TraceExporter + TraceRecord (Sprint 18 P1.T3 move 后天然适合) |
| **2.2 节点替换 / 加速** (fast impl) | **PASTE (speculative execution) + AgentDiet (trajectory reduction) + NVIDIA SLM** | vLLM / SGLang / Cortex | **P1**：PASTE 模式直接套到 DSLEngine runtime；SLM 模型分流加到 model_router plugin |
| **3.1 Wasm / 跨平台** | **WebLLM (TVM/WebGPU) + WasmEdge WASIX + Mozilla 3W** | Anthropic Skills API + Modular MAX | **P2/v2+**：v1 预留 WASM 边界，先 prototype <br>（HydraForge 已有 WasmEdge sandbox 替代 MemgrSandbox） |
| **3.2 Agent → Native 编译** | **Compiled AI + Agint (type floor) + λ_A (typed lambda calculus) + AgentIR (trajectory IR)** | OpenAI functions / Anthropic tool calls / PayPal DSL | **P0**：参考 Compiled AI 把 `.agent.md` 编译为 C++ 静态 graph（含 Sprint 14 PDK DECLARE_TOOL 已部分就绪） |
| **4.1 行为等价 / 回归测试** | **AgentAssay (token-efficient) + Behavioral fingerprinting** | DeepEval + LangSmith + Promptfoo | **P0**：v1 必选——直接 fuzz HydraForge ctest 套件 |
| **4.2 Formal Verification / 形式化** | **λ_A (Coq) + Lean4Agent + Oroboro (LTL) + Verified Code Reasoning** | Guardrails AI (Nickel) + NeMo Guardrails | **P2/v2+**：考虑为 layer×category 加 LTL 约束（与 ADR-0004 V2, Sprint 15 C6 已 ship 的 metadata 兼容） |

---

## 📚 引用索引

### 学术论文（43 个）

#### Topic 1 — Skill 进化（8 + 3 补充）
- arXiv:2410.10762 (ICLR 2025 Oral)
- arXiv:2408.08435
- arXiv:2605.25233
- arXiv:2507.19457 (ICLR 2026 Oral)
- arXiv:2310.03714 (ICLR 2024)
- arXiv:2407.12821
- arXiv:2602.11114
- arXiv:2602.01664
- ACL 2026: https://aclanthology.org/2026.acl-long.1278.pdf
- AAAI 2026: https://ojs.aaai.org/index.php/AAAI/article/view/40240
- arXiv:2509.21834

#### Topic 2 — Skill 优化（11）
- arXiv:2603.18897
- arXiv:2510.04371 (ICLR 2026)
- arXiv:2509.01920
- arXiv:2506.24045
- arXiv:2511.20048
- arXiv:2508.02736
- arXiv:2511.00739
- arXiv:2509.23586
- arXiv:2509.05584
- arXiv:2606.07846
- NVIDIA blog 2025-08

#### Topic 3 — Skill 编译（14）
- arXiv:2412.15803
- Mozilla AI blog 2025-08
- arXiv:2604.05150
- arXiv:2511.19635
- arXiv:2512.19769
- arXiv:2604.11767
- arXiv:2601.02045
- arXiv:2505.04254
- github.com/WhitzardAgent/agentir
- arXiv:2603.27299
- LangChain blog (Pregel Runtime)
- github.com/FoundationAgents/AFlow

#### Topic 4 — Skill 等价验证（12）
- arXiv:2603.02601
- arXiv:2508.17393
- arXiv:2507.14705
- arXiv:2601.18827
- arXiv:2509.19185
- arXiv:2508.20737
- arXiv:2607.02599
- arXiv:2509.20364
- arXiv:2606.06523
- arXiv:2509.26546
- arXiv:2401.13178
- arXiv:2604.11767 (λ_A empirical finding)

### 工业实践 / 平台（6 个）
- Anthropic Skills: https://www.anthropic.com/news/skills, https://platform.claude.com/docs/en/agents-and-tools/agent-skills/overview
- LangChain Skills: https://www.langchain.com/blog/langchain-skills
- Cline Skills: https://docs.cline.bot/customization/skills
- Continue.dev: https://docs.continue.dev/reference
- Magentic-One: https://arxiv.org/abs/2411.04468
- Semantic Kernel Magentic: https://learn.microsoft.com/en-us/semantic-kernel/frameworks/agent/agent-orchestration/magentic

### 项目仓库
- github.com/anthropics/skills
- github.com/langchain-ai/langchain-skills
- github.com/cline/sdk-skill
- github.com/WhitzardAgent/agentir
- github.com/FoundationAgents/AFlow
- github.com/mlc-ai/web-llm

### HydraForge 项目内部
- Sprint 14 C5 (AgentAssert)
- Sprint 14 C6 (DECLARE_TOOL V2)
- Sprint 18 P1.T3 (execution_session.cpp move)
- Sprint 19 (cost-tracking decorator)
- `pdk/llama_engine/` Phase 5 Llama Engine plugin