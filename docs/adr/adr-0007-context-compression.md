# ADR-0007: 上下文压缩机制

## 状态

**已批准** (2026-05-12)

## 背景

HydraForge AgenticDSL 引擎通过 DSL 执行长对话任务。随着对话轮次增加，`Context`（当前为 `nlohmann::json`）会无限增长，最终超出 LLM 的 `n_ctx` 上下文窗口限制，导致执行失败。

**问题根源**：
- `Context` 是无结构的 JSON blob，无压缩机制
- 每次节点执行都向 Context 追加数据
- 长对话（100+ 轮次）会导致上下文窗口爆炸

**参考系统**：
- LangChain LCEL：Message history + Condensed memory chain
- MemGPT：分层记忆 (recency/relevance/importance)
- Claude Code：Sub-8000 token management
- AutoGPT：Strategy compression (recent + summary)

---

## 决策

### 1. 压缩策略：对话摘要 + 选择性保留混合方案

```
┌─────────────────────────────────────────────────────────────┐
│  Context 分层结构                                            │
│                                                              │
│  L1: Recent Buffer (最近 N 轮，完整保留)                       │
│      ├─ 最近 5 轮对话                                       │
│      ├─ 当前任务目标                                         │
│      └─ 工具描述（永不压缩）                                  │
│                                                              │
│  L2: Compressed Archive (压缩归档区)                          │
│      ├─ 早期对话 → 摘要                                     │
│      ├─ 工具结果 → 关键摘要                                  │
│      └─ 压缩比：5-10x                                       │
│                                                              │
│  L3: System Prompt (永不压缩)                                 │
│      ├─ Agent prompt                                       │
│      └─ 静态配置                                             │
└─────────────────────────────────────────────────────────────┘
```

### 2. 核心参数

| 参数 | 值 | 理由 |
|------|-----|------|
| **压缩阈值** | n_ctx 的 70% | 留 30% 给新输入和压缩开销 |
| **Recent Buffer** | 最近 5 轮 | 足够短期记忆，平衡保留与压缩 |
| **Archive 摘要数** | 最多 10 个 | 超过则丢弃最旧的 |
| **摘要 LLM** | 轻量模型 (Qwen-0.6B) | 本地快速摘要 |
| **压缩比** | 5-10x | 典型摘要压缩率 |

### 3. 压缩触发条件

```cpp
// 触发条件（优先级递减）
enum class CompressionTrigger {
    TokenThreshold,   // Token 数超过阈值（主要触发）
    TurnThreshold,    // 轮次超过阈值（次要触发）
    TimeThreshold     // 长时间无活动（辅助触发）
};

// 检查时机：每次 DSLEngine::step() 执行前
bool should_compress(const Context& ctx) {
    size_t token_count = estimate_token_count(ctx);
    size_t threshold = llm_->context_window() * 70 / 100;
    return token_count > threshold;
}
```

### 4. 数据结构

```cpp
// ============================================================
// 压缩归档项
// ============================================================

struct ArchiveEntry {
    int turn_number;                      // 原始轮次编号
    std::string conversation_summary;     // 对话摘要
    std::vector<ToolResultSummary> tool_summaries;  // 工具结果摘要
    std::chrono::steady_clock::time_point timestamp;
};

struct ToolResultSummary {
    std::string tool_name;
    std::string key_output;     // 关键输出（截断长输出）
    bool success;
    std::string brief_error;    // 错误摘要（如果有）
};

// ============================================================
// 分层 Context
// ============================================================

struct LayeredContext {
    // L1: 永不压缩
    nlohmann::json system_prompt;      // Agent prompt
    nlohmann::json tool_definitions;   // 工具描述

    // L1: 完整保留
    std::vector<ContextTurn> recent_turns;  // 最近 5 轮

    // L2: 压缩归档
    std::vector<ArchiveEntry> archive;     // 压缩后的历史

    // 元数据
    struct Metadata {
        bool is_compressed = false;
        int original_turn_count = 0;
        float compression_ratio = 1.0f;
        int compress_count = 0;  // 累计压缩次数
    } metadata;
};

struct ContextTurn {
    int turn_number;
    std::string user_input;
    std::string llm_output;
    std::vector<ToolCall> tool_calls;
};
```

