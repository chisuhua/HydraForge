# SPEC-SSL-IR: SSL 三层 IR 格式规范 v1.0

**状态**: 已决 (2026-05-25)
**关联**: SPEC-SKILL-MD (SSL 在 SKILL.md 中的嵌入门), SPEC-COMPILED (编译产物使用)

---

## 1. 概述

SSL (Structured Skill Language) 是嵌入在 SKILL.md 文件中的三层 YAML 中间表示，作为编译器前端到后端的 IR。SSL 的作用是解耦自然语言创作与确定性编译。

## 2. 三层结构

```
SSL
├── scheduling       — 身份、意图、依赖（适合路由）
├── structural      — 阶段、指令、工具（适合代码生成）
└── logical         — 原子操作、风险（适合安全审计）
```

## 3. 完整 Schema

```yaml
ssl_version: "1.0"           # 必需。当前唯一合法值

scheduling:                  # 调度层 ─── 用于 route 节点
  name: "skill-name"         # 必需。技能唯一标识，应与文件名一致
  version: "1.0"             # 可选。语义化版本
  description: |             # 必需。用于 route 匹配的触发描述
    当用户请求生成日报时触发
  intent_signatures:         # 必需。至少 1 条
    - "当用户要求生成反馈日报时触发"
    - "当用户请求分析昨日评论时触发"
  tags:                      # 可选。3-5 个标签
    - feedback
    - report
    - automation
  dependencies:              # 必需。声明需要的外部能力
    - "tool:fs.read"
    - "tool:web_search"
    - "network:outbound"

structural:                  # 结构层 ─── 用于 Phase 4 直接映射
  stages:                    # 必需。至少 1 个 stage
    - id: 1                  # 必需。从 1 递增
      name: "fetch_data"     # 必需。snake_case
      type: "acquisition"    # 必需。取值见下方枚举
      instruction: |         # 必需。明确的阶段指令
        你只负责从 Reddit、App Store 抓取原始反馈。
        可用工具：fetch_reddit, fetch_appstore。
        不要分类，不要总结。
      tools:                 # 可选。该阶段可用的工具列表
        - "fetch_reddit"
        - "fetch_appstore"
      inputs:                # 可选。输入变量名
        - "date_range"
      outputs:               # 可选。输出变量名
        - "raw_feedback"
      needs_refs: false      # 可选。默认 false
      refs: []               # 可选。路径列表，相对于 SKILL.md 所在目录

logical:                     # 逻辑层 ─── 用于 Phase 3 安全审计
  operations:                # 必需。至少 1 个操作
    - action: "READ"         # 必需。取值见下方枚举
      resource: "web.reddit" # 必需。domain.entity 格式
      evidence: |            # 必需。原文中支持该操作的证据
        "从 Reddit 抓取用户反馈"
      risk_flag: null        # 可选。高风险标记
```

## 4. 字段约束

### 4.1 `structural.stages[].type` 枚举

| 类型 | 含义 | 典型温度 | 说明 |
|------|------|---------|------|
| `preparation` | 准备阶段 | 0.3 | 初始化、加载配置 |
| `acquisition` | 获取阶段 | 0.7 | 读取数据、抓取、查询 |
| `reasoning` | 推理阶段 | 0.3 | 分析、分类、评估、决策 |
| `action` | 执行阶段 | 0.7 | 生成、格式化、写入、推送 |
| `verification` | 验证阶段 | 0.1 | 检查、断言、审计 |
| `recovery` | 恢复阶段 | 0.5 | 错误处理、重试、兜底 |

### 4.2 `logical.operations[].action` 枚举（Closed Inventory）

| 动作 | 缩写 | 说明 | 典型 risk_flag |
|------|------|------|---------------|
| `MOVE` | MOV | 数据在系统内部迁移 | — |
| `CREATE` | CRT | 创建新资源/文件/记录 | — |
| `READ` | R | 读取已有资源 | — |
| `WRITE` | W | 修改已有资源 | — |
| `DELETE` | DEL | 删除资源 | `data_destruction` |
| `TRANSFORM` | TF | 数据格式/内容转换 | — |
| `TRANSFER` | TR | 跨网络边界传输 | `data_exfiltration` |
| `QUERY` | Q | 查询/检索 | — |
| `CONTROL` | CTL | 控制流操作 | `privilege_escalation` |

### 4.3 `risk_flag` 枚举

| 标记 | 含义 | 适用动作 |
|------|------|---------|
| `data_exfiltration` | 数据外传风险 | TRANSFER |
| `data_destruction` | 数据删除风险 | DELETE |
| `privilege_escalation` | 权限提升风险 | CONTROL |

## 5. 版本兼容

- `ssl_version: "1.0"` — 当前唯一合法版本
- 前向兼容规则：解析器不识别的新字段应被忽略（非丢弃）
- 后向兼容规则：1.x 版必须包含 `scheduling`, `structural`, `logical` 三个顶层键

## 6. 验证规则

| 规则 | 失败处理 |
|------|---------|
| `ssl_version` 必须是 "1.0" | 编译失败 |
| `scheduling.name` 不能为空 | 编译失败 |
| `structural.stages` 至少 1 个 | 编译失败 |
| `stages[].type` 必须在枚举内 | 编译失败 |
| `logical.operations` 至少 1 个 | 编译警告（无操作可审计） |
| `operations[].action` 必须在 Closed Inventory 内 | 编译警告（审计跳过未知动作） |
| `risk_flag` 声明但对应 action 不匹配 | 编译警告（可能的误标） |
