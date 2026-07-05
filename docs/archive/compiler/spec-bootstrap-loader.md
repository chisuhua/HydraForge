# SPEC-BOOTSTRAP: Bootstrap Loader DAG 规范

**状态**: 已决 (2026-05-25)
**关联**: ADR-0022, SSL-as-IR, SPEC-COMPILED

---

## 1. 概述

Bootstrap Loader 是用户进入技能系统的入口 DAG。它处理注册表中的 4 种技能类型，按路由 → 分类 → 执行/编译的流程处理用户输入。

### 技能类型处理矩阵

| 类型 | 注册表标记 | 处理方式 | 执行路径 |
|------|-----------|---------|---------|
| 手写 .agent.md | `type: native` | 全图解析，直接执行 | `/main/execute_compiled` |
| AOT 编译产物 | `type: compiled` | 全图解析，直接执行 | `/main/execute_compiled` |
| SKILL.md + SSL | `type: source, has_ssl: true` | Path A: 确定性 SSL 解析 + 编译 + JIT | `/main/compile_p2a` |
| SKILL.md - SSL | `type: source, has_ssl: false` | Path B: LLM 归一化 + 编译 + JIT | `/main/compile_p2b` |

## 2. DAG 结构

```
/main/start → /main/init
    ↓
/main/route           ← dsl_call: 匹配用户输入到 registry
    ↓
/main/check_match     ← assert: matched_skill_name != NONE
    ↓                     on_failure → /main/fallback
/main/lookup_entry    ← tool_call: registry.lookup → skill_entry
    ↓
/main/classify_skill  ← codelet_call: 分类技能类型 (native/compiled/source)
    ↓
/main/route_by_class  ← assert: type 分支
    ├─ native/compiled → /main/execute_compiled
    │                      → fs.read(.agent.md) → generate_subgraph 注入 → 执行
    │
    ├─ source + has_ssl → /main/compile_p2a
    │                      → generate_subgraph 调用编译器 (Path A) → JIT → 执行
    │
    └─ source + no SSL → /main/check_enforce_ssl
                           → enforce_ssl=false? /main/compile_p2b (Path B) → JIT
                           → enforce_ssl=true? /main/error_no_ssl
```

## 3. 关键节点设计

### 3.1 `/main/route` — 路由节点

- **类型**: `dsl_call`
- **温度**: 0.1 (低随机性)
- **max_tokens**: 256
- **prompt_template** 显示注册表中每个技能的:
  - `name`
  - `description`
  - `type` (native/compiled/source)
  - `tags` (如有)
  - `priority`
- 输出: `working.data.matched_skill_name`

### 3.2 `/main/classify_skill` — 技能分类

- **类型**: `codelet_call` (Python)
- **权限**: `privileged:codelet.run`
- **输入**: `skill_entry` (从 registry.lookup 获取的完整条目 JSON)
- **输出 JSON**:
  ```json
  {
    "type": "native" | "compiled" | "source",
    "has_ssl": true | false,
    "path": "/skills/xxx/SKILL.md"
  }
  ```

### 3.3 `/main/execute_compiled` — 直接执行已编译技能

- 对 `native` 和 `compiled` 类型通用
- 使用 `fs.read` 读取 .agent.md 文件
- 使用 `generate_subgraph` 注入到 `/dynamic/loaded/` 命名空间
- `next: "/dynamic/loaded/{name}/start"` 跳转到入口

### 3.4 `/main/compile_p2a` 和 `/main/compile_p2b` — 编译执行

- 使用 `generate_subgraph` 调用编译器技能 (`/lib/skills/skill-compiler@v1`)
- 编译器输出的 JIT 产物在 `/dynamic/compiled/` 命名空间
- `next: "/dynamic/compiled/{name}/start"` 跳转到编译产物入口
- P2A (has_ssl=true): 传给编译器 `enforce_ssl=true` → 走确定性解析
- P2B (has_ssl=false): 传给编译器 `enforce_ssl=false` → 走 LLM 归一化

## 4. C++ 引擎接口

```cpp
// 最小 C++ 层——初始化 + 执行 bootstrap DAG
auto engine = DSLEngine::from_file("bootstrap.agent.md");

// 注册基础设施工具
engine->register_tool("fs.read", ...);
engine->register_tool("registry.lookup", [](const auto& args) -> json {
    auto& loader = StandardLibraryLoader::instance();
    auto entry = loader.lookup(args.at("name"));
    return entry.to_json();
});

// 注册特权工具
engine->register_privileged_tool("codelet.run", codelet_handler);
engine->register_privileged_tool("skill.register", register_handler);

// 初始化注册表
StandardLibraryLoader::instance().scan_all();
// 扫描路径: lib/ + lib/skills/compiled/ + skills/

// 执行
auto result = engine->run(Context{{"user_query", user_input}});
```

## 5. Registry 条目 Schema

```json
{
  "name": "daily-report",
  "description": "生成用户反馈日报",
  "type": "source",
  "has_ssl": true,
  "ssl_version": "1.0",
  "path": "/skills/daily-report/SKILL.md",
  "tags": ["feedback", "report", "daily"],
  "priority": 0,
  "stages": [
    {"name": "fetch", "tools": ["fetch_reddit"]}
  ]
}
```

| 字段 | 类型 | 必填 | 说明 |
|------|------|------|------|
| `name` | string | ✅ | 技能唯一标识 |
| `description` | string | ✅ | 路由匹配用描述 |
| `type` | "native"\|"compiled"\|"source" | ✅ | 技能来源类型 |
| `has_ssl` | boolean | 仅 source | SSL 块是否存在 |
| `ssl_version` | string | 仅 has_ssl=true | SSL 格式版本 |
| `path` | string | ✅ | 文件路径 |
| `tags` | string[] | 否 | 辅助路由标签 |
| `priority` | int | ✅ | 0=手写, 50=编译, 100=自举 |
| `stages` | object[] | 否 | SSL stages 摘要（路由可参考） |

## 6. 边界情况

| 场景 | 行为 |
|------|------|
| 无匹配技能 | `/main/fallback` → LLM 通用回答 |
| 匹配技能文件不存在 | `fs.read` 错误 → `/main/fallback` |
| 编译失败（SSL 无效） | 编译器返回 `error_ssl_invalid` → 错误信息透传给用户 |
| enforce_ssl=true + 无 SSL | `/main/error_no_ssl` → 提示 transpile 命令 |
| 编译产物执行超预算 | 编译器产物自身有 `execution_budget` 声明 |
| 循环编译（递归） | 编译器不能编译自己后再编译自己——由编译器内部的 `config.lock` 防止 |
