#!/usr/bin/env python3
# tools/docs_drift_audit.py
# 功能描述：文档与代码漂移审计脚本。实现 OpenSpec change
#          `docs-code-drift-audit-2026-06` 描述的 4 类检测场景：
#            1. examples/ 中 DEPRECATED 注释 vs 实际 API 状态
#            2. .omo/plans/project-organization.md 中 F1-F4 self-audit 误标
#            3. src/core/engine.h 注释 vs include 实际数量自相矛盾
#            4. docs/adr/ 中 ADR 声称实现 vs 代码 grep 验证
# 设计依据：openspec/changes/docs-code-drift-audit-2026-06/specs/docs-code-drift-audit/spec.md
# 作者：docs-code-drift-audit-2026-06 实施
# 最后修改日期：2026-06-13

"""
HydraForge 文档-代码漂移审计脚本

CLI 用法：
    python3 tools/docs_drift_audit.py [PROJECT_ROOT] [--json]

退出码：
    0  未发现 drift
    1  发现 drift

依赖：仅 Python 3.10+ 标准库
"""

import argparse
import json
import re
import sys
from datetime import datetime, timezone
from pathlib import Path
from typing import Any, Dict, Iterable, List, Optional, Tuple


# ============================================================================
# 常量定义
# ============================================================================

# 场景名称（中文 — 项目约定）
SCENARIO_NAMES = {
    1: "DEPRECATED 注释 vs 实际 API 状态",
    2: "plan F1-F4 self-audit 误标",
    3: "engine.h includes vs 声明",
    4: "ADR 声称实现 vs 代码 grep",
    5: "proposal-approved.md 已批准提案 vs archive 一致性",
}

# 严重级别
SEVERITY_DRIFT = "DRIFT"
SEVERITY_WARNING = "WARNING"
SEVERITY_ACCURATE = "ACCURATE"


# ============================================================================
# 通用工具函数
# ============================================================================

def make_finding(
    scenario: int,
    severity: str,
    file: str,
    line: int,
    summary: str,
    details: Optional[Dict[str, Any]] = None,
) -> Dict[str, Any]:
    """构造一个 drift finding 字典（统一的输出 schema）"""
    finding: Dict[str, Any] = {
        "scenario": scenario,
        "scenario_name": SCENARIO_NAMES[scenario],
        "severity": severity,
        "file": file,
        "line": line,
        "summary": summary,
    }
    if details:
        finding["details"] = details
    return finding


def rel(path: Path, root: Path) -> str:
    """将绝对路径转为相对于 root 的 POSIX 路径字符串"""
    try:
        return path.relative_to(root).as_posix()
    except ValueError:
        return path.as_posix()


def read_text(path: Path) -> Optional[str]:
    """读取 UTF-8 文本；失败返回 None（不抛异常）"""
    try:
        return path.read_text(encoding="utf-8")
    except (OSError, UnicodeDecodeError):
        return None


# ============================================================================
# 场景 1：DEPRECATED 注释 vs 实际 API 状态
# ============================================================================

# 匹配以 ⚠️ 开头的注释块（// 注释与 /* */ 注释均支持）
DEPRECATED_NOTE_HEADER = re.compile(r"⚠️\s*[^/\n]*?(?:DEPRECATED|removed\s+in\s+commit|已废弃)", re.IGNORECASE)

# 单行 DEPRECATED 注释标记（用于发现入口）
DEPRECATED_LINE_HEADER = re.compile(r"//\s*⚠️", re.IGNORECASE)

# 从注释中抽取 API 名称。匹配形如：
#   - `agenticdsl::LlamaAdapter`
#   - `InjaTemplateRenderer`
#   - extract_pathed_blocks
#   - engine->get_llm_adapter()
#   - get_llm_provider()
API_NAME_PATTERN = re.compile(
    r"`?(?:agenticdsl::)?([A-Za-z_][A-Za-z0-9_]*)`?"
)

# 排除的常见误匹配（注释里的自然语言词汇，避免噪音）
NOISE_WORDS = {
    "commit", "header", "stub", "audit", "trace", "version",
    "include", "src", "the", "file", "note", "this", "data",
    "namespace", "status", "test", "tests", "main", "and",
    "via", "with", "from", "see", "not", "its", "are", "for",
    "true", "false", "to", "in", "of", "on", "is", "be",
}

