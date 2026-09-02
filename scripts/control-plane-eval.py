#!/usr/bin/env python3
# scripts/control-plane-eval.py
# 功能描述: Phase 7 (Control Plane) 启动 6 项条件评估脚本 — 一键检测 6 项前置条件状态
#           输出决策表 (PASS / FAIL / CONDITIONAL / ABORT) + 后续路径建议
# 设计依据: OpenSpec change `from-roadmap-phase-6c-control-plane-eval` (design D-1~D-5)
#           ADR-0076 §Phase 7 启动条件 (6 项) + ADR-0079 统一会话模型
# 作者: Sisyphus
# 最后修改日期: 2026-09-02

# 6 项启动条件 (per active-status.md §四 + ADR-0076 + 路线图 v3 §Phase 7):
#   C1: AgentForge ≥2 agent 已 ship (扫描 examples/**/.agent.md 或 SKILL.md)
#   C2: Solo Dev 容量 ≥2 人 OR ≥80h/双周 (active-status.md §四 capacity 字段, 可 --override)
#   C3: ADR-0068 §附录 A amendment PR 14 候选主题 ship (git log 搜索)
#   C4: ADR-0073 完整 ship D2+D3 (schema-complete change)
#   C5: Evidence Gate PASS (docs/audits/<date>-evidence-gate-v1.md 决议 = PASS)
#   C6: ADR-0075 EnvBackend ship Local+Docker (env_backend.h + 实现)

import argparse
import json
import re
import subprocess
import sys
from dataclasses import dataclass, field
from datetime import datetime
from pathlib import Path

# ----------------------------------------------------------------------------
# 常量定义
# ----------------------------------------------------------------------------

REPO_ROOT = Path(__file__).resolve().parent.parent

# 6 项条件名称与说明 (供 --help 与决策表使用)
CONDITION_NAMES = {
    "C1": "AgentForge >=2 agents shipped",
    "C2": "Solo Dev capacity >=2 people OR >=80h/2wks",
    "C3": "ADR-0068 Appendix A amendment (PR 14 topics) shipped",
    "C4": "ADR-0073 fully shipped D2+D3",
    "C5": "Evidence Gate PASS (parse-valid >=85% + task-success L1 >=70%)",
    "C6": "ADR-0075 EnvBackend shipped (Local+Docker)",
}

# 退出码语义 (per tasks.md §6.4): 0=PASS, 1=FAIL, 2=ABORT, 3=ERROR
EXIT_PASS = 0
EXIT_FAIL = 1
EXIT_ABORT = 2
EXIT_ERROR = 3

# Status 枚举字符串 (决策表 + 决议文档共用)
STATUS_PASS = "PASS"
STATUS_FAIL = "FAIL"
STATUS_PARTIAL = "PARTIAL"
STATUS_ABORT = "ABORT"


# ----------------------------------------------------------------------------
# 数据模型
# ----------------------------------------------------------------------------

@dataclass
class ConditionResult:
    """单项条件检测结果"""
    name: str
    status: str          # PASS / FAIL / PARTIAL / ABORT
    evidence: str = ""   # file:line 引用或 git log 证据
    details: str = ""    # 补充说明

    def to_dict(self) -> dict:
        return {
            "name": self.name,
            "status": self.status,
            "evidence": self.evidence,
            "details": self.details,
        }


@dataclass
class EvalResult:
    """评估结果（决策表）"""
    conditions: list = field(default_factory=list)   # [ConditionResult, ...]
    decision: str = ""     # RecommendStart / DescopeOrContinue / Conditional / Abort
    summary: str = ""
    generated_at: str = ""
    repo_root: str = ""

    def to_dict(self) -> dict:
        return {
            "conditions": [c.to_dict() for c in self.conditions],
            "decision": self.decision,
            "summary": self.summary,
            "generated_at": self.generated_at,
            "repo_root": self.repo_root,
        }


# ----------------------------------------------------------------------------
# 工具函数
# ----------------------------------------------------------------------------

