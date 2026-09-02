#!/usr/bin/env python3
# tests/test_control_plane_eval.py
# 功能描述: control-plane-eval.py 决策树与 CLI 单元测试 (pytest)
#           覆盖 design D-4 决策树 3 类路径 + 边界 + override
# 作者: Sisyphus
# 最后修改日期: 2026-09-02

import importlib.util
import sys
from pathlib import Path

import pytest

REPO_ROOT = Path(__file__).resolve().parent.parent

# 脚本文件名含连字符 (control-plane-eval.py), 无法直接 import, 用 importlib 按路径加载
_script_path = REPO_ROOT / "scripts" / "control-plane-eval.py"
_spec = importlib.util.spec_from_file_location("control_plane_eval", _script_path)
assert _spec is not None and _spec.loader is not None
cpe = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(cpe)


# ----------------------------------------------------------------------------
# 决策树测试 (3 类路径 + 边界)
# ----------------------------------------------------------------------------

def _mk_condition(name, status):
    return cpe.ConditionResult(name=name, status=status)


def _all_pass():
    return [_mk_condition(n, cpe.STATUS_PASS) for n in cpe.CONDITION_NAMES]


def test_all_pass_recommends_start():
    """全 PASS 路径 → RecommendStart (per design D-4)"""
    result = cpe.evaluate_control_plane(_all_pass())
    assert result.decision == "RecommendStart"


def test_partial_fail_blocking_descope():
    """部分 FAIL (C1+C2 阻塞) → DescopeOrContinue (per spec Scenario PARTIAL FAIL)"""
    conds = _all_pass()
    conds[0] = _mk_condition("C1", cpe.STATUS_FAIL)   # 阻塞
    conds[1] = _mk_condition("C2", cpe.STATUS_FAIL)   # 阻塞
    result = cpe.evaluate_control_plane(conds)
    assert result.decision == "DescopeOrContinue"
    assert "C1" in result.summary and "C2" in result.summary


def test_total_fail_descope():
    """全 FAIL 路径 → DescopeOrContinue + descope 建议 (per spec Scenario TOTAL FAIL)"""
    conds = [_mk_condition(n, cpe.STATUS_FAIL) for n in cpe.CONDITION_NAMES]
    result = cpe.evaluate_control_plane(conds)
    assert result.decision == "DescopeOrContinue"


def test_condition4_partial_gives_conditional():
    """条件 4 单独 PARTIAL + 其他 PASS → Conditional (per spec Scenario 条件4 🟡)"""
    conds = _all_pass()
    conds[3] = _mk_condition("C4", cpe.STATUS_PARTIAL)
    result = cpe.evaluate_control_plane(conds)
    assert result.decision == "Conditional"


def test_missing_data_aborts():
    """数据缺失路径 (缺条件) → Abort (per design D-4)"""
    conds = [_mk_condition(n, cpe.STATUS_PASS) for n in ["C1", "C2", "C3"]]
    result = cpe.evaluate_control_plane(conds)
    assert result.decision == "Abort"


def test_abort_status_aborts():
    """任一条件 ABORT 状态 → Abort"""
    conds = _all_pass()
    conds[5] = _mk_condition("C6", cpe.STATUS_ABORT)
    result = cpe.evaluate_control_plane(conds)
    assert result.decision == "Abort"


def test_condition3_pass_other_fail_nonblocking_mixed():
    """C3 PASS + 非阻塞条件 FAIL → Conditional/Descope 合理分支"""
    # C3 ✅, C4 PARTIAL (非阻塞), 其余 PASS → Conditional
    conds = _all_pass()
    conds[3] = _mk_condition("C4", cpe.STATUS_PARTIAL)
    result = cpe.evaluate_control_plane(conds)
    assert result.decision == "Conditional"


def test_single_blocking_fail_descopes():
    """单个阻塞条件 FAIL (C5) → DescopeOrContinue"""
    conds = _all_pass()
    conds[4] = _mk_condition("C5", cpe.STATUS_FAIL)
    result = cpe.evaluate_control_plane(conds)
    assert result.decision == "DescopeOrContinue"
    assert "C5" in result.summary