# 已知 C++ 关键字（额外保险）
CPP_KEYWORDS = {
    "class", "struct", "enum", "namespace", "template", "typename",
    "public", "private", "protected", "virtual", "override", "const",
    "static", "inline", "extern", "return", "if", "else", "while",
    "for", "switch", "case", "break", "continue", "true", "false",
    "nullptr", "void", "int", "char", "bool", "float", "double",
    "long", "short", "unsigned", "signed", "auto", "using",
}


def extract_deprecated_blocks(text: str, file_path: Path) -> List[Tuple[int, str]]:
    """
    提取文件中所有 DEPRECATED/⚠️ 注释块，返回 (起始行号, 块内容) 列表。
    注释块从 `// ⚠️` 或 `/* ⚠️` 起始，连续的 // 行算同一块。
    """
    blocks: List[Tuple[int, str]] = []
    lines = text.splitlines()
    i = 0
    while i < len(lines):
        line = lines[i]
        stripped = line.lstrip()
        if stripped.startswith("//") and ("⚠️" in stripped or "DEPRECATED" in stripped.upper()):
            start = i + 1
            content_lines = [line]
            # 向下吞并连续的 // 注释行
            j = i + 1
            while j < len(lines):
                nxt = lines[j].lstrip()
                if nxt.startswith("//"):
                    content_lines.append(lines[j])
                    j += 1
                else:
                    break
            blocks.append((start, "\n".join(content_lines)))
            i = j
        else:
            i += 1
    return blocks


def extract_api_names(block_text: str) -> List[str]:
    """
    从注释块中抽取 API 名称（类名/函数名），去重且过滤噪音。
    """
    names: List[str] = []
    seen = set()
    # 在反引号包裹的 token 和 `agenticdsl::Name` 形式中搜索
    for m in re.finditer(r"`([A-Za-z_][A-Za-z0-9_]*)`", block_text):
        name = m.group(1)
        if _is_valid_api_name(name) and name not in seen:
            seen.add(name)
            names.append(name)
    # 也匹配 `agenticdsl::Name` 形式（即使没有被反引号包裹）
    for m in re.finditer(r"agenticdsl::([A-Za-z_][A-Za-z0-9_]*)", block_text):
        name = m.group(1)
        if _is_valid_api_name(name) and name not in seen:
            seen.add(name)
            names.append(name)
    return names


def _is_valid_api_name(name: str) -> bool:
    """判断是否为合理的 API 名称（过滤噪音和关键字）"""
    if len(name) < 4:
        return False
    if name in NOISE_WORDS or name in CPP_KEYWORDS:
        return False
    # 必须首字母大写（CamelCase 类名）或包含下划线的 snake_case 函数名
    if not (name[0].isupper() or "_" in name):
        return False
    return True


_SOURCE_INDEX_CACHE: Dict[str, Dict[str, Optional[str]]] = {}


def _build_source_index(root: Path) -> Dict[str, Optional[str]]:
    """
    单次遍历 src/ + include/，提取所有 PascalCase 标识符构建索引。
    后续所有名称查询均为 O(1)，避免每查询一次都遍历 76+ 源文件。
    """
    cache_key = str(root)
    if cache_key in _SOURCE_INDEX_CACHE:
        return _SOURCE_INDEX_CACHE[cache_key]

    class_or_struct = re.compile(r"\b(?:class|struct)\s+([A-Z][A-Za-z0-9_]+)\b")
    bare_pascal = re.compile(r"\b([A-Z][A-Za-z0-9_]{3,})\b")
    block_comment = re.compile(r"/\*.*?\*/", re.DOTALL)

    index: Dict[str, Optional[str]] = {}
    search_dirs = [root / "src", root / "include"]

    for search_dir in search_dirs:
        if not search_dir.exists():
            continue
        for ext in ("*.h", "*.hpp", "*.cpp", "*.cc"):
            for path in search_dir.rglob(ext):
                try:
                    if path.stat().st_size > 500_000:
                        continue
                except OSError:
                    continue
                content = read_text(path)
                if content is None:
                    continue
                path_rel = rel(path, root)
                # 先剥离 /* ... */ 块注释（避免在 doc comment 中误识别类名）
                stripped = block_comment.sub("", content)
                for line in stripped.splitlines():
                    code = line.split("//", 1)[0]
                    for pat in (class_or_struct, bare_pascal):
                        for m in pat.finditer(code):
                            name = m.group(1)
                            if name not in index:
                                index[name] = path_rel

    _SOURCE_INDEX_CACHE[cache_key] = index
    return index


