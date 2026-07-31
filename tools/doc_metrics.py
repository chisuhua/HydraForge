#!/usr/bin/env python3
# tools/doc_metrics.py
# 功能描述: 文档计数指标生成器 — 为 docs/ 中的计数类数据提供可复现的单一事实源
#          - ADR 状态计数 (main / plugin / skill 三个命名空间分列)
#          - ctest 总数与最近一次运行结果 (build/ 目录)
#          - 事件总线 emit 调用点计数 (生产代码, 排除测试)
#          - ADR 子节点计数
#          治理要求: 文档中所有计数类数据必须可用本脚本复现 (见 docs/GOVERNANCE.md)
# 设计依据: 2026-07-31 文档治理审计 — active-status.md / gap-analysis /
#          layer-based-missing-capabilities-analysis.md 三处计数漂移 (83/93/106 ctest,
#          31/32/33 Approved), 根因是各自手写计数无工具校准
# 作者: Architecture Working Group
# 最后修改日期: 2026-07-31

import argparse
import re
import subprocess
import sys
from pathlib import Path

# ----------------------------------------------------------------------------
# 常量定义
# ----------------------------------------------------------------------------

REPO_ROOT = Path(__file__).resolve().parent.parent
ADR_DIR = REPO_ROOT / "docs" / "adr"
BUILD_DIR = REPO_ROOT / "build"

# 状态标签 → (匹配正则, 显示名)
STATUS_PATTERNS = [
    (re.compile(r"✅ Approved"), "✅ Approved"),
    (re.compile(r"🟡 Partial"), "🟡 Partial"),
    (re.compile(r"🔍 Proposed"), "🔍 Proposed"),
    (re.compile(r"❌ Not Implemented"), "❌ Not Implemented"),
    (re.compile(r"⛔ Superseded"), "⛔ Superseded"),
    (re.compile(r"📦 Archived"), "📦 Archived"),
]

# impl-scope 审计文档不参与 ADR 状态计数 (与 adr-implementation-status-gap-analysis.md 口径一致)
EXCLUDE_PATTERNS = ("impl-scope", "impl-scope-audit", "-audit")


# ----------------------------------------------------------------------------
# ADR 计数
# ----------------------------------------------------------------------------

def _is_adr_file(path: Path) -> bool:
    """判定是否为参与计数的 ADR 文件 (adr-NNNN-*.md 且非 impl-scope 审计)。"""
    name = path.name
    if not re.match(r"adr-\d{4}", name):
        return False
    return not any(pat in name for pat in EXCLUDE_PATTERNS)


def _first_status(path: Path) -> str:
    """提取 ADR 文件中**位置最早**出现的状态标签 (文件头状态为准)。

    注意: 不能按 STATUS_PATTERNS 顺序遍历匹配——ADR 正文的历史记录中
    可能提及多种状态, 必须取文件中出现位置最早的那个。
    """
    try:
        text = path.read_text(encoding="utf-8")
    except OSError:
        return "⚠ unreadable"
    best_pos, best_label = None, "⚠ no-status"
    for pattern, label in STATUS_PATTERNS:
        m = pattern.search(text)
        if m and (best_pos is None or m.start() < best_pos):
            best_pos, best_label = m.start(), label
    return best_label


def collect_adr_metrics() -> dict:
    """按命名空间 (main / plugin / skill) 统计 ADR 状态计数。"""
    result = {}
    namespaces = {
        "main": [p for p in ADR_DIR.glob("adr-*.md") if p.is_file()],
        "plugin": [p for p in (ADR_DIR / "plugin").glob("adr-*.md")],
        "skill": [p for p in (ADR_DIR / "skill").glob("adr-*.md")],
    }
    for ns, files in namespaces.items():
        counts: dict[str, int] = {}
        no_status: list[str] = []
        for f in sorted(files):
            if not _is_adr_file(f):
                continue
            st = _first_status(f)
            counts[st] = counts.get(st, 0) + 1
            if st == "⚠ no-status":
                no_status.append(f.name)
        result[ns] = {
            "total": sum(counts.values()),
            "counts": counts,
            "no_status": no_status,
        }
    return result


def print_adr_report(adr: dict) -> None:
    print("## ADR 状态计数 (tools/doc_metrics.py)\n")
    labels = [label for _, label in STATUS_PATTERNS]
    header = "| 状态 | main | plugin | skill | 合计 |"
    print(header)
    print("|------|-----:|-------:|------:|-----:|")
    totals = {ns: adr[ns]["total"] for ns in adr}
    for label in labels:
        row = [adr[ns]["counts"].get(label, 0) for ns in ("main", "plugin", "skill")]
        if sum(row) == 0:
            continue
        print(f"| {label} | {row[0]} | {row[1]} | {row[2]} | {sum(row)} |")
    print(f"| **总计** | **{totals['main']}** | **{totals['plugin']}** "
          f"| **{totals['skill']}** | **{sum(totals.values())}** |")
    for ns in adr:
        for name in adr[ns]["no_status"]:
            print(f"\n⚠ 无状态标签: {ns}/{name}", file=sys.stderr)