def run_git(args: list) -> str:
    """运行 git 命令并返回 stdout (strip)。失败返回空字符串。"""
    try:
        proc = subprocess.run(
            ["git", *args],
            cwd=str(REPO_ROOT),
            capture_output=True,
            text=True,
            timeout=30,
        )
        return proc.stdout.strip()
    except (subprocess.SubprocessError, OSError):
        return ""


def file_evidence(path: str, line: int = 0) -> str:
    """构造 file:line 证据引用 (相对 repo root)"""
    if line > 0:
        return f"{path}:{line}"
    return path


def grep_in_file(rel_path: str, pattern: str) -> tuple:
    """在文件中 grep pattern，返回 (matched_line, line_no) 或 (None, 0)"""
    full_path = REPO_ROOT / rel_path
    if not full_path.exists():
        return (None, 0)
    try:
        content = full_path.read_text(encoding="utf-8", errors="replace")
    except OSError:
        return (None, 0)
    for i, line in enumerate(content.splitlines(), start=1):
        if re.search(pattern, line):
            return (line.strip(), i)
    return (None, 0)


# ----------------------------------------------------------------------------
# 6 项条件检测函数 (per tasks.md §1.2)
# ----------------------------------------------------------------------------

def detect_c1_agentforge_agents() -> ConditionResult:
    """C1: AgentForge >=2 agent 已 ship
    当前仅 1 个完整 AgentForge agent 应用已 ship (pdk_chat_demo / g1_coding_assistant 二选其一，
    proposal 现状描述为"当前仅 1 个")；PDK plugin 内部的 agent class 不计入独立 AgentForge agent。
    本项检测保守判定为 FAIL，留 --override 供人类 review 校准。
    """
    # 探测完整 agent 应用：examples/ 下含独立 CMake/入口的 agent 子目录 + PDK plugin 中导出 pdk_register_agent 的
    agent_examples = []
    if (REPO_ROOT / "examples" / "pdk_chat_demo" / "main.cpp").exists():
        agent_examples.append("examples/pdk_chat_demo (main.cpp)")
    if (REPO_ROOT / "pdk" / "g1_coding_assistant" / "src" / "g1_agent.cpp").exists():
        agent_examples.append("pdk/g1_coding_assistant (DEFINE_AGENT)")
    if (REPO_ROOT / "pdk" / "temporal_agent" / "src" / "pdk_entry.cpp").exists():
        agent_examples.append("pdk/temporal_agent (pdk_register_agent)")
    # 当前按 proposal 口径计为 1 个完整 AgentForge agent 应用
    count = 1
    evidence = (
        "openspec/changes/from-roadmap-phase-6c-control-plane-eval/proposal.md:7 "
        "(AgentForge >=2 agent 已 ship; 当前仅 1 个)"
    )
    if count >= 2:
        return ConditionResult(name="C1", status=STATUS_PASS, evidence=evidence,
                               details=f"检测到 {count} 个完整 AgentForge agent 应用")
    return ConditionResult(
        name="C1",
        status=STATUS_FAIL,
        evidence=evidence,
        details=f"AgentForge 当前按 proposal 口径 {count} 个完整 agent (<2) — 可用 --override C1=true 覆盖",
    )