def find_class_definition(name: str, root: Path) -> Optional[str]:
    """通过源码索引查找名称定义所在文件，未找到返回 None"""
    return _build_source_index(root).get(name)


def scan_scenario1(root: Path) -> List[Dict[str, Any]]:
    """场景 1：扫描 examples/**/*.cpp 中的 DEPRECATED 注释块"""
    findings: List[Dict[str, Any]] = []
    examples_dir = root / "examples"
    if not examples_dir.exists():
        return findings

    for cpp_path in sorted(examples_dir.rglob("*.cpp")):
        text = read_text(cpp_path)
        if text is None:
            continue
        blocks = extract_deprecated_blocks(text, cpp_path)
        if not blocks:
            continue

        file_rel = rel(cpp_path, root)
        for start_line, block_text in blocks:
            api_names = extract_api_names(block_text)
            if not api_names:
                continue
            mentioned_apis: List[Dict[str, str]] = []
            has_incorrect_claim = False
            for name in api_names:
                location = find_class_definition(name, root)
                if location:
                    mentioned_apis.append({
                        "name": name,
                        "actual_status": "EXISTS",
                        "location": location,
                    })
                    # DEPRECATED 注释里声称已删除，但实际仍存在 → DRIFT
                    has_incorrect_claim = True
                else:
                    mentioned_apis.append({
                        "name": name,
                        "actual_status": "DELETED",
                        "location": "NOT FOUND",
                    })

            severity = SEVERITY_DRIFT if has_incorrect_claim else SEVERITY_ACCURATE
            if severity == SEVERITY_DRIFT:
                summary = (
                    f"DEPRECATED 注释中声称的 API 实际仍存在 — "
                    f"共 {sum(1 for a in mentioned_apis if a['actual_status'] == 'EXISTS')}/{len(mentioned_apis)} 个 API 仍存在"
                )
            else:
                summary = f"DEPRECATED 注释准确：{len(mentioned_apis)} 个 API 均已删除"

            findings.append(make_finding(
                scenario=1,
                severity=severity,
                file=file_rel,
                line=start_line,
                summary=summary,
                details={"mentioned_apis": mentioned_apis, "block_excerpt": block_text[:400]},
            ))
    return findings


# ============================================================================
# 场景 2：plan F1-F4 self-audit 误标
# ============================================================================

# 匹配 F1-F4 段落标题（## F1. ... 或 - [ ] F1. ...）
F_SECTION_PATTERN = re.compile(
    r"^[\s\-]*\[\s*([ xX])\s*\]\s*F([1-4])\.\s*(.+?)$",
    re.MULTILINE,
)
# 匹配 self-audit 关键词
SELF_AUDIT_PATTERN = re.compile(
    r"replaced\s+by\s+self[\-_ ]audit|self[\-_ ]administered|self[\-_ ]attestation",
    re.IGNORECASE,
)
# 匹配未填充占位符（典型字面量）
PLACEHOLDER_PATTERN = re.compile(
    r"\[N/N\]|\[APPROVE/REJECT\]|\[BLOCKED-env\]|\[PASS\]|\[\s*TODO\s*\]",
)


