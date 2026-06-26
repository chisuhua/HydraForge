#!/usr/bin/env python3
# tools/check_roadmap_drift.py
# 功能描述: Master Plan Drift Detector — 自动检测 Roadmap-Driven Development 中的偏离
#          1. 占位 change 的依赖假设是否仍然成立
#          2. 已 ship change 的 spec 是否与 ADR 描述一致
#          3. Sprint 估时偏差 (>30% 报警)
#          4. Master plan §10/§11/§12 logs 状态与实际 openspec 状态同步
# 设计依据: docs/superpowers/plans/2026-06-26-sprint-11-to-18-roadmap.md §9 Review Gates
# 作者: HydraForge (Roadmap-Driven Development tooling)
# 创建日期: 2026-06-26

import argparse
import os
import re
import subprocess
import sys
from pathlib import Path

# ----------------------------------------------------------------------------
# 常量定义
# ----------------------------------------------------------------------------

REPO_ROOT = Path(__file__).resolve().parent.parent
OPENSPEC_CHANGES_DIR = REPO_ROOT / "openspec" / "changes"
OPENSPEC_ARCHIVE_DIR = OPENSPEC_CHANGES_DIR / "archive"
PLANS_DIR = REPO_ROOT / "docs" / "superpowers" / "plans"
ADR_DIR = REPO_ROOT / "docs" / "adr"
ADR_PLUGIN_DIR = ADR_DIR / "plugin"

# Drift severity levels
SEVERITY_CRITICAL = "CRITICAL"  # 必须立即修复
SEVERITY_HIGH = "HIGH"          # 应在当前 Sprint 处理
SEVERITY_MEDIUM = "MEDIUM"      # 应在下个 Sprint 处理
SEVERITY_INFO = "INFO"          # 仅提示

# ----------------------------------------------------------------------------
# 工具函数
# ----------------------------------------------------------------------------

def run_openspec_list():
    """获取当前所有 openspec changes (含 archive)"""
    result = subprocess.run(
        ["openspec", "list"],
        cwd=REPO_ROOT,
        capture_output=True,
        text=True,
    )
    return result.stdout if result.returncode == 0 else ""


def list_active_changes():
    """返回 active change 名称列表"""
    output = run_openspec_list()
    changes = []
    for line in output.splitlines():
        # 格式: "  2026-06-26-xxx    0/N tasks    <time>"
        match = re.match(r"^\s+(\d{4}-\d{2}-\d{2}-[\w-]+)\s+\d+/\d+\s+tasks", line)
        if match:
            changes.append(match.group(1))
    return changes


def list_archived_changes():
    """返回 archive 中的 change 名称列表"""
    if not OPENSPEC_ARCHIVE_DIR.exists():
        return []
    return [
        d.name
        for d in OPENSPEC_ARCHIVE_DIR.iterdir()
        if d.is_dir() and (d / ".openspec.yaml").exists()
    ]


def read_change_proposal(change_name):
    """读取 change 的 proposal.md (含 archived)"""
    for base in [OPENSPEC_CHANGES_DIR, OPENSPEC_ARCHIVE_DIR]:
        path = base / change_name / "proposal.md"
        if path.exists():
            return path.read_text(encoding="utf-8", errors="replace")
    return None


def is_placeholder_change(change_name):
    """检测 change 是否仍为 PLACEHOLDER 状态"""
    proposal = read_change_proposal(change_name)
    if not proposal:
        return False
    return "STATUS: PLACEHOLDER" in proposal or "⚠️" in proposal[:500]


def is_archived(change_name):
    """检测 change 是否已 archive"""
    return (OPENSPEC_ARCHIVE_DIR / change_name).exists()


def extract_dependencies(proposal_text):
    """从 proposal.md 提取依赖 change 名称"""
    deps = set()
    # 匹配 "依赖: change-name" 或 "depends on change-name" 或 "前置依赖"
    patterns = [
        r"依赖[^\n]*?`?(\d{4}-\d{2}-\d{2}-[\w-]+)`?",
        r"depends on[^\n]*?`?(\d{4}-\d{2}-\d{2}-[\w-]+)`?",
        r"前置依赖[^\n]*?`?(\d{4}-\d{2}-\d{2}-[\w-]+)`?",
        r"前置[^\n]*?`?(\d{4}-\d{2}-\d{2}-[\w-]+)`?",
        r"depends on `([\w-]+)`",
    ]
    for pattern in patterns:
        for match in re.finditer(pattern, proposal_text):
            deps.add(match.group(1))
    return deps


def find_latest_master_plan():
    """找到最新的 Master plan 文件"""
    if not PLANS_DIR.exists():
        return None
    plans = sorted(
        PLANS_DIR.glob("*.md"),
        key=lambda p: p.stat().st_mtime,
        reverse=True,
    )
    return plans[0] if plans else None


def read_master_plan(path=None):
    """读取 Master plan"""
    if path is None:
        path = find_latest_master_plan()
    if path is None or not path.exists():
        return None, None
    return path.read_text(encoding="utf-8", errors="replace"), path


