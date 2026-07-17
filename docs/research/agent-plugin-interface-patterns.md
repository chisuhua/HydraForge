# Agent Plugin 接口与组合模式调研摘要

**调研日期**: 2026-07-16
**来源**: Librarian 后台调研（Task `bg_d1152ca8`）+ 架构组综合
**范围**: OSGi, COM, FIPA/JADE, ROS 2, Kubernetes Operator, 插件组合模式

---

## 一、核心模式目录

### 模式 1：OSGi Service Registry（发布-查找-绑定模型）

**来源**: OSGi Core R8 §5

**核心属性**：
- 服务是普通对象，注册到一个或多个接口下
- 客户端通过 `ServiceReference` 查询，运行时动态绑定
- 属性字典支持 LDAP 风格 filter 表达式
- `service.ranking` 实现多实现排序
- 三种作用域：`singleton` / `bundle` / `prototype`

**对 HydraForge 的适用性**：
- 直接映射为 Agent 插件的注册/发现层
- 属性字典机制天然适合承载"实现形式"和能力描述

### 模式 2：OSGi Bundle Lifecycle + Lazy Activation

**来源**: OSGi Core R8 §4

**核心属性**：
- 状态机：`INSTALLED → RESOLVED → STARTING → ACTIVE → STOPPING → UNINSTALLED`
- `BundleActivator` 的 `start(BundleContext)` / `stop(BundleContext)` 是同步入口
- Lazy activation：bundle 只在第一次类加载时才 ACTIVE
- **关键设计**：启动失败时 Framework 不自动调用 `stop`，启动函数必须自清理

**对 HydraForge 的适用性**：
- 映射为 Plugin Lifecycle
- 启动失败需自清理，不能依赖 OS 自动调用 shutdown

### 模式 3：COM Interface Immutability + IUnknown::QueryInterface

**来源**: Microsoft Learn

**核心属性**：
- 接口一旦发布，**不可变**
- 新功能 = 新接口（新 GUID），旧接口保留
- `IUnknown` 是根接口，提供 `QueryInterface` / `AddRef` / `Release`
- 通过接口继承实现版本演进

**关键铁律**：
> "For a COM interface, the version attribute cannot be used. Creating new interfaces and inheriting from the old interfaces is an equivalent of manipulating the version in RPC."

> "Interfaces are immutable. COM interfaces are never versioned... A new version of an interface, created by adding more functions or changing semantics, is an entirely new interface and is assigned a new, unique identifier."

**对 HydraForge 的适用性**：
- 如果 Agent 接口需要 5-10 年稳定，COM 模式比 OSGi 更合适
- 决策点：接口是"语义兼容演进"（OSGi）还是"不可变叠加"（COM）？
- **建议**：混合风格——接口 ID 用 UUID 标识，但允许通过接口继承添加 V2 capability

### 模式 4：FIPA Agent Platform + JADE

**来源**: FIPA 97 规范 + Bellifemine 2001 论文

**核心属性**：
- 强制系统 Agent：AMS（白页）、DF（黄页）、ACC（消息路由）
- 每个 agent 有 GUID + transport address
- FIPA 只规范外部行为，不约束内部实现

**JADE 实现细节**：
- 一个 JVM = 一个 Agent Container
- 每个 agent 一个 Java 线程
- `ServiceDescription` 描述 agent 能力（languages、ontologies、services）
- DF 按 capability 查询

**对 HydraForge 的适用性**：
- AMS → `ManifestRegistry`
- DF → `CapabilityRegistry`
- ACC → `IInteractionBus`
- 内部三形态（DSL/DSL-graph/C++）正是 FIPA 所说"implementation outside scope"的体现

### 模式 5：ROS 2 Managed Node Lifecycle + Composition

**来源**: `design.ros2.org/articles/node_lifecycle.html`

**核心属性**：
- 状态机：`Unconfigured → Inactive → Active → Finalized` + 6 过渡状态
- 7 个外部可控 transitions：`create / configure / cleanup / activate / deactivate / shutdown / destroy`
- Managed Node 是 black box：接口固定，内部自由
- Composition：每个组件编译为 shared library，可独立进程或共享进程

**对 HydraForge 的适用性**：
- "Managed Node 是 black box" 哲学完全契合 Agent-as-Plugin
- 提供统一的 lifecycle state machine 管理方式

### 模式 6：Kubernetes Operator Pattern

