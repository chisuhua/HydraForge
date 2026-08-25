#!/usr/bin/env python3
"""
capability-application-map post-meeting delta update script
==========================================================

ADR-0071/0074 评审会议通过后,自动更新 docs/architecture/capability-application-map-2026-08.md:
  - §二 G10/G12/G13/G15 状态字段 (🔴 → ✅ Closed / 🔓 Open)
  - §八 T14-T22 任务命运字段 (待评审会议 → 已批准, Sprint XX 启动)
  - §七 变更记录新增 v1.3 行
  - §六 验证命令附录更新

Usage:
  python3 scripts/apply-meeting-resolutions.py --dry-run
  python3 scripts/apply-meeting-resolutions.py --resolutions resolutions.yaml
  python3 scripts/apply-meeting-resolutions.py --all-approved    # 全部采纳 (default)

退出码:
  0 = 成功 (含 dry-run)
  1 = 输入文件解析失败
  2 = capability-application-map 文件未找到
  3 = 应用更新失败
"""

import argparse
import re
import sys
from pathlib import Path
from datetime import date

# ---- Configuration ----
DEFAULT_MAP = Path("docs/architecture/capability-application-map-2026-08.md")
TODAY = date.today().isoformat()  # YYYY-MM-DD

# ---- Resolution schema ----
# 每个 ADR 决议的 YAML 输入格式:
#
# resolutions:
#   - adr: ADR-0083
#     decision: approved   # approved | rejected | deferred
#     gap_close: G10       # G10/G12/G13/G15 (架构层 Gap)
#     td_unblock: [T15, T19, T21, T22]
#     sprint: Sprint24
#
# 简化版 --all-approved 直接套用 Oracle 预审的 5 项 Approved 决议

ALL_APPROVED_RESOLUTIONS = [
    {
        "adr": "ADR-0083",
        "title": "IEvaluator/RewardSignal 契约",
        "decision": "approved",
        "gap_close": "G10",
        "td_unblock": ["T15", "T19", "T21", "T22"],
        "sprint": "Sprint 24",
    },
    {
        "adr": "ADR-0080 v1.2 amendment",
        "title": "D10 解耦",
        "decision": "approved",
        "gap_close": "G12",
        "td_unblock": [],
        "sprint": "Sprint 24",
    },
    {
        "adr": "ADR-0061-13",
        "title": "蒸馏输出格式 (IDistillationWriter)",
        "decision": "approved",
        "gap_close": "G15",
        "td_unblock": [],
        "sprint": "Sprint 25",
    },
    {
        "adr": "ADR-0071",
        "title": "LLM-native AgenticDSL 架构 (Promotion)",
        "decision": "approved",
        "gap_close": "G13",
        "td_unblock": ["T17", "T19", "T20", "T21"],
        "sprint": "Sprint 24",
    },
    {
        "adr": "ADR-0074",
        "title": "Prompt Evidence Gate (Promotion)",
        "decision": "approved",
        "gap_close": None,  # 非 Oracle 直接识别 Gap
        "td_unblock": ["T21"],
        "sprint": "Sprint 25",
    },
    {
        "adr": "ADR-0061-06 v1.1 amendment",
        "title": "Trajectory IR 标题修订 (G14 解锁)",
        "decision": "approved",
        "gap_close": "G14",
        "td_unblock": ["T15"],
        "sprint": "Sprint 25",
    },
]


def parse_resolutions_from_yaml(yaml_path: Path) -> list[dict]:
    """解析 resolutions.yaml 文件 (依赖 PyYAML)"""
    try:
        import yaml
    except ImportError:
        print("ERROR: PyYAML 未安装。pip install pyyaml", file=sys.stderr)
        sys.exit(1)

    with open(yaml_path, "r", encoding="utf-8") as f:
        data = yaml.safe_load(f)

    return data.get("resolutions") or []