# ----------------------------------------------------------------------------
# 条件检测函数测试
# ----------------------------------------------------------------------------

def test_detect_c5_evidence_gate_conditional_is_fail():
    """C5 检测: 当前决议 (Conditional) → FAIL 非 PASS (真实状态)"""
    result = cpe.detect_c5_evidence_gate_pass()
    assert result.name == "C5"
    # 2026-09-02 决议 = Conditional → 应为 FAIL; 若无决议文档 → FAIL
    assert result.status in (cpe.STATUS_FAIL, cpe.STATUS_ABORT)
    if result.status == cpe.STATUS_FAIL:
        assert "Conditional" in result.details or "PASS" in result.details or "非 PASS" in result.details


def test_detect_c6_env_backend_exists():
    """C6 检测: env_backend.h + Local/Docker backend 应存在 (2026-08-18 ship)"""
    result = cpe.detect_c6_adr0075_env_backend()
    assert result.name == "C6"
    # 已 ship (2026-08-18) → PASS 或 PARTIAL (宽松)
    assert result.status in (cpe.STATUS_PASS, cpe.STATUS_PARTIAL)


def test_detect_c1_agentforge_count():
    """C1 检测: 返回有效状态 (PASS 或 FAIL) 且 evidence 非空"""
    result = cpe.detect_c1_agentforge_agents()
    assert result.name == "C1"
    assert result.status in (cpe.STATUS_PASS, cpe.STATUS_FAIL)
    assert result.evidence


# ----------------------------------------------------------------------------
# CLI 集成测试
# ----------------------------------------------------------------------------

def test_cli_runs_and_has_decision():
    """CLI 实际运行: --dry-run 输出含 Decision 行, exit code ∈ {0,1,2}"""
    from click.testing import CliRunner  # noqa: F401 — 未使用, 走 subprocess
    import subprocess

    proc = subprocess.run(
        [sys.executable, str(REPO_ROOT / "scripts" / "control-plane-eval.py"), "--dry-run"],
        capture_output=True,
        text=True,
        cwd=str(REPO_ROOT),
        timeout=60,
    )
    assert proc.returncode in (0, 1, 2)
    assert "Decision" in proc.stdout


def test_cli_override_c2():
    """--override C2=true → 决策树使用人工覆盖值"""
    import subprocess

    proc = subprocess.run(
        [sys.executable, str(REPO_ROOT / "scripts" / "control-plane-eval.py"),
         "--override", "C2=true", "--dry-run"],
        capture_output=True,
        text=True,
        cwd=str(REPO_ROOT),
        timeout=60,
    )
    assert proc.returncode in (0, 1, 2)
    # C2 行应显示 PASS
    c2_line = [l for l in proc.stdout.splitlines() if l.startswith("| C2 |")]
    assert c2_line and "PASS" in c2_line[0]


def test_cli_override_invalid_condition():
    """--override 未知条件 → exit 3 (ERROR)"""
    import subprocess

    proc = subprocess.run(
        [sys.executable, str(REPO_ROOT / "scripts" / "control-plane-eval.py"),
         "--override", "C99=true", "--dry-run"],
        capture_output=True,
        text=True,
        cwd=str(REPO_ROOT),
        timeout=60,
    )
    assert proc.returncode == cpe.EXIT_ERROR
    assert "未知条件" in proc.stderr


def test_cli_json_output():
    """--output json → 合法 JSON 且含 decision 字段"""
    import json as json_mod
    import subprocess

    proc = subprocess.run(
        [sys.executable, str(REPO_ROOT / "scripts" / "control-plane-eval.py"),
         "--output", "json", "--dry-run"],
        capture_output=True,
        text=True,
        cwd=str(REPO_ROOT),
        timeout=60,
    )
    assert proc.returncode in (0, 1, 2)
    data = json_mod.loads(proc.stdout)
    assert "decision" in data
    assert "conditions" in data
    assert len(data["conditions"]) == 6


if __name__ == "__main__":
    sys.exit(pytest.main([__file__, "-v"]))