# ----------------------------------------------------------------------------
# ctest 计数
# ----------------------------------------------------------------------------

def collect_ctest_metrics() -> dict:
    """从 build/ 目录收集 ctest 配置总数与最近一次运行结果。"""
    metrics: dict = {"configured": None, "passed": None, "failed": None,
                     "last_run": None, "error": None}
    if not BUILD_DIR.is_dir():
        metrics["error"] = "build/ 目录不存在 (尚未 cmake 配置)"
        return metrics
    try:
        out = subprocess.run(
            ["ctest", "-N"], cwd=BUILD_DIR, capture_output=True, text=True, timeout=60,
        )
        m = re.search(r"Total Tests:\s*(\d+)", out.stdout)
        if m:
            metrics["configured"] = int(m.group(1))
    except (subprocess.SubprocessError, FileNotFoundError) as exc:
        metrics["error"] = f"ctest -N 执行失败: {exc}"
    last_log = BUILD_DIR / "Testing" / "Temporary" / "LastTest.log"
    if last_log.is_file():
        try:
            text = last_log.read_text(encoding="utf-8", errors="replace")
            metrics["passed"] = text.count("Test Passed.")
            metrics["failed"] = text.count("Test Failed.")
            m = re.search(r"End testing:\s*(.+)", text)
            if m:
                metrics["last_run"] = m.group(1).strip()
        except OSError as exc:
            metrics["error"] = f"LastTest.log 读取失败: {exc}"
    return metrics


def print_ctest_report(ct: dict) -> None:
    print("\n## ctest 计数 (build/)\n")
    if ct["error"]:
        print(f"⚠ {ct['error']}")
        return
    print(f"- 配置总数 (`ctest -N`): **{ct['configured']}**")
    if ct["passed"] is not None and (ct["passed"] + ct["failed"]) > 0:
        print(f"- 最近运行: **{ct['passed']}/{ct['configured']} passed, "
              f"{ct['failed']} failed** ({ct['last_run']})")
    elif ct["passed"] is not None:
        print(f"- 最近运行: LastTest.log 为空壳 ({ct['last_run']}) — "
              "可能被打断或为过滤运行, 请以 `cd build && ctest` stdout 为准")
    else:
        print("- 最近运行: 无 LastTest.log (尚未执行 ctest)")


# ----------------------------------------------------------------------------
# emit 调用点计数
# ----------------------------------------------------------------------------

def collect_emit_metrics() -> dict:
    """统计生产代码中的事件 emit 调用点 (src/ + examples/pdk_chat_demo/, 排除测试)。"""
    roots = [REPO_ROOT / "src", REPO_ROOT / "examples" / "pdk_chat_demo"]
    emit_re = re.compile(r"(?:bus|bus_|g_bus|impl_->bus)(?:_|->)?->?emit\(")
    sites: dict[str, int] = {}
    total = 0
    for root in roots:
        if not root.is_dir():
            continue
        for ext in ("*.cpp", "*.h"):
            for f in root.rglob(ext):
                if "test" in f.name.lower():
                    continue
                try:
                    lines = f.read_text(encoding="utf-8", errors="replace").splitlines()
                except OSError:
                    continue
                n = sum(1 for ln in lines if emit_re.search(ln))
                if n:
                    sites[str(f.relative_to(REPO_ROOT))] = n
                    total += n
    return {"total": total, "by_file": sites}


def print_emit_report(em: dict) -> None:
    print("\n## 事件 emit 调用点 (生产代码, 排除测试)\n")
    print(f"**总计: {em['total']} 处**\n")
    print("| 文件 | 计数 |")
    print("|------|-----:|")
    for path, n in sorted(em["by_file"].items(), key=lambda kv: -kv[1]):
        print(f"| `{path}` | {n} |")


# ----------------------------------------------------------------------------
# 主入口
# ----------------------------------------------------------------------------

def main() -> int:
    parser = argparse.ArgumentParser(
        description="文档计数指标生成器 — docs/ 计数类数据的单一事实源",
    )
    parser.add_argument("--adr", action="store_true", help="仅输出 ADR 状态计数")
    parser.add_argument("--ctest", action="store_true", help="仅输出 ctest 计数")
    parser.add_argument("--emit", action="store_true", help="仅输出 emit 调用点计数")
    args = parser.parse_args()
    run_all = not (args.adr or args.ctest or args.emit)

    exit_code = 0
    if run_all or args.adr:
        adr = collect_adr_metrics()
        print_adr_report(adr)
        for ns in adr:
            if adr[ns]["no_status"]:
                exit_code = 1
    if run_all or args.ctest:
        print_ctest_report(collect_ctest_metrics())
    if run_all or args.emit:
        print_emit_report(collect_emit_metrics())
    return exit_code


if __name__ == "__main__":
    sys.exit(main())
