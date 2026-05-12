# llm 模块

**Generated:** 2026-05-11

## OVERVIEW
llama.cpp 封装，提供 LLM 生成能力。

## WHERE TO LOOK
| Task | Location |
|------|----------|
| 主适配器 | `llama_adapter.cpp/h` - generate() |
| LLM 工具封装 | `llama_tool.cpp/h` - ILLMTool 实现 |
| 参数结构 | `llm_tool.h` - LLMParams / LLMResult |

## CONFIG (LlamaAdapter::Config)
```cpp
struct Config {
    std::string model_path;
    int n_ctx = 2048;
    int n_threads = 4;
    float temperature = 0.7f;
    float min_p = 0.05f;
    int n_predict = 512;
};
```

## ANTI-PATTERNS
- 不要直接调用 llama C API，使用 LlamaAdapter
- 不要在 generate() 外持有 llama_token 向量（生命周期）