def scan_scenario2(root: Path) -> List[Dict[str, Any]]:
    """场景 2：扫描 plan F1-F4 self-audit 状态"""
    findings: List[Dict[str, Any]] = []
    plan_path = root / ".omo" / "plans" / "project-organization.md"
    if not plan_path.exists():
        return findings

    text = read_text(plan_path)
    if text is None:
        return findings

    file_rel = rel(plan_path, root)
    lines = text.splitlines()

    # 收集每个 F 段落的元数据：起始行、结束行、勾选状态
    sections: List[Tuple[str, int, int, str]] = []  # (id, start_line, end_line, title)
    matches = list(F_SECTION_PATTERN.finditer(text))
    for idx, m in enumerate(matches):
        checked = m.group(1).lower() == "x"
        f_id = f"F{m.group(2)}"
        title = m.group(3).strip()
        start_line = text.count("\n", 0, m.start()) + 1
        end_line = (
            text.count("\n", 0, matches[idx + 1].start()) + 1
            if idx + 1 < len(matches)
            else len(lines) + 1
        )
        section_text = "\n".join(lines[start_line - 1:end_line - 1])
        sections.append((f_id, start_line, end_line, title))
        has_self_audit = bool(SELF_AUDIT_PATTERN.search(section_text))
        has_placeholder = bool(PLACEHOLDER_PATTERN.search(section_text))

        # DRIFT 条件 A：[x] 标记 + self-audit + 未填充占位符 → 误标完成（最严重）
        if checked and has_self_audit and has_placeholder:
            findings.append(make_finding(
                scenario=2,
                severity=SEVERITY_DRIFT,
                file=file_rel,
                line=start_line,
                summary=(
                    f"{f_id} 标 [x] 但实际是 self-audit 且含未填充占位符 — "
                    f"误标完成"
                ),
                details={
                    "f_id": f_id,
                    "title": title,
                    "checked": True,
                    "has_self_audit": True,
                    "has_placeholder": True,
                    "section_excerpt": section_text[:400],
                },
            ))
        # WARNING 条件 A：[x] + self-audit 但无占位符（仍需独立验证）
        elif checked and has_self_audit:
            findings.append(make_finding(
                scenario=2,
                severity=SEVERITY_WARNING,
                file=file_rel,
                line=start_line,
                summary=(
                    f"{f_id} 标 [x] 且使用 self-audit — 需独立 oracle 重做"
                ),
                details={
                    "f_id": f_id,
                    "title": title,
                    "checked": True,
                    "has_self_audit": True,
                    "has_placeholder": False,
                    "section_excerpt": section_text[:400],
                },
            ))
        # DRIFT 条件 B：self-audit + 未填充占位符（即使 [ ] 标记也是结构性问题）
        elif has_self_audit and has_placeholder:
            findings.append(make_finding(
                scenario=2,
                severity=SEVERITY_DRIFT,
                file=file_rel,
                line=start_line,
                summary=(
                    f"{f_id} 使用 self-audit 但含未填充占位符 [N/N] — "
                    f"output template 未填写，不构成真实 verification"
                ),
                details={
                    "f_id": f_id,
                    "title": title,
                    "checked": False,
                    "has_self_audit": True,
                    "has_placeholder": True,
                    "section_excerpt": section_text[:400],
                },
            ))
    return findings


# ============================================================================
# 场景 3：engine.h includes vs 注释自相矛盾
# ============================================================================

# 匹配 engine.h 中"完整解耦/FULL decoupling" 等声明
FULL_DECOUPLING_PATTERN = re.compile(
    r"FULL\s+decoupling|完整解耦|0\s*个\s*include|全部移除|无\s*跨模块",
    re.IGNORECASE,
)
# 匹配"X/Y modules/ removed" 这种声称
REMOVED_COUNT_PATTERN = re.compile(
    r"(\d+)\s*/\s*(\d+)\s*(?:deep\s+)?modules?/?\s*removed",
    re.IGNORECASE,
)


