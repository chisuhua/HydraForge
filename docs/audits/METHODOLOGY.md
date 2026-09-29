# Audit Methodology（审计方法论 + Pitfall 速查）

> **目的**: 沉淀审计过程中已踩过的坑 + 标准化验证命令模板，避免下次重复犯错。
> **使用方式**: 任何新审计开始前先读 §0 模板；遇到 §1-§3 pitfalls 时按推荐做法替换命令。
> **沉淀时间**: 2026-09-29（首版基于 [`2026-09-29-harness-self-evolution-rsi-audit.md`](./2026-09-29-harness-self-evolution-rsi-audit.md) §10 审计元数据中 5 条方法论沉淀扩展）

---

## §0 标准化验证命令模板（5 步走）

任何"声称 ship / pass / implemented"的断言，按下面顺序跑一遍再下结论。

### 步骤 1 — 构建 + 测试基础设施

```bash
cd /workspace/project/HydraForge
cmake -S . -B build -DAGENTICDSL_BUILD_TESTS=ON -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)
```

**预期产出**: `find build -name "test_*" -type f -executable | wc -l` 应 = **98** 左右（每次小幅变更 ±5）

### 步骤 2 — 跑核心 ctest（按主题选择 regex）

```bash
# 自进化 / Harness / RSI 三方架构常覆盖的 12 个 test
ctest --test-dir build -R \
  "harness_rsi_pilot|genome_walk_ancestors|genome_registry|gepa_phase2|transition_guard|behavioral_regression|distillation_writer|skill_compiler|trajectory_ir|causal_ordering|causal_clock|credit_assignment" \
  --output-on-failure
```

**预期**: `100% tests passed, 0 tests failed out of 12`

### 步骤 3 — 跑 L2 evolution label ctest

```bash
# L2 reference example 用 l2-evolution label 标识
ctest --test-dir build -L l2-evolution -N   # 仅列出
ctest --test-dir build -L l2-evolution --output-on-failure  # 执行
```

**预期**: `Total Tests: 9`（含 hermetic_home / context_request_validation / evolution_tracer_schema / evolution_session_mutation / l2_event_emission / anti_cheat_* / reverse_indicators）

### 步骤 4 — 全量 ctest 规模枚举

```bash
ctest --test-dir build -N   # 仅枚举，不执行（沙箱下用此）
```

**预期输出**: `Total Tests: 266` 左右（每次 ship ±10）

**⚠️ 沙箱外**才执行 `ctest --test-dir build -j$(nproc) --output-on-failure`，沙箱性能受限下会超时。

### 步骤 5 — 项目级工具审计

```bash
python3 tools/adr_lint.py 2>&1 | tail -1
# 预期: "✓ 所有 ADR 通过 lint 检查"

python3 tools/docs_drift_audit.py 2>&1 | grep SUMMARY
# 预期: "SUMMARY: N DRIFT items, M WARNING items detected" (N < 10 为正常)
```

---

## §1 ADR 状态字段双格式（pifall #4 解决方案）

### 1.1 问题

ADR 文件的"主状态字段"有两种格式：
- **格式 A**（段头 + 内容）：`## 状态` 段头 + 下一段 `✅ Approved (...)` 行
  - 适用 ADR：ADR-0084 / ADR-0078 等较老的 ADR
- **格式 B**（内联一行）：`**状态**: ✅ **Approved (...)**` 一行内联
  - 适用 ADR：ADR-0086 / ADR-0088 等较新的 ADR（含 `**状态**:` 前缀行）

### 1.2 反模式

```bash
# ❌ 错误 1: 用 head 限制行数会漏掉格式 A 的段头+内容
head -50 docs/adr/adr-0084-mutation-governance-contract.md | grep "✅"
# 命中段头 "## 状态" 但可能不含 Approved 内容

# ❌ 错误 2: grep "✅ Approved" 会被 cross-reference 抢答
grep -m1 "✅ Approved" docs/adr/adr-0086-credit-assignment-contract.md
# 返回: "- ADR-0083 (✅ Approved + V2 Shipped) ..."  ← 错! 这是引用行, 不是主状态

# ❌ 错误 3: 用 ✅ Approved 简单空格匹配, markdown 加粗 `**Approved` 之间无空格
grep "✅ Approved" docs/adr/adr-0086-credit-assignment-contract.md
# 完全无命中（实际是 `✅ **Approved`）
```

