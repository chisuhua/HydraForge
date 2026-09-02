## 1. 评估脚本实施 (scripts/control-plane-eval.py)

- [x] 1.1 创建 `scripts/control-plane-eval.py` Python 3.11+ 一键评估脚本
- [x] 1.2 实现 6 项条件自动检测函数 `detect_conditions()`:
  - 条件 1: `count_agents_in_examples()` — 扫描 `examples/*/SKILL.md` 或 `.agent.md` 文件计数
  - 条件 2: `read_capacity_from_active_status()` — 解析 `docs/active-status.md` §四 capacity 字段（支持 `--override`）
  - 条件 3: `check_adr0068_appendix_a_shipped()` — git log 搜索 ADR-0068 amendment commit
  - 条件 4: `check_adr0073_d3_shipped()` — 检查 ADR-0073 D3 ship commit 或 openspec/changes/from-roadmap-phase-6c-schema-complete ship
  - 条件 5: `check_evidence_gate_pass()` — 检查 `docs/audits/<date>-evidence-gate-v1.md` 决议状态 = PASS
  - 条件 6: `check_adr0075_env_backend_shipped()` — 检查 ADR-0075 + LocalBackend/DockerBackend ship
- [x] 1.3 实现决策树函数 `evaluate_control_plane(conditions: dict) -> ControlPlaneStatus`
- [x] 1.4 实现 CLI 入口：`argparse` 接受 `--override "<cond>=<value>"` 多次 + `--output md|json` 格式 + `--dry-run`
- [x] 1.5 输出决策表 Markdown 格式 + JSON 格式（便于 CI 集成）

## 2. 决议文档模板创建

- [x] 2.1 创建 `docs/audits/2026-XX-XX-control-plane-eval-v1.md` 模板，4 章节固定结构
- [x] 2.2 §6 项条件状态章节：每项条件含 file:line 引用 + 自动检测结果
- [x] 2.3 §决策表章节：PASS / FAIL / CONDITIONAL / ABORT 4 选 1，含决策树推理路径
- [x] 2.4 §后续路径章节：RecommendStart / DescopeOrContinue / ContinuePriorShip 三档建议
- [x] 2.5 §决议章节：single-choice（启动 Phase 7a / descope / 继续前置 ship）

## 3. 单元测试 (tests/test_control_plane_eval.py)

- [x] 3.1 创建 `tests/test_control_plane_eval.py` pytest 测试文件
- [x] 3.2 全 PASS 路径测试：6 项条件全 ✅ → 决策 `RecommendStart`
- [x] 3.3 部分 FAIL 路径测试：条件 1+2 阻塞（FAIL）+ 条件 3+4+5+6 ✅ → 决策 `DescopeOrContinue`
- [x] 3.4 全 FAIL 路径测试：6 项全 ❌ → 决策 `DescopeOrContinue` + 触发 descope 建议
- [x] 3.5 条件 4 单独 🟡（D3 待 ship） → 决策 `Conditional` + 建议 ship C9 后再决议
- [x] 3.6 数据缺失路径：active-status.md 缺 capacity 字段 → 决策 `Abort` + 提示人工 override
- [x] 3.7 `--override` flag 测试：人工覆盖条件 2 → 脚本采纳 override 值

## 4. 集成测试 (3 类路径)

- [x] 4.1 全 PASS 路径：mock 6 项条件全 ✅ → 运行脚本 → 输出 `RecommendStart`
- [x] 4.2 部分 FAIL 路径：mock 条件 1+2 FAIL + 其他 ✅ → 输出 `DescopeOrContinue`
- [x] 4.3 全 FAIL 路径：mock 6 项全 FAIL → 输出 `DescopeOrContinue` + descope 建议列表

## 5. active-status.md 联动

- [x] 5.1 脚本入口验证 `docs/active-status.md` §四 Phase 7 启动条件项状态字段存在
- [x] 5.2 决议文档 ship 时同步更新 `docs/active-status.md` §四 6 项条件状态字段
- [x] 5.3 决议文档 + active-status.md 引用一致性 grep 验证（pre-commit hook）
- [x] 5.4 24h 时差限制：决议文档日期与 active-status.md 更新日期差 ≤24h

## 6. 架构合规性 + ctest 零回归

- [x] 6.1 grep 验证 `scripts/control-plane-eval.py` **不**修改任何 tracked 文件（除决议文档主动 ship）
- [x] 6.2 grep 验证脚本输出**仅**为建议，决议需 human review
- [ ] 6.3 `ctest --output-on-failure` 全量零回归（baseline 147/147 + 新增 `test_control_plane_eval` ≥7 case 全 PASS）
- [x] 6.4 脚本输出格式符合 CI 集成（exit code 0=PASS, 1=FAIL, 2=ABORT, 3=ERROR）

## 7. 文档 ship

- [x] 7.1 `scripts/control-plane-eval.py --help` 输出包含 6 项条件说明 + override flag 文档
- [x] 7.2 决议文档模板含 Markdown 渲染验证（CI `markdownlint` 通过）
- [x] 7.3 更新 `docs/active-status.md` §四 Phase 7 启动条件描述（引用评估脚本路径）

## 8. 阻塞 Phase 7 启动评估 (per proposal Acceptance)

- [x] 8.1 验证 `scripts/control-plane-eval.py` 可执行（`python3 scripts/control-plane-eval.py` exit 0/1/2）
- [x] 8.2 验证决议模板 + 决议文档 git-tracked
- [x] 8.3 验证 active-status.md 引用决议（≤ 24h 时差）
- [ ] 8.4 ctest 全量零回归
- [x] 8.5 阻塞 Phase 7a 启动评估（Phase 7 启动前必须运行评估脚本并产出决议文档）