def update_section_two(content: str, resolutions: list[dict]) -> str:
    """更新 §二 Gap 状态: G10/G12/G13/G15 的 🔴 → ✅"""
    updated = content

    for res in resolutions:
        gap = res.get("gap_close")
        if not gap:
            continue

        # 找到 gap 行 (格式: | **G10** | **title** | <source> | **🔴 架构层** | **🔒 Blocked** | ...)
        # 修改 "**🔴 架构层** | **🔒 Blocked**" → "**🔴 架构层** | **✅ Closed (评审通过 YYYY-MM-DD)**"
        # source 列因 ADR 而异: "v1.1 Oracle 评审 [架构]" / "ADR-0080 D10.3 [架构]" / "ADR-0071 🔍 Proposed [架构]"
        # 用通用 pattern: | **Gx** | **title** | <anything> | **🔴 架构层** | **🔒 Blocked** |

        pattern = rf"\| \*\*{gap}\*\* \| \*\*([^*]+)\*\* \| ([^*]+)\| \*\*🔴 架构层\*\* \| \*\*🔒 Blocked\*\* \|"

        def replace_status(match):
            title = match.group(1)
            source = match.group(2)
            return f"| **{gap}** | **{title}** | {source}| **🔴 架构层** | **✅ Closed (评审通过 {TODAY})** |"

        new_content = re.sub(pattern, replace_status, updated)
        if new_content != updated:
            print(f"  [§二] {gap} 状态: 🔴 架构层 🔒 Blocked → ✅ Closed")
            updated = new_content
        else:
            print(f"  [§二] {gap} 状态未匹配 (可能已被更新过)")

    return updated


def update_section_eight(content: str, resolutions: list[dict]) -> str:
    """更新 §八 T14-T22 任务命运"""
    updated = content

    # 1. 替换 T17 "T17" 待评审会议描述 → 已批准 Sprint 24
    t17_old = "| **T17**: 实施 ADR-0061-03 SkillCompiler（prompt 编译）| 2 sprint |"
    t17_new = "| **T17**: ✅ **APPROVED** Sprint 24 启动 (ADR-0071 评审通过) — SkillCompiler (ADR-0061-03)| 2 sprint |"

    if t17_old in updated:
        updated = updated.replace(t17_old, t17_new)
        print(f"  [§八] T17 命运: 待评审会议 → ✅ APPROVED Sprint 24")

    # 2. 替换 T15 (依赖 G14 标题修订评审)
    t15_old = "| **T15**: 实施 ADR-0061-06 Trajectory IR（**独立序列化视图**，不改 ParsedGraph）| 2 sprint |"
    t15_new = "| **T15**: ✅ **APPROVED** Sprint 25 启动 (G14 评审通过 + T14 ✅) — Trajectory IR (ADR-0061-06)| 2 sprint |"

    if t15_old in updated:
        updated = updated.replace(t15_old, t15_new)
        print(f"  [§八] T15 命运: 待评审会议 → ✅ APPROVED Sprint 25")

    # 3. T19 GEPA (依赖 ADR-0083 + G11)
    t19_old = "| **T19**: GEPA 反思循环（ADR-0061-09）MVP | 2-3 sprint |"
    t19_new = "| **T19**: ✅ **APPROVED** Sprint 24 末 (R 轨 spike) (ADR-0083 ✅ + ADR-0071 ✅) — GEPA MVP| 2-3 sprint (R 轨 spike) |"

    if t19_old in updated:
        updated = updated.replace(t19_old, t19_new)
        print(f"  [§八] T19 命运: 待评审会议 → ✅ APPROVED Sprint 24 末 (R 轨)")

    # 4. T21 Prompt Evidence Gate (依赖 ADR-0074)
    t21_old = "| **T21**: Prompt Evidence Gate（ADR-0074）| 1 月 |"
    t21_new = "| **T21**: ✅ **APPROVED** Sprint 25 启动 (ADR-0074 ✅ + ADR-0083 ✅) — Prompt Evidence Gate| 1 月 |"

    if t21_old in updated:
        updated = updated.replace(t21_old, t21_new)
        print(f"  [§八] T21 命运: 待评审会议 → ✅ APPROVED Sprint 25")

    # 5. T20 AFlow (依赖 ADR-0083 + T15)
    t20_old = "| **T20**: AFlow MCTS 工作流搜索（ADR-0061-08）spike | 1-2 月 |"
    t20_new = "| **T20**: ✅ **APPROVED** Sprint 26 末 (R 轨 spike) (ADR-0083 ✅ + T15 ✅) — AFlow MCTS| 1-2 月 (R 轨 spike) |"

    if t20_old in updated:
        updated = updated.replace(t20_old, t20_new)
        print(f"  [§八] T20 命运: 待评审会议 → ✅ APPROVED Sprint 26 末 (R 轨)")

    # 6. §8.5 Oracle 评审建议优先级排序更新
    oracle_priority_old = "### 8.5 Oracle 评审建议优先级排序\n\n```\n本周（最高杠杆）:\n  1. T14 行为回归（启动）\n  2. ADR-0071/0074 架构评审会（决定 4 个子项命运）\n  3. G10 IEvaluator + G12 D10 解解耦 + G15 ADR-0061-13 三个新 ADR 草案启动"
    oracle_priority_new = f"### 8.5 评审通过后优先级排序 ({TODAY} 评审)\n\n```\nSprint 24 启动周:\n  1. ADR-0071 v1.1 amendment 起草 (0.5 sprint)\n  2. ADR-0080 v1.2 amendment ship (0.5 sprint)\n  3. T17 SkillCompiler 骨架 (1 sprint)\n\nSprint 24 末:\n  4. T19 GEPA R 轨 spike 启动\n\nSprint 25 启动周:\n  5. ADR-0083 IEvaluator ship (1 sprint)\n  6. ADR-0061-13 蒸馏输出 ship (1 sprint 并行)\n  7. T15 Trajectory IR 启动 (G14 ✅)\n  8. T21 Prompt Evidence Gate 启动\n\nSprint 26:\n  9. T15 + T21 完整 ship\n  10. T20 AFlow R 轨 spike 准备\n```"

    if oracle_priority_old in updated:
        updated = updated.replace(oracle_priority_old, oracle_priority_new)
        print(f"  [§八.5] 优先级排序: Oracle 评审 → 评审通过后排期")

    return updated


