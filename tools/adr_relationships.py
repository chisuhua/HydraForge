#!/usr/bin/env python3
# tools/adr_relationships.py
# 功能描述: 自动生成 docs/adr-management/relationships.md
#          - 扫描 docs/adr/adr-NNNN-*.md + docs/adr/plugin/adr-NNNN-*.md
#          - 解析状态/日期/depends-on/supersedes 关系
#          - 输出 5 段 markdown: 状态总览 / Mermaid 依赖图 / 引用次数 / 状态统计 / 阶段映射
# 设计依据: project-organization Stage 5 / Task 27; 2026-06-16 扩展 plugin 子目录扫描
# 复用约定: 正则模式与 tools/adr_lint.py 保持一致（ADR_PATTERN / STATUS_PATTERN / 等）
# 作者: AgenticDSL Stage 5

import argparse
import re
import sys
from collections import defaultdict
from pathlib import Path


# ADR 子目录列表（与 docs/adr/ 根目录同级，存放同一编号空间的子集）
# 必须与 tools/adr_lint.py 的 ADR_SUBDIRS 保持一致
ADR_SUBDIRS = ["plugin"]


def _candidate_paths(adr_dir: Path) -> list[Path]:
    """返回 ADR 根目录下所有待扫描的 ADR 文件路径列表（根 + ADR_SUBDIRS 子目录）。"""
    paths: list[Path] = []
    if adr_dir.exists():
        paths.extend(sorted(adr_dir.glob("adr-*.md")))
        for sub in ADR_SUBDIRS:
            sub_dir = adr_dir / sub
            if sub_dir.exists() and sub_dir.is_dir():
                paths.extend(sorted(sub_dir.glob("adr-*.md")))
    return paths


# ----------------------------------------------------------------------------
# 正则模式（与 tools/adr_lint.py 保持一致，确保两个工具对 ADR 解析结果一致）
# ----------------------------------------------------------------------------

# ADR 文件名模式：adr-NNNN-name.md
ADR_PATTERN = re.compile(r"^adr-(\d{4})-.*\.md$")

# 状态行识别模式：匹配 emoji+英文 形式（容忍可选加粗 `**...**`）
# 例: **✅ Approved**、✅ Approved、**🟡 Partial (2026-06-08)**
STATUS_PATTERN = re.compile(
    r"\*{0,2}\s*"
    r"(?:✅\s*Approved|🟡\s*Partial|❌\s*Not\s*Implemented|⛔\s*Superseded|🔍\s*Proposed|📋\s*Reserved)"
    r"\s*\*{0,2}",
    re.IGNORECASE,
)

# depends-on 引用：匹配 "depends-on: ADR-NNNN"、"相关: ADR-NNNN"、"依赖: ADR-NNNN"
DEPENDS_ON_PATTERN = re.compile(
    r"(?:depends-on|相关|依赖)[:：]?\s*\[?[Aa][Dd][Rr]\s*-\s*(\d{4})\b\]?",
    re.IGNORECASE,
)

# supersedes 引用：明确替代/取代标记
SUPERSEDES_PATTERN = re.compile(
    r"(?:supersedes|被\s*替代|已替代)\s*[:：]?\s*\[?[Aa][Dd][Rr]\s*-\s*(\d{4})\b\]?"
    r"|(?:已被\s*\[?[Aa][Dd][Rr]\s*-\s*(\d{4})\b[^\n]{0,40}?替代)"
    r"|(?:\[?[Aa][Dd][Rr]\s*-\s*(\d{4})\b[^\n]{0,40}?已替代)",
    re.IGNORECASE,
)

# 状态行内日期：(YYYY-MM-DD)，通常在 emoji 状态后括号内
DATE_IN_STATUS = re.compile(r"\((\d{4}-\d{2}-\d{2})\)")

# 文件头部 frontmatter 日期（如 "Date: 2026-05-28"），作为状态行无日期时的回退
DATE_FRONT_MATTER = re.compile(r"(?:date|日期)\s*[:：]\s*(\d{4}-\d{2}-\d{2})", re.IGNORECASE)


# ----------------------------------------------------------------------------
# 解析函数
# ----------------------------------------------------------------------------

