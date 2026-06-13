# ADR-0008: 结构化 Context

## 状态

**✅ Approved** (2026-05-12 起草, 2026-06-12 实现完成)

> **实现状态 (2026-06-12, Stage 3 / Tasks 12-13)**:
> - **规范**：`docs/specs/dsl.md` §4.1 完整定义 L1-L5 结构 ✅
> - **类型**：`include/agenticdsl/types/layered_context.h` (LayeredContext struct, 261 行) ✅
> - **测试**：`tests/test_layered_context.cpp` (10 test cases, 23/23 ctest pass) ✅
> - **兼容层**：`include/agenticdsl/types/context_flatten.h` (flatten 桥接) + `template_renderer` LayeredContext 重载 ✅
> - **遗留**：`using Context = nlohmann::json;` 仍保留为兼容别名 (deprecation note 已加) — Stage 4 (core-interface-inversion) 负责彻底迁移
>
> **向后兼容性**：所有现有调用方（6 个 DSLNode::execute 虚函数, DSLEngine::run, InjaTemplateRenderer）继续使用 flat Context，新代码可选用 LayeredContext + flatten() 桥接。

## 背景

HydraForge 的 Context 当前是 `using Context = nlohmann::json`——一个无结构的类型别名，所有数据混在一起。缺乏清晰的 schema 边界导致：
- Context 读写无章法
- 难以判断哪些数据该保留/压缩
- 工具访问无权限控制

**参考系统**：
- LangChain LCEL：MessageHistory + Condensed memory chain
- MemGPT：分层记忆 (recency/relevance/importance)
- LlamaIndex：Hierarchical index structure
- AgenticOS v2.2：`state.read` / `state.write` 工具

**设计原则**：
- **类型安全**：C++ struct 定义分层边界
- **JSON 灵活性**：层内部保持 nlohmann::json
- **压缩感知**：Archive/Recent 是原生字段（集成 ADR-7）
- **工具可控**：路径前缀控制读写权限

---

## 决策

### 1. Schema 设计：混合方案（结构化外壳 + JSON 内部）

```
┌─────────────────────────────────────────────────────────────┐
│  LayeredContext (C++ struct)                                │
│                                                              │
│  C++ 类型安全                                                 │
│  ├─ 分层边界清晰                                             │
│  ├─ 编译期类型检查                                          │
│  └─ IDE 自动补全                                            │
│                                                              │
│  nlohmann::json 内部                                        │
│  ├─ 保持 AgenticDSL 的 JSON 灵活性                         │
│  ├─ 工具层仍用 JSON 操作                                    │
│  └─ 压缩/序列化透明                                          │
└─────────────────────────────────────────────────────────────┘
```

### 2. 分层结构：L1-L5

```cpp
// ============================================================
// LayeredContext 完整结构
// ============================================================

struct LayeredContext {
    // ────────────────────────────────────────────────────────
    // L1: System（永不压缩/丢弃）
    // ────────────────────────────────────────────────────────
    struct SystemLayer {
        std::string agent_prompt;                        // Agent 提示词
        std::vector<ToolDef> tool_definitions;            // 工具定义
        std::string current_task;                          // 当前任务描述
        DSLVersion dsl_version;                           // DSL 版本
    } system;

    // ────────────────────────────────────────────────────────
    // L2: Recent（最近 N 轮，完整保留）
    // 由 ADR-7 ContextCompressor 管理
    // ────────────────────────────────────────────────────────
    std::vector<ContextTurn> recent_turns;               // 最近 5 轮

    // ────────────────────────────────────────────────────────
    // L3: Archive（压缩后历史）
    // 由 ADR-7 ContextCompressor 管理
    // ────────────────────────────────────────────────────────
    std::vector<ArchiveEntry> archive;                   // 压缩归档

    // ────────────────────────────────────────────────────────
    // L4: Working（当前执行状态，工具可写）
    // Phase 1: 简单 key-value
    // Phase 2: 可扩展为复杂结构
    // ────────────────────────────────────────────────────────
    struct WorkingLayer {
        nlohmann::json data;                             // 工具通过 state.write 写入

        // 预设常用路径
        std::string& operator[](const std::string& key) { return data[key]; }
        const nlohmann::json& at(const std::string& key) const { return data.at(key); }
        bool contains(const std::string& key) const { return data.contains(key); }
    } working;

    // ────────────────────────────────────────────────────────
    // L5: Meta（元数据）
    // ────────────────────────────────────────────────────────
    struct MetaLayer {
        int schema_version = CURRENT_SCHEMA_VERSION;     // Schema 版本
        std::string task_id;                              // 任务 ID
        int total_turns = 0;                              // 累计轮次
        int compress_count = 0;                           // 累计压缩次数
        std::chrono::steady_clock::time_point created_at;
        std::chrono::steady_clock::time_point last_updated_at;
    } meta;
};

// ============================================================
// ArchiveEntry（来自 ADR-7）
// ============================================================

struct ArchiveEntry {
    int turn_start;                                     // 起始轮次
    int turn_end;                                       // 结束轮次
    std::string conversation_summary;                   // 对话摘要
    std::vector<ToolResultSummary> tool_summaries;     // 工具摘要
    std::chrono::steady_clock::time_point timestamp;
};

// ============================================================
// ContextTurn（来自 ADR-7）
// ============================================================

struct ContextTurn {
    int turn_number;
    std::string user_input;
    std::string llm_output;
    std::vector<ToolCall> tool_calls;                  // 该轮工具调用
    std::string error;                                  // 错误（如果有）
};
```