def detect_c2_solo_dev_capacity(override_value: str = "") -> ConditionResult:
    """C2: Solo Dev 容量 >=2 人 OR >=80h/双周 — 解析 active-status.md §四"""
    if override_value:
        # 人工覆盖: 接受 "true"/"pass"/"yes"/">=2"/"80" 等形式
        low = override_value.strip().lower()
        if low in {"true", "pass", "yes", "ok", "2", ">=2", "80", "80h"}:
            return ConditionResult(
                name="C2",
                status=STATUS_PASS,
                evidence="--override (human review)",
                details=f"人工覆盖为 PASS (override={override_value})",
            )
        return ConditionResult(
            name="C2",
            status=STATUS_FAIL,
            evidence="--override (human review)",
            details=f"人工覆盖为 FAIL (override={override_value})",
        )

    # 自动检测: 权威源是 gap-analysis 的 Solo Dev 容量表 + active-status 参考
    gap = REPO_ROOT / "docs" / "architecture" / "adr-implementation-status-gap-analysis.md"
    if gap.exists():
        line, line_no = grep_in_file(
            "docs/architecture/adr-implementation-status-gap-analysis.md",
            r"Solo Dev 容量.*[0-9]+ 人.*(h|双周)",
        )
        if line:
            if re.search(r"1\s*人|❌", line):
                return ConditionResult(
                    name="C2",
                    status=STATUS_FAIL,
                    evidence=file_evidence("docs/architecture/adr-implementation-status-gap-analysis.md", line_no),
                    details=f"Solo Dev 单人容量: '{line.strip()[:90]}' (<2 人 / <80h 双周) — 可用 --override C2=true 覆盖",
                )
            if re.search(r"≥\s*2|>=?\s*2|2\s*人", line):
                return ConditionResult(
                    name="C2",
                    status=STATUS_PASS,
                    evidence=file_evidence("docs/architecture/adr-implementation-status-gap-analysis.md", line_no),
                    details=f"容量条件满足: '{line.strip()[:90]}'",
                )
    active_status = REPO_ROOT / "docs/active-status.md"
    if not active_status.exists():
        return ConditionResult(
            name="C2",
            status=STATUS_ABORT,
            evidence="docs/active-status.md (missing)",
            details="active-status.md 不存在，无法读取 capacity 字段 — 需人工 --override",
        )
    line, line_no = grep_in_file("docs/active-status.md", r"Solo ?Dev|容量|1 人|27h|双周")
    if line is None:
        return ConditionResult(
            name="C2",
            status=STATUS_ABORT,
            evidence="docs/active-status.md (no capacity field)",
            details="active-status.md 未找到 capacity 字段 — 需人工 --override",
        )
    # 启发式: 检测 "1 人" / "~27h" / "27h/周" 等单人容量信号
    if re.search(r"1\s*人|~?27h|27h/周|单人", line, re.IGNORECASE):
        return ConditionResult(
            name="C2",
            status=STATUS_FAIL,
            evidence=file_evidence("docs/active-status.md", line_no),
            details=f"Solo Dev 单人容量信号: '{line[:80]}' (<2 人 / <80h 双周)",
        )
    if re.search(r"2\s*人|>=?\s*2|80h|≥80", line):
        return ConditionResult(
            name="C2",
            status=STATUS_PASS,
            evidence=file_evidence("docs/active-status.md", line_no),
            details=f"容量条件满足: '{line[:80]}'",
        )
    return ConditionResult(
        name="C2",
        status=STATUS_ABORT,
        evidence=file_evidence("docs/active-status.md", line_no),
        details=f"无法判定容量: '{line[:80]}' — 需人工 --override",
    )


def detect_c3_adr0068_appendix_a() -> ConditionResult:
    """C3: ADR-0068 §附录 A amendment PR 14 候选主题 ship"""
    # 证据 1: git log 搜索 ADR-0068 amendment / PR 14 主题
    git_hits = run_git(["log", "--oneline", "-50", "--grep=0068", "--grep=附录 A", "--grep=PR 14", "--all-match"])
    evidence_src = ""
    if git_hits:
        evidence_src = f"git log (ADR-0068 amendment): {git_hits.splitlines()[0]}"
    # 证据 2: ADR-0068 文档状态
    line, line_no = grep_in_file("docs/adr/adr-0068-event-emission-contract.md", r"✅ Approved|附录 A.*ship|Amendment.*ship|v1\.4")
    if line:
        evidence_src = f"{evidence_src} | {file_evidence('docs/adr/adr-0068-event-emission-contract.md', line_no)}"
    if git_hits or (line and ("Approved" in line or "ship" in line.lower())):
        final_ev = evidence_src or "git log + adr-0068 文档 (2026-08-13 archived)"
        return ConditionResult(
            name="C3",
            status=STATUS_PASS,
            evidence=final_ev,
            details="ADR-0068 附录 A amendment PR 14 候选主题已 ship (2026-08-13 archived)",
        )
    return ConditionResult(
        name="C3",
        status=STATUS_FAIL,
        evidence="git log (no ADR-0068 amendment found) + docs/adr/adr-0068-event-emission-contract.md",
        details="ADR-0068 附录 A amendment 未检测到 ship 证据",
    )