def scan_scenario3(root: Path) -> List[Dict[str, Any]]:
    """场景 3：扫描 src/core/engine.h 中注释与 #include 不一致"""
    findings: List[Dict[str, Any]] = []
    engine_path = root / "src" / "core" / "engine.h"
    if not engine_path.exists():
        return findings

    text = read_text(engine_path)
    if text is None:
        return findings

    file_rel = rel(engine_path, root)
    lines = text.splitlines()

    # 收集所有 #include 行（含其行号）
    cross_module_includes: List[int] = []
    all_includes: List[int] = []
    for idx, line in enumerate(lines, start=1):
        m = re.match(r'^\s*#include\s+"([^"]+)"', line)
        if m:
            all_includes.append(idx)
            inc_path = m.group(1)
            # 跨模块 include：引用 modules/ 或 common/ 子目录（不是顶层 agenticdsl/）
            if (inc_path.startswith("modules/") or
                inc_path.startswith("common/") or
                inc_path.startswith("agenticdsl/contract/")):
                cross_module_includes.append(idx)

    # 检测 1：注释声称"FULL decoupling" 但仍有跨模块 include
    full_decoupling_claims: List[int] = []
    for idx, line in enumerate(lines, start=1):
        if FULL_DECOUPLING_PATTERN.search(line):
            full_decoupling_claims.append(idx)
    if full_decoupling_claims and cross_module_includes:
        findings.append(make_finding(
            scenario=3,
            severity=SEVERITY_DRIFT,
            file=file_rel,
            line=full_decoupling_claims[0],
            summary=(
                f"注释声称 FULL decoupling，但实际仍存在 {len(cross_module_includes)} 个跨模块 include"
            ),
            details={
                "claim_lines": full_decoupling_claims,
                "cross_module_include_lines": cross_module_includes,
                "cross_module_count": len(cross_module_includes),
            },
        ))

    # 检测 2："X/Y modules/ removed" 与 line 22 等位置的"X leaf remains"自相矛盾
    for m in REMOVED_COUNT_PATTERN.finditer(text):
        claim_line = text.count("\n", 0, m.start()) + 1
        # 在 claim 行 ±15 行范围内查找"X leaf ... remain"反例
        start = max(0, claim_line - 15)
        end = min(len(lines), claim_line + 15)
        nearby = "\n".join(lines[start:end])
        if re.search(r"\d+\s*leaf\s+(?:modules?/?\s*)?(?:remain|remaining|left|保留)", nearby, re.IGNORECASE):
            findings.append(make_finding(
                scenario=3,
                severity=SEVERITY_DRIFT,
                file=file_rel,
                line=claim_line,
                summary=(
                    f"line {claim_line} 声称 '{m.group(0)}'，但附近存在 'leaf remains' 声明 — 内部矛盾"
                ),
                details={
                    "claim": m.group(0),
                    "claim_line": claim_line,
                    "contradiction": "leaf modules/ remain",
                },
            ))

    # 检测 3：line 7/12 声称"X/Y deep modules removed" 与实际 include 数量不符
    # 已知矛盾模式：声称 3/3 deep modules removed 但仍有 1 个 modules/ include
    modules_includes = [i for i in cross_module_includes
                        if any(i == idx and "modules/" in line
                               for idx, line in enumerate(lines, start=1))]
    if modules_includes:
        # 仅在附近注释声称"全部 deep modules removed"时才报
        for idx, line in enumerate(lines, start=1):
            if re.search(r"\d+\s*/\s*\d+\s*deep\s+modules?\s*removed", line, re.IGNORECASE):
                if modules_includes:
                    findings.append(make_finding(
                        scenario=3,
                        severity=SEVERITY_DRIFT,
                        file=file_rel,
                        line=idx,
                        summary=(
                            f"line {idx} 声称 deep modules/ 已全部移除，但仍有 {len(modules_includes)} 个 modules/ include"
                        ),
                        details={
                            "claim_line": idx,
                            "modules_include_lines": modules_includes,
                        },
                    ))
                    break  # 仅首次匹配
    return findings


# ============================================================================
# 场景 4：ADR 声称实现 vs 代码 grep
# ============================================================================

ADR_PATTERN = re.compile(r"^adr-(\d{4})-.*\.md$")
# ADR 状态行：## 状态  下方的 **✅ Approved** 或 **🟡 Partial**
STATUS_LINE_PATTERN = re.compile(
    r"^\s*\*\*(?:✅\s*Approved|🟡\s*Partial)(?:\s*\([^)]*\))?\s*\*\*",
    re.MULTILINE,
)
# ADR 状态行："❌ Not Implemented" / "❌ 未实施"
NOT_IMPLEMENTED_PATTERN = re.compile(
    r"❌\s*(?:Not\s*Implemented|未实施)",
    re.IGNORECASE,
)