### 3. JSON 表示

```json
{
    "system": {
        "agent_prompt": "You are a helpful assistant...",
        "tool_definitions": [
            {"name": "fs.read", "description": "Read file", "params": {...}},
            {"name": "state.write", "description": "Write to working memory", "params": {...}}
        ],
        "current_task": "Analyze the codebase and generate a report",
        "dsl_version": "3.10"
    },

    "recent_turns": [
        {
            "turn_number": 11,
            "user_input": "Show me the main files",
            "llm_output": "I'll analyze the structure...",
            "tool_calls": [
                {"name": "fs.read", "arguments": {"path": "src/main.cpp"}, "result": "..."}
            ]
        },
        {
            "turn_number": 12,
            "user_input": "Good, now summarize",
            "llm_output": "Based on my analysis...",
            "tool_calls": []
        }
    ],

    "archive": [
        {
            "turn_start": 1,
            "turn_end": 5,
            "conversation_summary": "User explored project structure, read README and main files. Identified 3 key modules.",
            "tool_summaries": [
                {"tool": "fs.read", "key_output": "README.md: 200 lines", "success": true},
                {"tool": "fs.read", "key_output": "src/main.cpp: 150 lines", "success": true}
            ],
            "timestamp": "2026-05-12T10:30:00Z"
        }
    ],

    "working": {
        "data": {
            "analysis_results": {"modules_found": 3, "key_files": ["a.cpp", "b.cpp"]},
            "user_preferences": {"verbose": false}
        }
    },

    "meta": {
        "schema_version": 1,
        "task_id": "task_001",
        "total_turns": 12,
        "compress_count": 1,
        "created_at": "2026-05-12T10:00:00Z",
        "last_updated_at": "2026-05-12T10:35:00Z"
    }
}
```

### 4. 工具访问：state.read / state.write

```cpp
// ============================================================
// StateTool：工具访问 Context 的唯一入口
// ============================================================

class StateTools {
public:
    explicit StateTools(LayeredContext& ctx) : ctx_(ctx) {}

    // 读接口
    nlohmann::json read(const std::string& path) {
        auto [layer, key] = parse_path(path);

        switch (layer) {
            case Layer::System:
                // system.* 只读
                return ctx_.system.at(key);

            case Layer::Recent:
                // recent.* 只读
                return ctx_.recent_turns.at(std::stoi(key));

            case Layer::Archive:
                // archive.* 只读
                return ctx_.archive.at(std::stoi(key));

            case Layer::Working:
                // working.data.* 可读写
                return ctx_.working.at(key);

            case Layer::Meta:
                // meta.* 只读
                return ctx_.meta.at(key);

            default:
                throw StateError{"Invalid path: " + path};
        }
    }

    // 写接口（只允许 working.data.*）
    void write(const std::string& path, const nlohmann::json& value) {
        auto [layer, key] = parse_path(path);

        if (layer != Layer::Working) {
            throw StateError{
                "Write not allowed to " + path + ". Only working.* is writable."
            };
        }

        ctx_.working[key] = value;
    }

    // 列出可用键
    std::vector<std::string> list_keys(const std::string& prefix) {
        // 返回符合前缀的键列表
    }

private:
    LayeredContext& ctx_;

    enum class Layer { System, Recent, Archive, Working, Meta };

    std::pair<Layer, std::string> parse_path(const std::string& path) {
        // "system.agent_prompt" → {Layer::System, "agent_prompt"}
        // "working.data.results" → {Layer::Working, "data.results"}
        // "memory.custom" → {Layer::Working, "data.custom"}  // memory 是 working.data 的别名
    }
};

// ============================================================
// 在 ToolRegistry 中注册
// ============================================================

void register_state_tools(ToolRegistry& registry, LayeredContext& ctx) {
    StateTools state(ctx);

    registry.register_tool("state.read", [&](const auto& args) -> json {
        auto path = args.at("path");
        return state.read(path);
    });

    registry.register_tool("state.write", [&](const auto& args) -> json {
        auto path = args.at("path");
        auto value = args.at("value");
        state.write(path, value);
        return {{"success", true}};
    });

    registry.register_tool("state.keys", [&](const auto& args) -> json {
        auto prefix = args.value("prefix", "working.");
        return {{"keys", state.list_keys(prefix)}};
    });
}
```

