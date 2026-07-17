# ADR-0061-03: SkillCompiler 实施

**日期**: 2026-07-16
**状态**: ✅ Approved (P0, 父 ADR-0061 拆分)
**父 ADR**: [../adr-0061-agent-evolution-and-solidification.md](../adr-0061-agent-evolution-and-solidification.md)

---

## 背景

`docs/proposals/skill-system/04-skill-compiler-design.md` 已经定义了完整的 SkillCompiler 设计（SectionParser → AxisClassifier → TemplateEngine → NodeGen → DAGBuilder）。本 ADR 负责具体实施。

## 决策

### 决策 1 — 5 轴 TemplateEngine

复用现有 `04-skill-compiler-design.md` 的 5 轴模板：

| 轴 | 模板类型 | 适用 Skill |
|----|---------|-----------|
| 1 流程/方法论 | 顺序流水线 + 分支循环 | brainstorming, TDD |
| 2 领域/工具 | 工具调用序列 | cmake, git_master |
| 3 审查/质量 | Fork-Join 并行 | review_work |
| 4 UI/前端 | LLM 生成流水线 | frontend_ui_ux |
| 5 项目专用 | 工具命令 | openspec_* |

### 决策 2 — 编译器组件

```cpp
class SkillCompiler {
public:
    // 1. 解析章节
    std::vector<Section> parse_sections(const std::string& skill_path);
    
    // 2. 按 frontmatter.category 判断轴
    Axis classify_axis(const SkillMetadata& metadata);
    
    // 3. 按轴选模板 + 变量填充
    Template select_template(Axis axis);
    std::string fill_template(const Template& t, const Section& body);
    
    // 4. 编译为 .agent.md
    nlohmann::json compile(const std::string& skill_path);
    
    // 5. 注册到 SkillRegistry
    void compile_and_register(const std::string& skill_path);
};
```

### 决策 3 — 编译原则

| 原则 | 实现 |
|------|------|
| 模板驱动 | 每种轴有专用 DAG 模板，变量填充 |
| 确定性输出 | 同 SKILL.md 每次结果一致 |
| 增量验证 | 每步骤独立验证 |
| 可逆元数据 | 编译结果保留 `source_skill` 路径 |

### 决策 4 — SkillRegistry 集成

```cpp
class SkillRegistry {
public:
    void compile_and_register(const std::string& skill_path);
    void load_all_from_directory(const std::string& dir);
    
    // Agent 运行时热更新
    void reload_skill(const std::string& name);
};
```

### 决策 5 — 7 步实施路径

| Step | 内容 | 产出 |
|------|------|------|
| 1 | 实现 SectionParser | `src/common/skills/section_parser.h` |
| 2 | 流程轴模板 + NodeGen | 10+ 流程技能可编译 |
| 3 | 领域轴模板 + NodeGen | 11+ 领域技能可编译 |
| 4 | 审查轴 Fork-Join 模板 | 6+ 审查技能可编译 |
| 5 | UI + 项目轴模板 | 6+ 技能可编译 |
| 6 | SkillRegistry.compile_and_register 集成 | 启动时自动编译 |
| 7 | 验证覆盖率 > 90% | 与手写版本对比 |

## 实施

- 文件: `src/common/skills/skill_compiler.{h,cpp}`
- 测试: `tests/test_skill_compiler.cpp`
- 工作量: 3 weeks
- 优先级: P0

## 参考

- `docs/proposals/skill-system/04-skill-compiler-design.md`
- [ADR-0061-01-skill-std](./adr-0061-01-skill-std.md)
- [ADR-0061-02-behavioral-regression](./adr-0061-02-behavioral-regression.md)