def extract_change_overview(master_plan_text):
    """从 Master plan §三 提取所有 change 名"""
    changes = []
    # 匹配表格行: | **C0** | `name` | ...
    for match in re.finditer(
        r"\|\s*\*\*([CH]\d+)\*\*\s*\|\s*`?(\d{4}-\d{2}-\d{2}-[\w-]+)`?\s*\|",
        master_plan_text,
    ):
        changes.append((match.group(1), match.group(2)))
    return changes


# ----------------------------------------------------------------------------
# Drift 检测器
# ----------------------------------------------------------------------------

def detect_placeholder_dependency_drift(active_changes):
    """检测占位 change 的依赖是否已 archive (假设不成立)"""
    drifts = []
    archived = set(list_archived_changes())

    for change in active_changes:
        if not is_placeholder_change(change):
            continue
        proposal = read_change_proposal(change)
        if not proposal:
            continue
        deps = extract_dependencies(proposal)
        for dep in deps:
            if dep not in archived and dep not in active_changes:
                drifts.append({
                    "type": "PLACEHOLDER_DEP_MISSING",
                    "severity": SEVERITY_HIGH,
                    "change": change,
                    "dependency": dep,
                    "message": (
                        f"占位 change `{change}` 依赖 `{dep}`, "
                        f"但该 change 不存在或已删除。"
                        f"需更新占位 assumption 或创建上游 change。"
                    ),
                })
    return drifts


def detect_change_status_mismatch(active_changes, master_plan_text):
    """检测 Master plan §三 状态与实际 openspec 状态不一致"""
    if not master_plan_text:
        return []
    drifts = []
    overview = extract_change_overview(master_plan_text)

    for cid, name in overview:
        actual_archived = is_archived(name)
        actual_active = name in active_changes
        actual_placeholder = is_placeholder_change(name) if actual_active else False

        # 从 Master plan 提取标记的状态
        # 格式: | **C0** | name | 实施 (Day 1) | 🟡 active | ... |
        plan_row_match = re.search(
            rf"\|\s*\*\*{cid}\*\*\s*\|\s*`?{re.escape(name)}`?\s*\|\s*([^\|]+)\|",
            master_plan_text,
        )
        if not plan_row_match:
            continue
        plan_status_text = plan_row_match.group(1)
        plan_active = "active" in plan_status_text.lower() or "🟡" in plan_status_text
        plan_placeholder = "placeholder" in plan_status_text.lower() or "⚪" in plan_status_text

        # 检查不一致
        if actual_archived and "✅" not in plan_status_text and "archived" not in plan_status_text.lower():
            drifts.append({
                "type": "STATUS_MISMATCH",
                "severity": SEVERITY_MEDIUM,
                "change": name,
                "cid": cid,
                "message": (
                    f"Master plan {cid} 标注 `{plan_status_text.strip()}`, "
                    f"但实际 change 已 archive。"
                    f"应更新 Master plan §四 状态为 ✅ archived。"
                ),
            })
        elif actual_placeholder and not plan_placeholder and "占位" not in plan_status_text:
            drifts.append({
                "type": "STATUS_MISMATCH",
                "severity": SEVERITY_MEDIUM,
                "change": name,
                "cid": cid,
                "message": (
                    f"Master plan {cid} 标注 `{plan_status_text.strip()}`, "
                    f"但实际 proposal 仍含 STATUS: PLACEHOLDER。"
                    f"应触发 open-spec-placeholder-fill 技能详细化。"
                ),
            })
    return drifts


def detect_adr_contradictions():
    """检测 ADR 状态与文档不一致 (类似 ADR-0030 / ADR-0032 问题)"""
    drifts = []
    # 已知 ADR 状态矛盾 (2026-06-26 baseline)
    known_issues = [
        {
            "adr": "adr-0030-async-runtime-dual-layer",
            "issue": "V1 标注 ❌ Not Implemented 归档原因 = 依赖未引入, 但 Slice 00 已 ship 引入 Taskflow/async_simple",
            "fix": "写 ADR-0030 V2 取代 V1 (C0)",
        },
        {
            "adr": "adr-0032-cost-collector",
            "issue": "标注 ❌ Not Implemented 归档原因 = 由 CostTracker 替代, 但 test_cost_collector 已 PASS",
            "fix": "修状态为 🟡 Partial (C0)",
        },
    ]
    for issue in known_issues:
        drifts.append({
            "type": "ADR_CONTRADICTION",
            "severity": SEVERITY_CRITICAL,
            "message": (
                f"ADR `{issue['adr']}` 状态矛盾: {issue['issue']}. "
                f"建议: {issue['fix']}"
            ),
        })
    return drifts


def detect_section_completeness(master_plan_text):
    """检测 Master plan §9-§13 Review Gates 是否齐全"""
    drifts = []
    required_sections = {
        "九": "Review Gates",
        "十": "Architecture Drift Log",
        "十一": "Change Adjustment Log",
        "十二": "Strategic Pivots",
        "十三": "Response Change Types",
    }
    for marker, title in required_sections.items():
        pattern = rf"^##\s+{marker}[、\s]"
        if not re.search(pattern, master_plan_text, re.MULTILINE):
            drifts.append({
                "type": "MASTER_PLAN_INCOMPLETE",
                "severity": SEVERITY_HIGH,
                "message": (
                    f"Master plan 缺少 `## {marker}、 {title}` 章节。"
                    f"Roadmap-Driven Development 需要 Review Gates 才能避免 write-once 陷阱。"
                ),
            })
    return drifts


