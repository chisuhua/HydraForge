# ADR-0061-02: AgentAssay-style 行为回归套件

**日期**: 2026-07-16
**状态**: ✅ Approved (P0, 父 ADR-0061 拆分)
**父 ADR**: [../adr-0061-agent-evolution-and-solidification.md](../adr-0061-agent-evolution-and-solidification.md)

---

## 背景

**核心洞察**：Hasan 2025 (arXiv:2509.19185) 实证研究 39 frameworks × 439 apps，发现 **prompts（trigger）测试覆盖率仅 1%**——这是工业空白。HydraForge 应直接填补。

AgentAssay 2026 (arXiv:2603.02601) 提供：
- Token-efficient 三值 verdict (Pass/Fail/Inconclusive)
- 行为指纹（Hotelling T²）
- Adaptive budget
- Trace-first offline 模式
- 比 binary pass/fail **多 5-20× 效率**

## 决策

### 决策 1 — 三值 Verdict

| Verdict | 含义 | 行动 |
|---------|------|------|
| **Pass** | Hotelling T² < 阈值 | 接受升级 |
| **Fail** | Hotelling T² ≥ 阈值 | 阻断升级，回滚 |
| **Inconclusive** | 证据不足 | 增加测试用例或调整阈值 |

### 决策 2 — 行为指纹（Hotelling T²）

```cpp
// tests/behavioral_regression/skill_evolution_test.cpp
TEST_CASE("code-review SKILL vs .agent.md 行为等价") {
    // 1. N 个测试用例
    auto cases = load_test_cases("code-review/test_cases.json");
    
    // 2. SKILL 执行
    auto skill_results = run_all(cases, [&](auto& c) {
        return SkillInterpreter::run("skills/code-review/SKILL.md", c);
    });
    
    // 3. DSL 执行
    auto dsl_results = run_all(cases, [&](auto& c) {
        return DSLEngine::run(load("agents/code-review.agent.md"), c);
    });
    
    // 4. Hotelling T² 指纹对比
    auto fp_skill = compute_fingerprint(skill_results);
    auto fp_dsl = compute_fingerprint(dsl_results);
    
    REQUIRE(hotelling_t2(fp_skill, fp_dsl) < THRESHOLD);
}
```

### 决策 3 — 演化前后必跑的测试矩阵

| 演化阶段 | 测试用例 |
|---------|---------|
| SKILL.md → .agent.md | skill_results vs dsl_results |
| .agent.md → C++ | dsl_results vs cpp_results |
| C++ → Wasm | cpp_results vs wasm_results |
| .agent.md → 优化后 .agent.md | old_dsl_results vs new_dsl_results |

### 决策 4 — Adaptive Budget

```cpp
struct RegressionBudget {
    uint32_t max_tokens = 10000;          // 每次 regression 最多 token
    uint32_t adaptive_test_count = 50;    // 自动调整测试数量
    double confidence_threshold = 0.95;   // 置信度阈值
    uint32_t max_wallclock_ms = 60000;    // 最长耗时
};
```

### 决策 5 — 集成点

- Sprint 14 C5 `AgentAssert`：扩展为 behavioral fingerprint
- Sprint 19 cost-tracking decorator：控制 regression 测试 token 开销
- ExecutionSession TraceRecord：trace-first offline 模式

## 实施

- 文件: `tests/behavioral_regression/` (新目录)
- 核心: `include/agenticdsl/testing/behavioral_regression.h`
- 工作量: 2 weeks
- 优先级: P0

## 参考

- AgentAssay: arXiv:2603.02601
- Hasan 2025 (Prompts 1%): arXiv:2509.19185
- Sprint 14 C5 (AgentAssert)
- [ADR-0061-03-skill-compiler](./adr-0061-03-skill-compiler.md)