### 5. Schema 演进

```cpp
// ============================================================
// Schema 版本管理
// ============================================================

constexpr int CURRENT_SCHEMA_VERSION = 1;

struct ContextMigrations {
    // v0 → v1: 初始分层结构
    static LayeredContext migrate_v0_to_v1(const nlohmann::json& j) {
        LayeredContext ctx;
        ctx.meta.schema_version = 1;

        // 从旧的扁平结构迁移
        if (j.contains("agent_prompt")) ctx.system.agent_prompt = j["agent_prompt"];
        if (j.contains("conversation")) {
            // 旧: conversation[] → 新: recent_turns
            for (const auto& turn : j["conversation"]) {
                ctx.recent_turns.push_back(parse_turn(turn));
            }
        }
        if (j.contains("summary")) {
            // 旧: summary → 新: archive[0]
            ctx.archive.push_back({1, 0, j["summary"], {}, {}});
        }

        return ctx;
    }

    // v1 → v2（预留）
    static LayeredContext migrate_v1_to_v2(LayeredContext ctx) {
        ctx.meta.schema_version = 2;
        // v2 迁移逻辑
        return ctx;
    }
};

// ============================================================
// 安全加载
// ============================================================

LayeredContext load_context(const nlohmann::json& j) {
    // 处理缺失 meta
    int version = j.value("meta", {}).value("schema_version", 0);

    // 自动迁移到当前版本
    nlohmann::json current = j;
    while (version < CURRENT_SCHEMA_VERSION) {
        switch (version) {
            case 0: current = ContextMigrations::migrate_v0_to_v1(current); break;
            case 1: current = ContextMigrations::migrate_v1_to_v2(current); break;
            default: throw std::runtime_error("Unknown schema version: " + std::to_string(version));
        }
        version++;
    }

    return parse_context(current);
}

// ============================================================
// 向后兼容的访问（缺失字段返回默认值）
// ============================================================

namespace compat {
    // 为旧代码提供兼容访问
    nlohmann::json get_legacy_context(const LayeredContext& ctx) {
        nlohmann::json j;
        j["agent_prompt"] = ctx.system.agent_prompt;
        j["conversation"] = ctx.recent_turns;
        j["summary"] = ctx.archive.empty() ? "" : ctx.archive[0].conversation_summary;
        j["meta"] = ctx.meta;
        return j;
    }
}
```

### 6. DSL 集成

```yaml
# DSL 中使用 state 工具
AgenticDSL `/main/analyze`
type: tool_call
tool: state.write
arguments:
  path: "working.data.analysis_results"
  value: {"modules": 3, "status": "in_progress"}
next: "/main/continue"

AgenticDSL `/main/use_memory`
type: tool_call
tool: state.read
arguments:
  path: "working.data.previous_results"  # 读取之前存储的结果
next: "/main/continue"
```

---

## Phase 1 vs Phase 2

### Phase 1 实现

| 任务 | 描述 |
|------|------|
| LayeredContext struct | C++ 分层结构定义 |
| JSON 序列化/反序列化 | ↔ nlohmann::json |
| state.read / state.write | 工具注册 |
| 路径前缀控制 | 读写权限管理 |
| Schema 版本迁移 | v0 → v1 迁移 |

### Phase 2 扩展