def parse_adr(path: Path) -> dict | None:
    """解析单个 ADR 文件，返回元数据字典；非 ADR 文件返回 None。"""
    m = ADR_PATTERN.match(path.name)
    if not m:
        return None
    number = m.group(1)

    text = path.read_text(encoding="utf-8")
    lines = text.splitlines()

    # 1. 标题：从首个 `# ` 一级标题取，否则用文件名 stem
    title = path.stem
    for line in lines:
        if line.startswith("# "):
            title = line[2:].strip()
            # 移除前缀 "ADR-NNNN: " 以保持简洁
            prefix = f"ADR-{number}:"
            if title.startswith(prefix):
                title = title[len(prefix):].strip()
            break

    # 2. 状态：定位 `## 状态` 章节，扫描后续非空行直到下一个二级标题
    status = "Unknown"
    in_status = False
    for line in lines:
        stripped = line.strip()
        if stripped.startswith("## 状态") or stripped.startswith("## Status"):
            in_status = True
            continue
        if in_status:
            # 遇到下一个二级标题则终止状态扫描
            if stripped.startswith("## "):
                break
            sm = STATUS_PATTERN.search(line)
            if sm:
                status = sm.group(0).strip().strip("*").strip()
                break

    # 3. 日期：优先取状态行括号内的日期；否则取文件 frontmatter 日期
    date = "Unknown"
    for line in lines:
        if STATUS_PATTERN.search(line):
            dm = DATE_IN_STATUS.search(line)
            if dm:
                date = dm.group(1)
                break
    if date == "Unknown":
        fm = DATE_FRONT_MATTER.search(text)
        if fm:
            date = fm.group(1)

    # 4. 跨文件引用：depends-on 与 supersedes
    depends_on: set[str] = set()
    for m2 in DEPENDS_ON_PATTERN.finditer(text):
        depends_on.add(m2.group(1))

    supersedes: set[str] = set()
    for m2 in SUPERSEDES_PATTERN.finditer(text):
        for g in m2.groups():
            if g:
                supersedes.add(g)

    return {
        "number": number,
        "path": path,
        "title": title,
        "status": status,
        "date": date,
        "depends_on": depends_on,
        "supersedes": supersedes,
    }


# ----------------------------------------------------------------------------
# 渲染函数
# ----------------------------------------------------------------------------

def render_markdown(adrs: list[dict]) -> str:
    """将解析后的 ADR 列表渲染为 markdown 文档（5 段结构）。"""
    adrs = sorted(adrs, key=lambda a: int(a["number"]))

    # 已有 ADR 编号集合（用于过滤悬空引用）
    known_numbers = {a["number"] for a in adrs}

    lines: list[str] = []
    lines.append("# ADR 关联性分析（自动生成）")
    lines.append("")
    lines.append(f"> 本文件由 `tools/adr_relationships.py` 自动生成，**请勿手动编辑**。")
    lines.append(f"> 任何手动修改会在下次运行时被覆盖。")
    lines.append(f"> 最后更新: 由 `tools/adr_relationships.py` 生成（运行时刻见 git commit 时间戳）")
    lines.append(f"> ADR 总数: {len(adrs)}")
    lines.append("")
    lines.append("---")
    lines.append("")

    # === 第一节：状态总览表 ===
    lines.append("## 一、状态总览")
    lines.append("")
    lines.append("| ADR | 议题 | 状态 | 日期 | 替代关系 |")
    lines.append("|-----|------|------|------|---------|")
    for a in adrs:
        supersedes_str = ""
        if a["supersedes"]:
            valid = sorted(s for s in a["supersedes"] if s in known_numbers)
            if valid:
                supersedes_str = f"替代 adr-{', adr-'.join(valid)}"
        lines.append(
            f"| adr-{a['number']} | {a['title']} | {a['status']} | {a['date']} | {supersedes_str} |"
        )
    lines.append("")
    lines.append("---")
    lines.append("")

    # === 第二节：依赖关系图（Mermaid） ===
    lines.append("## 二、依赖关系图")
    lines.append("")
    lines.append("```mermaid")
    lines.append("graph TD")

    # 节点：截断标题避免标签过长
    for a in adrs:
        short_title = a["title"][:40].replace('"', "'")
        lines.append(f"    adr_{a['number']}[\"adr-{a['number']}: {short_title}\"]")
    lines.append("")

    # 边：depends-on（实线箭头）
    dep_edges = 0
    for a in adrs:
        for dep in sorted(a["depends_on"]):
            if dep in known_numbers:
                lines.append(f"    adr_{a['number']} --> adr_{dep}")
                dep_edges += 1

    # 边：supersedes（虚线带标签）
    sup_edges = 0
    for a in adrs:
        for sup in sorted(a["supersedes"]):
            if sup in known_numbers:
                lines.append(f"    adr_{a['number']} -.->|supersedes| adr_{sup}")
                sup_edges += 1

    lines.append("```")
    lines.append("")
    lines.append(f"> 图中包含 {len(adrs)} 个节点、{dep_edges} 条依赖边、{sup_edges} 条替代边。")
    lines.append("> 渲染说明：实线 (`-->`) 表示依赖关系；虚线带标签 (`-.->|supersedes|`) 表示替代关系。")
    lines.append("")
    lines.append("---")
    lines.append("")

    # === 第三节：被引用次数（反向索引：谁引用了谁） ===
    lines.append("## 三、被引用次数（被引用方 ← 引用方）")
    lines.append("")
    refs: dict[str, list[str]] = defaultdict(list)
    for a in adrs:
        for dep in a["depends_on"]:
            if dep in known_numbers:
                refs[dep].append(f"adr-{a['number']} (depends-on)")
        for sup in a["supersedes"]:
            if sup in known_numbers:
                refs[sup].append(f"adr-{a['number']} (supersedes)")

    if refs:
        lines.append("| 被引用 ADR | 引用方 |")
        lines.append("|------------|--------|")
        for ref_num in sorted(refs, key=lambda x: int(x)):
            ref_by = ", ".join(refs[ref_num])
            lines.append(f"| adr-{ref_num} | {ref_by} |")
    else:
        lines.append("（无依赖或替代关系）")
    lines.append("")
    lines.append("---")
    lines.append("")

    # === 第四节：按状态统计 ===
    lines.append("## 四、按状态统计")
    lines.append("")
    # 规范化状态标签到 6 个标准状态之一
    canonical_order = [
        ("✅ Approved", "Approved"),
        ("🟡 Partial", "Partial"),
        ("❌ Not Implemented", "Not Implemented"),
        ("⛔ Superseded", "Superseded"),
        ("🔍 Proposed", "Proposed"),
        ("📋 Reserved", "Reserved"),
    ]
    status_counts: dict[str, int] = {label: 0 for label, _ in canonical_order}
    status_counts["❓ Unknown"] = 0
    for a in adrs:
        matched = False
        for label, keyword in canonical_order:
            if keyword in a["status"]:
                status_counts[label] += 1
                matched = True
                break
        if not matched:
            status_counts["❓ Unknown"] += 1

    lines.append("| 状态 | 数量 |")
    lines.append("|------|------|")
    for label, _ in canonical_order:
        if status_counts[label] > 0:
            lines.append(f"| {label} | {status_counts[label]} |")
    if status_counts["❓ Unknown"] > 0:
        lines.append(f"| ❓ Unknown | {status_counts['❓ Unknown']} |")
    lines.append("")
    lines.append("---")
    lines.append("")

    # === 第五节：阶段映射（历史视角） ===
    lines.append("## 五、按阶段分类（历史视角）")
    lines.append("")
    lines.append("> ADR 编号反映历史阶段分类（参见 `.omo/plans/project-organization.md`）：")
    lines.append(">")
    lines.append("> - 0001-0009: 基础设施层（ILLMProvider/EventBus/DSLEngine/ToolRegistry/Context 等）")
    lines.append("> - 0010-0014: 记忆系统（已大部分归档到 `docs/archive/adr/`）")
    lines.append("> - 0015-0018: 推理引擎（已大部分归档）")
    lines.append("> - 0019-0023: 智能体层（InteractionBus/ThreadModel/PDK/Plugin/ToolResult）")
    lines.append("> - 0024-0028: 预留范围（无 ADR）")
    lines.append("> - 0029+ 0030-0036: 异步/策略/路由/内核（大部分已归档）")
    lines.append(">")
    lines.append("> 当前活动 ADR 主要集中在 **0001-0009（基础）** 与 **0019-0033（智能体+策略）** 范围。")
    lines.append("> 13 个已废弃 ADR 已归档到 `docs/archive/adr/`（参见 2026-06-12 Stage 2 / Task 7）。")
    lines.append("")

    return "\n".join(lines) + "\n"