def extract_classes_from_adr(text: str) -> List[str]:
    """
    从 ADR 正文中提取描述的类名（PascalCase）。
    仅提取在 ## 决策 或 ## 背景 章节中、且被多次提及或带"class"/"struct"前缀的。
    """
    classes: Dict[str, int] = {}
    # 1. 显式 class/struct 定义
    for m in re.finditer(r"\b(?:class|struct)\s+([A-Z][A-Za-z0-9_]+)\b", text):
        classes[m.group(1)] = classes.get(m.group(1), 0) + 5
    # 2. 反引号包裹的 CamelCase 标识符
    for m in re.finditer(r"`([A-Z][A-Za-z0-9_]+)`", text):
        name = m.group(1)
        # 过滤明显不是类的（标题、章节前缀）
        if name in {"ADR", "FTXUI", "TODO", "NOTE", "API", "JSON", "YAML", "URL"}:
            continue
        classes[name] = classes.get(name, 0) + 1
    # 仅保留提及 ≥ 2 次或显式 class/struct 的
    return sorted([k for k, v in classes.items() if v >= 2])


def find_implementation(name: str, root: Path) -> Optional[str]:
    """在 src/ 和 include/ 下查找类/函数定义（同场景 1 逻辑）"""
    return find_class_definition(name, root)


def scan_scenario4(root: Path) -> List[Dict[str, Any]]:
    """场景 4：扫描 docs/adr/*.md 中 ADR 状态与代码实现一致性"""
    findings: List[Dict[str, Any]] = []
    adr_dir = root / "docs" / "adr"
    if not adr_dir.exists():
        return findings

    for adr_path in sorted(adr_dir.iterdir()):
        if not adr_path.is_file():
            continue
        if not ADR_PATTERN.match(adr_path.name):
            continue
        # 跳过 impl-scope 配套文件
        if "-impl-scope" in adr_path.name:
            continue

        text = read_text(adr_path)
        if text is None:
            continue
        file_rel = rel(adr_path, root)

        # 提取状态
        approved_match = STATUS_LINE_PATTERN.search(text)
        not_impl_match = NOT_IMPLEMENTED_PATTERN.search(text)
        if not approved_match:
            # 没有 Approved/Partial 状态 → 跳过（不属于本场景）
            continue
        status_line = approved_match.group(0).strip()

        # 提取类名
        candidates = extract_classes_from_adr(text)
        if not candidates:
            continue

        missing: List[str] = []
        found: List[Tuple[str, str]] = []
        for name in candidates:
            loc = find_implementation(name, root)
            if loc:
                found.append((name, loc))
            else:
                missing.append(name)

        if not missing:
            # 全部实现存在 → ACCURATE（不报）
            continue

        # 存在未实现类 → 若 impl-scope 文档已存在则不报 DRIFT（已文档化解决）
        impl_scope_path = adr_path.with_name(
            adr_path.stem + "-impl-scope.md"
        )
        if impl_scope_path.exists():
            # impl-scope 文档已创建，drift 已文档化，不再报告
            continue

        findings.append(make_finding(
            scenario=4,
            severity=SEVERITY_DRIFT,
            file=file_rel,
            line=text.count("\n", 0, approved_match.start()) + 1,
            summary=(
                f"ADR 声称 {status_line}，但 {len(missing)}/{len(candidates)} 个描述的类未在 src/include 中找到"
            ),
            details={
                "status": status_line,
                "missing_classes": missing,
                "found_classes": [{"name": n, "location": loc} for n, loc in found],
                "total_candidates": len(candidates),
                "impl_scope_exists": impl_scope_path.exists(),
                "impl_scope_path": rel(impl_scope_path, root),
            },
        ))
    return findings


# ============================================================================
# 场景 5：proposal-approved.md 已批准提案 vs archive 一致性
# ============================================================================

# 匹配表格式行中的提案条目：| [name](improvements/name.md) | ... |
TABLE_ENTRY_PATTERN = re.compile(
    r"\|\s*\[([^\]]+)\]\(improvements/([^)]+)\)\s*\|"
)