**来源**: Kubebuilder Book + Kubernetes 官方文档

**核心属性**：
- CRD 声明 schema 扩展
- Controller / Reconciler 实现 `Reconcile(ctx, req)` 循环
- 单向收敛 desired state vs actual state
- Reconciler 返回 `Result{}` 和 error，框架决定是否重试

**对 HydraForge 的适用性**：
- 如果应用层用声明式 YAML 描述 Agent 组合，Operator pattern 是最成熟参考
- 适合 Phase B 的 Agent Marketplace 管理

### 模式 7：Capability-Based Discovery

**核心属性**：
- OSGi 用 LDAP 风格 filter：`(objectClass=ILogger)&(priority>=5)`
- FIPA/JADE 用 `ServiceDescription` POJO（languages、ontologies、services）
- 本质相同：声明式 capability matching

**对 HydraForge 的适用性**：
- Agent-as-Plugin 天然适合 capability matching
- Loop Agent 注册 `{input: UserMessage, output: ToolCall[]}`
- Budget Agent 注册 `{input: CostProjection, output: ApprovalDecision}`
- 组合查询：`(input ⊇ required_inputs) AND (output ⊇ required_outputs)`

---

## 二、模式对比表

| 模式 | 接口可变性 | 多实现选择 | 组合机制 | 适合部署规模 |
|------|-----------|------------|----------|--------------|
| OSGi Service Registry | 接口可演进 | `service.ranking` + filter | ServiceTracker + DS | 中大型单进程 |
| COM IUnknown | 接口不可变 | `QueryInterface` 探测 | Interface inheritance | 系统级长期稳定 |
| FIPA/JADE | 行为规范不可变 | ServiceDescription + DF | ACL message passing | 跨平台分布式 |
| ROS 2 Managed Node | Lifecycle 接口固定 | launch file 声明 | Composition | 机器人节点级 |
| K8s Operator | CRD schema 强约束 | label selector | Reconciler + owner reference | 集群级跨进程 |

---

## 三、直接映射到三个设计问题

### 问题 1：Agent 接口契约

**推荐方案**：融合 ROS 2 Managed Node + FIPA ServiceDescription + OSGi Properties

```cpp
struct IAgentV1 {
    virtual ~IAgentV1() = default;
    virtual const AgentMetadata& metadata() = 0;  // FIPA ServiceDescription 思路
    virtual void configure(const AgentConfig&) = 0;  // ROS 2 onConfigure
    virtual AgentResult execute(const AgentInput&, IExecutionContext*) = 0;
    virtual void shutdown() = 0;  // OSGi stop 思路
};

struct IAgentV2 : public IAgentV1 {  // COM 风格接口继承
    virtual std::optional<AgentCapability> query_capability(const std::string&) = 0;
};
```

**关键选择**：
- 接口不可变 + 版本叠加（COM 风格）：长期稳定
- 接口可演进 + 强兼容性约束（OSGi 风格）：快速迭代
- **建议**：混合风格——接口 ID 用 UUID/GUID，但允许通过接口继承添加 V2 capability

### 问题 2：多形式实现分派（DSL / DSL-graph / C++ / Wasm）

**推荐方案**：内部 Strategy 模式 + OSGi Property 标记实现形式

```cpp
enum class ImplementationForm { MARKDOWN_DSL, AGENTICDSL_GRAPH, NATIVE_CPP, WASM };

struct AgentMetadata {
    std::string name;
    std::string version;
    ImplementationForm form;
    std::string entry_point;  // DSL: .md path; Graph: graph id; C++: factory name
    InputSchema inputs;
    OutputSchema outputs;
    std::vector<std::string> capabilities;
};

std::unique_ptr<IAgentV1> AgentFactory::create(const AgentMetadata& m) {
    switch (m.form) {
        case ImplementationForm::MARKDOWN_DSL:
            return std::make_unique<MarkdownDSLAdapter>(m.entry_point);
        case ImplementationForm::AGENTICDSL_GRAPH:
            return std::make_unique<AgenticDSLGraphAdapter>(m.entry_point);
        case ImplementationForm::NATIVE_CPP:
            return lookup_native_factory(m.entry_point)();
        case ImplementationForm::WASM:
            return WasmRuntime::instantiate(m.entry_point);
    }
}
```

### 问题 3：插件发现与组合协议

**推荐方案**：三层组合（FIPA + OSGi）

