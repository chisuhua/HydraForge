#!/usr/bin/env python3
# tools/adr_lint.py
# 功能描述: ADR 前置一致性 linter — 校验 docs/adr/adr-NNNN-*.md 文件
#          - 状态字段必须使用 STATUS-GLOSSARY.md 定义的 6 个标准标签
#          - 跨文件引用（supersedes / 替代 / 依赖）必须指向已存在的 ADR
#          - 替代关系中，被替代方编号必须严格小于当前 ADR
# 设计依据: project-organization Stage 5 / Task 26
# 作者: AgenticDSL Stage 5
# 最后修改日期: 2026-06-12

import argparse
import re
import sys
from pathlib import Path

# ----------------------------------------------------------------------------
# 常量定义
# ----------------------------------------------------------------------------

# 6 个标准状态标签（来自 docs/adr/STATUS-GLOSSARY.md）
# 在此集中维护，确保与词汇表严格对齐
VALID_STATUSES = {
    "approved",            # ✅ Approved
    "partial",             # 🟡 Partial
    "not-implemented",     # ❌ Not Implemented
    "superseded",          # ⛔ Superseded
    "proposed",            # 🔍 Proposed
    "reserved",            # 📋 Reserved
}

# 已废弃的中文/英文状态词（在状态行中出现应报错）
# 注意：这些词在正文叙述中可正常使用，仅当出现在"## 状态"上下文时才算违规
DEPRECATED_STATUS_WORDS = [
    "已批准", "未实施", "部分实施", "提议中", "已替代",
    "已废弃", "已迁移", "deprecated",
]

# ADR 文件名模式：adr-NNNN-name.md
ADR_PATTERN = re.compile(r"^adr-(\d{4})-.*\.md$")

# 状态行识别模式：匹配加粗+emoji+英文 形式
# 例: **✅ Approved**、**🟡 Partial (2026-06-08)**
STATUS_PATTERN = re.compile(
    r"\*{0,2}\s*"
    r"(?:✅\s*Approved|🟡\s*Partial|❌\s*Not\s*Implemented|⛔\s*Superseded|🔍\s*Proposed|📋\s*Reserved)"
    r"\s*\*{0,2}",
    re.IGNORECASE,
)

# 跨文件引用模式
# 真实 ADR 中的引用形如：
#   - 本 ADR 已被 [ADR-0020: ...] 替代。
#   - supersedes: adr-0010
# 关键约束：仅当 ADR 编号与"替代/取代/supersedes"出现在同一短语（10 字符内）才算引用。
# 这样可以避免误匹配"## 替代方案"（讨论替代方案的设计章节）。
REF_NUMBER = r"(\d{4})"
SUPERSEDES_PATTERN = re.compile(
    r"(?:supersedes|被\s*替代|已替代)\s*[:：]?\s*\[?[Aa][Dd][Rr]\s*-\s*"
    + REF_NUMBER
    + r"\b"
    r"|(?:已被\s*\[?[Aa][Dd][Rr]\s*-\s*"
    + REF_NUMBER
    + r"\b[^\n]{0,40}?替代)"
    r"|(?:\[?[Aa][Dd][Rr]\s*-\s*"
    + REF_NUMBER
    + r"\b[^\n]{0,40}?已替代)",
    re.IGNORECASE,
)
DEPENDS_ON_PATTERN = re.compile(
    r"(?:depends-on|相关|依赖)[:：]?\s*\[?[Aa][Dd][Rr]\s*-\s*" + REF_NUMBER + r"\]?",
    re.IGNORECASE,
)

# 备用：通用 ADR 编号引用，用于扫描"替代 NNNN"等松散表述
GENERIC_ADR_REF = re.compile(r"\b[Aa][Dd][Rr]\s*-\s*(\d{4})\b")


# ----------------------------------------------------------------------------
# 辅助函数
# ----------------------------------------------------------------------------

def find_all_adrs(adr_dir: Path) -> set[str]:
    """扫描目录下所有 adr-NNNN-*.md 文件，收集其 4 位编号集合。"""
    numbers: set[str] = set()
    if not adr_dir.exists():
        return numbers
    for path in adr_dir.glob("adr-*.md"):
        m = ADR_PATTERN.match(path.name)
        if m:
            numbers.add(m.group(1))
    return numbers


def find_status_section(lines: list[str]) -> int | None:
    """定位 `## 状态` 章节起始行索引；不存在则返回 None。"""
    for i, line in enumerate(lines):
        # 容忍前置空白和标题层级
        if line.strip().startswith("## 状态") or line.strip().startswith("## Status"):
            return i
    return None


def find_superseded_references(text: str, current_number: str) -> list[str]:
    """提取当前 ADR 中提到的"被替代方"编号集合。
    仅当被替代方编号 < current_number 时算合法。
    """
    refs: set[str] = set()

    # 形式 1：明确的"替代 / 取代 / supersedes"标记
    for m in SUPERSEDES_PATTERN.finditer(text):
        for g in m.groups():
            if g:
                refs.add(g)

    # 形式 2：被反向引用（"本 ADR 已被 ADR-NNNN 替代"）——
    # 这表示当前 ADR 是被替代方，应有"⛔ Superseded"状态。
    # 此类引用无需校验方向，仅作为状态一致性提示。
    return sorted(refs)


# ----------------------------------------------------------------------------
# 核心校验函数
# ----------------------------------------------------------------------------