### 5. ContextCompressor 实现

```cpp
// ============================================================
// ContextCompressor
// ============================================================

class ContextCompressor {
public:
    struct Config {
        size_t compression_threshold_pct = 70;   // n_ctx 的 70%
        size_t recent_buffer_turns = 5;           // 保留最近 5 轮
        size_t max_archive_entries = 10;          // Archive 最多 10 个
        float summary_compression_ratio = 0.15f; // 摘要约为原文的 15%
    };

    explicit ContextCompressor(Config config, ILLMProvider* summarizer)
        : config_(config), summarizer_(summarizer) {}

    // 检查是否需要压缩
    bool should_compress(const nlohmann::json& ctx, size_t n_ctx) const {
        size_t token_count = estimate_token_count(ctx);
        size_t threshold = n_ctx * config_.compression_threshold_pct / 100;
        return token_count > threshold;
    }

    // 执行压缩
    CompressionResult compress(nlohmann::json& ctx, std::stop_token token) {
        CompressionResult result;

        // 1. 分离 recent 和 archive
        auto [recent, archive_candidates] = split_turns(ctx);

        // 2. 生成对话摘要
        std::string conv_summary = summarize_conversation(
            archive_candidates, token
        );

        // 3. 生成工具摘要
        std::vector<ToolResultSummary> tool_summaries;
        for (const auto& turn : archive_candidates) {
            for (const auto& tool : turn.tool_calls) {
                tool_summaries.push_back(summarize_tool_result(tool));
            }
        }

        // 4. 构建 ArchiveEntry
        ArchiveEntry entry;
        entry.turn_number = archive_candidates.front().turn_number;
        entry.conversation_summary = conv_summary;
        entry.tool_summaries = tool_summaries;
        entry.timestamp = std::chrono::steady_clock::now();

        // 5. 更新 Archive（丢弃最旧的如果超出上限）
        update_archive(ctx, entry);

        // 6. 保留 Recent Buffer
        ctx["recent_turns"] = serialize_recent(recent);

        // 7. 更新元数据
        ctx["_meta"]["is_compressed"] = true;
        ctx["_meta"]["original_turn_count"] = count_total_turns(ctx);
        ctx["_meta"]["compress_count"]++;

        result.entry = entry;
        result.new_token_count = estimate_token_count(ctx);
        result.compression_ratio = calculate_ratio(ctx, archive_candidates);

        return result;
    }

private:
    Config config_;
    ILLMProvider* summarizer_;  // 轻量 LLM 做摘要

    // 估算 token 数（简单实现：字符数 / 4）
    size_t estimate_token_count(const nlohmann::json& ctx) const {
        std::string serialized = ctx.dump();
        return serialized.size() / 4;
    }

    // 分离 recent buffer 和待压缩内容
    std::pair<std::vector<ContextTurn>, std::vector<ContextTurn>>
    split_turns(const nlohmann::json& ctx) const {
        std::vector<ContextTurn> all_turns = parse_turns(ctx);
        if (all_turns.size() <= config_.recent_buffer_turns) {
            return {{}, all_turns};
        }
        auto split_it = all_turns.end() - config_.recent_buffer_turns;
        return {
            {split_it, all_turns.end()},
            {all_turns.begin(), split_it}
        };
    }

    // 生成对话摘要
    std::string summarize_conversation(
        const std::vector<ContextTurn>& turns,
        std::stop_token token
    ) {
        std::string prompt = build_summary_prompt(turns);
        GenerationRequest req{
            prompt,
            {0.3f, 256, 0.95f}  // temperature, max_tokens, top_p
        };
        auto result = summarizer_->generate(req, token);
        return result->text;
    }

    // 更新 Archive
    void update_archive(nlohmann::json& ctx, const ArchiveEntry& entry) {
        auto& archive = ctx["archive"];
        archive.insert(archive.begin(), entry);  // 新条目在前面

        // 丢弃最旧的如果超出上限
        while (archive.size() > config_.max_archive_entries) {
            archive.erase(archive.begin());
        }
    }
};
```

