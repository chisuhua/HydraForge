# ADR-0065: 多语言 PDK 支持（仅 Python → Wasm）

## 状态

✅ Approved (2026-07-16, 架构评审确认 + 调整为 Python 单一语言)
**原计划** C++/Rust 多语言 SDK → **调整为** 仅 C++ → Wasm + Python → Wasm（Rust 不做）

## 领域

Agent-as-Plugin 架构 / 多语言生态

## 关联

- [ADR-0021 — PDK Design](../adr-0021-pdk-design.md) — C++ PDK 基础
- [ADR-0056 — WebAssembly Agent 运行时](./adr-0056-wasm-runtime.md) — Wasm runtime
- [ADR-0061-05 — C++ → Wasm Toolchain](./skill/adr-0061-05-cpp-wasm-toolchain.md) — C++ → Wasm 工具链
- [ADR-0061-12 — WebLLM 集成](./skill/adr-0061-12-webllm.md) — 浏览器端集成
- [ADR-0062 — Agent Marketplace](./adr-0062-agent-marketplace.md) — 包格式
- [ADR-0064 — PDK Conformance Test Suite](./adr-0064-pdk-conformance-test-suite.md) — 合规测试
- Pyodide: pyodide.org
- componentize-py: github.com/dylanh666/componentize-py

## 背景

### 问题

PDK Plugin 当前仅支持 C++（`DEFINE_AGENT` + `DECLARE_TOOL`），生态受限：

- **Python 开发者**（数据科学 / ML 主流）难以为 HydraForge 写 Plugin
- **C++ 是 HydraForge 的"必要不充分"条件**——C ABI 仍是 C++ 唯一接口
- Marketplace 生态受限（ADR-0062）

### 范围调整

**原计划**：C++ / Rust / Python / Go 多语言 SDK（覆盖高性能 + 易用性）
**调整为**：仅 **C++ → Wasm** + **Python → Wasm**（Rust 不做）

**理由**：
- C++ → Wasm 已在 ADR-0061-05 详细讨论（wasi-sdk 工具链）
- Python → Wasm 是新路径，覆盖 Python 生态（数据科学 / ML / 脚本）
- Wasm 是统一中间表示（与 ADR-0056 WasmRuntime 一致）
- Rust SDK 需求弱（性能用户用 C++，易用用户用 Python）
- 资源聚焦：先验证 C++ → Wasm + Python → Wasm 两条路径

### 目标

让 Python 开发者能为 HydraForge 写 Agent Plugin，无需安装 Python 运行时（运行时只需 Wasm）。

## 决策

### 决策 1 — 单一路径：Python → Wasm

**C++ → Wasm** 路径：
- 详见 ADR-0061-05（wasi-sdk + WasmEdge）
- 不重复定义

**Python → Wasm** 路径（本 ADR 新增）：
- 用 Pyodide 工具链（生态成熟）
- 运行时只需 WasmRuntime，**不需用户安装 Python**

### 决策 2 — 两条 Python → Wasm 工具链选型

| 工具链 | 输出 | 优缺点 | 推荐 |
|--------|------|--------|:----:|
| **componentize-py** | WASI component | 符合 WASI component model 标准；较新；生态不广 | Phase 2 |
| **pyodide-build** | Pyodide Wasm | Pyodide 生态成熟（NumPy/Pandas/Matplotlib/SciPy）；bundle 较大 | **v1** |

**v1 推荐**：pyodide-build（生态成熟、风险低）
**Phase 2 探索**：componentize-py（更标准的未来）

### 决策 3 — Python PDK SDK 设计

```python
# hydraforge_pdk Python package
from hydraforge_pdk import tool, agent, PluginInfo, ToolMetadata
from hydraforge_pdk.runtime import call_tool, emit_event, consume_budget

@tool(
    name="code_review/run",
    description="审查代码",
    input_schema={
        "type": "object",
        "properties": {
            "code": {"type": "string"},
            "language": {"type": "string", "enum": ["cpp", "python", "rust"]},
            "severity": {"type": "string", "default": "medium"}
        },
        "required": ["code", "language"]
    },
    output_schema={
        "type": "object",
        "properties": {
            "issues": {"type": "array", "items": {"type": "object"}},
            "summary": {"type": "string"}
        }
    },
    category="axis3-review",
    capabilities=["code_review", "static_analysis"],
    requires_isolation=True,
    timeout_ms=30000,
    budget_limit_usd=0.05
)
def review_code(code: str, language: str, severity: str = "medium") -> dict:
    """审查代码并返回 issues"""
    # 通过 host function 调用其他 Agent
    similar_issues = call_tool("kb/query", {"query": code[:100]})
    
    # 业务逻辑（Python 生态优势：numpy/pandas）
    issues = analyze_with_pandas(code, language, severity, similar_issues)
    
    # emit 事件供其他 Agent 订阅
    emit_event("code_review.completed", {"count": len(issues)})
    consume_budget(0.005)
    
    return {"issues": issues, "summary": f"Found {len(issues)} issues"}
```