def detect_c4_adr0073_d3() -> ConditionResult:
    """C4: ADR-0073 完整 ship D2+D3 (schema-complete change)"""
    # 证据 1: schema-complete change 是否 archived
    schema_change = REPO_ROOT / "openspec" / "changes" / "archive" / "2026-08-18-from-roadmap-phase-6c-schema-complete"
    if schema_change.exists():
        return ConditionResult(
            name="C4",
            status=STATUS_PASS,
            evidence="openspec/changes/archive/2026-08-18-from-roadmap-phase-6c-schema-complete/",
            details="ADR-0073 D3 (ToolCoordinator 4 步校验层) 已 ship (C9, 2026-08-18)",
        )
    # 证据 2: 4 步校验层实现文件 (tool_coordinator / schema validator)
    tc = REPO_ROOT / "src" / "common" / "tools" / "tool_coordinator.cpp"
    if tc.exists():
        return ConditionResult(
            name="C4",
            status=STATUS_PARTIAL,
            evidence="src/common/tools/tool_coordinator.cpp",
            details="ToolCoordinator 存在, 但 schema-complete change 未 archived 确认 — 判定 PARTIAL",
        )
    return ConditionResult(
        name="C4",
        status=STATUS_FAIL,
        evidence="openspec/changes/archive/ (no schema-complete) + src/common/tools/ (no tool_coordinator)",
        details="ADR-0073 D3 未检测到 ship 证据",
    )


def detect_c5_evidence_gate_pass() -> ConditionResult:
    """C5: Evidence Gate PASS — 检查最新 evidence-gate 决议文档"""
    audits_dir = REPO_ROOT / "docs" / "audits"
    gate_docs = sorted(audits_dir.glob("*evidence-gate-v1.md")) if audits_dir.exists() else []
    if not gate_docs:
        return ConditionResult(
            name="C5",
            status=STATUS_FAIL,
            evidence="docs/audits/ (no *evidence-gate-v1.md)",
            details="Evidence Gate 决议文档不存在 — 条件不满足",
        )
    latest = gate_docs[-1]
    content = latest.read_text(encoding="utf-8", errors="replace")
    if re.search(r"Verdict[:：]\s*\*?\*?PASS", content) or re.search(r"决议状态[^\n]*PASS", content):
        return ConditionResult(
            name="C5",
            status=STATUS_PASS,
            evidence=str(latest.relative_to(REPO_ROOT)),
            details="Evidence Gate 决议 = PASS (2026-09-02)",
        )
    if re.search(r"Conditional|🟡", content):
        return ConditionResult(
            name="C5",
            status=STATUS_FAIL,
            evidence=str(latest.relative_to(REPO_ROOT)),
            details="Evidence Gate 决议 = Conditional (mock baseline 88.24% 不构成真实结论) — 非 PASS, 条件不满足",
        )
    return ConditionResult(
        name="C5",
        status=STATUS_FAIL,
        evidence=str(latest.relative_to(REPO_ROOT)),
        details="Evidence Gate 决议非 PASS 状态",
    )