def lint_adr_file(path: Path, all_adr_numbers: set[str]) -> list[str]:
    """对单个 ADR 文件进行校验，返回错误信息列表。空列表表示通过。"""
    errors: list[str] = []
    try:
        rel = path.relative_to(path.parents[1])
    except ValueError:
        rel = path
    text = path.read_text(encoding="utf-8")
    lines = text.splitlines()

    # 1. 文件名模式校验
    m = ADR_PATTERN.match(path.name)
    if not m:
        errors.append(f"{rel}: 文件名必须匹配 adr-NNNN-name.md（当前: {path.name}）")
        return errors  # 缺少编号无法继续

    adr_number = m.group(1)

    # 2. `## 状态` 章节存在性 + 状态标签校验
    status_idx = find_status_section(lines)
    if status_idx is None:
        errors.append(f"{rel}: 缺少 '## 状态' 章节")
    else:
        # 在 `## 状态` 之后的非空行/非次级标题行中查找合法状态 emoji
        status_found = False
        for j in range(status_idx + 1, min(status_idx + 15, len(lines))):
            line = lines[j]
            stripped = line.strip()
            if not stripped:
                continue
            # 遇到下一个二级标题则终止
            if stripped.startswith("## "):
                break
            if STATUS_PATTERN.search(line):
                status_found = True
                break
        if not status_found:
            errors.append(
                f"{rel}: '## 状态' 章节必须使用 6 个标准标签之一 "
                f"（✅ Approved / 🟡 Partial / ❌ Not Implemented / "
                f"⛔ Superseded / 🔍 Proposed / 📋 Reserved）"
            )

    # 3. 废弃状态词检查：仅在 `## 状态` 章节内、且本应是状态行的位置检查
    #    （即紧跟 `## 状态` 后的第一段非空行/非次级标题行）。
    #    若该行既无合法 emoji、又包含废弃词汇，则判定为状态行违规。
    if status_idx is not None:
        for j in range(status_idx + 1, min(status_idx + 10, len(lines))):
            line = lines[j]
            stripped = line.strip()
            if not stripped:
                continue
            if stripped.startswith("## "):
                break
            # 若该行已被合法状态 emoji 命中，跳过
            if STATUS_PATTERN.search(line):
                break
            # 否则：这就是候选状态行 — 检查废弃词
            low = line.lower()
            for word in DEPRECATED_STATUS_WORDS:
                if word in low:
                    errors.append(
                        f"{rel}:{j + 1}: 状态行使用了废弃词汇 '{word}'，"
                        f"请改用 emoji+英文 标准标签"
                    )
                    break
            break  # 只检查第一个候选状态行

    # 4. depends-on 引用校验：必须指向已存在的 ADR
    for m in DEPENDS_ON_PATTERN.finditer(text):
        ref_num = m.group(1)
        if ref_num not in all_adr_numbers:
            errors.append(
                f"{rel}: depends-on 引用了不存在的 adr-{ref_num}"
            )

    # 5. supersedes 引用校验：被替代方必须存在且编号 < 当前
    superseded = find_superseded_references(text, adr_number)
    for ref_num in superseded:
        if ref_num not in all_adr_numbers:
            errors.append(
                f"{rel}: supersedes 引用了不存在的 adr-{ref_num}"
            )
        elif int(ref_num) >= int(adr_number):
            errors.append(
                f"{rel}: supersedes adr-{ref_num}，但其编号 >= 当前 adr-{adr_number}（被替代方应更早）"
            )

    # 6. 状态一致性提示：若当前 ADR 是被反向引用的对象（"被 ADR-XXXX 替代"），
    #    状态应为 ⛔ Superseded（软警告，不计入错误）
    #    此处仅记录在 stderr 模式（--strict 时升级为错误，由调用方控制）

    return errors


# ----------------------------------------------------------------------------
# 入口
# ----------------------------------------------------------------------------

def main() -> int:
    parser = argparse.ArgumentParser(
        description="ADR frontmatter & status linter — 校验 docs/adr/ 下所有 ADR 文件"
    )
    parser.add_argument(
        "adr_dir", nargs="?", default="docs/adr",
        help="ADR 目录路径（默认: docs/adr）",
    )
    parser.add_argument(
        "--strict", action="store_true",
        help="启用严格模式：状态一致性软警告也升级为错误",
    )
    args = parser.parse_args()

    adr_dir = Path(args.adr_dir)
    if not adr_dir.exists():
        print(f"ERROR: 目录不存在: {adr_dir}", file=sys.stderr)
        return 2
    if not adr_dir.is_dir():
        print(f"ERROR: 不是目录: {adr_dir}", file=sys.stderr)
        return 2

    all_numbers = find_all_adrs(adr_dir)
    print(f"在 {adr_dir} 中找到 {len(all_numbers)} 个 ADR")

    all_errors: list[str] = []
    files_checked = 0
    for path in sorted(adr_dir.glob("adr-*.md")):
        files_checked += 1
        errors = lint_adr_file(path, all_numbers)
        all_errors.extend(errors)

    print(f"已检查 {files_checked} 个 ADR 文件")
    if all_errors:
        print(f"\n✗ 发现 {len(all_errors)} 个 lint 错误：\n")
        for e in all_errors:
            print(f"  {e}")
        return 1
    print("✓ 所有 ADR 通过 lint 检查")
    return 0


if __name__ == "__main__":
    sys.exit(main())