| 任务 | 描述 | 优先级 |
|------|------|--------|
| L4 Important 显式标记 | 重要记忆永不丢弃 | 🟡 中 |
| state.delete 工具 | 删除 working 数据 | 🟡 中 |
| state.subscribe 工具 | 变更订阅（EventBus） | 🟢 低 |
| 向量记忆 | Archive 支持向量检索 | 🟢 低 |

---

## 权衡

### 为什么不是纯强类型 struct？

| 方案 | 优点 | 缺点 |
|------|------|------|
| **纯强类型** | 类型安全，IDE 支持 | 工具层需重新设计，JSON 灵活性丢失 |
| **混合（推荐）** | 类型安全 + JSON 灵活性 | 需要路径解析开销 |
| **纯 JSON** | 简单灵活 | 无类型边界，难以维护 |

**选择混合的理由**：
- C++ 层需要类型安全（分层边界清晰）
- 工具层保持 JSON（符合 AgenticDSL 设计哲学）
- 压缩（ADR-7）作为 struct 方法实现

### 为什么工具只能写 working.*？

- `system.*` 影响 Agent 行为，禁止工具修改
- `recent.*` 和 `archive.*` 由引擎管理，工具不应直接操作
- `working.*` 是工具的"沙箱"，安全可控

---

## 实现要求

### Phase 1 必须完成

| # | 任务 | 验证方式 |
|---|------|---------|
| 1 | LayeredContext struct | 单元测试：字段访问正确 |
| 2 | JSON 序列化 | 测试：round-trip 保持数据 |
| 3 | state.read/write 工具 | 测试：working.* 可读写，system.* 只读 |
| 4 | Schema 迁移 | 测试：v0 JSON 正确迁移到 v1 |
| 5 | DSL 集成 | 测试：DSL 中调用 state 工具 |

### 测试用例

```cpp
TEST_CASE("StateTools enforces read-only system layer") {
    LayeredContext ctx;
    StateTools tools(ctx);

    // 读 system 应该成功
    CHECK_NOTHROW(tools.read("system.agent_prompt"));

    // 写 system 应该失败
    CHECK_THROWS_AS(tools.write("system.agent_prompt", "new prompt"), StateError);
}

TEST_CASE("StateTools allows write to working layer") {
    LayeredContext ctx;
    StateTools tools(ctx);

    tools.write("working.data.test", {{"key", "value"}});
    CHECK(ctx.working.data["test"]["key"] == "value");
}

TEST_CASE("Schema migration v0 to v1") {
    nlohmann::json v0 = {
        {"agent_prompt", "..."},
        {"conversation", {{"turn_number", 1}, {"user_input", "hi"}}}
    };

    auto ctx = load_context(v0);
    CHECK(ctx.meta.schema_version == 1);
    CHECK(ctx.system.agent_prompt == "...");
    CHECK(ctx.recent_turns.size() == 1);
}
```

---

## 影响范围

| 组件 | 变更 |
|------|------|
| `src/core/types/context.h/cpp` | 新增 LayeredContext struct |
| `src/modules/context/context_engine.h/cpp` | 重构为使用 LayeredContext |
| `src/common/tools/registry.h/cpp` | 注册 state 工具 |
| `src/modules/executor/node_executor.h/cpp` | 工具调用使用 StateTools |
| `src/modules/llm/llm_adapter.h/cpp` | 可能需要调整 Prompt 构建 |

---

## 替代方案

### 替代 1：纯强类型 struct（被否决）

**否决理由**：工具层需要重新设计，丢失 AgenticDSL 的 JSON 灵活性。

### 替代 2：纯 JSON，无结构（被否决）

**否决理由**：无类型边界，Context 读写无章法，难以维护。

### 替代 3：分散在多个 JSON 文件（被否决）

**否决理由**：增加 I/O 复杂度，序列化/反序列化复杂。

---

## 结论

采用结构化外壳 + JSON 内部的混合方案：

- **C++ struct**：`LayeredContext` 定义 L1-L5 分层
- **JSON 内部**：层内部保持 nlohmann::json 灵活性
- **工具访问**：`state.read` / `state.write` + 路径前缀控制
- **Schema 演进**：版本字段 + 自动迁移函数
- **ADR-7 集成**：Archive/Recent 是原生字段

此设计支持：
- **Phase 1**：清晰的分层 Context + 安全的工具访问
- **Phase 2**：L4 Important 标记 + 向量记忆

---

*文档版本: v1.0*
*最后更新: 2026-05-12*