### 1.3 正确做法（通用 awk）

```bash
# 通用提取主状态字段（覆盖两种格式 + 跳过 cross-reference）
for f in docs/adr/<adr-filename>.md; do
  awk 'BEGIN { found=0 }
       /^\*\*状态\*\*:/ { print; exit }                  # 格式 B: **状态**: ...
       /^## 状态/ { in_section=1; next }                # 格式 A: ## 状态 段头
       in_section && /✅/ && /Approved/ { print; exit }' "$f" | head -c 100
  echo
done
```

**实测输出**（per 2026-09-29）:
```
adr-0084-mutation-governance-contract.md: ✅ Approved (评审通过 2026-08-26 — V1 gate-and-audit 代码 ship, commit `a2b2d52`)
adr-0086-credit-assignment-contract.md: **状态**: ✅ **Approved (v1.1)** (2026-09-20 — Oracle dual-agent review `ses_...
adr-0088-h-d-m-transition-guard.md: **状态**: ✅ **Approved** (2026-09-20 — Phase 6c MetaRSI-v1 C3 部分 ship; OpenSpe...
adr-0078-finetune-base-model.md: ✅ Approved (Wave 3 Phase 1 Pilot 激活, 2026-09-23 — ...)
```

---

## §2 grep 陷阱（pifall #5 解决方案）

### 2.1 反模式 1：跨行内容匹配误报

```bash
# ❌ grep "✅ Approved" 会匹配 cross-reference 行
grep -l "✅ Approved" docs/adr/adr-*.md | xargs -I {} basename {}
# 返回: 0084 + 0086 + 0086-impl-scope + 0088 + 0078
#       其中 0086-impl-scope 的 ✅ Approved 在 quote 块里（描述"误报"），主状态实际是 🔍 Proposed
```

### 2.2 反模式 2：粗体 markdown 字符干扰

```bash
# ❌ "✅ Approved" 之间无空格（实际是 `✅ **Approved`）
grep -F "✅ Approved" docs/adr/adr-0086-credit-assignment-contract.md
# 命中: 跨引用行 + 主状态行都含 "✅ Approved" 但中间有 markdown 加粗
# 需用更宽松的 "✅.*Approved" 或 awk 模板（见 §1.3）
```

### 2.3 反模式 3：用 head -N 假设已知行号

```bash
# ❌ E1-E6 红线矩阵在 self-evolution SoT 行 445-450, head -50 完全错过
sed -n '/^## 十二/,/^## /p' docs/architecture/self-evolution-architecture-2026-08.md | head -50
# 命中: §十二 头部说明 + R8/R9 段头, 但 E1-E6 矩阵在 440-460 范围, head -50 切到 50 行刚好截断

# ✅ 改为已知行号 + 上下界
sed -n '440,460p' docs/architecture/self-evolution-architecture-2026-08.md
# 或用 grep -n 定位
grep -n "^| \*\*E[1-6]" docs/architecture/self-evolution-architecture-2026-08.md
```

### 2.4 解决方案：grep + sed 双阶段 + head -c 截断

```bash
# 标准模式：定位 + 提取
grep -n "PATTERN" file.md | head -3   # 1. 定位行号
sed -n 'LINENO_RANGE p' file.md      # 2. 按行号精确提取
```

---

## §3 路径自动发现（pifall #1-3 解决方案）

### 3.1 问题

按"语义猜测"路径会失败：
- `event_log.cpp` 实际在 `src/core/`，不在 `src/modules/trace/`
- `BehavioralRegressionGate` 实际在 `mutation_governor.h` 和 `behavioral_equivalence_evaluator.cpp`，不在 `gepa_loop.cpp`

### 3.2 正确做法：find + grep -rln

```bash
# 找任意 .cpp/.h 中的类 / 函数 / 符号
find src include -name "*.cpp" -o -name "*.h" | xargs grep -l "CLASS_NAME\|FUNCTION_NAME" 2>/dev/null

# 实例：找 EventLog 实装
find src -name "event_log*"
# 预期: src/core/event_log.cpp + src/core/event_log.h + src/core/types/event_log_config.h

# 实例：找 BehavioralRegressionGate 实装
grep -rln "BehavioralRegressionGate" src/ include/ 2>/dev/null
# 预期: src/common/governance/mutation_governor.h + src/modules/cognitive/behavioral_equivalence_evaluator.cpp + ...

# 实例：找 apply_harness_mutation 实装
grep -rln "apply_harness_mutation" include/ src/ 2>/dev/null | head -3
# 预期: include/agenticdsl/evolution/harness_rsi.h + src/evolution/CMakeLists.txt + src/evolution/harness_rsi.cpp
```

### 3.3 关键 API 路径速查（已 ship，2026-09-29 实测）

| 类 / 接口 | 头文件 | 源文件 |
|---------|--------|--------|
| `DSLEngine` | `include/core/engine.h` | `src/core/engine.cpp` |
| `IEvaluator` | `include/agenticdsl/contract/ievaluator.h` | `src/core/evaluator*.cpp` |
| `IGenomeRegistry` | `include/agenticdsl/genome/genome.h` | `src/core/genome/registry_filesystem.cpp` |
| `apply_harness_mutation` | `include/agenticdsl/evolution/harness_rsi.h` | `src/evolution/harness_rsi.cpp` |
| `EventLog` / `EventBuilder` | `include/core/event_log.h` | `src/core/event_log.cpp` |
| `BehavioralRegressionGate` | `include/agenticdsl/cognitive/behavioral_equivalence_evaluator.h` | `src/modules/cognitive/behavioral_equivalence_evaluator.cpp` |
| `IDistillationWriter` | `include/agenticdsl/contract/idistillation_writer.h` | `src/modules/distillation/file_writer.cpp` |
| `TrajectoryIR` | `include/agenticdsl/ir/trajectory_ir.h` | `src/modules/ir/trajectory_ir_backend.cpp` |

> **维护规则**: 任何新接口加入时，本表需追加一行（per AGENTS.md §工程层 — 公共 API 一旦定义必有路径表登记）。

---

## §4 文档计数实测规则（pifall #3 扩展）

任何文档里写 "98 binaries" / "Total Tests: 266" / "11 ship commit hashes" 等数字，必须用实测命令而非估算：

```bash
# test binary 总数
find build -name "test_*" -type f -executable | wc -l

# Total Tests
ctest --test-dir build -N 2>&1 | grep "Total Tests"

# ship commit hash 存在性
git log --oneline | grep "<hash>

# ADR 状态计数
python3 tools/adr_lint.py 2>&1 | grep "WARNING\[" | wc -l   # 不应持续增长
```

---

## §5 沙箱性能边界（强制声明）

任何"全量 ctest 零回归"声称：
- **沙箱内**: 不准声称。文档必须标 NOT-RUN（per AGENTS.md §"主会话补 ctest post-merge"）。
- **沙箱外**: 跑 `ctest --test-dir build -j$(nproc) --output-on-failure` 才能声称。

差异: 沙箱性能下 232 binary × 平均 50ms = 12+ 秒，本机实测 ≈ 88s。性能差距是 5-10×，所以 NOT-RUN 是诚实声明。

---

## §6 methodology 维护规则

| 触发 | 更新 |
|------|------|
| 任何 audit 总结出新的 pitfall | 追加到 §1-§5 对应章节 |
| 任何 §0 模板命令失效 | 更新命令 + 在 commit message 说明 |
| 任何新核心 API 加入 | 追加 §3.3 速查表一行 |
| 任何 ADR 头部格式变更 | 更新 §1 awk 模板 |

**版本追踪**: 本文档首版 2026-09-29，未来扩展按 §6 规则持续维护。