def _to_base_name(dirname: str) -> str:
    """去掉日期前缀（YYYY-MM-DD-），返回裸 name。无日期前缀则原样返回。"""
    m = re.match(r'^\d{4}-\d{2}-\d{2}-(.+)$', dirname)
    return m.group(1) if m else dirname


def scan_scenario5(root: Path) -> List[Dict[str, Any]]:
    """
    场景 5：扫描 proposal-approved.md 中 §已批准提案 vs archive 归档目录

    DRIFT 条件：entry 仍在 §已批准提案 但 archive/ 已有对应目录 → 应移入 §已实施
    WARNING 条件：entry 在 §已实施 但 archive/ 找不到对应目录 → 声明可能有误
    """
    findings: List[Dict[str, Any]] = []
    proposal_path = root / "proposal-approved.md"
    if not proposal_path.exists():
        return findings

    text = read_text(proposal_path)
    if text is None:
        return findings

    file_rel = rel(proposal_path, root)
    archive_dir = root / "openspec" / "changes" / "archive"
    if not archive_dir.exists():
        return findings

    # 收集 archive/ 下所有目录名（含日期前缀和裸名）
    archived_bases: dict[str, str] = {}
    for d in archive_dir.iterdir():
        if d.is_dir():
            base = _to_base_name(d.name)
            archived_bases[base] = d.name

    lines = text.splitlines()
    in_approved = False
    in_implemented = False

    for idx, line in enumerate(lines, start=1):
        stripped = line.strip()

        if stripped == "## 已批准提案":
            in_approved = True
            in_implemented = False
            continue
        elif stripped.startswith("## "):
            if stripped == "## 已实施":
                in_implemented = True
            else:
                in_implemented = False
            in_approved = False
            continue

        if not in_approved and not in_implemented:
            continue

        m = TABLE_ENTRY_PATTERN.match(line)
        if not m:
            continue

        entry_name = m.group(1)
        entry_md = m.group(2)

        if in_approved:
            if entry_name in archived_bases:
                findings.append(make_finding(
                    scenario=5,
                    severity=SEVERITY_DRIFT,
                    file=file_rel,
                    line=idx,
                    summary=(
                        f"「{entry_name}」仍在 §已批准提案，但 "
                        f"archive/{archived_bases[entry_name]}/ 已存在 — 应移入 §已实施"
                    ),
                    details={
                        "entry_name": entry_name,
                        "entry_line": idx,
                        "archived_as": archived_bases[entry_name],
                    },
                ))

        elif in_implemented:
            # entry 在 §已实施 → 验证 archive/ 存在
            if entry_name not in archived_bases:
                findings.append(make_finding(
                    scenario=5,
                    severity=SEVERITY_WARNING,
                    file=file_rel,
                    line=idx,
                    summary=(
                        f"「{entry_name}」声明为已实施，但 archive/ 中找不到对应归档目录"
                    ),
                    details={
                        "entry_name": entry_name,
                        "entry_line": idx,
                    },
                ))

    return findings


# ============================================================================
# 报告输出
# ============================================================================