### 决策 4 — 构建流程

```
src/my_agent.py
  ↓ pip install hydraforge-pdk + pyodide-build
  ↓
build/my_agent-0.2.0.wasm       # Pyodide Wasm bundle (~10-30MB)
  ↓ hydraforge packager
  ↓
dist/my_agent-0.2.0.hfpkg      # Marketplace 包（含 .wasm）
```

**详细命令**：
```bash
# 开发者本地构建
pyodide build \
  --exports pyodide \
  --output build/my_agent.wasm \
  src/my_agent.py

# HydraForge packager
hf packager build \
  --plugin=build/my_agent.wasm \
  --manifest=manifest.json \
  --output=dist/my_agent-0.2.0.hfpkg

# Marketplace 上传
hf marketplace upload dist/my_agent-0.2.0.hfpkg
```

### 决策 5 — Python Wasm 与 OS 服务的集成

**Host functions 白名单**（与 ADR-0056 Wasm runtime 共享）：

```python
# Python runtime 提供的内置模块
from hydraforge_pdk.runtime import (
    call_tool,       # host_call_tool
    emit_event,      # host_emit_event  
    consume_budget,  # host_consume_budget
    log,             # host_log
    read_context,    # host_read_context
)
```

**Host function 签名**（来自 ADR-0056）：

| Host function | Python 等价 | 用途 |
|--------------|------------|------|
| `host_call_tool(name, args)` | `call_tool(name, args)` | 调用其他 Agent |
| `host_emit_event(topic, payload)` | `emit_event(topic, payload)` | 事件推送 |
| `host_consume_budget(amount)` | `consume_budget(amount)` | 预算消耗 |
| `host_log(level, message)` | `log(level, message)` | 日志 |
| `host_read_context(key)` | `read_context(key)` | 上下文读取 |

### 决策 6 — Marketplace 包格式

```
my_agent-0.2.0.hfpkg  (tar.gz 重命名)
├── manifest.json
├── wasm/
│   └── my_agent.wasm        # Pyodide 编译产物
├── config/default.json
├── tests/test_*.py          # Python 测试（用于 build-time 验证）
├── LICENSE
└── CHANGELOG.md
```

**包大小估算**：
- Pyodide runtime: ~10MB
- Python 解释器 + 标准库: ~5MB
- 用户代码 + 依赖: ~1-5MB
- 总计: ~15-20MB

**优化**：v2 引入 lazy import + tree-shaking。

### 决策 7 — 与 Conformance Test Suite 集成

ADR-0064 的 Level 2/3 测试自动适用于 Python Wasm Plugin：
- Level 2: `load .wasm` → `invoke("register_tools")` → 测试工具调用
- Level 3: 行为指纹对比（基于 ADR-0061-02）

## 替代方案

### 方案 A：扩展 C ABI 支持 Rust

**否决理由**：
- 用户决定不做 Rust
- C++ → Wasm 已覆盖高性能场景

### 方案 B：用 CPython 直接打包（不用 Wasm）

**否决理由**：
- 用户需装 Python 运行时（违背"运行时只需 Wasm"原则）
- 跨平台分发困难
- 安全性不如 Wasm 沙箱

### 方案 C：Pyodide vs componentize-py 早期决策 componentize-py

**否决理由**：
- componentize-py 较新，生态不成熟
- Pyodide 已经支持大量科学计算包
- v2 探索 componentize-py

## 不变量

- Python Wasm Plugin 必须通过 Pyodide 标准构建
- host functions 调用与 ADR-0056 共享白名单
- `requires_isolation` 在 Pyodide Wasm 中自动为 true（沙箱强制）
- Marketplace 包格式与 ADR-0062 `.hfpkg` 一致

## 权衡

| 决策 | 选择 | 理由 |
|------|------|------|
| 工具链 | Pyodide (v1) | 生态成熟、风险低 |
| 标准 | WASI component | 未来兼容 |
| 包大小 | ~15-20MB | Pyodide runtime 必须 |
| 多语言 | 不支持 | 用户明确决定 |
| 性能 | 中（解释执行） | 适合脚本任务，不适合热路径 |

## 后续行动

- 实现 `hydraforge_pdk` Python package（pip 包）
- 集成 Pyodide 构建到 HydraForge CI（ADR-0061-05 类似）
- ADR-0064 Conformance Level 3 集成 Python Wasm
- Phase 2: componentize-py 探索
- 文档：`docs/guides/python-pdk-guide.md`

## 参考

- ADR-0021 / 0056 / 0061-05 / 0061-12 / 0062 / 0064
- Pyodide: https://pyodide.org/
- componentize-py: https://github.com/dylanh666/componentize-py
- WASI Component Model: https://github.com/WebAssembly/WASI/blob/main/Proposals.md
- Pyodide-build: https://pyodide.org/en/stable/development/building-and-testing-packages.html