# ----------------------------------------------------------------------------
# 入口
# ----------------------------------------------------------------------------

def main() -> int:
    parser = argparse.ArgumentParser(
        description="自动生成 docs/adr-management/relationships.md — 扫描 ADR 文件并渲染关联性分析"
    )
    parser.add_argument(
        "adr_dir", nargs="?", default="docs/adr",
        help="ADR 目录路径（默认: docs/adr）",
    )
    parser.add_argument(
        "-o", "--output", default="docs/adr-management/relationships.md",
        help="输出文件路径（默认: docs/adr-management/relationships.md）",
    )
    parser.add_argument(
        "--check", action="store_true",
        help="检查模式：若输出文件与生成结果不一致则退出 1（CI 集成用）",
    )
    args = parser.parse_args()

    adr_dir = Path(args.adr_dir)
    if not adr_dir.exists():
        print(f"ERROR: 目录不存在: {adr_dir}", file=sys.stderr)
        return 2
    if not adr_dir.is_dir():
        print(f"ERROR: 不是目录: {adr_dir}", file=sys.stderr)
        return 2

    # 扫描并解析所有 ADR
    adrs: list[dict] = []
    for path in _candidate_paths(adr_dir):
        parsed = parse_adr(path)
        if parsed:
            adrs.append(parsed)

    if not adrs:
        print(f"ERROR: 在 {adr_dir} 中未找到任何 ADR 文件", file=sys.stderr)
        return 2

    output = render_markdown(adrs)

    if args.check:
        out_path = Path(args.output)
        if out_path.exists() and out_path.read_text(encoding="utf-8") == output:
            print(f"✓ {args.output} 已最新（{len(adrs)} 个 ADR）")
            return 0
        print(f"✗ {args.output} 已过期 — 请运行: python3 tools/adr_relationships.py")
        return 1

    # 写入模式
    out_path = Path(args.output)
    out_path.write_text(output, encoding="utf-8")
    print(f"✓ 已写入 {args.output}（{len(adrs)} 个 ADR，{len(output)} 字节）")
    return 0


if __name__ == "__main__":
    sys.exit(main())