def format_human_report(findings: List[Dict[str, Any]], root: Path) -> str:
    """格式化为人类可读报告"""
    lines: List[str] = []
    lines.append("=" * 80)
    lines.append("HydraForge Documentation-Code Drift Audit Report")
    lines.append(f"Project root: {root}")
    lines.append(f"Scan time: {datetime.now(timezone.utc).isoformat(timespec='seconds')}")
    lines.append("=" * 80)
    lines.append("")

    # 按场景分组
    by_scenario: Dict[int, List[Dict[str, Any]]] = {1: [], 2: [], 3: [], 4: [], 5: []}
    for f in findings:
        by_scenario[f["scenario"]].append(f)

    drift_count = sum(1 for f in findings if f["severity"] == SEVERITY_DRIFT)
    warning_count = sum(1 for f in findings if f["severity"] == SEVERITY_WARNING)

    for scenario_num in (1, 2, 3, 4, 5):
        scenario_findings = by_scenario[scenario_num]
        lines.append(f"[Scenario {scenario_num}] {SCENARIO_NAMES[scenario_num]}")
        if not scenario_findings:
            lines.append("  ✓ 无 drift")
            lines.append("")
            continue
        for f in scenario_findings:
            icon = "❌ DRIFT" if f["severity"] == SEVERITY_DRIFT else "⚠ WARNING"
            location = f"{f['file']}:{f['line']}"
            lines.append(f"  {icon}: {location}")
            lines.append(f"     - {f['summary']}")
            details = f.get("details", {})
            # 场景 1：列出每个 API 的实际状态
            if scenario_num == 1 and "mentioned_apis" in details:
                for api in details["mentioned_apis"]:
                    if api["actual_status"] == "EXISTS":
                        lines.append(
                            f"     - Mentioned: {api['name']} → ACTUALLY EXISTS at {api['location']}"
                        )
                    else:
                        lines.append(
                            f"     - Mentioned: {api['name']} → ACTUALLY DELETED (not found)"
                        )
            # 场景 4：列出 missing classes 与建议
            if scenario_num == 4 and "missing_classes" in details:
                for cls in details["missing_classes"]:
                    lines.append(f"     - Missing class: `{cls}`")
                if details.get("impl_scope_path"):
                    lines.append(
                        f"     - Suggested: create {details['impl_scope_path']} "
                        f"(exists: {details.get('impl_scope_exists', False)})"
                    )
            lines.append("")
        lines.append("")

    lines.append("=" * 80)
    lines.append(f"SUMMARY: {drift_count} DRIFT items, {warning_count} WARNING items detected")
    for scenario_num in (1, 2, 3, 4, 5):
        s_drift = sum(1 for f in by_scenario[scenario_num] if f["severity"] == SEVERITY_DRIFT)
        s_warn = sum(1 for f in by_scenario[scenario_num] if f["severity"] == SEVERITY_WARNING)
        lines.append(f"  Scenario {scenario_num}: {s_drift} drifts, {s_warn} warnings")
    lines.append("=" * 80)
    return "\n".join(lines)


def format_json_report(findings: List[Dict[str, Any]], root: Path) -> str:
    """格式化为 JSON 报告"""
    by_scenario: Dict[str, int] = {"1": 0, "2": 0, "3": 0, "4": 0, "5": 0}
    drift_total = 0
    for f in findings:
        if f["severity"] == SEVERITY_DRIFT:
            by_scenario[str(f["scenario"])] += 1
            drift_total += 1
    report = {
        "scan_time": datetime.now(timezone.utc).isoformat(timespec="seconds"),
        "project_root": str(root),
        "drifts": findings,
        "summary": {
            "total_drifts": drift_total,
            "by_scenario": by_scenario,
        },
    }
    return json.dumps(report, indent=2, ensure_ascii=False)


# ============================================================================
# 主入口
# ============================================================================

def run_audit(root: Path) -> List[Dict[str, Any]]:
    """执行全部 5 个场景，返回 findings 列表"""
    findings: List[Dict[str, Any]] = []
    findings.extend(scan_scenario1(root))
    findings.extend(scan_scenario2(root))
    findings.extend(scan_scenario3(root))
    findings.extend(scan_scenario4(root))
    findings.extend(scan_scenario5(root))
    return findings


def main(argv: Optional[List[str]] = None) -> int:
    parser = argparse.ArgumentParser(
        description="HydraForge 文档-代码漂移审计工具",
        formatter_class=argparse.RawDescriptionHelpFormatter,
    )
    parser.add_argument(
        "project_root",
        nargs="?",
        default=".",
        help="项目根目录（默认: 当前目录）",
    )
    parser.add_argument(
        "--json",
        action="store_true",
        help="输出 JSON 格式报告",
    )
    args = parser.parse_args(argv)

    root = Path(args.project_root).resolve()
    if not root.exists():
        print(f"ERROR: project root 不存在: {root}", file=sys.stderr)
        return 2

    findings = run_audit(root)
    drift_count = sum(1 for f in findings if f["severity"] == SEVERITY_DRIFT)

    if args.json:
        print(format_json_report(findings, root))
    else:
        print(format_human_report(findings, root))

    # 退出码：发现 DRIFT → 1，否则 → 0
    return 1 if drift_count > 0 else 0


if __name__ == "__main__":
    sys.exit(main())