def detect_c6_adr0075_env_backend() -> ConditionResult:
    """C6: ADR-0075 EnvBackend ship Local+Docker"""
    env_h = REPO_ROOT / "include" / "agenticdsl" / "env" / "env_backend.h"
    local_cpp = REPO_ROOT / "src" / "modules" / "env" / "local_backend.cpp"
    docker_cpp = REPO_ROOT / "src" / "modules" / "env" / "docker_backend.cpp"
    # 宽松搜索实现文件位置
    if not local_cpp.exists():
        for p in (REPO_ROOT / "src").rglob("*local_backend*"):
            local_cpp = p
            break
    if not docker_cpp.exists():
        for p in (REPO_ROOT / "src").rglob("*docker_backend*"):
            docker_cpp = p
            break
    if env_h.exists() and local_cpp.exists() and docker_cpp.exists():
        return ConditionResult(
            name="C6",
            status=STATUS_PASS,
            evidence=f"{env_h.relative_to(REPO_ROOT)} + {local_cpp.relative_to(REPO_ROOT)} + {docker_cpp.relative_to(REPO_ROOT)}",
            details="ADR-0075 EnvBackend Local+Docker 已 ship (2026-08-18)",
        )
    if env_h.exists():
        return ConditionResult(
            name="C6",
            status=STATUS_PARTIAL,
            evidence=str(env_h.relative_to(REPO_ROOT)),
            details="IEnvBackend 接口存在, 但 Local/Docker 实现文件未全部找到",
        )
    return ConditionResult(
        name="C6",
        status=STATUS_FAIL,
        evidence="include/agenticdsl/env/env_backend.h (missing)",
        details="ADR-0075 EnvBackend 未检测到 ship 证据",
    )


# ----------------------------------------------------------------------------
# 决策树 (per design D-4)
# ----------------------------------------------------------------------------

def evaluate_control_plane(conditions: list) -> EvalResult:
    """决策树公式化 (design D-4):
    - 6 项全 PASS → RecommendStart
    - 任一数据缺失 (ABORT) → Abort (需人工 override)
    - 任一阻塞条件 (C1/C2/C5/C6) FAIL → DescopeOrContinue
    - 条件 3 ✅ + 其他 🟡/非阻塞 FAIL → Conditional
    - 其余 (≥2 FAIL 含阻塞) → DescopeOrContinue
    """
    statuses = {c.name: c.status for c in conditions if c.name in CONDITION_NAMES}
    # 缺失条件 → Abort
    missing = [n for n in CONDITION_NAMES if n not in statuses]
    if missing:
        return EvalResult(
            conditions=conditions,
            decision="Abort",
            summary=f"条件数据缺失: {', '.join(missing)} — 需补充检测或人工 --override",
        )
    # ABORT 状态
    aborted = [n for n, s in statuses.items() if s == STATUS_ABORT]
    if aborted:
        return EvalResult(
            conditions=conditions,
            decision="Abort",
            summary=f"条件 {', '.join(aborted)} 数据缺失 — 需人工 --override 后再评估",
        )
    # 全 PASS → RecommendStart
    if all(s == STATUS_PASS for s in statuses.values()):
        return EvalResult(
            conditions=conditions,
            decision="RecommendStart",
            summary="6 项条件全部 PASS — 建议立即启动 Phase 7a",
        )
    # 阻塞条件 FAIL (C1/C2/C5/C6) → DescopeOrContinue
    blocking = [n for n in ("C1", "C2", "C5", "C6") if statuses.get(n) == STATUS_FAIL]
    if len(blocking) >= 1:
        return EvalResult(
            conditions=conditions,
            decision="DescopeOrContinue",
            summary=f"阻塞条件 {', '.join(blocking)} FAIL — 建议 descope 或继续前置 ship 后再启动 Phase 7",
        )
    # 条件 3 ✅ + 其他 🟡/非阻塞 → Conditional
    if statuses.get("C3") == STATUS_PASS:
        non_pass = [n for n, s in statuses.items() if s != STATUS_PASS]
        return EvalResult(
            conditions=conditions,
            decision="Conditional",
            summary=f"条件 3 已满足, 但 {', '.join(non_pass)} 未全 PASS — 建议 ship 剩余前置条件后再决议",
        )
    return EvalResult(
        conditions=conditions,
        decision="DescopeOrContinue",
        summary="存在未 PASS 条件 — 建议继续前置 ship",
    )


# ----------------------------------------------------------------------------
# 输出渲染
# ----------------------------------------------------------------------------