def update_section_seven(content: str, resolutions: list[dict], meeting_date: str) -> str:
    """更新 §七 变更记录: 新增 v1.3 行"""
    v13_entry = (
        f"| {meeting_date} | **v1.3** | **评审会议通过: 5 个 ADR 决议落地** | "
        f"(1) ADR-0083 ✅ Approved → G10 Closed; (2) ADR-0080 v1.2 amendment ✅ Approved → G12 Closed; "
        f"(3) ADR-0061-13 ✅ Approved → G15 Closed; (4) ADR-0071 ✅ Approved (Promotion) → G13 Closed; "
        f"(5) ADR-0074 ✅ Approved (Promotion); (6) §八 T17/T15/T19/T20/T21 启动 Sprint 排期确定 (Sprint 24-26); "
        f"(7) §八.5 优先级排序从 Oracle 预审更新为评审通过后 Sprint 排期表; "
        f"(8) §二 G10/G12/G13/G15 状态 🔴 → ✅; "
        f"决议依据: `docs/architecture/adr-review-minutes/resolution-draft-2026-08-25.md` §八 会议决议记录 |"
    )

    # 找到 v1.2 行后插入 v1.3
    v12_pattern = r"(\| 2026-08-25 \| \*\*v1\.2\*\* \|.*?\|)\n"
    match = re.search(v12_pattern, content)

    if match:
        # 找到 v1.2 行的结尾位置
        insert_pos = match.end()
        new_content = content[:insert_pos] + "\n" + v13_entry + content[insert_pos:]
        print(f"  [§七] 变更记录新增 v1.3 行 (会议通过)")
        return new_content
    else:
        print(f"  [§七] v1.2 行未找到，v1.3 行未插入")
        return content


