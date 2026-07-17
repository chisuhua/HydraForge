# ADR 候选议题：Agent-as-Plugin 架构

**日期**: 2026-07-16
**状态**: 🟡 Proposed (待讨论)
**作者**: Architecture Working Group

---

## 说明

本文档列出基于 Agent-as-Plugin 架构研究需要新增或修订的 ADR。议题按优先级分组，供架构评审讨论。

---

## P0：本期必须讨论并决策

### ADR-0052: Agent Plugin Manifest 规范

**议题**: 定义 `pdk_manifest.json` 的机器可读格式、必填字段、版本协商机制。

**需要决策**：
1. 字段集：是否需要 `input_schema` / `output_schema` / `capabilities` / `activation_events`？
2. 版本约束：`abi_version` 是否足够？是否需要 `min_host_version` / `max_host_version`？
3. 签名与信任：`publisher` / `signature` 字段是否纳入 v1？
4. 与 `pdk_plugin_info` 的关系：是否最终合并为单一 manifest？

**关联**: ADR-0021 (PDK), ADR-0022 (Plugin Loading), ADR-0051 (Composition Spike)

---

### ADR-0053: AgentDescriptor 与 `pdk_register_agent` 接口

**议题**: 定义 Agent 如何向 OS 注册自己的形态、能力、依赖和契约。

**需要决策**：
1. `AgentForm` 枚举是否包含 `Skill` / `DSL` / `Cpp` / `Wasm` / `Hybrid`？
2. `forms` 字段是 `vector`（支持多形态）还是单一值？
3. `requires_isolation` 是否强制 SKILL 形态为 true？
4. `interface_versions` 是否采用 COM 风格的 UUID/GUID？
5. 是否允许一个 Plugin 注册多个 Agent？

**关联**: ADR-0021 (PDK), ADR-0019 (IInteractionBus), ADR-0051 (Composition Spike)

---

### ADR-0054: Capability-Based Agent Discovery

**议题**: 定义 OS 如何按 input/output schema 和能力发现 Agent，而不是按名字硬编码。

**需要决策**：
1. Capability 描述格式：JSON Schema 还是自定义 DSL？
2. 查询协议：FIPA DF 风格还是 OSGi LDAP filter 风格？
3. 多实现选择策略：ranking / version / trust_level？
4. 动态加载时如何更新 capability index？

**关联**: FIPA 规范, OSGi Service Registry, ADR-0022 (Plugin Loading)

---

### ADR-0055: SKILL.md 执行与隔离模型

**议题**: 定义 OS 如何解释执行 SKILL.md，以及必须提供的隔离保证。

**需要决策**：
1. 隔离技术选型：进程沙箱 / Wasm 解释器 / 受限脚本引擎 / cgroups + seccomp？
2. SKILL.md 语法子集：禁止哪些构造（如无限循环、文件系统访问、网络访问）？
3. Capability 注入：SKILL 能调用哪些 OS 服务？
4. 资源限制：timeout / max_concurrent / memory 如何声明和执行？
5. 与现有 `examples/skill_porting/` 的格式兼容性问题。

**关联**: ADR-0021 (SafeExec), ADR-0004 (ToolRegistry Security), ADR-0031 (Execution Policy)

---

### ADR-0056: WebAssembly Agent 运行时

**议题**: 定义如何将 Agent 编译为 Wasm，以及 OS 如何加载和执行 Wasm Agent。

**需要决策**：
1. 编译路径：DSL → Wasm 还是 C++ → Wasm，还是两者都支持？
2. Wasm 运行时选型：WAMR / Wasmtime / wasmer / 自研？
3. Host functions 白名单：哪些 OS 服务可以暴露给 Wasm？
4. Capability 模型：如何限制 Wasm 的 host function 权限？
5. 内存与性能：Wasm 内存模型是否限制 Agent 的上下文大小？

**关联**: ADR-0021 (PDK), ADR-0004 (ToolRegistry Security), ADR-0055 (Skill Isolation)

---

### ADR-0057: Agent 生命周期管理（Lifecycle）

**议题**: 定义 Agent Plugin 的安装、初始化、激活、停用、卸载、热更新生命周期。

**需要决策**：
1. 状态机：是否采用 ROS 2 的 `Unconfigured → Inactive → Active → Finalized`？
2. 是否支持 lazy-load / activation events（VS Code 模式）？
3. 热更新：是否支持运行时替换 Plugin？还是需要进程重启？
4. 启动失败：是否像 OSGi 一样不自动调用 stop，要求自清理？
5. 依赖管理：如果 Agent A 依赖 Agent B，卸载顺序如何保证？

**关联**: ADR-0022 (Plugin Loading), ADR-0041 (PluginLoader Lifecycle Extension), OSGi, ROS 2

---

## P1：强烈建议本期讨论

### ADR-0058: Tool Input/Output Schema 强制校验

**议题**: 定义工具调用时如何强制校验输入输出 schema。

**需要决策**：
1. Schema 格式：JSON Schema 2020-12 是否足够？
2. 校验时机：注册时 / 调用时 / 两者都校验？
3. `ToolMetadata` 是否需要扩展 `input_schema` / `output_schema` 字段？
4. 错误处理：schema 校验失败返回什么错误码？

**关联**: ADR-0004 (ToolRegistry Security), ADR-0023 (ToolResult Standard), MCP 2026-07-28 RC

---

### ADR-0059: 跨进程/跨网络 Agent 协议

**议题**: 定义 HydraForge Agent 如何与外部 Agent 或 MCP/A2A 服务交互。