| 层 | 角色 | 协议 |
|----|------|------|
| L1 Registry (AMS) | 静态索引：列出所有已加载 .so | `pdk_plugin_info` 导出符号 + 索引文件 |
| L2 Capability Index (DF) | 动态查询：按 input/output schema 找 Agent | `registry.query(required_inputs, required_outputs)` |
| L3 Composition (ACC) | 运行时：组合 Agent 形成工作流 | Agent-as-Node，DAG 编排（ROS 2 launch file 风格） |

**L1 plugin manifest 建议格式**：
```json
{
    "plugin_id": "agent_loop_v3",
    "version": "3.2.0",
    "abi_version": 2,
    "interface_versions": ["IAgentV1", "IAgentV2"],
    "implementation_forms": ["MARKDOWN_DSL", "AGENTICDSL_GRAPH"],
    "capabilities": ["react_loop", "plan_execute"],
    "factory_symbol": "create_agent_loop",
    "metadata_symbol": "pdk_plugin_info"
}
```

---

## 四、关键风险与缓解（来自历史教训）

1. **OSGi 复杂度过高**：ServiceTracker + Bundle lifecycle 回调地狱
   - **缓解**：参考 OSGi Declarative Services 注解式声明，改用 YAML/Markdown

2. **COM 的 GUID 蔓延**：每个新接口需要新 GUID
   - **缓解**：用稳定的接口继承层级 + capability 查询兜底

3. **FIPA ACL 文本消息过于冗长**：JADE 论文承认解析开销大
   - **缓解**：内部使用强类型结构（C++ struct + JSON schema），跨进程边界才用文本

4. **ROS 1 ABI 不稳定**：ROS 1 假设 ABI 不兼容，每次重建下游
   - **缓解**：发布 ABI version 字段，每个 plugin 显式声明兼容版本

5. **Kubernetes operator 反馈循环复杂性**：Reconciler 必须幂等
   - **缓解**：Agent 执行天然幂等（DSL 重跑 = 同结果），契合此约束

---

## 五、参考来源

### 规范与官方文档
- OSGi Core R8 §5: `osgi.github.io/osgi/core/framework.service.html`
- OSGi Core R8 §4: `osgi.github.io/osgi/core/framework.lifecycle.html`
- OSGi Core R8 §701 Tracker: `osgi.github.io/osgi/core/util.tracker.html`
- OSGi Architecture: `osgi.org/resources/architecture/`
- OSGi Service Loader Mediator: `docs.osgi.org/specification/osgi.cmpn/8.0.0/service.loader.html`
- Microsoft Learn: Versioning Theory for RPC and COM: `learn.microsoft.com/en-us/windows/win32/rpc/the-versioning-theory-for-rpc-and-com`
- Microsoft Learn: COM Technical Overview: `learn.microsoft.com/en-us/windows/win32/com/com-technical-overview`
- Microsoft Learn: Changing Interfaces in a Backward Compatible Manner: `learn.microsoft.com/en-us/windows/win32/rpc/changing-interfaces-in-a-backward-compatible-manner`
- FIPA 97 Agent Management Spec: `ptacts.uspto.gov/...`
- FIPA Abstract Architecture PC00001C: `www.yumpu.com/en/document/view/30381556/fipa-abstract-architecture-specification`
- ROS 2 Why ROS 2: `design.ros2.org/articles/why_ros2.html`
- ROS 2 Changes: `design.ros2.org/articles/changes.html`
- ROS 2 Lifecycle: `design.ros2.org/articles/node_lifecycle.html`
- Kubebuilder Book Controller Overview: `book.kubebuilder.io/cronjob-tutorial/controller-overview`
- Kubebuilder Architecture: `book.kubebuilder.io/architecture.html`

### 学术论文
- Bellifemine, Poggi, Rimassa (2001). *Developing multi-agent systems with a FIPA-compliant agent framework*. `jmvidal.cse.sc.edu/library/bellifemine01a.pdf`
- Macenski et al. (2022). *Robot Operating System 2: Design, architecture, and uses in the wild*. Science Robotics. `DOI: 10.1126/scirobotics.abm6074`

### 实现参考
- Eclipse Equinox: `github.com/eclipse-equinox/equinox`
- EclipseSource OSGi Lifecycle: `eclipsesource.com/blogs/2013/01/23/how-to-track-lifecycle-changes-of-osgi-bundles/`
- vogella OSGi Tutorial: `www.vogella.com/tutorials/OSGi/article.html`