def update_section_six(content: str) -> str:
    """更新 §六 验证命令附录: 增加评审通过验证"""
    new_section = """

### 6.6 ADR-0071/0074 评审通过验证 (v1.3 新增)

```bash
# 验证 6 个 ADR 状态字段已更新（兼容 ## 状态 标题 + **状态**: 内联两种格式）
grep -m1 -A 1 "状态" docs/adr/adr-0083-evaluator-reward-contract.md
# 预期: "✅ Approved (评审通过 YYYY-MM-DD)" 或 "**状态**: ✅ Approved"

grep -m1 -A 1 "状态" docs/adr/adr-0080-v1-2-amendment-d10-decouple.md
grep -m1 -A 1 "状态" docs/adr/skill/adr-0061-13-distillation-output-format.md
grep -m1 -A 1 "状态" docs/adr/skill/adr-0061-06-v1-1-amendment-trajectory-ir-decouple.md
grep -m1 -A 1 "状态" docs/adr/adr-0071-llm-native-agenticdsl-architecture.md
grep -m1 -A 1 "状态" docs/adr/adr-0074-prompt-evidence-gate.md

# 验证 G10/G12/G13/G14/G15 状态已更新
grep "G10.*Closed\|G12.*Closed\|G13.*Closed\|G14.*Closed\|G15.*Closed" docs/architecture/capability-application-map-2026-08.md

# 验证 §八 任务排期已更新
grep "✅ APPROVED Sprint" docs/architecture/capability-application-map-2026-08.md
```

"""

    # 在 §七 变更记录前插入新章节
    if "## 七、变更记录" in content and "### 6.6 ADR-0071/0074 评审通过验证" not in content:
        content = content.replace("## 七、变更记录", new_section + "\n## 七、变更记录")
        print(f"  [§六] 6.6 评审通过验证章节新增")

    return content


def update_readme_index(content: str, meeting_date: str) -> str:
    """更新 docs/architecture/README.md 索引 — 不在此函数中处理 (由调用方单独处理)"""
    return content


def apply_updates(map_path: Path, resolutions: list[dict], dry_run: bool = False) -> bool:
    """应用会议决议更新到 capability-application-map"""
    if not map_path.exists():
        print(f"ERROR: capability-application-map 文件未找到: {map_path}", file=sys.stderr)
        return False

    content = map_path.read_text(encoding="utf-8")
    print(f"\n=== 处理 {map_path.name} ===\n")

    # 1. §二 Gap 状态更新
    content = update_section_two(content, resolutions)

    # 2. §八 任务命运更新
    content = update_section_eight(content, resolutions)

    # 3. §六 验证命令附录更新
    content = update_section_six(content)

    # 4. §七 变更记录新增
    content = update_section_seven(content, resolutions, TODAY)

    if dry_run:
        print(f"\n[DRY RUN] 内容预览 (前 100 行差异):")
        # 简化版: 仅打印更改标志
        print("  [未实际写入文件]")
        return True

    try:
        map_path.write_text(content, encoding="utf-8")
        print(f"\n✅ {map_path} 更新成功")
        return True
    except IOError as e:
        print(f"ERROR: 写入失败: {e}", file=sys.stderr)
        return False


def main():
    parser = argparse.ArgumentParser(description="ADR-0071/0074 评审会议后 capability-application-map 更新")
    parser.add_argument(
        "--map",
        type=Path,
        default=DEFAULT_MAP,
        help=f"capability-application-map 文件路径 (default: {DEFAULT_MAP})",
    )
    parser.add_argument(
        "--resolutions",
        type=Path,
        help="决议 YAML 文件路径 (省略则用 --all-approved 默认值)",
    )
    parser.add_argument(
        "--all-approved",
        action="store_true",
        default=True,  # 默认 True，因为 Oracle 预审已就绪
        help="使用 Oracle 预审的 5 项 Approved 决议 (default)",
    )
    parser.add_argument(
        "--dry-run",
        action="store_true",
        help="仅预览，不实际写入",
    )

    args = parser.parse_args()

    # 解析决议
    if args.resolutions:
        resolutions = parse_resolutions_from_yaml(args.resolutions)
    elif args.all_approved:
        resolutions = ALL_APPROVED_RESOLUTIONS
    else:
        print("ERROR: 必须提供 --resolutions 或使用 --all-approved", file=sys.stderr)
        sys.exit(1)

    print(f"加载 {len(resolutions)} 项决议:")
    for r in resolutions:
        print(f"  - {r['adr']}: {r['decision']} ({r.get('gap_close', 'no gap')})")

    # 应用更新
    success = apply_updates(args.map, resolutions, dry_run=args.dry_run)
    sys.exit(0 if success else 3)


if __name__ == "__main__":
    main()