**需要决策**：
1. 是否支持 MCP 协议？A2A 协议？两者都支持？
2. 进程内 Agent 与远程 Agent 是否有统一接口？
3. 序列化格式：JSON / Protobuf / gRPC？
4. 安全模型：远程 Agent 如何认证？Capability 如何传递？
5. 与 Phase 6 服务化（ADR-0050）的关系。

**关联**: ADR-0019 (IInteractionBus), ADR-0050 (Phase 6 Strategic Evaluation), ADR-0051

---

### ADR-0060: Agent 组合协议与声明式编排

**议题**: 定义应用如何通过声明式配置组合多个 Agent。

**需要决策**：
1. 编排格式：YAML / JSON / .agent.md？
2. 组合模式：工具调用链 / DSL 子图 / 事件驱动 / 是否允许嵌套？
3. 是否需要 `AgentOrchestrator` 组件实现 K8s Operator 风格的 Reconciler？
4. 动态编排：是否允许运行时加载/卸载 Agent？

**关联**: ADR-0019 (IInteractionBus), ADR-0033 (Session Hierarchy), ADR-0051

---

### ADR-0061: Agent 进化与固化（Solidification）

**议题**: 定义 SKILL.md 如何转化为 .agent.md，以及 DSL 如何转化为 C++ / Wasm。

**需要决策**：
1. 固化是否必须保持行为等价？允许的偏差范围？
2. 固化是自动（LLM）还是半自动（人工 review）？
3. 固化产物如何版本化？
4. 性能化过程：如何识别 DSL 热点并替换为 C++？
5. 可移植化：哪些 Agent 适合编译为 Wasm？

**关联**: ADR-0055 (Skill), ADR-0056 (Wasm), ADR-0021 (PDK)

---

## P2：可选增强，留待后续

### ADR-0062: Agent Marketplace 与包格式

**议题**: 定义 HydraForge Agent 的分发包格式、签名验证、沙箱隔离、版本管理、声誉系统。

**需要决策**：
1. 包格式：`.hfpkg` 是否是 zip/tar？
2. 签名算法：ED25519 / secp256k1 / 其他？
3. 沙箱级别：cgroups / seccomp / Firecracker / Wasm？
4. 与 Layer 4.5 声誉系统的集成方式。

**关联**: ADR-0052 (Manifest), ADR-0056 (Wasm), Layer 4.5 Social Layer

---

### ADR-0063: OpenTelemetry / Distributed Tracing 集成

**议题**: 定义如何将 HydraForge 的 TraceRecord 与 OpenTelemetry 对齐。

**需要决策**：
1. 是否添加 `OpenTelemetryExporter` 作为可选 trace sink？
2. 是否支持 MCP `traceparent` 标准？
3. Trace 语义：如何表示 Agent 调用链、DAG 节点、Tool 调用？

**关联**: ADR-0023 (ToolResult), ADR-0031 (ToolCoordinator Audit Events), SW4RM

---

### ADR-0064: PDK Conformance Test Suite

**议题**: 定义外部 Agent 开发者如何验证其 Plugin 符合 HydraForge 规范。

**需要决策**：
1. Conformance 测试范围：manifest / tool schema / lifecycle / security / capability？
2. 测试框架：Catch2 扩展还是独立工具？
3. 是否随 SDK 分发？

**关联**: ADR-0052 (Manifest), ADR-0058 (Schema Validation), ADR-0021 (PDK)

---

### ADR-0065: 多语言 PDK 支持

**议题**: 定义是否以及如何让 Python / Rust 开发者也能编写 HydraForge Agent Plugin。

**需要决策**：
1. 是否支持 C ABI 之外的 PDK？
2. 多语言 Plugin 的隔离模型是否与 C++ 不同？
3. 是否优先通过 Wasm 支持多语言？

**关联**: ADR-0056 (Wasm), ADR-0021 (PDK), ATD `Binding` trait

---

## 需要立即讨论并决策的议题

建议在下一轮架构评审中优先讨论以下 6 个 P0 议题：

1. **ADR-0052**: Agent Plugin Manifest 规范
2. **ADR-0053**: AgentDescriptor 与 `pdk_register_agent` 接口
3. **ADR-0054**: Capability-Based Agent Discovery
4. **ADR-0055**: SKILL.md 执行与隔离模型
5. **ADR-0056**: WebAssembly Agent 运行时
6. **ADR-0057**: Agent 生命周期管理（Lifecycle）

这 6 个 ADR 决定了后续 `examples/pdk_chat_demo` 和其他 Agent Plugin 的基础设施形态，需要在开始编码前达成一致。

---

## 关联现有 ADR

| 现有 ADR | 关联的新议题 |
|---------|------------|
| ADR-0019 (IInteractionBus) | ADR-0059, ADR-0060 |
| ADR-0020 (Thread Model) | ADR-0055, ADR-0056, ADR-0057 |
| ADR-0021 (PDK) | ADR-0052, ADR-0053, ADR-0061, ADR-0064, ADR-0065 |
| ADR-0022 (Plugin Loading) | ADR-0052, ADR-0054, ADR-0057 |
| ADR-0023 (ToolResult) | ADR-0058, ADR-0063 |
| ADR-0031 (Execution Policy) | ADR-0055, ADR-0058 |
| ADR-0033 (Session Hierarchy) | ADR-0060 |
| ADR-0043 (Tool Naming) | ADR-0052, ADR-0058 |
| ADR-0050 (Phase 6 Strategic) | ADR-0059 |
| ADR-0051 (Composition Spike) | ADR-0053, ADR-0054, ADR-0060 |