### 6. DSLEngine 集成

```cpp
// ============================================================
// DSLEngine 集成点
// ============================================================

class DSLEngine {
public:
    void set_compressor(std::unique_ptr<ContextCompressor> compressor) {
        compressor_ = std::move(compressor);
    }

    void set_llm(std::unique_ptr<ILLMProvider> llm) {
        llm_ = std::move(llm);
        if (compressor_) {
            compressor_->set_summarizer(llm_.get());
        }
    }

    ExecutionResult step(std::stop_token token) {
        // 执行前检查压缩
        if (compressor_ && compressor_->should_compress(context_, llm_->context_window())) {
            auto result = compressor_->compress(context_, token);

            // 通过 EventBus 通知 TUI
            if (event_bus_) {
                event_bus_->push(UIEvent{
                    type = EventType::CONTEXT_COMPRESSED,
                    priority = EventPriority::Low,
                    payload = {
                        {"original_turns", result.entry.turn_number},
                        {"summary_turns", result.entry.conversation_summary.size()},
                        {"compression_ratio", result.compression_ratio}
                    }
                });
            }
        }

        // 正常执行...
    }

private:
    std::unique_ptr<ContextCompressor> compressor_;
    std::unique_ptr<ILLMProvider> llm_;
};
```

### 7. 摘要提示词模板

```cpp
// 摘要提示词
std::string build_summary_prompt(const std::vector<ContextTurn>& turns) {
    return R"(
请将以下对话历史压缩为简洁摘要。

要求：
1. 保留关键用户意图和任务目标
2. 保留重要决策和结论
3. 保留关键工具执行结果
4. 删除重复内容和中间过程
5. 摘要应约为原文的 15%

对话历史：
)" + serialize_turns(turns) + R"(

摘要：
)";
}

// 工具结果摘要
std::string summarize_tool_result(const ToolCall& tool) {
    return R"(
工具: )" + tool.name + R"(
输入: )" + truncate(tool.arguments.dump(), 200) + R"(
输出: )" + truncate(tool.result.value_or(""), 500) + R"(
)" ;
}
```

---

## 压缩流程图

```
┌─────────────────────────────────────────────────────────────┐
│  DSLEngine::step()                                           │
│                                                              │
│  1. 检查 should_compress()                                   │
│     ├─ token_count > n_ctx * 70%                             │
│     └─ return false → 继续执行                               │
│                                                              │
│  2. 执行 compress()                                           │
│     │                                                         │
│     ├─ split_turns()                                         │
│     │   ├─ recent: 最近 5 轮                                 │
│     │   └─ archive: 其余轮次                                 │
│     │                                                         │
│     ├─ summarize_conversation()                               │
│     │   └─ 调用轻量 LLM 生成摘要                             │
│     │                                                         │
│     ├─ summarize_tool_results()                               │
│     │   └─ 工具结果关键信息提取                               │
│     │                                                         │
│     └─ update_archive()                                       │
│         └─ 保留最新 10 个 ArchiveEntry                        │
│                                                              │
│  3. EventBus 推送 CONTEXT_COMPRESSED                         │
│     └─ TUI 显示 "上下文已压缩 (5.2x)"                         │
│                                                              │
│  4. 继续正常执行                                              │
└─────────────────────────────────────────────────────────────┘
```

---

## 权衡

### 为什么不是纯选择性保留？

| 方案 | 优点 | 缺点 |
|------|------|------|
| **纯选择性保留** | 保留特定重要记忆 | 需要额外分类器，可能丢失关键上下文 |
| **纯摘要** | 简单，完整压缩 | 可能丢失细节信息 |
| **混合方案 (推荐)** | 平衡近期完整性和历史压缩 | 略复杂 |

**选择混合方案的理由**：
- Recent Buffer 保留最近完整上下文，保证短期记忆
- Archive 压缩历史，节省空间
- 摘要损失可接受（历史对话的细节在长期记忆中价值低）

### 为什么用轻量 LLM 做摘要？

