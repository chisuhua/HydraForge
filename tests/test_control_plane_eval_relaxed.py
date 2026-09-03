#!/usr/bin/env python3
# tests/test_control_plane_eval_relaxed.py
# control-plane-eval.py --relaxed 模式 5 类测试 (Sprint 25 Change #2)
# per Oracle session ses_f9ab25dcfffetx4J5UFA7JYBKV P0 建议 + roadmap.md Q2b 决策树放松口径

import importlib.util
import subprocess
import sys
from pathlib import Path

import pytest

REPO_ROOT = Path(__file__).resolve().parent.parent
_script_path = REPO_ROOT / "scripts" / "control-plane-eval.py"
_spec = importlib.util.spec_from_file_location("cpe_relaxed", _script_path)
assert _spec is not None and _spec.loader is not None
cpe = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(cpe)


def _mk(name, status):
    return cpe.ConditionResult(name=name, status=status)


def _all_pass():
    return [_mk(n, cpe.STATUS_PASS) for n in cpe.CONDITION_NAMES]


def _with_c2_fail():
    conds = _all_pass()
    conds[1] = _mk("C2", cpe.STATUS_FAIL)
    return conds


def test_evaluate_relaxed_c2_fail_excluded_from_blocking():
    """--relaxed 模式下 C2 FAIL 不再触发 DescopeOrContinue (新行为)"""
    conds = _with_c2_fail()
    result = cpe.evaluate_control_plane(conds, relaxed=True)
    # C2 不在 blocking 集合, C3 PASS 应触发 Conditional
    assert result.decision == "Conditional"
    assert "(relaxed mode: not blocking)" in conds[1].details


def test_evaluate_default_c2_fail_still_blocking():
    """默认模式 C2 FAIL 仍触发 DescopeOrContinue (行为保持)"""
    conds = _with_c2_fail()
    result = cpe.evaluate_control_plane(conds, relaxed=False)
    assert result.decision == "DescopeOrContinue"


def test_evaluate_relaxed_c1_still_blocking():
    """--relaxed 模式不能绕过 C1 阻塞 (边界)"""
    conds = _all_pass()
    conds[0] = _mk("C1", cpe.STATUS_FAIL)  # C1 阻塞
    conds[1] = _mk("C2", cpe.STATUS_FAIL)  # C2 也 FAIL
    result = cpe.evaluate_control_plane(conds, relaxed=True)
    # C1 仍阻塞 → DescopeOrContinue
    assert result.decision == "DescopeOrContinue"


def test_evaluate_relaxed_c2_pass_no_effect():
    """--relaxed 模式 C2 PASS 时无影响 (新行为)"""
    conds = _all_pass()  # C2 也是 PASS
    result_default = cpe.evaluate_control_plane(conds, relaxed=False)
    result_relaxed = cpe.evaluate_control_plane(conds, relaxed=True)
    # C2 PASS 时行为一致
    assert result_default.decision == result_relaxed.decision == "RecommendStart"


def test_cli_relaxed_c2_excluded_from_blocking_summary():
    """CLI --relaxed 验证: C2 不在 blocking summary 中 (per Oracle 修正 false-positive)"""
    # --override 仅 C2 detector 消费 (per scripts/control-plane-eval.py L527)
    # 真实仓库环境 C1/C5 真实 FAIL, 因此 Decision 仍为 DescopeOrContinue
    # 但 relaxed 应保证 C2 不在 blocking summary 中 (核心语义)
    proc = subprocess.run(
        [sys.executable, str(REPO_ROOT / "scripts" / "control-plane-eval.py"),
         "--override", "C2=false",  # C2 FAIL via override
         "--relaxed", "--dry-run"],
        capture_output=True, text=True, cwd=str(REPO_ROOT), timeout=60,
    )
    # C2 行 details 含 relaxed marker (诚实标记)
    c2_line = [l for l in proc.stdout.splitlines() if l.startswith("| C2 |")]
    assert c2_line and "(relaxed mode: not blocking)" in c2_line[0]
    # C2 不在 blocking summary 中 (核心 relaxed 语义)
    # Decision 行应不包含 "C2" 在 blocking 列表里
    decision_line = [l for l in proc.stdout.splitlines() if l.startswith("- **Decision**:")]
    assert decision_line
    # relaxed 下 C2 被排除, blocking 应为 C1/C5 (或 C1 或 C5 真实 FAIL)
    summary_line = [l for l in proc.stdout.splitlines() if l.startswith("- **Summary**:")]
    assert summary_line
    summary_text = summary_line[0]
    if "FAIL" in summary_text and "阻塞条件" in summary_text:
        # 提取 C 列表, 验证 C2 不在其中
        import re
        blocking_cs = re.findall(r"\bC\d+\b", summary_text.split("阻塞条件")[1].split(" FAIL")[0])
        assert "C2" not in blocking_cs, f"C2 应在 relaxed 模式下被排除, 但 blocking list={blocking_cs}"