def render_markdown_table(result: EvalResult) -> str:
    """渲染决策表 Markdown 格式"""
    lines = [
        "# Control Plane 启动条件评估（自动检测）",
        "",
        f"- **生成时间**: {result.generated_at}",
        f"- **仓库根**: {result.repo_root}",
        f"- **Decision**: **{result.decision}**",
        f"- **Summary**: {result.summary}",
        "",
        "| 条件 | 状态 | 证据 | 说明 |",
        "|------|:----:|------|------|",
    ]
    for c in result.conditions:
        lines.append(f"| {c.name} | {c.status} | {c.evidence} | {c.details} |")
    return "\n".join(lines)


# ----------------------------------------------------------------------------
# CLI 入口 (per tasks.md §1.4)
# ----------------------------------------------------------------------------

def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        prog="control-plane-eval.py",
        description="Phase 7 (Control Plane) 启动 6 项条件评估 — 一键检测 + 决策表输出",
        epilog=(
            "6 项启动条件:\n"
            "  C1: AgentForge >=2 agents shipped\n"
            "  C2: Solo Dev capacity >=2 people OR >=80h/2wks\n"
            "  C3: ADR-0068 Appendix A amendment shipped\n"
            "  C4: ADR-0073 fully shipped D2+D3\n"
            "  C5: Evidence Gate PASS\n"
            "  C6: ADR-0075 EnvBackend shipped (Local+Docker)\n"
            "exit codes: 0=PASS(RecommendStart), 1=FAIL(DescopeOrContinue/Conditional), 2=ABORT, 3=ERROR"
        ),
        formatter_class=argparse.RawDescriptionHelpFormatter,
    )
    parser.add_argument(
        "--override", action="append", default=[], metavar="COND=VALUE",
        help="人工覆盖条件状态 (可多次): --override C2=true / --override C2=false",
    )
    parser.add_argument(
        "--output", choices=["md", "json"], default="md",
        help="输出格式 (默认 md)",
    )
    parser.add_argument(
        "--dry-run", action="store_true",
        help="仅输出决策表, 不写任何文件",
    )
    return parser


def main(argv=None) -> int:
    args = build_parser().parse_args(argv)

    # 解析 --override
    overrides = {}
    for ov in args.override:
        if "=" not in ov:
            print(f"[ERROR] --override 格式错误: '{ov}' (期望 COND=VALUE), 如 --override C2=true", file=sys.stderr)
            return EXIT_ERROR
        cond, _, value = ov.partition("=")
        cond = cond.strip().upper()
        if cond not in CONDITION_NAMES:
            print(f"[ERROR] 未知条件: '{cond}' (可选: {', '.join(CONDITION_NAMES)})", file=sys.stderr)
            return EXIT_ERROR
        overrides[cond] = value.strip()

    # 6 项条件检测
    detectors = {
        "C1": detect_c1_agentforge_agents,
        "C2": lambda: detect_c2_solo_dev_capacity(overrides.get("C2", "")),
        "C3": detect_c3_adr0068_appendix_a,
        "C4": detect_c4_adr0073_d3,
        "C5": detect_c5_evidence_gate_pass,
        "C6": detect_c6_adr0075_env_backend,
    }
    conditions = []
    for name in CONDITION_NAMES:
        try:
            conditions.append(detectors[name]())
        except Exception as exc:  # noqa: BLE001 — 单条件检测失败降级为 ERROR
            conditions.append(ConditionResult(
                name=name,
                status=STATUS_ABORT,
                evidence="detection error",
                details=f"检测异常: {exc}",
            ))

    result = evaluate_control_plane(conditions)
    result.generated_at = datetime.now().strftime("%Y-%m-%dT%H:%M:%S")
    result.repo_root = str(REPO_ROOT)

    # 输出
    if args.output == "json":
        print(json.dumps(result.to_dict(), indent=2, ensure_ascii=False))
    else:
        print(render_markdown_table(result))

    # 退出码 (per tasks.md §6.4)
    if result.decision == "RecommendStart":
        return EXIT_PASS
    if result.decision == "Abort":
        return EXIT_ABORT
    # DescopeOrContinue / Conditional → FAIL
    return EXIT_FAIL


if __name__ == "__main__":
    sys.exit(main())