- 本地模型（Qwen-0.6B）延迟低，成本低
- 摘要任务相对简单，不需要 GPT-4o 级别能力
- Phase 1 无需网络调用

---

## Phase 1 vs Phase 2

### Phase 1（必须实现）

| 任务 | 描述 |
|------|------|
| 基础压缩 | Token 阈值触发 + LLM 摘要生成 |
| Recent Buffer | 最近 5 轮完整保留 |
| Archive | 压缩后历史摘要存储 |
| EventBus 通知 | TUI 显示压缩状态 |

### Phase 2（扩展功能）

| 任务 | 描述 | 优先级 |
|------|------|--------|
| 智能选择性保留 | 基于重要性评分的 selective retention | 🔴 高 |
| 向量检索 | Archive 转为向量存储，支持语义检索 | 🟡 中 |
| 分层记忆 | L1/L2/L3 完整分层 (MemGPT 风格) | 🟡 中 |
| 多模态压缩 | 图片、文件等非文本内容处理 | 🟢 低 |

---

## 实现要求

### Phase 1 必须完成

| # | 任务 | 验证方式 |
|---|------|---------|
| 1 | ContextCompressor 核心实现 | 单元测试：压缩后 token 数 < 阈值 |
| 2 | Recent Buffer 逻辑 | 测试：最近 5 轮完整保留 |
| 3 | Archive 管理 | 测试：超过 10 个条目时丢弃最旧的 |
| 4 | 摘要生成 | 集成测试：调用 LLM 生成有效摘要 |
| 5 | EventBus 集成 | 测试：压缩后 TUI 显示状态 |

### 测试用例

```cpp
TEST_CASE("ContextCompressor reduces token count") {
    ContextCompressor compressor(config, &mock_llm_);
    nlohmann::json ctx = create_large_context(10000);  // ~10000 tokens

    auto result = compressor.compress(ctx, {});

    CHECK(estimate_tokens(ctx) < 10000 * 0.5);  // 压缩后 < 50%
}

TEST_CASE("Recent turns are preserved") {
    ContextCompressor compressor(config, &mock_llm_);
    nlohmann::json ctx = create_context_with_turns(10);

    compressor.compress(ctx, {});

    CHECK(ctx["recent_turns"].size() == 5);  // 保留最近 5 轮
}

TEST_CASE("Archive respects max size") {
    ContextCompressor compressor(config, &mock_llm_);
    nlohmann::json ctx = create_context_with_turns(100);

    compressor.compress(ctx, {});

    CHECK(ctx["archive"].size() <= 10);  // 最多 10 个
}
```

---

## 影响范围

| 组件 | 变更 |
|------|------|
| `src/modules/context/context_compressor.h/cpp` | 新增 ContextCompressor 类 |
| `src/core/engine.h/cpp` | 集成压缩检查 |
| `src/common/llm/illm_provider.h` | 可能需要添加 `context_window()` 方法 |
| `src/harness/event_bus.h` | 添加 CONTEXT_COMPRESSED 事件 |

---

## 替代方案

### 替代 1：纯选择性保留（被否决）

**否决理由**：需要额外的重要性分类器，实现复杂，且可能遗漏关键上下文。

### 替代 2：向量检索替代压缩（被否决）

**否决理由**：Phase 1 过早优化。压缩是更简单直接的方案，向量检索 Phase 2 再考虑。

### 替代 3：固定窗口丢弃（被否决）

**否决理由**：直接丢弃早期对话会丢失关键上下文（任务目标、关键决策）。

---

## 结论

采用对话摘要 + 选择性保留混合方案：

- **触发条件**：Token 计数超过 n_ctx 的 70%
- **Recent Buffer**：最近 5 轮完整保留
- **Archive**：压缩后历史摘要，最多 10 个
- **摘要生成**：调用轻量 LLM（Qwen-0.6B）
- **EventBus 通知**：TUI 显示压缩状态

此设计支持：
- **Phase 1**：防止上下文窗口爆炸
- **Phase 2**：智能选择性保留 + 向量检索

---

*文档版本: v1.0*
*最后更新: 2026-05-12*