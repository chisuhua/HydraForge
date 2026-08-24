# docs_drift_audit.py 测试夹具

## 用途

回归测试 fixture，用于验证 `tools/docs_drift_audit.py::scan_scenario7()` 在 defect-truth-table 文档与代码真相产生漂移时能正确检测。

## 维护方法

每个 Sprint 收官时：

1. 在 defect-truth-table 文档版本号升级后（如 v1.1.1 → v1.1.2），将旧版本快照保存为新 fixture
2. 命名格式：`defect-truth-table-v{X}.{Y}.{Z}.md`（不含月份日期，因为 fixture 代表文档版本而非日期）
3. 测试命令：

```bash
python3 -c "
from pathlib import Path
import sys; sys.path.insert(0, 'tools')
from docs_drift_audit import scan_scenario7
root = Path('.')
fixture = Path('tools/tests/fixtures/defect-truth-table-v1.1.1.md')
findings = scan_scenario7(root, fixture_path=fixture)
drifts = [f for f in findings if f['severity'] == 'DRIFT']
print(f'Fixture v1.1.1: {len(drifts)} drifts')
assert len(drifts) == 12, f'expected 12 drifts, got {len(drifts)}'
"
```

## 当前 fixture

| 文件 | 对应 commit | 期望 drift 数 | 说明 |
|------|-------------|--------------|------|
| `defect-truth-table-v1.1.1.md` | `e1422c8` (docs/adr-0073 Wave-1 followup P1 closure) | 12 | v1.1.1 文档状态，共12 处漂移对应修复 v1.1.2 修订项 |

## 扩展指南

新增 fixture 时：

1. `git show <commit>:docs/architecture/defect-truth-table-<date>.md > tools/tests/fixtures/defect-truth-table-v<X>.<Y>.<Z>.md`
2. 在上表添加新行
3. 在 `tools/docs_drift_audit.py::scan_scenario7` 文档注释中更新期望 drift 数
4. 验证：`scan_scenario7(root, fixture_path=fixture)` 输出符合预期

## 设计原则

- Fixture 不可修改（commit 后冻结）—— 代表"过去某时刻的文档真相"
- 每次 v1.X.Y 版本升级前快照旧版本
- 不保留月份日期在文件名中（避免与原文档的 `defect-truth-table-2026-08.md` 命名冲突）