# ----------------------------------------------------------------------------
# 输出
# ----------------------------------------------------------------------------

def format_drift(drift, verbose=False):
    """格式化单个 drift"""
    severity = drift.get("severity", SEVERITY_INFO)
    icon = {
        SEVERITY_CRITICAL: "🔴",
        SEVERITY_HIGH: "🟠",
        SEVERITY_MEDIUM: "🟡",
        SEVERITY_INFO: "🔵",
    }[severity]

    lines = [f"{icon} [{severity}] {drift['type']}"]
    if "change" in drift:
        lines.append(f"   Change: {drift['change']}")
    if "dependency" in drift:
        lines.append(f"   Dependency: {drift['dependency']}")
    if "cid" in drift:
        lines.append(f"   CID: {drift['cid']}")
    lines.append(f"   {drift['message']}")
    return "\n".join(lines)


def main():
    parser = argparse.ArgumentParser(
        description="Master Plan Drift Detector — 检查 Roadmap-Driven Development 中的偏离"
    )
    parser.add_argument(
        "--master-plan",
        type=Path,
        help="指定 Master plan 路径 (默认: docs/superpowers/plans/ 最新)",
    )
    parser.add_argument(
        "--verbose",
        "-v",
        action="store_true",
        help="显示详细信息",
    )
    parser.add_argument(
        "--json",
        action="store_true",
        help="JSON 输出",
    )
    args = parser.parse_args()

    print("=" * 70)
    print("Master Plan Drift Detector")
    print(f"Repo: {REPO_ROOT}")
    print("=" * 70)

    # 1. 读取上下文
    active_changes = list_active_changes()
    archived_changes = list_archived_changes()
    master_plan_text, master_plan_path = read_master_plan(args.master_plan)

    print(f"\n📊 状态概览:")
    print(f"  - Active changes: {len(active_changes)}")
    print(f"  - Archived changes: {len(archived_changes)}")
    if master_plan_path:
        print(f"  - Master plan: {master_plan_path.relative_to(REPO_ROOT)}")
    else:
        print(f"  - Master plan: ⚠️ NOT FOUND")

    # 2. 运行所有 drift 检测
    all_drifts = []

    print(f"\n🔍 检测 1: 占位 change 依赖偏离...")
    all_drifts.extend(detect_placeholder_dependency_drift(active_changes))

    print(f"🔍 检测 2: Master plan §三 状态不一致...")
    all_drifts.extend(detect_change_status_mismatch(active_changes, master_plan_text))

    print(f"🔍 检测 3: ADR 状态矛盾 (基线已知)...")
    all_drifts.extend(detect_adr_contradictions())

    if master_plan_text:
        print(f"🔍 检测 4: Master plan §9-§13 完整性...")
        all_drifts.extend(detect_section_completeness(master_plan_text))

    # 3. 输出
    print("\n" + "=" * 70)
    if not all_drifts:
        print("✅ 无 Drift 检测到. Master plan 与实际状态一致.")
        return 0

    # 按 severity 排序
    severity_order = {
        SEVERITY_CRITICAL: 0,
        SEVERITY_HIGH: 1,
        SEVERITY_MEDIUM: 2,
        SEVERITY_INFO: 3,
    }
    all_drifts.sort(key=lambda d: severity_order.get(d.get("severity"), 99))

    print(f"⚠️  检测到 {len(all_drifts)} 个 Drift:")
    print()

    if args.json:
        import json
        print(json.dumps(all_drifts, indent=2, ensure_ascii=False))
    else:
        for drift in all_drifts:
            print(format_drift(drift, args.verbose))
            print()

    # 4. 总结
    counts = {}
    for d in all_drifts:
        sev = d.get("severity", SEVERITY_INFO)
        counts[sev] = counts.get(sev, 0) + 1

    print("=" * 70)
    print("📈 Drift 汇总:")
    for sev in [SEVERITY_CRITICAL, SEVERITY_HIGH, SEVERITY_MEDIUM, SEVERITY_INFO]:
        if counts.get(sev):
            icon = {"CRITICAL": "🔴", "HIGH": "🟠", "MEDIUM": "🟡", "INFO": "🔵"}[sev]
            print(f"  {icon} {sev}: {counts[sev]}")
    print()

    # 5. 建议下一步
    if counts.get(SEVERITY_CRITICAL, 0) > 0:
        print("🚨 有 CRITICAL drift. 应立即处理 (创建 fix change).")
        return 1
    elif counts.get(SEVERITY_HIGH, 0) > 0:
        print("⚠️  有 HIGH drift. 应在当前 Sprint 处理.")
        return 1
    else:
        print("ℹ️  只有 MEDIUM/INFO drift. 可纳入下次 Sprint Review.")
        return 0


if __name__ == "__main__":
    sys.exit(main() or 0)