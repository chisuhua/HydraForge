# ADR-0021: SKILL Compiler 架构

**状态**: 讨论中 (D7/D9 已决)
**关联决策点**: SR1 (SSL-as-IR), D7 (Phase 检测), D9 (格式兼容), D11 (验证策略), D12 (模块边界)

---

## 背景

编译器技能是本设计的核心——它将 **含 SSL 块的 SKILL.md** 编译为可执行的 AgenticDSL 计算图。编译器自身也是一个 Skill，可被自举编译。

## 架构概览

```
SKILL.md（含 SSL YAML 块）
    │
    ├─ scheduling: 元数据
    ├─ structural.stages: N 个阶段
    └─ logical.operations: M 个原子操作
    │
    ▼ P1-P2
SSL IR (确定性解析 / LLM 归一化)
    │
    ▼ P3
安全审计 ← audit-operations.py
    │
    ▼ P4
结构层直接映射 → phase 分段节点
    │
    ▼ P5
收尾 + 拼接 + validate-dsl.py 验证
    │
    ▼ P6
自举检测？→ 编译器自身时做自举优化
    │
    ▼ P7
发射：JIT (generate_subgraph) 或 AOT (fs.write + skill.register)
```

## 决定

### SR1: SSL-as-IR 作为编译器中 IR（2026-05-25）

**决策**: 采用 SSL (Structured Skill Language) 三层 YAML 作为编译器中间表示。

**三层结构**:
- `scheduling`: 身份、意图签名、标签、依赖
- `structural`: 阶段列表（type, instruction, tools, refs）
- `logical`: 原子操作清单（action, resource, risk_flag）

**依据**:
- 解耦 LLM 创作阶段与确定性编译阶段
- 编译管线中 P2/P4 可走零 LLM 的确定性路径
- SSL 块作为可审查的版本控制产物
- 自举时编译器编译自身无需 LLM 依赖

### D7: Phase 检测粒度（2026-05-25）

**决策**: 由 SSL `structural.stages` 直接提供 phase 定义，P4 做确定性模板映射。

**不采用**: 纯 LLM 从自然语言指令中提取 phase（旧设计中 P2 路径 B 的做法）。

**依据**:
- SSL 的 staging 信息已经结构化，无需 LLM 重新推理
- 遗留技能通过 transpile 命令一次性生成 SSL 块
- 映射规则定义在 `phase-codegen-patterns.md` 中

### D9: 编译/手写格式兼容（2026-05-25）

**决策**: Option A — 编译-创作分离。

**规则**:
- `enforce_ssl=true` 为编译器的默认行为
- 无 SSL 块的 SKILL.md 编译失败，提示使用 transpile 命令
- `transpile` 命令：LLM 归一化 SKILL.md 为 SKILL.md+SSL，写入原文件
- `enforce_ssl=false` 允许跳过检查（用于非生产或探索场景）
- 编译产物是标准 `.agent.md`，与手写产物格式完全一致

**依据**:
- 自举要求：编译器编译自身不能依赖 LLM
- 用户获得可审查的 SSL 产物
- 编译管线保持确定性

### D11: 验证策略（已决，2026-05-25）

**决策**: Option A — 交叉编译 + 结构验证。

**自举验证流程**:
1. 编译器编译自身 → 产物写入 `/lib/skills/compiled/skill-compiler@v1`
2. 用编译后的编译器编译 `/skills/daily-report/SKILL.md`（含 SSL 块）
3. `validate-dsl.py` 对产物做结构完整性检查：
   - `entry_point` 存在且有效
   - `/main/route`, `/main/load_body`, `/main/end` 存在
   - 至少 2 个 phase 分段节点
   - 所有 `next` 指针指向已注册节点
   - `fork/join` 配对
   - 命名空间合规
4. 所有检查通过 → 自举成功
5. 任何检查失败 → 自举失败，回滚到手写编译器

**选择依据**:
- P4 LLM 代码生成虽是非确定性的，但不影响节点拓扑（SSL stage 到 AgenticDSL 的映射是固定的）
- daily-report 的 SSL 块保证 P2 走确定性路径 A（无 LLM 归一化）
- P3 (audit-operations.py) + P5 (validate-dsl.py) 已经提供编译期验证
- 功能测试（实际执行 daily-report）对于 experimental 阶段是过度设计

### D12: 模块边界（已决，2026-05-25）

**决策**: Option B — 分散到现有模块，不建新模块。

**归属表**:

| 组件 | 模块 | 文件 |
|------|------|------|
| `CodeletNode` + `NodeType::CODELET_CALL` | `core/types/` | `node.h` |
| `execute_codelet()` | `executor/` | `node_executor.cpp` |
| `check_permissions()` privileged 增强 | `executor/` | `node_executor.cpp` |
| `codelet.run` 内置工具 | `common/tools/` | `registry.cpp` (register_default_tools) |
| `skill.register` + `registry.lookup` | `library/` | `library_loader.cpp` |
| `StandardLibraryLoader::scan_all()` | `library/` | `library_loader.cpp` |
| 编译器 DSL 逻辑 (SKILL.md + refs + scripts) | — | `/skills/skill-compiler/`（技能目录） |

**选择依据**:
- 各组件的依赖和技术栈不同：executor 节点、library 注册表、common 工具
- 不共享内部状态，不满足新建模块的"高内聚"标准
- 新模块与现有模块之间有循环依赖风险（executor ↔ compiler ↔ parser）
- C++ 层只做支撑，编译器的主逻辑在 DSL 层（SKILL.md）
- 如果以后真正需要 AST/IR/codegen，那时再提取 compiler 模块
